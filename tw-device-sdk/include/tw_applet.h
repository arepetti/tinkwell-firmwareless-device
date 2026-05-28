/*
 * tw_applet.h -- Applet lifecycle for the WASM applet runtime.
 *
 * Only meaningful when CONFIG_TW_APPLET_ENABLED is set.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_APPLET_H
#define TW_APPLET_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Lifecycle state of the optional WebAssembly applet runtime. */
typedef enum {
    TW_APPLET_NONE,     /**< No applet loaded or feature disabled at build time. */
    TW_APPLET_LOADING,  /**< Applet binary is being fetched or instantiated. */
    TW_APPLET_RUNNING,  /**< Applet is active and may handle requests. */
    TW_APPLET_ERROR,    /**< Applet failed to load or crashed; see platform logs. */
} tw_applet_state_t;

/**
 * @brief Returns the current applet runtime state.
 * @return One of ::tw_applet_state_t.
 */
tw_applet_state_t tw_applet_state(void);

/**
 * @brief Returns a human-readable applet or bundle version string, if any.
 * @return NUL-terminated version string owned by the SDK; NULL if none or unknown.
 */
const char       *tw_applet_version(void);

#ifdef __cplusplus
}
#endif

#endif /* TW_APPLET_H */
