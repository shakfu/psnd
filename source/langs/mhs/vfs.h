/* vfs.h - Virtual Filesystem for embedded Haskell libraries */

#ifndef VFS_H
#define VFS_H

#include <stdio.h>
#include <stddef.h>
#include <dirent.h>

/* Virtual root path - all embedded files appear under this prefix */
#define VFS_VIRTUAL_ROOT "/mhs-embedded"

/* Initialize VFS.
 * Returns 0 on success, -1 on failure.
 */
int vfs_init(void);

/* Get the virtual root path (for setting MHSDIR).
 * Returns "/mhs-embedded" - a virtual path prefix.
 */
const char* vfs_get_temp_dir(void);

/* Open a file - checks embedded files first, then falls back to filesystem.
 * For paths starting with the virtual root, looks up in embedded files
 * and uses fmemopen() to return a FILE* from memory.
 */
FILE* vfs_fopen(const char* path, const char* mode);

/* Get total number of embedded files */
int vfs_file_count(void);

/* Get total size of embedded content */
size_t vfs_total_size(void);

/* Print VFS statistics */
void vfs_print_stats(void);

/* List all embedded files (for debugging) */
void vfs_list_files(void);

/* Extract all embedded files to a temp directory.
 * Returns the path to the temp directory, or NULL on failure.
 * Use this when compiling to executable (cc needs real files).
 */
char* vfs_extract_to_temp(void);

/* Clean up extracted temp directory */
void vfs_cleanup_temp(char* temp_dir);

/* Directory operations for VFS
 * These intercept opendir/readdir/closedir for virtual paths
 */

/* Open a directory - checks VFS first, then falls back to filesystem */
DIR* vfs_opendir(const char* path);

/* Read next entry from directory */
struct dirent* vfs_readdir(DIR* dirp);

/* Close directory */
int vfs_closedir(DIR* dirp);

/* Whether .mhscache holds nothing worth reusing: it is missing, empty, or
 * older than this binary, which can carry different embedded packages. The
 * empty and stale cases are deleted here.
 *
 * Callers use this to pick both the cache flag and the preload flags. A cold
 * run takes -C and -pbase -pmusic, so the packages land in the cache. A warm
 * run takes -CR and no -p: the packages are already cached, mhs appends
 * another copy of each on every -p run without checking, and writing the
 * cache back costs ~1.8s of LZMA for a file that would not change.
 */
int mhs_cache_is_cold(void);

#endif /* VFS_H */
