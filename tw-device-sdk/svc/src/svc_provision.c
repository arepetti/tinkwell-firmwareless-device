/*
 * svc_provision.c -- Two-phase provisioning flow.
 *
 * Phase 1 (Factory): Identity fields written via BLE / LAN / serial.
 *   Sets vendor_id, product_id, display names, variant, serial,
 *   initial UUID, and identity key.  Does NOT set hub_provisioned,
 *   so the device still enters provisioning mode on next boot.
 *
 * Phase 2 (Hub): Network credentials + hub address + final UUID.
 *   Written via BLE GATT, SoftAP CoAP, or POSIX env vars.
 *   Sets hub_provisioned.
 *
 * All provisioning data uses the kvtext wire format (key=value lines).
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_types.h"
#include "tw_device.h"
#include "tw_identity.h"
#include "tw_kvtext.h"
#include "tw_hub.h"
#include "tw_msg.h"
#include "pal_nvs.h"
#include "pal_log.h"
#include "pal_os.h"
#include "pal_system.h"

#ifdef CONFIG_TW_USE_PROTOBUF
#include <pb_decode.h>
#include <pb_encode.h>
#include "tw_protocol.pb.h"
#endif

#include <string.h>
#include <stdlib.h>

#define TAG "provision"

/* NVS keys for idempotent factory provisioning. */
#define NVS_FACTORY_DONE       "fac_done"
#define NVS_FACTORY_FINALIZED  "fac_final"

#ifndef CONFIG_TW_FACTORY_PROVISION_REQUIRED
#define CONFIG_TW_FACTORY_PROVISION_REQUIRED 0
#endif

#define REBOOT_DELAY_MS 500

#ifndef CONFIG_TW_PROVISION_SOFTAP_MAX_CONN
#define CONFIG_TW_PROVISION_SOFTAP_MAX_CONN 4
#endif

#ifndef CONFIG_TW_PROVISION_POLL_INTERVAL_MS
#define CONFIG_TW_PROVISION_POLL_INTERVAL_MS 500
#endif

/* -------------------------------------------------------------------
 * Shared provisioning state -- populated by kvtext visitor
 * -----------------------------------------------------------------*/

typedef struct {
    char     ssid[TW_WIFI_SSID_BUF_SIZE];
    char     password[TW_WIFI_PASS_BUF_SIZE];
    char     hub_url[128];
    int32_t  vendor_id;
    int32_t  product_id;
    char     vendor_display_name[TW_DISPLAY_NAME_MAX];
    char     product_display_name[TW_DISPLAY_NAME_MAX];
    uint8_t  variant;
    int32_t  serial_number;
    uint8_t  uuid[TW_UUID_SIZE];
    bool     uuid_set;
    uint8_t  key[TW_KEY_SIZE];
    bool     key_set;
    int32_t  key_type;
    char     phase[16];     /* "factory" or "hub" */
    char     cmd[16];       /* "start", "commit", "reset" */
    bool     has_network;   /* ssid was provided */
    bool     has_identity;  /* at least one factory field was set */
} prov_data_t;

static prov_data_t s_prov;
static bool        s_prov_done;

#define HEX_INVALID 0xFF

/** Convert one hex character to a nibble, or HEX_INVALID (0xFF) if not hex. */
static uint8_t hex_digit(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return HEX_INVALID;
}

