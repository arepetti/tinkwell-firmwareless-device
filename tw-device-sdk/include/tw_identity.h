/*
 * tw_identity.h -- Device identity: UUID, vendor/product metadata, keys.
 *
 * The identity struct holds factory-provisioned and hub-assigned fields,
 * backed by NVS with compile-time defaults from tw_device_config_t.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_IDENTITY_H
#define TW_IDENTITY_H

#include "tw_types.h"
#include "tw_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Byte length of a binary UUID (RFC 4122 128-bit value). */
#define TW_UUID_SIZE        16
/** @brief Maximum length in bytes of symmetric or private key material stored in identity. */
#define TW_KEY_SIZE         32
/** @brief Maximum length of NUL-terminated display name buffers in ::tw_device_identity_t. */
#define TW_DISPLAY_NAME_MAX 48
/** @brief Buffer size for RFC 4122 string form "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" plus NUL. */
#define TW_UUID_STR_SIZE    37
/** @brief Byte length of an Ed25519 public key (RFC 8032, raw encoding). */
#define TW_PUBKEY_SIZE      32

/* -------------------------------------------------------------------------
 * Identity key type
 * -----------------------------------------------------------------------*/

/** @brief Cryptographic key material associated with the device identity. */
typedef enum {
    TW_KEY_NONE    = 0, /**< No key loaded or key type unknown. */
    TW_KEY_ED25519 = 1, /**< Ed25519 key pair per RFC 8032; 32-byte public key derivable from private. */
    TW_KEY_PSK     = 2, /**< Pre-shared key for symmetric authentication. */
} tw_identity_key_type_t;

/* -------------------------------------------------------------------------
 * Device identity
 * -------------------------------------------------------------------------*/

/** @brief Snapshot of device identity as stored or defaulted (NVS-backed fields). */
typedef struct {
    int32_t  vendor_id;                          /**< Factory OEM/vendor id. */
    int32_t  product_id;                         /**< Factory product id. */
    char     vendor_display_name[TW_DISPLAY_NAME_MAX];  /**< Factory vendor label (UTF-8). */
    char     product_display_name[TW_DISPLAY_NAME_MAX];  /**< Factory product label (UTF-8). */
    uint8_t  variant;                          /**< Hardware or SKU variant. */
    int32_t  serial_number;                      /**< Factory serial number if provisioned. */

    uint8_t  uuid[TW_UUID_SIZE];                 /**< Device UUID in RFC 4122 binary form when valid. */
    bool     uuid_valid;                         /**< True if @a uuid has been set (hub or factory). */

    bool     factory_provisioned;                /**< True if factory identity fields were written. */
    bool     hub_provisioned;                    /**< True if hub-assigned identity (e.g. UUID) is present. */

    tw_identity_key_type_t key_type;             /**< Kind of material in @a key. */
    uint8_t  key[TW_KEY_SIZE];                   /**< Raw key bytes; interpretation depends on @a key_type. */
    bool     key_valid;                          /**< True if @a key contains loaded material. */
} tw_device_identity_t;

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------*/

struct tw_device_config;

/**
 * @brief Loads identity from NVS and applies compile-time defaults from @a cfg.
 * @param cfg Device configuration supplying fallback vendor/product strings and ids.
 * @retval TW_OK if identity subsystem is ready.
 * @retval Other ::tw_err_t on storage or validation failure.
 */
tw_err_t                    tw_identity_init(const struct tw_device_config *cfg);

/**
 * @brief Returns the current in-memory identity snapshot.
 * @return Pointer to static or internal storage; valid until the next identity mutation.
 */
const tw_device_identity_t *tw_identity_get(void);

/**
 * @brief Stores the device UUID (RFC 4122 binary) in NVS and updates runtime state.
 * @param uuid Exactly ::TW_UUID_SIZE bytes; canonical RFC 4122 layout.
 * @retval TW_OK on success.
 * @retval TW_ERR_INVAL if @a uuid is NULL.
 */
tw_err_t tw_identity_set_uuid(const uint8_t uuid[TW_UUID_SIZE]);

