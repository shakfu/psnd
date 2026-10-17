/**
 * @file bog_async.c
 * @brief Asynchronous scheduler tick thread for Bog REPL.
 *
 * Runs a background thread that continuously ticks the Bog scheduler,
 * allowing the REPL to remain responsive during live coding.
 */

#include "bog_async.h"
#include "scheduler.h"
#include "livecoding.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
/* Windows threading compatibility */
typedef HANDLE pthread_t;
#define pthread_create(t, attr, func, arg) \
    ((*(t) = (HANDLE)_beginthreadex(NULL, 0, func, arg, 0, NULL)) == 0 ? -1 : 0)
#define pthread_join(t, retval) (WaitForSingleObject(t, INFINITE), CloseHandle(t))
#define usleep(us) Sleep((us) / 1000)
typedef SRWLOCK bog_mutex_t;
#define BOG_MUTEX_INIT SRWLOCK_INIT
#define bog_mutex_lock(m) AcquireSRWLockExclusive(m)
#define bog_mutex_unlock(m) ReleaseSRWLockExclusive(m)
/* MSVC atomic compatibility */
#define atomic_int volatile int
#define atomic_store(ptr, val) (*(ptr) = (val))
#define atomic_load(ptr) (*(ptr))
#else
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
typedef pthread_mutex_t bog_mutex_t;
#define BOG_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#define bog_mutex_lock(m) pthread_mutex_lock(m)
#define bog_mutex_unlock(m) pthread_mutex_unlock(m)
#endif

/* ============================================================================
 * Thread State
 * ============================================================================ */

static pthread_t g_tick_thread;
static atomic_int g_running = 0;
static atomic_int g_initialized = 0;

static BogScheduler *g_scheduler = NULL;
static BogTransitionManager *g_transition = NULL;

/* Held by the tick thread for each tick; see bog_async_lock() */
static bog_mutex_t g_lock = BOG_MUTEX_INIT;

/* Tick interval in microseconds (10ms) */
#define TICK_INTERVAL_US 10000

/* ============================================================================
 * Tick Thread
 * ============================================================================ */

#ifdef _WIN32
static unsigned __stdcall tick_thread_func(void *arg) {
#else
static void *tick_thread_func(void *arg) {
#endif
    (void)arg;

    while (atomic_load(&g_running)) {
        bog_mutex_lock(&g_lock);
        /* Tick scheduler */
        if (g_scheduler) {
            bog_scheduler_tick(g_scheduler);
        }

        /* Process transitions */
        if (g_transition && g_scheduler) {
            double now = bog_scheduler_now(g_scheduler);
            bog_transition_manager_process(g_transition, now);
        }
        bog_mutex_unlock(&g_lock);

        /* Sleep before next tick - check running flag periodically for quick shutdown */
        for (int i = 0; i < 10 && atomic_load(&g_running); i++) {
            usleep(TICK_INTERVAL_US / 10);
        }
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int bog_async_init(void) {
    if (atomic_load(&g_initialized)) {
        return 0;  /* Already initialized */
    }

    g_scheduler = NULL;
    g_transition = NULL;
    atomic_store(&g_running, 0);
    atomic_store(&g_initialized, 1);

    return 0;
}

void bog_async_cleanup(void) {
    if (!atomic_load(&g_initialized)) {
        return;
    }

    bog_async_stop();

    g_scheduler = NULL;
    g_transition = NULL;
    atomic_store(&g_initialized, 0);
}

int bog_async_start(BogScheduler *scheduler, BogTransitionManager *transition) {
    if (!atomic_load(&g_initialized)) {
        if (bog_async_init() != 0) {
            return -1;
        }
    }

    if (atomic_load(&g_running)) {
        /* Already running - update references */
        g_scheduler = scheduler;
        g_transition = transition;
        return 0;
    }

    g_scheduler = scheduler;
    g_transition = transition;
    atomic_store(&g_running, 1);

    if (pthread_create(&g_tick_thread, NULL, tick_thread_func, NULL) != 0) {
        atomic_store(&g_running, 0);
        return -1;
    }

    return 0;
}

void bog_async_stop(void) {
    if (!atomic_load(&g_running)) {
        return;
    }

    atomic_store(&g_running, 0);
    pthread_join(g_tick_thread, NULL);
}

void bog_async_lock(void) {
    bog_mutex_lock(&g_lock);
}

void bog_async_unlock(void) {
    bog_mutex_unlock(&g_lock);
}

int bog_async_is_running(void) {
    return atomic_load(&g_running);
}

int bog_async_get_current_source_line(void) {
    if (!g_scheduler || !atomic_load(&g_running)) {
        return 0;
    }
    bog_mutex_lock(&g_lock);
    int line = bog_scheduler_get_current_source_line(g_scheduler);
    bog_mutex_unlock(&g_lock);
    return line;
}