/** Decode @a out_len bytes from @a hex (2 hex digits per byte). */
static bool hex_decode(const char *hex, uint8_t *out, size_t out_len)
{
    for (size_t i = 0; i < out_len; i++) {
        uint8_t hi = hex_digit(hex[i * 2]);
        uint8_t lo = hex_digit(hex[i * 2 + 1]);
        if (hi == 0xFF || lo == 0xFF) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

/** kvtext visitor: populate @a ctx (prov_data_t) from key=value pairs. */
static void prov_visitor(void *ctx, const char *key, const char *value)
{
    prov_data_t *d = (prov_data_t *)ctx;

    if (strcmp(key, "ssid") == 0) {
        strncpy(d->ssid, value, sizeof(d->ssid) - 1);
        d->ssid[sizeof(d->ssid) - 1] = '\0';
        d->has_network = true;
    } else if (strcmp(key, "password") == 0) {
        strncpy(d->password, value, sizeof(d->password) - 1);
        d->password[sizeof(d->password) - 1] = '\0';
    } else if (strcmp(key, "hub-url") == 0) {
        strncpy(d->hub_url, value, sizeof(d->hub_url) - 1);
        d->hub_url[sizeof(d->hub_url) - 1] = '\0';
    } else if (strcmp(key, "vendor-id") == 0) {
        d->vendor_id = (int32_t)atoi(value);
        d->has_identity = true;
    } else if (strcmp(key, "product-id") == 0) {
        d->product_id = (int32_t)atoi(value);
        d->has_identity = true;
    } else if (strcmp(key, "vendor-display-name") == 0) {
        strncpy(d->vendor_display_name, value, TW_DISPLAY_NAME_MAX - 1);
        d->vendor_display_name[TW_DISPLAY_NAME_MAX - 1] = '\0';
        d->has_identity = true;
    } else if (strcmp(key, "product-display-name") == 0) {
        strncpy(d->product_display_name, value, TW_DISPLAY_NAME_MAX - 1);
        d->product_display_name[TW_DISPLAY_NAME_MAX - 1] = '\0';
        d->has_identity = true;
    } else if (strcmp(key, "variant") == 0) {
        d->variant = (uint8_t)atoi(value);
        d->has_identity = true;
    } else if (strcmp(key, "serial-number") == 0) {
        d->serial_number = (int32_t)atoi(value);
        d->has_identity = true;
    } else if (strcmp(key, "uuid") == 0) {
        if (strlen(value) >= TW_UUID_SIZE * 2) {
            d->uuid_set = hex_decode(value, d->uuid, TW_UUID_SIZE);
            d->has_identity = true;
        }
    } else if (strcmp(key, "psk") == 0 || strcmp(key, "key") == 0) {
        if (strlen(value) >= TW_KEY_SIZE * 2) {
            d->key_set = hex_decode(value, d->key, TW_KEY_SIZE);
        }
    } else if (strcmp(key, "key-type") == 0) {
        d->key_type = (int32_t)atoi(value);
    } else if (strcmp(key, "provision-phase") == 0) {
        strncpy(d->phase, value, sizeof(d->phase) - 1);
        d->phase[sizeof(d->phase) - 1] = '\0';
    } else if (strcmp(key, "provision-cmd") == 0) {
        strncpy(d->cmd, value, sizeof(d->cmd) - 1);
        d->cmd[sizeof(d->cmd) - 1] = '\0';
    }
}

/* -------------------------------------------------------------------
 * Apply provisioning data
 * -----------------------------------------------------------------*/

/** Persist identity, WiFi, and hub URL from @a d according to phase. */
static void apply_provisioning(const tw_device_config_t *cfg, prov_data_t *d)
{
    /* Phase 1: factory identity */
    if (d->has_identity || strcmp(d->phase, "factory") == 0) {
        tw_identity_set_factory(
            d->vendor_id  ? d->vendor_id  : cfg->vendor_id,
            d->product_id ? d->product_id : cfg->product_id,
            d->vendor_display_name[0] ? d->vendor_display_name : cfg->vendor_display_name,
            d->product_display_name[0] ? d->product_display_name : cfg->product_display_name,
            d->variant,
            d->serial_number,
            d->uuid_set ? d->uuid : NULL,
            (tw_identity_key_type_t)d->key_type,
            d->key_set ? d->key : NULL,
            d->key_set ? TW_KEY_SIZE : 0);

        PAL_LOGI(TAG, "factory identity applied");

        /* If phase is explicitly "factory", don't mark hub-provisioned. */
        if (strcmp(d->phase, "factory") == 0 && !d->has_network) {
            pal_nvs_commit();
            return;
        }
    }

    /* Phase 2: network + hub */
    if (d->has_network) {
        pal_nvs_set_str("wifi_ssid", d->ssid);
        pal_nvs_set_str("wifi_pass", d->password);
        PAL_LOGI(TAG, "provisioned WiFi: %s", d->ssid);
    }

    if (d->hub_url[0]) {
        tw_hub_set_address(d->hub_url);
        PAL_LOGI(TAG, "provisioned hub: %s", d->hub_url);
    }

    if (d->uuid_set) {
        tw_identity_set_uuid(d->uuid);
    }

    if (d->has_network || d->hub_url[0]) {
        pal_nvs_set_i32("id_hubprov", 1);
        PAL_LOGI(TAG, "hub provisioning complete");
    }

    pal_nvs_commit();
}

/* -------------------------------------------------------------------
 * Protobuf-based provisioning CoAP handlers
 * -----------------------------------------------------------------*/

#ifdef CONFIG_TW_USE_PROTOBUF

static bool factory_is_done(void)
{
    int32_t flag = 0;
    return (pal_nvs_get_i32(NVS_FACTORY_DONE, &flag) == TW_OK && flag == 1);
}

static bool factory_is_finalized(void)
{
    int32_t flag = 0;
    return (pal_nvs_get_i32(NVS_FACTORY_FINALIZED, &flag) == TW_OK && flag == 1);
}

/**
 * POST /tw/provision/factory -- idempotent factory provisioning.
 * Only non-default fields in FactoryProvisionCmd are written.
 * Returns 4.03 Forbidden if factory provisioning is finalized.
 */
static tw_err_t prov_on_factory(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    if (factory_is_finalized()) {
        return tw_msg_respond_with_code(resp, TW_MSG_403_FORBIDDEN,
                                         "factory provisioning locked");
    }

    if (!req->payload || req->payload_len == 0) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "empty payload");
    }

    tw_FactoryProvisionCmd msg = tw_FactoryProvisionCmd_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(req->payload, req->payload_len);
    if (!pb_decode(&stream, tw_FactoryProvisionCmd_fields, &msg)) {
        PAL_LOGW(TAG, "FactoryProvisionCmd decode failed: %s", PB_GET_ERROR(&stream));
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "decode error");
    }

    /*
     * H8 fix: use proto3 `optional` field presence (has_* flags generated
     * by nanopb) to distinguish "not set" from "set to 0/empty".
     */
    if (msg.has_vendor_id) {
        pal_nvs_set_i32("id_vendor", msg.vendor_id);
        PAL_LOGI(TAG, "factory: vendor_id=%d", (int)msg.vendor_id);
    }
    if (msg.has_vendor_display_name) {
        pal_nvs_set_str("id_vname", msg.vendor_display_name);
        PAL_LOGI(TAG, "factory: vendor_name=%s", msg.vendor_display_name);
    }
    if (msg.has_product_id) {
        pal_nvs_set_i32("id_product", msg.product_id);
        PAL_LOGI(TAG, "factory: product_id=%d", (int)msg.product_id);
    }
    if (msg.has_product_display_name) {
        pal_nvs_set_str("id_pname", msg.product_display_name);
        PAL_LOGI(TAG, "factory: product_name=%s", msg.product_display_name);
    }
    if (msg.has_serial_number) {
        pal_nvs_set_i32("id_serial", msg.serial_number);
        PAL_LOGI(TAG, "factory: serial=%d", (int)msg.serial_number);
    }
    if (msg.variant.size > 0) {
        pal_nvs_set_i32("id_variant", (int32_t)msg.variant.bytes[0]);
        PAL_LOGI(TAG, "factory: variant=%u", (unsigned)msg.variant.bytes[0]);
    }
    if (msg.id.size == TW_UUID_SIZE) {
        pal_nvs_set_blob("id_uuid", msg.id.bytes, TW_UUID_SIZE);
        PAL_LOGI(TAG, "factory: id set");
    }
    /* L4 fix: reject if both PSK and Ed25519 are provided in one call. */
    if (msg.psk.size > 0 && msg.ed25519_private_key.size > 0) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ,
                                         "cannot set both psk and ed25519 in one call");
    }
    if (msg.psk.size > 0) {
        /* H3 fix: PSK must be exactly TW_KEY_SIZE bytes. */
        if (msg.psk.size != TW_KEY_SIZE) {
            return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ,
                                             "psk must be 32 bytes");
        }
        pal_nvs_set_blob("id_key", msg.psk.bytes, TW_KEY_SIZE);
        pal_nvs_set_i32("id_key_type", (int32_t)TW_KEY_PSK);
        PAL_LOGI(TAG, "factory: PSK set");
    }
    if (msg.ed25519_private_key.size == TW_KEY_SIZE) {
        pal_nvs_set_blob("id_key", msg.ed25519_private_key.bytes, TW_KEY_SIZE);
        pal_nvs_set_i32("id_key_type", (int32_t)TW_KEY_ED25519);
        PAL_LOGI(TAG, "factory: Ed25519 key set");
    }

    /* Mark factory as done. M7 fix: use a single canonical key. */
    pal_nvs_set_i32(NVS_FACTORY_DONE, 1);

    /* If finalize requested, lock factory provisioning permanently. */
    if (msg.finalize) {
        pal_nvs_set_i32(NVS_FACTORY_FINALIZED, 1);
        PAL_LOGW(TAG, "factory provisioning FINALIZED (locked)");
    }

    pal_nvs_commit();

    tw_ProvisionReply reply = tw_ProvisionReply_init_zero;
    reply.success = true;
    uint8_t rbuf[tw_ProvisionReply_size];
    pb_ostream_t rs = pb_ostream_from_buffer(rbuf, sizeof(rbuf));
    if (pb_encode(&rs, tw_ProvisionReply_fields, &reply)) {
        resp->code = TW_MSG_204_CHANGED;
        return tw_msg_respond_buf(resp, rbuf, rs.bytes_written);
    }
    return tw_msg_respond_empty(resp, TW_MSG_204_CHANGED);
}

