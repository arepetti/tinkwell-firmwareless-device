/*
 * svc_identity.c -- Device identity service.
 *
 * Manages the tw_device_identity_t lifecycle:
 *   - Load compile-time defaults from tw_device_config_t
 *   - Override with NVS values (factory + hub provisioning)
 *   - /tw/info CoAP resource (full identity card)
 *   - /tw/identity/pubkey CoAP resource (Ed25519 public key export)
 *   - Identity field updates (provisioning-only via FactoryProvisionCmd)
 *
 * Ed25519 key derivation uses the mbedTLS API on ESP-IDF and a stub
 * on POSIX (returns the raw private key bytes as a placeholder).
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_identity.h"
#include "tw_device.h"
#include "tw_lock.h"
#include "tw_kvtext.h"
#include "pal_nvs.h"
#include "pal_system.h"
#include "pal_log.h"

#ifdef CONFIG_TW_USE_PROTOBUF
#include <pb_encode.h>
#include "tw_protocol.pb.h"
#endif

#include <string.h>
#include <stdio.h>

#define TAG "identity"

/* NVS keys */
#define NVS_ID_UUID      "id_uuid"
#define NVS_ID_VENDOR    "id_vendor"
#define NVS_ID_PRODUCT   "id_product"
#define NVS_ID_VNAME     "id_vname"
#define NVS_ID_PNAME     "id_pname"
#define NVS_ID_VARIANT   "id_variant"
#define NVS_ID_SERIAL    "id_serial"
#define NVS_ID_FACTORY   "id_factory"
#define NVS_ID_HUBPROV   "id_hubprov"
#define NVS_ID_KEY       "id_key"
#define NVS_ID_KEY_TYPE  "id_key_type"

/* Kconfig default for key type (0 = none if not configured). */
#ifndef CONFIG_TW_IDENTITY_KEY_TYPE_DEFAULT
#define CONFIG_TW_IDENTITY_KEY_TYPE_DEFAULT 0
#endif

#ifndef CONFIG_TW_IDENTITY_VENDOR_ID
#define CONFIG_TW_IDENTITY_VENDOR_ID 0
#endif

#ifndef CONFIG_TW_IDENTITY_PRODUCT_ID
#define CONFIG_TW_IDENTITY_PRODUCT_ID 0
#endif

static tw_lock_t s_lock;
static tw_device_identity_t s_id;
static const struct tw_device_config *s_cfg;

/* ---- Ed25519 public key derivation ---- */

#ifdef TW_PLATFORM_ESPIDF
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"

/*
 * mbedTLS on ESP-IDF supports Ed25519 via the PSA Crypto API (ESP-IDF >= 5.x)
 * or via the montgomery curve interface.  For maximum compatibility we use the
 * low-level Ed25519 scalar multiplication when available, otherwise fall back
 * to raw export.
 *
 * This is a first-draft: the actual signing path is deferred.  We only need
 * to export the public key corresponding to the 32-byte private seed.
 */

#if defined(MBEDTLS_PSA_CRYPTO_C)
#include "psa/crypto.h"

/** Derives the Ed25519 public key via PSA import/export (ESP-IDF PSA Crypto). */
static tw_err_t derive_ed25519_pubkey(const uint8_t priv[TW_KEY_SIZE],
                                      uint8_t pub[TW_PUBKEY_SIZE])
{
    psa_status_t status;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = 0;

    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_EXPORT | PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_PURE_EDDSA);
    psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
    psa_set_key_bits(&attr, 255);

    status = psa_import_key(&attr, priv, TW_KEY_SIZE, &key_id);
    if (status != PSA_SUCCESS) return TW_ERR_IO;

    uint8_t raw_pub[TW_PUBKEY_SIZE];
    size_t pub_len = 0;
    status = psa_export_public_key(key_id, raw_pub, sizeof(raw_pub), &pub_len);
    psa_destroy_key(key_id);

    if (status != PSA_SUCCESS || pub_len != TW_PUBKEY_SIZE)
        return TW_ERR_IO;

    memcpy(pub, raw_pub, TW_PUBKEY_SIZE);
    return TW_OK;
}

