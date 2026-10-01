/* test_process.h - Process utilities for integration tests
 *
 * Provides fork/exec based process execution for integration tests,
 * replacing system() calls with proper process management.
 *
 * Features:
 * - Direct process execution without shell
 * - Proper exit code capture
 * - Temporary directory management with nftw cleanup
 */

/* Feature test macros - must be before any system includes.
 * These enable POSIX functions like mkdtemp() and nftw(). */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

#ifndef TEST_PROCESS_H
#define TEST_PROCESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#define TEST_PROC_PATH_SEP '\\'
#else
#include <unistd.h>
#include <sys/wait.h>
#include <ftw.h>
#define TEST_PROC_PATH_SEP '/'
#endif

/* Set an environment variable inherited by processes started with test_exec */
static inline void test_setenv(const char *name, const char *value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

/* Maximum path length for temp directories */
#define TEST_PROC_MAX_PATH 512

/* =============================================================================
 * Process Execution
 * =============================================================================
 */

#ifdef _WIN32
/**
 * Open an inheritable handle to the null device, the Windows spelling of
 * /dev/null.
 *
 * STARTF_USESTDHANDLES with a NULL member does not give the child an empty
 * stream: the CRT falls back to the console CREATE_NO_WINDOW allocated for it,
 * so isatty() reports a terminal and a child that reads stdin blocks forever.
 * A real NUL handle reports non-tty and reads EOF, matching the POSIX branch.
 *
 * @param access  GENERIC_READ for stdin, GENERIC_WRITE for stdout/stderr
 * @return        Handle, or INVALID_HANDLE_VALUE on failure
 */
static inline HANDLE test_proc_open_nul(DWORD access) {
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    return CreateFileA("NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       &sa, OPEN_EXISTING, 0, NULL);
}
#endif

/**
 * Execute a binary with arguments using fork/exec.
 *
 * @param binary_path  Path to the binary to execute
 * @param args         NULL-terminated array of arguments (argv[0] should be program name)
 * @return             Exit code of the process, or -1 on fork/exec failure
 *
 * Example:
 *   char *args[] = {"psnd", "play", "test.joy", NULL};
 *   int result = test_exec(PSND_BINARY, args);
 */
static inline int test_exec(const char *binary_path, char *const args[]) {
#ifdef _WIN32
    /* Build command line from args */
    char cmdline[4096] = {0};
    int pos = 0;
    for (int i = 0; args[i] != NULL; i++) {
        if (i > 0) cmdline[pos++] = ' ';
        pos += snprintf(cmdline + pos, sizeof(cmdline) - pos, "\"%s\"", args[i]);
    }

    /* stdin, stdout and stderr all to NUL, as the POSIX branch does. */
    HANDLE nul_in = test_proc_open_nul(GENERIC_READ);
    HANDLE nul_out = test_proc_open_nul(GENERIC_WRITE);
    if (nul_in == INVALID_HANDLE_VALUE || nul_out == INVALID_HANDLE_VALUE) {
        if (nul_in != INVALID_HANDLE_VALUE) CloseHandle(nul_in);
        if (nul_out != INVALID_HANDLE_VALUE) CloseHandle(nul_out);
        return -1;
    }

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul_in;
    si.hStdOutput = nul_out;
    si.hStdError = nul_out;

    /* STARTF_USESTDHANDLES requires bInheritHandles TRUE; with FALSE the child
       does not receive the handles above. */
    if (!CreateProcessA(binary_path, cmdline, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(nul_in);
        CloseHandle(nul_out);
        return -1;
    }
    CloseHandle(nul_in);
    CloseHandle(nul_out);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else
    pid_t pid = fork();

    if (pid < 0) {
        /* Fork failed */
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        /* Redirect stdout and stderr to /dev/null for quiet tests */
        if (freopen("/dev/null", "w", stdout) == NULL) _exit(127);
        if (freopen("/dev/null", "w", stderr) == NULL) _exit(127);
        /* Give the child an empty stdin. Inheriting the runner's stdin makes a
           child that reads it (psnd drops into a REPL after loading a file)
           either hang or steal the runner's input, depending on what stdin
           happens to be when the suite runs. */
        if (freopen("/dev/null", "r", stdin) == NULL) _exit(127);

        execv(binary_path, args);

        /* execv only returns on error */
        _exit(127);
    }

    /* Parent process - wait for child */
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    /* Process was killed by signal */
    return -1;
#endif
}

/**
 * Execute a binary with arguments, capturing stdout.
 *
 * @param binary_path  Path to the binary to execute
 * @param args         NULL-terminated array of arguments
 * @param output       Buffer to store stdout (can be NULL)
 * @param output_size  Size of output buffer
 * @return             Exit code of the process, or -1 on failure
 */
static inline int test_exec_capture(const char *binary_path, char *const args[],
                                     char *output, size_t output_size) {
#ifdef _WIN32
    /* Build command line from args */
    char cmdline[4096] = {0};
    int pos = 0;
    for (int i = 0; args[i] != NULL; i++) {
        if (i > 0) cmdline[pos++] = ' ';
        pos += snprintf(cmdline + pos, sizeof(cmdline) - pos, "\"%s\"", args[i]);
    }

    /* Create pipe for stdout */
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return -1;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    /* stdin and stderr to NUL (see test_proc_open_nul); stdout to the pipe. */
    HANDLE nul_in = test_proc_open_nul(GENERIC_READ);
    HANDLE nul_err = test_proc_open_nul(GENERIC_WRITE);
    if (nul_in == INVALID_HANDLE_VALUE || nul_err == INVALID_HANDLE_VALUE) {
        if (nul_in != INVALID_HANDLE_VALUE) CloseHandle(nul_in);
        if (nul_err != INVALID_HANDLE_VALUE) CloseHandle(nul_err);
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return -1;
    }

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul_in;
    si.hStdOutput = hWritePipe;
    si.hStdError = nul_err;

    if (!CreateProcessA(binary_path, cmdline, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(nul_in);
        CloseHandle(nul_err);
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return -1;
    }
    CloseHandle(nul_in);
    CloseHandle(nul_err);
    CloseHandle(hWritePipe);

    /* Drain to EOF. ReadFile returns as soon as any bytes are available, so a
       single call truncates output that arrives in more than one chunk. Reads
       past output_size are discarded rather than left in the pipe, so a child
       writing more than the pipe buffer holds does not block on write. */
    {
        size_t total = 0;
        char scratch[4096];
        DWORD bytes_read;
        for (;;) {
            char *dst = scratch;
            DWORD want = (DWORD)sizeof(scratch);
            if (output && output_size > 0 && total < output_size - 1) {
                dst = output + total;
                want = (DWORD)(output_size - 1 - total);
            }
            if (!ReadFile(hReadPipe, dst, want, &bytes_read, NULL) || bytes_read == 0) {
                break;
            }
            if (dst != scratch) total += bytes_read;
        }
        if (output && output_size > 0) output[total] = '\0';
    }
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        close(pipefd[0]);  /* Close read end */
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        /* Redirect stderr and stdin to /dev/null (see test_exec on stdin) */
        if (freopen("/dev/null", "w", stderr) == NULL) _exit(127);
        if (freopen("/dev/null", "r", stdin) == NULL) _exit(127);

        execv(binary_path, args);
        _exit(127);
    }

    /* Parent process */
    close(pipefd[1]);  /* Close write end */

    /* Drain to EOF; see the Windows branch on why one read() is not enough. */
    {
        size_t total = 0;
        char scratch[4096];
        for (;;) {
            char *dst = scratch;
            size_t want = sizeof(scratch);
            if (output && output_size > 0 && total < output_size - 1) {
                dst = output + total;
                want = output_size - 1 - total;
            }
            ssize_t bytes_read = read(pipefd[0], dst, want);
            if (bytes_read < 0 && errno == EINTR) continue;
            if (bytes_read <= 0) break;
            if (dst != scratch) total += (size_t)bytes_read;
        }
        if (output && output_size > 0) output[total] = '\0';
    }
    close(pipefd[0]);

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
#endif
}

/* =============================================================================
 * Temporary Directory Management
 * =============================================================================
 */

/**
 * Create a unique temporary directory for tests.
 *
 * @param prefix  Prefix for the directory name (e.g., "psnd_test")
 * @param path    Buffer to store the created path (must be at least TEST_PROC_MAX_PATH)
 * @return        0 on success, -1 on failure
 *
 * Example:
 *   char temp_dir[TEST_PROC_MAX_PATH];
 *   test_mkdtemp("psnd_test", temp_dir);
 *   // Use temp_dir...
 *   test_rmdir_recursive(temp_dir);
 */
static inline int test_mkdtemp(const char *prefix, char *path) {
#ifdef _WIN32
    char temp_path[MAX_PATH];
    if (GetTempPathA(MAX_PATH, temp_path) == 0) {
        return -1;
    }
    snprintf(path, TEST_PROC_MAX_PATH, "%s%s_%u", temp_path, prefix, (unsigned)GetTickCount());
    if (_mkdir(path) != 0) {
        return -1;
    }
    return 0;
#else
    snprintf(path, TEST_PROC_MAX_PATH, "/tmp/%s_XXXXXX", prefix);
    if (mkdtemp(path) == NULL) {
        return -1;
    }
    return 0;
#endif
}

#ifndef _WIN32
/* Helper callback for nftw to remove files/directories */
static inline int test_rmdir_callback(const char *fpath, const struct stat *sb,
                                       int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)typeflag;
    (void)ftwbuf;
    return remove(fpath);
}
#endif

/**
 * Recursively remove a directory and all its contents.
 *
 * @param path  Path to the directory to remove
 * @return      0 on success, -1 on failure
 *
 * Note: Uses nftw() instead of system("rm -rf ...") for safety and portability.
 */
static inline int test_rmdir_recursive(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return -1;
    }

#ifdef _WIN32
    /* Safety check: don't remove root directories */
    if (strlen(path) <= 3) {  /* C:\ or similar */
        return -1;
    }

    /* Use system command for simplicity on Windows */
    char cmd[TEST_PROC_MAX_PATH + 32];
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\" 2>NUL", path);
    return system(cmd) == 0 ? 0 : -1;
#else
    /* Safety check: don't remove root or home directories */
    if (strcmp(path, "/") == 0 || strcmp(path, getenv("HOME")) == 0) {
        return -1;
    }

    /* Use nftw to traverse and remove */
    return nftw(path, test_rmdir_callback, 64, FTW_DEPTH | FTW_PHYS);
#endif
}

/**
 * Write content to a file in a directory.
 *
 * @param dir      Directory path
 * @param filename Filename (no path separators)
 * @param content  Content to write
 * @return         0 on success, -1 on failure
 */
static inline int test_write_file(const char *dir, const char *filename,
                                   const char *content) {
    char path[TEST_PROC_MAX_PATH];
    int n = snprintf(path, sizeof(path), "%s%c%s", dir, TEST_PROC_PATH_SEP, filename);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        return -1;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        return -1;
    }

    if (fputs(content, f) == EOF) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

/**
 * Build a file path from directory and filename.
 *
 * @param dir      Directory path
 * @param filename Filename
 * @param path     Buffer to store result (must be at least TEST_PROC_MAX_PATH)
 */
static inline void test_build_path(const char *dir, const char *filename, char *path) {
    int n = snprintf(path, TEST_PROC_MAX_PATH, "%s%c%s", dir, TEST_PROC_PATH_SEP, filename);
    if (n < 0 || (size_t)n >= TEST_PROC_MAX_PATH) {
        path[0] = '\0';
    }
}

#endif /* TEST_PROCESS_H */