/**
 * POST /tw/provision/hub -- hub provisioning (id-only).
 * Requires factory_done if TW_FACTORY_PROVISION_REQUIRED is enabled.
 */
static tw_err_t prov_on_hub(tw_msg_request_t *req, tw_msg_response_t *resp)
{
#if CONFIG_TW_FACTORY_PROVISION_REQUIRED
    if (!factory_is_done()) {
        return tw_msg_respond_with_code(resp, TW_MSG_403_FORBIDDEN,
                                         "factory provisioning required first");
    }
#endif

    if (!req->payload || req->payload_len == 0) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "empty payload");
    }

    tw_HubProvisionCmd msg = tw_HubProvisionCmd_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(req->payload, req->payload_len);
    if (!pb_decode(&stream, tw_HubProvisionCmd_fields, &msg)) {
        PAL_LOGW(TAG, "HubProvisionCmd decode failed: %s", PB_GET_ERROR(&stream));
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "decode error");
    }

    if (msg.id.size != TW_UUID_SIZE) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "invalid id size");
    }

    pal_nvs_set_blob("id_uuid", msg.id.bytes, TW_UUID_SIZE);
    PAL_LOGI(TAG, "hub provisioning: device id assigned");

    tw_ProvisionReply reply = tw_ProvisionReply_init_zero;
    reply.success = true;
    uint8_t rbuf[tw_ProvisionReply_size];
    pb_ostream_t rs = pb_ostream_from_buffer(rbuf, sizeof(rbuf));
    if (pb_encode(&rs, tw_ProvisionReply_fields, &reply)) {
        resp->code = TW_MSG_204_CHANGED;
        return tw_msg_respond_buf(resp, rbuf, rs.bytes_written);
    }
    return tw_msg_respond_empty(resp, TW_MSG_204_CHANGED);
}