#else /* no PSA -- raw fallback */

/** Signals that pubkey export is unavailable without PSA so callers avoid silent wrong keys. */
static tw_err_t derive_ed25519_pubkey(const uint8_t priv[TW_KEY_SIZE],
                                      uint8_t pub[TW_PUBKEY_SIZE])
{
    /*
     * Fallback: without PSA Crypto we cannot derive the public key.
     * Return an error so callers know the feature is unavailable.
     */
    (void)priv;
    (void)pub;
    PAL_LOGW(TAG, "Ed25519 pubkey derivation not available (needs PSA Crypto)");
    return TW_ERR_NOT_READY;
}

#endif /* MBEDTLS_PSA_CRYPTO_C */

#else /* POSIX */

/**
 * POSIX stub: xor-fold placeholder (not Ed25519). Real host builds should link a proper library;
 * this exists so deterministic bytes can exercise the pubkey path without claiming validity.
 */
static tw_err_t derive_ed25519_pubkey(const uint8_t priv[TW_KEY_SIZE],
                                      uint8_t pub[TW_PUBKEY_SIZE])
{
    for (int i = 0; i < TW_PUBKEY_SIZE; i++)
        pub[i] = priv[i] ^ 0xA5;
    PAL_LOGW(TAG, "POSIX Ed25519 stub -- pubkey is NOT cryptographically valid");
    return TW_OK;
}

#endif /* TW_PLATFORM_ESPIDF */

/* ---- Helpers ---- */

static const char hex_digits[] = "0123456789abcdef";

/**
 * Formats a 16-octet UUID as canonical text. RFC 4122 section 3 defines hyphen
 * separators after the 8th, 12th, 16th, and 20th hex digit (8-4-4-4-12 nibbles).
 */
void tw_identity_uuid_to_str(const uint8_t uuid[TW_UUID_SIZE],
                             char *buf, size_t buf_size)
{
    if (buf_size < TW_UUID_STR_SIZE) {
        if (buf_size > 0) buf[0] = '\0';
        return;
    }
    /* Dash insertion after octets 4, 6, 8, 10 (RFC 4122 canonical string layout). */
    static const int dashes[] = { 4, 6, 8, 10 };
    int di = 0, pos = 0;
    for (int i = 0; i < TW_UUID_SIZE; i++) {
        if (di < 4 && i == dashes[di]) { buf[pos++] = '-'; di++; }
        buf[pos++] = hex_digits[(uuid[i] >> 4) & 0x0F];
        buf[pos++] = hex_digits[ uuid[i]       & 0x0F];
    }
    buf[pos] = '\0';
}

/** Lowercase hex string for fixed-size binary blobs (no separators); out must hold 2*len + 1. */
static void hex_encode(const uint8_t *data, size_t len, char *out)
{
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = hex_digits[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_digits[ data[i]       & 0x0F];
    }
    out[len * 2] = '\0';
}

/** Treats all-zero UUID as “unset” so provisioning can detect a missing factory value. */
static bool uuid_is_zero(const uint8_t uuid[TW_UUID_SIZE])
{
    for (int i = 0; i < TW_UUID_SIZE; i++)
        if (uuid[i] != 0) return false;
    return true;
}

/* ---- Init ---- */