/**
 * @brief Copies the stored UUID into @a uuid_out.
 * @param uuid_out Receives ::TW_UUID_SIZE bytes on success.
 * @retval TW_OK if a valid UUID is stored.
 * @retval TW_ERR_NOT_FOUND or ::TW_ERR_NOT_READY if none is provisioned.
 */
tw_err_t tw_identity_get_uuid(uint8_t uuid_out[TW_UUID_SIZE]);

/**
 * @brief Formats a binary UUID as RFC 4122 hyphenated lowercase hex.
 * @param uuid Source ::TW_UUID_SIZE-byte UUID (RFC 4122 layout).
 * @param buf Output buffer for the NUL-terminated string.
 * @param buf_size Size of @a buf in bytes; must be at least ::TW_UUID_STR_SIZE including NUL.
 */
void tw_identity_uuid_to_str(const uint8_t uuid[TW_UUID_SIZE],
                             char *buf, size_t buf_size);

/**
 * @brief Returns the type of key material currently associated with the identity.
 * @return Current ::tw_identity_key_type_t from stored identity.
 */
tw_identity_key_type_t tw_identity_get_key_type(void);

/**
 * @brief Returns whether any key material is marked valid.
 * @return True if a key is loaded and valid; false otherwise.
 */
bool                   tw_identity_has_key(void);

/**
 * @brief Derives the Ed25519 public key (RFC 8032) from the stored private key.
 * @param pubkey_out Must point to at least ::TW_PUBKEY_SIZE bytes.
 * @retval TW_OK on success.
 * @retval TW_ERR_INVAL if @a key_type is not ::TW_KEY_ED25519 or no key is loaded.
 */
tw_err_t tw_identity_get_ed25519_pubkey(uint8_t pubkey_out[TW_PUBKEY_SIZE]);

/* -------------------------------------------------------------------------
 * Internal (called by svc_provision / svc_device -- not for applications)
 * -------------------------------------------------------------------------*/

/**
 * @brief Atomically sets factory identity and optional UUID/key (SDK provisioning services only).
 * @param vendor_id OEM vendor identifier.
 * @param product_id Product identifier within the vendor namespace.
 * @param vendor_name NUL-terminated vendor display name (UTF-8).
 * @param product_name NUL-terminated product display name (UTF-8).
 * @param variant Hardware or SKU variant byte.
 * @param serial Factory serial number.
 * @param uuid Binary RFC 4122 UUID to store, or NULL to leave UUID unchanged.
 * @param key_type Cryptographic key type for the following buffer.
 * @param key Key material bytes (length implied by @a key_type and @a key_len).
 * @param key_len Length of @a key in bytes.
 * @retval TW_OK on success.
 * @retval Other ::tw_err_t on validation or NVS failure.
 */
tw_err_t tw_identity_set_factory(int32_t vendor_id, int32_t product_id,
                                 const char *vendor_name, const char *product_name,
                                 uint8_t variant, int32_t serial,
                                 const uint8_t *uuid,
                                 tw_identity_key_type_t key_type,
                                 const uint8_t *key, size_t key_len);

/**
 * @brief Writes a raw NVS field by key (SDK internal; provisioning pipelines).
 * @param key NUL-terminated NVS key string.
 * @param value Bytes to store.
 * @param len Length of @a value in bytes.
 * @retval TW_OK on success.
 * @retval Other ::tw_err_t on storage failure.
 */
tw_err_t tw_identity_set_field(const char *key, const uint8_t *value, size_t len);

/**
 * @brief Message handler for the device identity summary resource (registered by svc_device).
 * @param req Inbound request (RFC 7252-mapped semantics via ::tw_msg_request_t).
 * @param resp Response buffer to fill.
 * @retval TW_OK if the response was produced.
 * @retval Other ::tw_err_t on handler failure.
 */
tw_err_t tw_identity_msg_info(tw_msg_request_t *req, tw_msg_response_t *resp);

/**
 * @brief Message handler for the device public-key resource (registered by svc_device).
 * @param req Inbound request.
 * @param resp Response buffer to fill (typically key bytes suitable for hub verification).
 * @retval TW_OK if the response was produced.
 * @retval Other ::tw_err_t on handler failure.
 */
tw_err_t tw_identity_msg_pubkey(tw_msg_request_t *req, tw_msg_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* TW_IDENTITY_H */