/**
 * POST /tw/provision/set -- set WiFi/hub config via ProvisionSetCmd protobuf.
 *
 * Allowed keys:
 *   - WiFi:  "ssid" / "wifi-ssid", "password" / "wifi-pass"
 *   - Hub:   "hub-url"
 *   - App:   any key starting with "APP_" (application-defined settings,
 *            e.g. APP_temp_min, APP_temp_max for a thermostat safe range)
 *
 * These keys are always writable -- including after factory provisioning
 * has been finalized -- because WiFi/hub config and app settings are not
 * factory identity fields (H2).  Unknown keys are rejected (H1).
 */
static tw_err_t prov_on_set_proto(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    if (!req->payload || req->payload_len == 0) {
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "empty payload");
    }

    tw_ProvisionSetCmd msg = tw_ProvisionSetCmd_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(req->payload, req->payload_len);
    if (!pb_decode(&stream, tw_ProvisionSetCmd_fields, &msg)) {
        PAL_LOGW(TAG, "ProvisionSetCmd decode failed: %s", PB_GET_ERROR(&stream));
        return tw_msg_respond_with_code(resp, TW_MSG_400_BAD_REQ, "decode error");
    }

    for (pb_size_t i = 0; i < msg.fields_count; i++) {
        const tw_ConfigEntry *e = &msg.fields[i];

        /* L3 fix: reject keys/values containing newlines to prevent injection. */
        if (strchr(e->key, '\n') || strchr(e->key, '\r') ||
            strchr(e->value, '\n') || strchr(e->value, '\r')) {
            PAL_LOGW(TAG, "provision config: rejected key/value with newlines");
            continue;
        }

        PAL_LOGI(TAG, "provision config: %s=%s", e->key, e->value);

        if (strcmp(e->key, "ssid") == 0 || strcmp(e->key, "wifi-ssid") == 0) {
            pal_nvs_set_str("wifi_ssid", e->value);
        } else if (strcmp(e->key, "password") == 0 || strcmp(e->key, "wifi-pass") == 0) {
            pal_nvs_set_str("wifi_pass", e->value);
        } else if (strcmp(e->key, "hub-url") == 0) {
            tw_hub_set_address(e->value);
        } else if (strncmp(e->key, "APP_", 4) == 0) {
            pal_nvs_set_str(e->key, e->value);
        } else {
            PAL_LOGW(TAG, "provision config: unknown key '%s' rejected", e->key);
        }
    }

    pal_nvs_set_i32("id_hubprov", 1);
    pal_nvs_commit();
    s_prov_done = true;

    PAL_LOGI(TAG, "provision set committed");

    tw_ProvisionReply reply = tw_ProvisionReply_init_zero;
    reply.success = true;
    uint8_t rbuf[tw_ProvisionReply_size];
    pb_ostream_t rs = pb_ostream_from_buffer(rbuf, sizeof(rbuf));
    if (pb_encode(&rs, tw_ProvisionReply_fields, &reply)) {
        resp->code = TW_MSG_204_CHANGED;
        return tw_msg_respond_buf(resp, rbuf, rs.bytes_written);
    }
    return tw_msg_respond_empty(resp, TW_MSG_204_CHANGED);
}

/**
 * GET /tw/provision/info -- returns ProvisionInfo protobuf with
 * status, device info, factory_done, and factory_finalized flags.
 */