tw_err_t tw_identity_init(const struct tw_device_config *cfg)
{
    tw_lock_init(&s_lock);
    memset(&s_id, 0, sizeof(s_id));
    s_cfg = cfg;

    /* 1) Start with compile-time defaults. 0 is a valid "unknown/default". */
    s_id.vendor_id  = cfg->vendor_id;
    s_id.product_id = cfg->product_id;

    if (cfg->vendor_display_name) {
        strncpy(s_id.vendor_display_name, cfg->vendor_display_name,
                TW_DISPLAY_NAME_MAX - 1);
        s_id.vendor_display_name[TW_DISPLAY_NAME_MAX - 1] = '\0';
    }
    if (cfg->product_display_name) {
        strncpy(s_id.product_display_name, cfg->product_display_name,
                TW_DISPLAY_NAME_MAX - 1);
        s_id.product_display_name[TW_DISPLAY_NAME_MAX - 1] = '\0';
    }

    s_id.variant       = cfg->variant;
    s_id.serial_number = 0;
    s_id.key_type      = (tw_identity_key_type_t)CONFIG_TW_IDENTITY_KEY_TYPE_DEFAULT;

    /* 2) Override with NVS values where present. */
    int32_t tmp;
    if (pal_nvs_get_i32(NVS_ID_VENDOR, &tmp) == TW_OK)  s_id.vendor_id  = tmp;
    if (pal_nvs_get_i32(NVS_ID_PRODUCT, &tmp) == TW_OK) s_id.product_id = tmp;
    if (pal_nvs_get_i32(NVS_ID_VARIANT, &tmp) == TW_OK) s_id.variant    = (uint8_t)tmp;
    if (pal_nvs_get_i32(NVS_ID_SERIAL, &tmp) == TW_OK)  s_id.serial_number = tmp;

    char name_buf[TW_DISPLAY_NAME_MAX];
    if (pal_nvs_get_str(NVS_ID_VNAME, name_buf, sizeof(name_buf)) == TW_OK) {
        strncpy(s_id.vendor_display_name, name_buf, TW_DISPLAY_NAME_MAX - 1);
        s_id.vendor_display_name[TW_DISPLAY_NAME_MAX - 1] = '\0';
    }
    if (pal_nvs_get_str(NVS_ID_PNAME, name_buf, sizeof(name_buf)) == TW_OK) {
        strncpy(s_id.product_display_name, name_buf, TW_DISPLAY_NAME_MAX - 1);
        s_id.product_display_name[TW_DISPLAY_NAME_MAX - 1] = '\0';
    }

    /* UUID */
    size_t uuid_len = TW_UUID_SIZE;
    if (pal_nvs_get_blob(NVS_ID_UUID, s_id.uuid, &uuid_len) == TW_OK &&
        uuid_len == TW_UUID_SIZE) {
        s_id.uuid_valid = !uuid_is_zero(s_id.uuid);
    }

    /* Provisioning state flags. */
    int32_t flag = 0;
    if (pal_nvs_get_i32(NVS_ID_FACTORY, &flag) == TW_OK && flag == 1)
        s_id.factory_provisioned = true;
    flag = 0;
    if (pal_nvs_get_i32(NVS_ID_HUBPROV, &flag) == TW_OK && flag == 1)
        s_id.hub_provisioned = true;

    /* Identity key */
    if (pal_nvs_get_i32(NVS_ID_KEY_TYPE, &tmp) == TW_OK)
        s_id.key_type = (tw_identity_key_type_t)tmp;

    size_t key_len = TW_KEY_SIZE;
    if (pal_nvs_get_blob(NVS_ID_KEY, s_id.key, &key_len) == TW_OK &&
        key_len == TW_KEY_SIZE) {
        s_id.key_valid = true;
    }

    char uuid_str[TW_UUID_STR_SIZE];
    tw_identity_uuid_to_str(s_id.uuid, uuid_str, sizeof(uuid_str));
    PAL_LOGI(TAG, "identity: vendor=%d product=%d serial=%d variant=%u uuid=%s "
             "factory=%d hub=%d key_type=%d key_valid=%d",
             s_id.vendor_id, s_id.product_id, s_id.serial_number,
             s_id.variant, uuid_str,
             s_id.factory_provisioned, s_id.hub_provisioned,
             (int)s_id.key_type, s_id.key_valid);

    return TW_OK;
}

