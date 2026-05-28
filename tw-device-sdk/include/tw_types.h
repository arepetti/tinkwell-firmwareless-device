/*
 * tw_types.h -- Common types, error codes, and macros for the TW Device SDK.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_TYPES_H
#define TW_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Error codes
 * -------------------------------------------------------------------------*/

/** @brief Signed status type returned by most SDK APIs. */
typedef int32_t tw_err_t;

/** @brief Operation completed successfully. */
#define TW_OK            ((tw_err_t)  0)
/** @brief Insufficient memory or storage for the requested operation. */
#define TW_ERR_NOMEM     ((tw_err_t) -1)
/** @brief An argument or state was invalid for the operation. */
#define TW_ERR_INVAL     ((tw_err_t) -2)
/** @brief A low-level I/O or transport operation failed. */
#define TW_ERR_IO        ((tw_err_t) -3)
/** @brief The operation did not complete within the allowed time. */
#define TW_ERR_TIMEOUT   ((tw_err_t) -4)
/** @brief A resource is busy; the operation cannot proceed now. */
#define TW_ERR_BUSY      ((tw_err_t) -5)
/** @brief The requested item or key does not exist. */
#define TW_ERR_NOT_FOUND ((tw_err_t) -6)
/** @brief A prerequisite (initialization, provisioning, hardware) is not satisfied. */
#define TW_ERR_NOT_READY ((tw_err_t) -7)
/** @brief The peer or policy refused the operation. */
#define TW_ERR_REFUSED   ((tw_err_t) -8)
/** @brief A buffer or numeric range would be exceeded. */
#define TW_ERR_OVERFLOW  ((tw_err_t) -9)
/** @brief The operation was aborted or superseded before completion. */
#define TW_ERR_CANCELLED ((tw_err_t) -10)

/**
 * @brief Tests whether an SDK status code indicates success.
 * @param err Status code from an SDK API.
 * @return True if @a err equals ::TW_OK; false otherwise.
 */
static inline bool tw_ok(tw_err_t err) { return err == TW_OK; }

/* ---------------------------------------------------------------------------
 * Utility macros
 * -------------------------------------------------------------------------*/

/** @brief Marks a parameter or variable as intentionally unused (silences warnings). */
#define TW_UNUSED(x)       ((void)(x))
/** @brief Number of elements in a fixed-size array (compile-time). */
#define TW_ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))
/** @brief Lesser of two values, evaluated twice; use only with side-effect-free expressions. */
#define TW_MIN(a, b)       (((a) < (b)) ? (a) : (b))
/** @brief Greater of two values, evaluated twice; use only with side-effect-free expressions. */
#define TW_MAX(a, b)       (((a) > (b)) ? (a) : (b))
/** @brief Clamps @a v to the inclusive range [@a lo, @a hi]. */
#define TW_CLAMP(v, lo, hi) TW_MIN(TW_MAX((v), (lo)), (hi))

/*
 * Hub command types -- REMOVED.
 *
 * The old tw_cmd_type_t enum and tw_hub_command_t struct have been
 * replaced by per-endpoint CoAP handlers in svc_cmd.c.  Commands
 * arrive as individual hub-pushed CoAP POSTs with protobuf payloads.
 * Application code now uses the on_command callback in tw_device_config_t.
 */

#ifdef __cplusplus
}
#endif

#endif /* TW_TYPES_H */