static tw_err_t prov_on_info_proto(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    (void)req;

    tw_ProvisionInfo info = tw_ProvisionInfo_init_zero;

    bool fac_done = factory_is_done();
    bool fac_final = factory_is_finalized();
    int32_t hub_prov = 0;
    pal_nvs_get_i32("id_hubprov", &hub_prov);

    if (hub_prov == 1) {
        strncpy(info.status, "provisioned", sizeof(info.status) - 1);
    } else if (fac_done) {
        strncpy(info.status, "hub-pending", sizeof(info.status) - 1);
    } else {
        strncpy(info.status, "factory-pending", sizeof(info.status) - 1);
    }

    info.factory_done = fac_done;
    info.factory_finalized = fac_final;

    /* Embed DeviceInfo. */
    const tw_device_identity_t *id = tw_identity_get();
    info.has_device = true;
    memcpy(info.device.id.bytes, id->uuid, TW_UUID_SIZE);
    info.device.id.size = TW_UUID_SIZE;
    info.device.vendor_id = id->vendor_id;
    info.device.product_id = id->product_id;
    info.device.serial_number = id->serial_number;
    if (id->variant) {
        info.device.variant.bytes[0] = id->variant;
        info.device.variant.size = 1;
    }
    strncpy(info.device.vendor_display_name, id->vendor_display_name,
            sizeof(info.device.vendor_display_name) - 1);
    strncpy(info.device.product_display_name, id->product_display_name,
            sizeof(info.device.product_display_name) - 1);

    uint8_t buf[tw_ProvisionInfo_size];
    pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
    if (!pb_encode(&stream, tw_ProvisionInfo_fields, &info)) {
        PAL_LOGE(TAG, "ProvisionInfo encode failed: %s", PB_GET_ERROR(&stream));
        return tw_msg_respond_empty(resp, TW_MSG_500_INTERNAL);
    }

    resp->code = TW_MSG_205_CONTENT;
    return tw_msg_respond_buf(resp, buf, stream.bytes_written);
}

#endif /* CONFIG_TW_USE_PROTOBUF */

/* -------------------------------------------------------------------
 * Public API
 * -----------------------------------------------------------------*/

bool svc_provision_is_needed(void)
{
    int32_t hub_prov = 0;
    if (pal_nvs_get_i32("id_hubprov", &hub_prov) == TW_OK && hub_prov == 1)
        return false;
    return true;
}

/* -------------------------------------------------------------------
 * POSIX: environment variables
 * -----------------------------------------------------------------*/

#ifndef TW_PLATFORM_ESPIDF

tw_err_t svc_provision_run(const tw_device_config_t *cfg)
{
    if (cfg->on_provision)
        return cfg->on_provision(cfg);

    PAL_LOGI(TAG, "POSIX provisioning: checking environment variables");

    memset(&s_prov, 0, sizeof(s_prov));

    const char *env_vendor   = getenv("TW_VENDOR_ID");
    const char *env_product  = getenv("TW_PRODUCT_ID");
    const char *env_serial   = getenv("TW_SERIAL");
    const char *env_variant  = getenv("TW_VARIANT");
    const char *env_vname    = getenv("TW_VENDOR_NAME");
    const char *env_pname    = getenv("TW_PRODUCT_NAME");
    const char *env_uuid     = getenv("TW_UUID");
    const char *env_key_type = getenv("TW_KEY_TYPE");
    const char *env_key      = getenv("TW_KEY");
    const char *ssid         = getenv("TW_WIFI_SSID");
    const char *pass         = getenv("TW_WIFI_PASS");
    const char *hub          = getenv("TW_HUB_ADDR");
    const char *hub_uuid     = getenv("TW_HUB_UUID");

    if (env_vendor)   s_prov.vendor_id  = (int32_t)atoi(env_vendor);
    if (env_product)  s_prov.product_id = (int32_t)atoi(env_product);
    if (env_serial)   s_prov.serial_number = (int32_t)atoi(env_serial);
    if (env_variant)  s_prov.variant = (uint8_t)atoi(env_variant);

    if (env_vname) {
        strncpy(s_prov.vendor_display_name, env_vname, TW_DISPLAY_NAME_MAX - 1);
        s_prov.vendor_display_name[TW_DISPLAY_NAME_MAX - 1] = '\0';
    }
    if (env_pname) {
        strncpy(s_prov.product_display_name, env_pname, TW_DISPLAY_NAME_MAX - 1);
        s_prov.product_display_name[TW_DISPLAY_NAME_MAX - 1] = '\0';
    }

    if (env_uuid && strlen(env_uuid) >= TW_UUID_SIZE * 2)
        s_prov.uuid_set = hex_decode(env_uuid, s_prov.uuid, TW_UUID_SIZE);

    if (env_key_type) s_prov.key_type = (int32_t)atoi(env_key_type);
    if (env_key && strlen(env_key) >= TW_KEY_SIZE * 2)
        s_prov.key_set = hex_decode(env_key, s_prov.key, TW_KEY_SIZE);

    s_prov.has_identity = env_vendor || env_product || env_serial ||
                          env_variant || env_vname || env_pname ||
                          env_uuid || env_key_type || env_key;

    if (ssid && ssid[0]) {
        strncpy(s_prov.ssid, ssid, sizeof(s_prov.ssid) - 1);
        s_prov.ssid[sizeof(s_prov.ssid) - 1] = '\0';
        s_prov.has_network = true;
    }
    if (pass) {
        strncpy(s_prov.password, pass, sizeof(s_prov.password) - 1);
        s_prov.password[sizeof(s_prov.password) - 1] = '\0';
    }
    if (hub && hub[0]) {
        strncpy(s_prov.hub_url, hub, sizeof(s_prov.hub_url) - 1);
        s_prov.hub_url[sizeof(s_prov.hub_url) - 1] = '\0';
    }

    if (hub_uuid && strlen(hub_uuid) >= TW_UUID_SIZE * 2) {
        s_prov.uuid_set = hex_decode(hub_uuid, s_prov.uuid, TW_UUID_SIZE);
    }

    apply_provisioning(cfg, &s_prov);

    if (!s_prov.has_network) {
        PAL_LOGW(TAG, "no network credentials (set TW_WIFI_SSID, TW_WIFI_PASS, TW_HUB_ADDR)");
        PAL_LOGW(TAG, "continuing without hub provisioning");
    }

    return TW_OK;
}