/* ---- Accessors ---- */

const tw_device_identity_t *tw_identity_get(void) { return &s_id; }

tw_err_t tw_identity_set_uuid(const uint8_t uuid[TW_UUID_SIZE])
{
    if (!uuid) return TW_ERR_INVAL;
    tw_lock_acquire(s_lock);
    memcpy(s_id.uuid, uuid, TW_UUID_SIZE);
    s_id.uuid_valid = !uuid_is_zero(uuid);
    pal_nvs_set_blob(NVS_ID_UUID, uuid, TW_UUID_SIZE);
    pal_nvs_commit();
    tw_lock_release(s_lock);
    return TW_OK;
}

tw_err_t tw_identity_get_uuid(uint8_t uuid_out[TW_UUID_SIZE])
{
    if (!uuid_out) return TW_ERR_INVAL;
    if (!s_id.uuid_valid) return TW_ERR_NOT_READY;
    memcpy(uuid_out, s_id.uuid, TW_UUID_SIZE);
    return TW_OK;
}

tw_identity_key_type_t tw_identity_get_key_type(void) { return s_id.key_type; }
bool tw_identity_has_key(void) { return s_id.key_valid; }

tw_err_t tw_identity_get_ed25519_pubkey(uint8_t pubkey_out[TW_PUBKEY_SIZE])
{
    if (!pubkey_out) return TW_ERR_INVAL;
    if (s_id.key_type != TW_KEY_ED25519) return TW_ERR_INVAL;
    if (!s_id.key_valid) return TW_ERR_NOT_READY;

    return derive_ed25519_pubkey(s_id.key, pubkey_out);
}

/* ---- Factory provisioning ---- */

tw_err_t tw_identity_set_factory(int32_t vendor_id, int32_t product_id,
                                 const char *vendor_name, const char *product_name,
                                 uint8_t variant, int32_t serial,
                                 const uint8_t *uuid,
                                 tw_identity_key_type_t key_type,
                                 const uint8_t *key, size_t key_len)
{
    tw_lock_acquire(s_lock);

    s_id.vendor_id  = vendor_id;
    s_id.product_id = product_id;
    s_id.variant    = variant;
    s_id.serial_number = serial;

    if (vendor_name) {
        strncpy(s_id.vendor_display_name, vendor_name, TW_DISPLAY_NAME_MAX - 1);
        s_id.vendor_display_name[TW_DISPLAY_NAME_MAX - 1] = '\0';
    }
    if (product_name) {
        strncpy(s_id.product_display_name, product_name, TW_DISPLAY_NAME_MAX - 1);
        s_id.product_display_name[TW_DISPLAY_NAME_MAX - 1] = '\0';
    }

    pal_nvs_set_i32(NVS_ID_VENDOR,  vendor_id);
    pal_nvs_set_i32(NVS_ID_PRODUCT, product_id);
    pal_nvs_set_i32(NVS_ID_VARIANT, (int32_t)variant);
    pal_nvs_set_i32(NVS_ID_SERIAL,  serial);

    if (vendor_name)
        pal_nvs_set_str(NVS_ID_VNAME, vendor_name);
    if (product_name)
        pal_nvs_set_str(NVS_ID_PNAME, product_name);

    if (uuid) {
        memcpy(s_id.uuid, uuid, TW_UUID_SIZE);
        s_id.uuid_valid = !uuid_is_zero(uuid);
        pal_nvs_set_blob(NVS_ID_UUID, uuid, TW_UUID_SIZE);
    }

    s_id.key_type = key_type;
    pal_nvs_set_i32(NVS_ID_KEY_TYPE, (int32_t)key_type);

    if (key && key_len == TW_KEY_SIZE) {
        /* Wipe previous key material before overwriting (L1). */
        memset(s_id.key, 0, TW_KEY_SIZE);
        memcpy(s_id.key, key, TW_KEY_SIZE);
        s_id.key_valid = true;
        pal_nvs_set_blob(NVS_ID_KEY, key, TW_KEY_SIZE);
    }

    s_id.factory_provisioned = true;
    pal_nvs_set_i32(NVS_ID_FACTORY, 1);

    pal_nvs_commit();
    tw_lock_release(s_lock);
    PAL_LOGI(TAG, "factory provisioning complete");
    return TW_OK;
}

