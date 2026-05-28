/*
 * pal_log.h -- Logging abstraction.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_LOG_H
#define PAL_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log severity levels for pal_log and convenience macros.
 *
 * Numeric order: lower values are more severe (@c PAL_LOG_ERROR) and higher values are more
 * verbose (@c PAL_LOG_TRACE). Filtering typically enables a minimum level (for example, @c PAL_LOG_INFO
 * suppresses @c PAL_LOG_DEBUG and @c PAL_LOG_TRACE but still shows @c PAL_LOG_ERROR, @c PAL_LOG_WARN,
 * and @c PAL_LOG_INFO).
 */
typedef enum {
    PAL_LOG_ERROR = 0, /**< Fatal or error conditions requiring attention. */
    PAL_LOG_WARN  = 1, /**< Recoverable issues or deprecation notices. */
    PAL_LOG_INFO  = 2, /**< Informational messages for normal operation. */
    PAL_LOG_DEBUG = 3, /**< Detailed diagnostics for development. */
    PAL_LOG_TRACE = 4, /**< Very verbose step-by-step tracing. */
} pal_log_level_t;

/**
 * @brief Emit a log line at the given severity.
 *
 * Backend contract: Format @p fmt using standard printf rules and optional variadic arguments,
 * prefix with @p tag (typically a module name), and route to UART, RTT, syslog, or equivalent.
 * Must treat @p fmt as a format string (callers must not pass untrusted format strings).
 *
 * Thread-safety: Must be safe for concurrent calls from multiple tasks (line atomicity may be
 * best-effort for long messages).
 *
 * @param level Minimum severity of this message; may be filtered by compile-time or runtime config.
 * @param tag   Short module or component name (NUL-terminated ASCII).
 * @param fmt   printf-style format string, followed by arguments matching the specifiers.
 *
 * @note @c PAL_LOG_TRACE has no @c PAL_LOGT-style macro in this header; use
 *       <tt>pal_log(PAL_LOG_TRACE, tag, ...)</tt> for the same pattern as @c PAL_LOGE / @c PAL_LOGD.
 */
void pal_log(pal_log_level_t level, const char *tag, const char *fmt, ...);

/** @brief Log at @c PAL_LOG_ERROR with printf-style arguments after @p tag. */
#define PAL_LOGE(tag, ...) pal_log(PAL_LOG_ERROR, tag, __VA_ARGS__)
/** @brief Log at @c PAL_LOG_WARN with printf-style arguments after @p tag. */
#define PAL_LOGW(tag, ...) pal_log(PAL_LOG_WARN,  tag, __VA_ARGS__)
/** @brief Log at @c PAL_LOG_INFO with printf-style arguments after @p tag. */
#define PAL_LOGI(tag, ...) pal_log(PAL_LOG_INFO,  tag, __VA_ARGS__)
/** @brief Log at @c PAL_LOG_DEBUG with printf-style arguments after @p tag. */
#define PAL_LOGD(tag, ...) pal_log(PAL_LOG_DEBUG, tag, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* PAL_LOG_H */