#else /* TW_PLATFORM_ESPIDF */

/* -------------------------------------------------------------------
 * ESP-IDF: BLE GATT provisioning
 * -----------------------------------------------------------------*/

#ifdef CONFIG_TW_TRANSPORT_BLE_PROVISIONING

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"

/*
 * BLE Provisioning GATT Service
 *
 * A single writable characteristic accepts kvtext-formatted payloads.
 * The companion app sends key=value lines, ending with
 * "provision-cmd=commit\n" to finalize.
 */

#define PROV_SVC_UUID          0x1200
#define PROV_CHAR_UUID_CONFIG  0x1201
#define PROV_CHAR_UUID_STATUS  0x1202

#define PROV_GATTS_APP_ID      0

/* Bluetooth Core Specification Vol 6, Part B, section 4.4.2.2 -- units of 0.625 ms */
#define PROV_ADV_INTERVAL_MIN 0x20  /* 20 ms */
#define PROV_ADV_INTERVAL_MAX 0x40  /* 40 ms */
/* Bluetooth Core Specification Vol 3, Part F, section 3.2.9 */
#define PROV_CHAR_VAL_LEN_MAX 512
/* Bluetooth Core Specification Vol 3, Part C, section 12 -- connection interval units of 1.25 ms */
#define PROV_CONN_INTERVAL_MIN 0x0006  /* 7.5 ms */
#define PROV_CONN_INTERVAL_MAX 0x0010  /* 20 ms */

static uint16_t s_gatts_if;
static uint16_t s_conn_id;
static uint16_t s_handle_svc;
static uint16_t s_handle_config_val;
static uint16_t s_handle_status_val;
static bool     s_connected_ble;

static const tw_device_config_t *s_cfg;

static uint8_t s_adv_svc_uuid128[16] = {
    0x6a, 0x3d, 0x1c, 0x9a, 0x3b, 0xbc, 0x5b, 0x9f,
    0x6a, 0x4f, 0x37, 0xe2, 0x01, 0x20, 0xe1, 0xa9,
};