/* ---- Field-level update (used by provisioning partition) ---- */

tw_err_t tw_identity_set_field(const char *field, const uint8_t *value, size_t len)
{
    if (!field || !value) return TW_ERR_INVAL;
    tw_lock_acquire(s_lock);

    if (strcmp(field, "vendor_id") == 0 && len >= 4) {
        int32_t v;
        memcpy(&v, value, 4);
        s_id.vendor_id = v;
        pal_nvs_set_i32(NVS_ID_VENDOR, v);
    } else if (strcmp(field, "product_id") == 0 && len >= 4) {
        int32_t v;
        memcpy(&v, value, 4);
        s_id.product_id = v;
        pal_nvs_set_i32(NVS_ID_PRODUCT, v);
    } else if (strcmp(field, "vendor_name") == 0) {
        size_t n = len < TW_DISPLAY_NAME_MAX - 1 ? len : TW_DISPLAY_NAME_MAX - 1;
        memcpy(s_id.vendor_display_name, value, n);
        s_id.vendor_display_name[n] = '\0';
        pal_nvs_set_str(NVS_ID_VNAME, s_id.vendor_display_name);
    } else if (strcmp(field, "product_name") == 0) {
        size_t n = len < TW_DISPLAY_NAME_MAX - 1 ? len : TW_DISPLAY_NAME_MAX - 1;
        memcpy(s_id.product_display_name, value, n);
        s_id.product_display_name[n] = '\0';
        pal_nvs_set_str(NVS_ID_PNAME, s_id.product_display_name);
    } else if (strcmp(field, "variant") == 0 && len >= 1) {
        s_id.variant = value[0];
        pal_nvs_set_i32(NVS_ID_VARIANT, (int32_t)value[0]);
    } else if (strcmp(field, "serial") == 0 && len >= 4) {
        int32_t v;
        memcpy(&v, value, 4);
        s_id.serial_number = v;
        pal_nvs_set_i32(NVS_ID_SERIAL, v);
    } else if (strcmp(field, "uuid") == 0 && len == TW_UUID_SIZE) {
        memcpy(s_id.uuid, value, TW_UUID_SIZE);
        s_id.uuid_valid = !uuid_is_zero(value);
        pal_nvs_set_blob(NVS_ID_UUID, value, TW_UUID_SIZE);
    } else if (strcmp(field, "key") == 0 && len == TW_KEY_SIZE) {
        memcpy(s_id.key, value, TW_KEY_SIZE);
        s_id.key_valid = true;
        pal_nvs_set_blob(NVS_ID_KEY, value, TW_KEY_SIZE);
    } else if (strcmp(field, "key_type") == 0 && len >= 1) {
        s_id.key_type = (tw_identity_key_type_t)value[0];
        pal_nvs_set_i32(NVS_ID_KEY_TYPE, (int32_t)value[0]);
    } else {
        tw_lock_release(s_lock);
        return TW_ERR_NOT_FOUND;
    }

    pal_nvs_commit();
    tw_lock_release(s_lock);
    return TW_OK;
}

/* ---- /tw/info (GET) ---- */

/**
 * CoAP GET /tw/info: returns device identity as a protobuf DeviceInfo
 * message (or kvtext fallback when protobuf is disabled).
 */
