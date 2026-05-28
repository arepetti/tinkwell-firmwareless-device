/*
 * pal_os.h -- OS primitives: tasks, mutexes, semaphores, timers.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_OS_H
#define PAL_OS_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Mutex --- */

/** @brief Opaque mutex handle (recursive or non-recursive per implementation; document locally). */
typedef struct pal_mutex *pal_mutex_t;

/**
 * @brief Allocate and initialize a mutex.
 *
 * Backend contract: Create a mutex object and store a valid handle in @p *out. The mutex is
 * initially unlocked. Destroy with pal_mutex_destroy when no longer needed.
 *
 * Thread-safety: Safe to call from one init context; not required to be safe concurrent with use of @p *out.
 *
 * @param out Non-NULL pointer to receive the new mutex handle.
 * @retval TW_OK       Mutex created.
 * @retval TW_ERR_NOMEM Allocation or OS limit failure.
 * @retval TW_ERR_IO    OS error creating the mutex.
 */
tw_err_t pal_mutex_create(pal_mutex_t *out);

/**
 * @brief Acquire the mutex, blocking until available.
 *
 * Backend contract: Block the calling task until the mutex is acquired. Do not call from ISR
 * unless the implementation explicitly supports it (normally forbidden).
 *
 * Thread-safety: Must not be called with the same mutex held by the same task if non-recursive
 * and the implementation does not support recursion (deadlock).
 *
 * @param m Valid mutex from pal_mutex_create.
 * @retval TW_OK       Lock acquired.
 * @retval TW_ERR_INVAL Invalid @p m.
 * @retval TW_ERR_IO    OS error (e.g. deleted mutex).
 */
tw_err_t pal_mutex_lock(pal_mutex_t m);

/**
 * @brief Release the mutex.
 *
 * Backend contract: Release one lock level held by the current task. Unlocking an unheld mutex
 * is undefined unless documented.
 *
 * @param m Mutex to unlock.
 * @retval TW_OK       Lock released.
 * @retval TW_ERR_INVAL Caller did not hold the lock or @p m invalid.
 * @retval TW_ERR_IO    OS error.
 */
tw_err_t pal_mutex_unlock(pal_mutex_t m);

/**
 * @brief Free a mutex and invalidate its handle.
 *
 * Backend contract: Destroy @p m; subsequent use of @p m is invalid. Do not destroy while
 * another task is blocked on or holding the mutex.
 *
 * Thread-safety: Call only when no other task needs @p m.
 *
 * @param m Mutex to destroy (may be NULL if implementation allows).
 */
void     pal_mutex_destroy(pal_mutex_t m);

/* --- Semaphore --- */

/** @brief Opaque counting semaphore handle. */
typedef struct pal_sem *pal_sem_t;

/**
 * @brief Create a counting semaphore.
 *
 * Backend contract: Initialize semaphore count to @p initial (often 0 for sync, >0 for credits).
 *
 * @param out      Non-NULL pointer to receive the semaphore handle.
 * @param initial  Initial count (non-negative).
 * @retval TW_OK       Semaphore created.
 * @retval TW_ERR_NOMEM Allocation failure.
 * @retval TW_ERR_INVAL Invalid @p initial.
 * @retval TW_ERR_IO    OS error.
 */
tw_err_t pal_sem_create(pal_sem_t *out, unsigned int initial);

/**
 * @brief Wait for the semaphore count to become positive, optionally with a timeout.
 *
 * Backend contract: Decrement count if >0; otherwise block until pal_sem_post or timeout.
 * @p timeout_ms: @c 0 may try once without blocking; negative may mean infinite wait per RTOS.
 * Return success if the wait succeeded.
 *
 * Thread-safety: Do not call from ISR unless documented.
 *
 * @param s          Valid semaphore.
 * @param timeout_ms Maximum time to wait in milliseconds (semantics as above).
 * @retval TW_OK          Acquired.
 * @retval TW_ERR_TIMEOUT Timed out (if applicable).
 * @retval TW_ERR_INVAL   Invalid @p s.
 * @retval TW_ERR_IO      OS error.
 */
tw_err_t pal_sem_wait(pal_sem_t s, int timeout_ms);

/**
 * @brief Increment the semaphore count, potentially unblocking a waiter.
 *
 * Backend contract: Post/signal the semaphore. May wake one waiting task.
 *
 * Thread-safety: Often ISR-safe for binary semaphores; document platform rules.
 *
 * @param s Valid semaphore.
 * @retval TW_OK       Posted.
 * @retval TW_ERR_INVAL Invalid @p s.
 * @retval TW_ERR_IO    OS error (e.g. count overflow if bounded).
 */
tw_err_t pal_sem_post(pal_sem_t s);

/**
 * @brief Destroy a semaphore and release resources.
 *
 * Backend contract: Invalidate @p s. Undefined if tasks are still waiting.
 *
 * @param s Semaphore to destroy.
 */
void     pal_sem_destroy(pal_sem_t s);

/* --- Task --- */

/**
 * @brief Entry point for a PAL task.
 *
 * @param arg User argument passed from pal_task_create.
 */
typedef void (*pal_task_fn_t)(void *arg);

/**
 * @brief Create a new schedulable task (thread).
 *
 * Backend contract: Spawn a task that runs @p fn(@p arg) with a stack of at least @p stack_size
 * bytes and scheduling priority @p priority (larger = higher urgency, or inverse per RTOS—document
 * in the port). @p name is a human-readable label for debuggers.
 *
 * Thread-safety: Safe to call from init or running tasks per RTOS rules; not from ISR.
 *
 * @param name        Task name (NUL-terminated; may be truncated).
 * @param fn          Entry function (must not be NULL).
 * @param arg         Argument passed to @p fn.
 * @param stack_size  Stack size in bytes (implementation may round up).
 * @param priority    Scheduling priority (platform-specific scale).
 * @retval TW_OK       Task started.
 * @retval TW_ERR_NOMEM Could not allocate stack or TCB.
 * @retval TW_ERR_INVAL Invalid parameters.
 * @retval TW_ERR_IO    OS error creating the task.
 */
tw_err_t pal_task_create(const char *name, pal_task_fn_t fn, void *arg,
                         size_t stack_size, int priority);

/* --- Time --- */

/**
 * @brief Delay the current task for a fixed duration.
 *
 * Backend contract: Block for at least @p ms milliseconds (subject to tick resolution).
 * Not for use from ISR.
 *
 * Thread-safety: Task-context only.
 *
 * @param ms Sleep duration in milliseconds.
 */
void     pal_sleep_ms(uint32_t ms);

/**
 * @brief Monotonic time since boot in milliseconds.
 *
 * Backend contract: Return a non-decreasing timestamp suitable for intervals and timeouts.
 * Wrap behavior after UINT64_MAX is acceptable; document if not monotonic across sleep.
 *
 * Thread-safety: Typically safe to call from any context; confirm for the port if called from ISR.
 *
 * @return Milliseconds since boot (monotonic clock).
 */
uint64_t pal_uptime_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_OS_H */