static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min       = PROV_ADV_INTERVAL_MIN,
    .adv_int_max       = PROV_ADV_INTERVAL_MAX,
    .adv_type          = ADV_TYPE_IND,
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t s_adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = false,
    .min_interval        = PROV_CONN_INTERVAL_MIN,
    .max_interval        = PROV_CONN_INTERVAL_MAX,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(s_adv_svc_uuid128),
    .p_service_uuid      = s_adv_svc_uuid128,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&s_adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
            PAL_LOGE(TAG, "advertising start failed");
        else
            PAL_LOGI(TAG, "BLE advertising started");
        break;
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        s_gatts_if = gatts_if;
        esp_ble_gap_set_device_name(s_cfg ? s_cfg->name : "TW-Device");
        esp_ble_gap_config_adv_data(&s_adv_data);

        /* Register the provisioning service. */
        esp_gatt_srvc_id_t svc_id = {
            .is_primary = true,
            .id = { .inst_id = 0, .uuid = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = PROV_SVC_UUID } } },
        };
        esp_ble_gatts_create_service(gatts_if, &svc_id, 8);
        break;

    case ESP_GATTS_CREATE_EVT:
        s_handle_svc = param->create.service_handle;
        esp_ble_gatts_start_service(s_handle_svc);

        /* Config characteristic (write) */
        {
            esp_bt_uuid_t uuid = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = PROV_CHAR_UUID_CONFIG } };
            esp_ble_gatts_add_char(s_handle_svc, &uuid,
                                   ESP_GATT_PERM_WRITE,
                                   ESP_GATT_CHAR_PROP_BIT_WRITE,
                                   NULL, NULL);
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        if (param->add_char.char_uuid.uuid.uuid16 == PROV_CHAR_UUID_CONFIG) {
            s_handle_config_val = param->add_char.attr_handle;

            /* Status characteristic (read/notify) */
            esp_bt_uuid_t uuid = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = PROV_CHAR_UUID_STATUS } };
            esp_ble_gatts_add_char(s_handle_svc, &uuid,
                                   ESP_GATT_PERM_READ,
                                   ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                   NULL, NULL);
        } else if (param->add_char.char_uuid.uuid.uuid16 == PROV_CHAR_UUID_STATUS) {
            s_handle_status_val = param->add_char.attr_handle;
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        if (param->write.handle == s_handle_config_val) {
            PAL_LOGI(TAG, "BLE config write: %d bytes", param->write.len);

            tw_kvtext_parse((const char *)param->write.value,
                            param->write.len, prov_visitor, &s_prov);

            if (strcmp(s_prov.cmd, "commit") == 0) {
                apply_provisioning(s_cfg, &s_prov);
                s_prov_done = true;
            } else if (strcmp(s_prov.cmd, "reset") == 0) {
                memset(&s_prov, 0, sizeof(s_prov));
                PAL_LOGI(TAG, "provisioning data reset");
            }

            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                            param->write.trans_id,
                                            ESP_GATT_OK, NULL);
            }
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        s_conn_id = param->connect.conn_id;
        s_connected_ble = true;
        PAL_LOGI(TAG, "provisioning peer connected");
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        s_connected_ble = false;
        if (s_prov_done) {
            PAL_LOGI(TAG, "provisioning complete, rebooting...");
            pal_sleep_ms(REBOOT_DELAY_MS);
            pal_system_reboot();
        }
        esp_ble_gap_start_advertising(&s_adv_params);
        break;

    default:
        break;
    }
}

/** Run GATT-based kvtext provisioning until commit, then reboot. */
static tw_err_t run_ble_provisioning(const tw_device_config_t *cfg)
{
    s_cfg = cfg;
    memset(&s_prov, 0, sizeof(s_prov));
    s_prov_done = false;

    PAL_LOGI(TAG, "starting BLE provisioning...");

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(PROV_GATTS_APP_ID);

    PAL_LOGI(TAG, "BLE advertising -- waiting for companion app...");

    while (!s_prov_done) {
        pal_sleep_ms(CONFIG_TW_PROVISION_POLL_INTERVAL_MS);
    }

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    return TW_OK;
}

#endif /* CONFIG_TW_TRANSPORT_BLE_PROVISIONING */

/* -------------------------------------------------------------------
 * ESP-IDF: SoftAP + CoAP provisioning
 * -----------------------------------------------------------------*/

#ifdef CONFIG_TW_PROVISION_SOFTAP

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "tw_msg.h"

#ifndef CONFIG_TW_PROVISION_SOFTAP_SSID_PREFIX
#define CONFIG_TW_PROVISION_SOFTAP_SSID_PREFIX "TW-Prov"
#endif

#ifndef CONFIG_TW_PROVISION_SOFTAP_PASSWORD
#define CONFIG_TW_PROVISION_SOFTAP_PASSWORD ""
#endif

/** POST /tw/provision/set — parse kvtext and commit or reset provisioning. */
static tw_err_t prov_coap_set(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    if (!req->payload || req->payload_len == 0)
        return tw_msg_respond_empty(resp, TW_MSG_400_BAD_REQ);

    tw_kvtext_parse((const char *)req->payload, req->payload_len,
                    prov_visitor, &s_prov);

    if (strcmp(s_prov.cmd, "commit") == 0) {
        apply_provisioning(s_cfg, &s_prov);
        s_prov_done = true;
        return tw_msg_respond_with_code(resp, TW_MSG_204_CHANGED, "committed");
    } else if (strcmp(s_prov.cmd, "reset") == 0) {
        memset(&s_prov, 0, sizeof(s_prov));
        return tw_msg_respond_with_code(resp, TW_MSG_204_CHANGED, "reset");
    }

    return tw_msg_respond_with_code(resp, TW_MSG_204_CHANGED, "accepted");
}

/** GET /tw/provision/info — device identity summary. */
static tw_err_t prov_coap_info(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    (void)req;
    return tw_identity_msg_info(req, resp);
}

static tw_msg_resource_t s_prov_resources[] = {
#ifdef CONFIG_TW_USE_PROTOBUF
    { "/tw/provision/factory", TW_MSG_POST, prov_on_factory },
    { "/tw/provision/hub",     TW_MSG_POST, prov_on_hub },
    { "/tw/provision/set",     TW_MSG_POST, prov_on_set_proto },
    { "/tw/provision/info",    TW_MSG_GET,  prov_on_info_proto },
#else
    { "/tw/provision/set",     TW_MSG_POST, prov_coap_set },
    { "/tw/provision/info",    TW_MSG_GET,  prov_coap_info },
#endif
    TW_MSG_RESOURCE_END
};

