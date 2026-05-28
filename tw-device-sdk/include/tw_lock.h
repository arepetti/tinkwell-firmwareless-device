/*
 * tw_lock.h -- Lightweight lock abstraction for SDK thread safety.
 *
 * When CONFIG_TW_THREAD_SAFETY is enabled (default), these macros
 * expand to pal_mutex_* calls.  When disabled, they compile to no-ops
 * for single-threaded / resource-constrained builds.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_LOCK_H
#define TW_LOCK_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build-time switch: non-zero = real mutexes (default); zero = stubs for single-threaded firmware.
 *
 * Normally supplied by Kconfig as `CONFIG_TW_THREAD_SAFETY`; the default below preserves
 * thread-safe SDK behavior when the build system omits the symbol.
 */
#ifndef CONFIG_TW_THREAD_SAFETY
#define CONFIG_TW_THREAD_SAFETY 1
#endif

#if CONFIG_TW_THREAD_SAFETY

#include "pal_os.h"

/** @brief Mutex handle; aliases ::pal_mutex_t when thread safety is enabled. */
typedef pal_mutex_t tw_lock_t;

/**
 * @brief Allocates and initializes a mutex for SDK or application critical sections.
 * @param lock Out pointer receiving the mutex handle.
 * @retval TW_OK on success.
 * @retval Other ::tw_err_t from ::pal_mutex_create on failure.
 */
static inline tw_err_t tw_lock_init(tw_lock_t *lock)
{
    return pal_mutex_create(lock);
}


/**
 * @brief Acquires the mutex, blocking until available.
 * @param lock Mutex from ::tw_lock_init; NULL is ignored (no-op).
 */
static inline void tw_lock_acquire(tw_lock_t lock)
{
    if (lock) pal_mutex_lock(lock);
}

/**
 * @brief Releases the mutex.
 * @param lock Mutex held by the caller; NULL is ignored (no-op).
 */
static inline void tw_lock_release(tw_lock_t lock)
{
    if (lock) pal_mutex_unlock(lock);
}

/**
 * @brief Destroys the mutex and releases OS resources.
 * @param lock Mutex to destroy; must not be used afterward.
 */
static inline void tw_lock_destroy(tw_lock_t lock)
{
    pal_mutex_destroy(lock);
}

#else /* CONFIG_TW_THREAD_SAFETY == 0 */

/** @brief Stub handle when thread safety is disabled (no underlying mutex). */
typedef void *tw_lock_t;

/**
 * @brief No-op initializer: succeeds without allocating a mutex.
 * @param lock Unused when stubs are enabled.
 * @retval TW_OK always.
 */
static inline tw_err_t tw_lock_init(tw_lock_t *lock)
{
    (void)lock;
    return TW_OK;
}

/**
 * @brief No-op acquire when thread safety is disabled.
 * @param lock Unused.
 */
static inline void tw_lock_acquire(tw_lock_t lock) { (void)lock; }

/**
 * @brief No-op release when thread safety is disabled.
 * @param lock Unused.
 */
static inline void tw_lock_release(tw_lock_t lock) { (void)lock; }

/**
 * @brief No-op destroy when thread safety is disabled.
 * @param lock Unused.
 */
static inline void tw_lock_destroy(tw_lock_t lock) { (void)lock; }

#endif /* CONFIG_TW_THREAD_SAFETY */

#ifdef __cplusplus
}
#endif

#endif /* TW_LOCK_H */