tw_err_t tw_identity_msg_info(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    (void)req;

    const char *dev_name = (s_cfg && s_cfg->name) ? s_cfg->name : "";
    const char *dev_fw   = (s_cfg && s_cfg->fw_version) ? s_cfg->fw_version : "";

#ifdef CONFIG_TW_USE_PROTOBUF
    tw_DeviceInfo msg = tw_DeviceInfo_init_zero;

    memcpy(msg.id.bytes, s_id.uuid, TW_UUID_SIZE);
    msg.id.size = TW_UUID_SIZE;
    msg.vendor_id  = s_id.vendor_id;
    msg.product_id = s_id.product_id;
    msg.serial_number = s_id.serial_number;
    memcpy(msg.variant.bytes, &s_id.variant, 1);
    msg.variant.size = 1;
    strncpy(msg.vendor_display_name, s_id.vendor_display_name,
            sizeof(msg.vendor_display_name) - 1);
    strncpy(msg.product_display_name, s_id.product_display_name,
            sizeof(msg.product_display_name) - 1);
    strncpy(msg.fw_version, dev_fw, sizeof(msg.fw_version) - 1);
    strncpy(msg.device_name, dev_name, sizeof(msg.device_name) - 1);

    uint8_t buf[tw_DeviceInfo_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));

    if (!pb_encode(&stream, tw_DeviceInfo_fields, &msg)) {
        PAL_LOGE(TAG, "DeviceInfo encode failed: %s", PB_GET_ERROR(&stream));
        return tw_msg_respond_empty(resp, TW_MSG_500_INTERNAL);
    }

    resp->code = TW_MSG_205_CONTENT;
    return tw_msg_respond_buf(resp, buf, stream.bytes_written);
#else
    char buf[512];
    size_t pos = 0;

    tw_kvtext_write_hex(buf, sizeof(buf), &pos, "uuid",
                        s_id.uuid, TW_UUID_SIZE);
    tw_kvtext_write_i32(buf, sizeof(buf), &pos, "vendor-id", s_id.vendor_id);
    tw_kvtext_write_str(buf, sizeof(buf), &pos, "vendor-display-name",
                        s_id.vendor_display_name);
    tw_kvtext_write_i32(buf, sizeof(buf), &pos, "product-id", s_id.product_id);
    tw_kvtext_write_str(buf, sizeof(buf), &pos, "product-display-name",
                        s_id.product_display_name);
    tw_kvtext_write_u8(buf, sizeof(buf), &pos, "variant", s_id.variant);
    tw_kvtext_write_i32(buf, sizeof(buf), &pos, "serial-number",
                        s_id.serial_number);
    tw_kvtext_write_str(buf, sizeof(buf), &pos, "device-name", dev_name);
    tw_kvtext_write_str(buf, sizeof(buf), &pos, "fw-version", dev_fw);

    if (pos == 0)
        return tw_msg_respond_empty(resp, TW_MSG_500_INTERNAL);

    return tw_msg_respond_buf(resp, buf, pos);
#endif
}

/* ---- /tw/identity/pubkey (GET) ---- */

/**
 * CoAP GET /tw/identity/pubkey: returns raw Ed25519 public key bytes when a key is provisioned
 * so peers can pin or verify signatures without parsing text.
 */
tw_err_t tw_identity_msg_pubkey(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    (void)req;

    if (s_id.key_type != TW_KEY_ED25519) {
        return tw_msg_respond_with_code(resp, TW_MSG_205_CONTENT, "key_type=none");
    }
    if (!s_id.key_valid) {
        return tw_msg_respond_empty(resp, TW_MSG_404_NOT_FOUND);
    }

    uint8_t pubkey[TW_PUBKEY_SIZE];
    tw_err_t err = derive_ed25519_pubkey(s_id.key, pubkey);
    if (err != TW_OK) {
        return tw_msg_respond_empty(resp, TW_MSG_500_INTERNAL);
    }

    return tw_msg_respond_buf(resp, pubkey, TW_PUBKEY_SIZE);
}