/** Bring up SoftAP and a CoAP listener until provisioning commits. */
static tw_err_t run_softap_provisioning(const tw_device_config_t *cfg)
{
    s_cfg = cfg;
    memset(&s_prov, 0, sizeof(s_prov));
    s_prov_done = false;

    PAL_LOGI(TAG, "starting SoftAP provisioning...");

    /* Build AP SSID with MAC suffix. */
    uint8_t mac[TW_MAC_ADDR_LEN];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char ap_ssid[TW_WIFI_SSID_BUF_SIZE];
    snprintf(ap_ssid, sizeof(ap_ssid), "%s-%02X%02X",
             CONFIG_TW_PROVISION_SOFTAP_SSID_PREFIX, mac[4], mac[5]);

    wifi_config_t ap_cfg;
    memset(&ap_cfg, 0, sizeof(ap_cfg));
    memcpy(ap_cfg.ap.ssid, ap_ssid, strlen(ap_ssid));
    ap_cfg.ap.ssid_len = (uint8_t)strlen(ap_ssid);
    ap_cfg.ap.max_connection = CONFIG_TW_PROVISION_SOFTAP_MAX_CONN;

    const char *ap_pass = CONFIG_TW_PROVISION_SOFTAP_PASSWORD;
    if (ap_pass[0]) {
        memcpy(ap_cfg.ap.password, ap_pass, strlen(ap_pass));
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    PAL_LOGI(TAG, "SoftAP started: %s (password: %s)",
             ap_ssid, ap_pass[0] ? "***" : "open");

    /* Start a minimal CoAP server on the AP interface. */
    tw_protocol_t *proto = tw_protocol_create();
    tw_err_t err = proto->init(proto, s_prov_resources, TW_COAP_DEFAULT_PORT);
    if (!tw_ok(err)) {
        PAL_LOGE(TAG, "provisioning CoAP server failed: %d", err);
        return err;
    }

    while (!s_prov_done) {
        proto->poll(proto, CONFIG_TW_PROVISION_POLL_INTERVAL_MS);
    }

    proto->deinit(proto);
    esp_wifi_stop();
    esp_wifi_deinit();

    PAL_LOGI(TAG, "SoftAP provisioning complete, rebooting...");
    pal_sleep_ms(REBOOT_DELAY_MS);
    pal_system_reboot();
    return TW_OK;
}

#endif /* CONFIG_TW_PROVISION_SOFTAP */

/* -------------------------------------------------------------------
 * ESP-IDF: CoAP listener on existing network (LAN provisioning)
 * -----------------------------------------------------------------*/

/** Listen on the station interface for CoAP provisioning until commit. */
static tw_err_t run_lan_provisioning(const tw_device_config_t *cfg)
{
    s_cfg = cfg;
    memset(&s_prov, 0, sizeof(s_prov));
    s_prov_done = false;

    PAL_LOGI(TAG, "waiting for LAN provisioning via CoAP...");

    static tw_msg_resource_t lan_resources[] = {
#ifdef CONFIG_TW_USE_PROTOBUF
        { "/tw/provision/factory", TW_MSG_POST, prov_on_factory },
        { "/tw/provision/hub",     TW_MSG_POST, prov_on_hub },
        { "/tw/provision/set",     TW_MSG_POST, prov_on_set_proto },
        { "/tw/provision/info",    TW_MSG_GET,  prov_on_info_proto },
#else
        { "/tw/provision/set",     TW_MSG_POST, prov_coap_set },
        { "/tw/provision/info",    TW_MSG_GET,  prov_coap_info },
#endif
        TW_MSG_RESOURCE_END
    };

    tw_protocol_t *proto = tw_protocol_create();
    tw_err_t err = proto->init(proto, lan_resources, TW_COAP_DEFAULT_PORT);
    if (!tw_ok(err)) return err;

    while (!s_prov_done) {
        proto->poll(proto, CONFIG_TW_PROVISION_POLL_INTERVAL_MS);
    }

    proto->deinit(proto);
    return TW_OK;
}

/* -------------------------------------------------------------------
 * ESP-IDF: main provisioning entry point
 * -----------------------------------------------------------------*/

tw_err_t svc_provision_run(const tw_device_config_t *cfg)
{
    if (cfg->on_provision)
        return cfg->on_provision(cfg);

#ifdef CONFIG_TW_TRANSPORT_BLE_PROVISIONING
    return run_ble_provisioning(cfg);
#elif defined(CONFIG_TW_PROVISION_SOFTAP)
    return run_softap_provisioning(cfg);
#else
    return run_lan_provisioning(cfg);
#endif
}

#endif /* TW_PLATFORM_ESPIDF */
