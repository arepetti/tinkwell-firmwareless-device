/*
 * test_provision_proto.c -- Unit tests for protobuf provisioning handlers.
 *
 * Tests the provisioning state machine (NVS-based factory_done,
 * factory_finalized, hub_provisioned flags) and the protobuf CoAP
 * handlers.
 *
 * Since the protobuf handlers (prov_on_factory, prov_on_hub, etc.)
 * are static within svc_provision.c and exposed only through resource
 * tables in ESP-IDF provisioning paths, these tests focus on:
 *   - Public API: svc_provision_is_needed()
 *   - NVS state round-trip through the POSIX provisioning path
 *   - Protobuf message encode/decode used by the handlers
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tw_types.h"
#include "tw_device.h"
#include "tw_identity.h"
#include "tw_hub.h"
#include "tw_msg.h"
#include "mock_pal.h"

#include <pb_encode.h>
#include <pb_decode.h>
#include "tw_protocol.pb.h"

#include <string.h>
#include <stdlib.h>

/* Forward declarations from svc_provision.c */
extern bool svc_provision_is_needed(void);
extern tw_err_t svc_provision_run(const tw_device_config_t *cfg);

/* ── Test device config ────────────────────────────────────────────── */

static const tw_device_config_t s_cfg = {
    .name                 = "test-prov",
    .fw_version           = "0.1.0",
    .vendor_id            = 42,
    .product_id           = 100,
    .vendor_display_name  = "TestVendor",
    .product_display_name = "TestProduct",
    .variant              = 0,
    .tick_interval_ms     = 1000,
};

/* ── setUp / tearDown ──────────────────────────────────────────────── */

void setUp(void)
{
    mock_reset();
    mock_config_reset();
    pal_nvs_init();
    tw_identity_init(&s_cfg);
}

void tearDown(void) {}

/* ── svc_provision_is_needed ───────────────────────────────────────── */

void test_provisioning_needed_when_fresh(void)
{
    TEST_ASSERT_TRUE(svc_provision_is_needed());
}

void test_provisioning_not_needed_when_hub_done(void)
{
    pal_nvs_set_i32("id_hubprov", 1);
    pal_nvs_commit();
    TEST_ASSERT_FALSE(svc_provision_is_needed());
}

/* ── Factory provisioning NVS keys ─────────────────────────────────── */

void test_factory_done_flag(void)
{
    int32_t flag = 0;
    TEST_ASSERT_NOT_EQUAL(TW_OK, pal_nvs_get_i32("fac_done", &flag));

    pal_nvs_set_i32("fac_done", 1);
    pal_nvs_commit();

    TEST_ASSERT_EQUAL(TW_OK, pal_nvs_get_i32("fac_done", &flag));
    TEST_ASSERT_EQUAL(1, flag);
}

void test_factory_finalized_flag(void)
{
    int32_t flag = 0;
    TEST_ASSERT_NOT_EQUAL(TW_OK, pal_nvs_get_i32("fac_final", &flag));

    pal_nvs_set_i32("fac_final", 1);
    pal_nvs_commit();

    TEST_ASSERT_EQUAL(TW_OK, pal_nvs_get_i32("fac_final", &flag));
    TEST_ASSERT_EQUAL(1, flag);
}

/* ── Protobuf message validation ───────────────────────────────────── */

void test_hub_provision_requires_16_byte_id(void)
{
    tw_HubProvisionCmd msg = tw_HubProvisionCmd_init_zero;
    uint8_t id[16];
    memset(id, 0xAA, 16);
    memcpy(msg.id.bytes, id, 16);
    msg.id.size = 16;

    uint8_t buf[64];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    TEST_ASSERT_TRUE(pb_encode(&os, tw_HubProvisionCmd_fields, &msg));

    tw_HubProvisionCmd dec = tw_HubProvisionCmd_init_zero;
    pb_istream_t is = pb_istream_from_buffer(buf, os.bytes_written);
    TEST_ASSERT_TRUE(pb_decode(&is, tw_HubProvisionCmd_fields, &dec));

    TEST_ASSERT_EQUAL(16, dec.id.size);
    TEST_ASSERT_EQUAL_MEMORY(id, dec.id.bytes, 16);
}

void test_factory_provision_with_finalize(void)
{
    tw_FactoryProvisionCmd msg = tw_FactoryProvisionCmd_init_zero;
    msg.vendor_id = 99;
    msg.product_id = 200;
    msg.finalize = true;

    uint8_t buf[256];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    TEST_ASSERT_TRUE(pb_encode(&os, tw_FactoryProvisionCmd_fields, &msg));

    tw_FactoryProvisionCmd dec = tw_FactoryProvisionCmd_init_zero;
    pb_istream_t is = pb_istream_from_buffer(buf, os.bytes_written);
    TEST_ASSERT_TRUE(pb_decode(&is, tw_FactoryProvisionCmd_fields, &dec));

    TEST_ASSERT_EQUAL(99, dec.vendor_id);
    TEST_ASSERT_EQUAL(200, dec.product_id);
    TEST_ASSERT_TRUE(dec.finalize);
}

void test_provision_info_encodes_status(void)
{
    tw_ProvisionInfo info = tw_ProvisionInfo_init_zero;
    strncpy(info.status, "factory-pending", sizeof(info.status) - 1);
    info.factory_done = false;
    info.factory_finalized = false;

    uint8_t buf[512];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    TEST_ASSERT_TRUE(pb_encode(&os, tw_ProvisionInfo_fields, &info));

    tw_ProvisionInfo dec = tw_ProvisionInfo_init_zero;
    pb_istream_t is = pb_istream_from_buffer(buf, os.bytes_written);
    TEST_ASSERT_TRUE(pb_decode(&is, tw_ProvisionInfo_fields, &dec));

    TEST_ASSERT_EQUAL_STRING("factory-pending", dec.status);
    TEST_ASSERT_FALSE(dec.factory_done);
    TEST_ASSERT_FALSE(dec.factory_finalized);
}

void test_provision_info_with_device_info(void)
{
    tw_ProvisionInfo info = tw_ProvisionInfo_init_zero;
    strncpy(info.status, "provisioned", sizeof(info.status) - 1);
    info.factory_done = true;
    info.factory_finalized = true;
    info.has_device = true;
    info.device.vendor_id = 42;
    info.device.product_id = 100;
    strncpy(info.device.fw_version, "1.0.0",
            sizeof(info.device.fw_version) - 1);

    uint8_t buf[512];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    TEST_ASSERT_TRUE(pb_encode(&os, tw_ProvisionInfo_fields, &info));

    tw_ProvisionInfo dec = tw_ProvisionInfo_init_zero;
    pb_istream_t is = pb_istream_from_buffer(buf, os.bytes_written);
    TEST_ASSERT_TRUE(pb_decode(&is, tw_ProvisionInfo_fields, &dec));

    TEST_ASSERT_TRUE(dec.factory_done);
    TEST_ASSERT_TRUE(dec.factory_finalized);
    TEST_ASSERT_TRUE(dec.has_device);
    TEST_ASSERT_EQUAL(42, dec.device.vendor_id);
    TEST_ASSERT_EQUAL(100, dec.device.product_id);
    TEST_ASSERT_EQUAL_STRING("1.0.0", dec.device.fw_version);
}

/* ── POSIX provisioning path ───────────────────────────────────────── */

void test_posix_provision_with_env_vars(void)
{
    /* Set environment variables for POSIX provisioning. */
    #ifdef _WIN32
    _putenv_s("TW_VENDOR_ID", "77");
    _putenv_s("TW_PRODUCT_ID", "88");
    _putenv_s("TW_WIFI_SSID", "TestNet");
    _putenv_s("TW_WIFI_PASS", "secret123");
    _putenv_s("TW_HUB_ADDR", "coap://10.0.0.1:5684");
    #else
    setenv("TW_VENDOR_ID", "77", 1);
    setenv("TW_PRODUCT_ID", "88", 1);
    setenv("TW_WIFI_SSID", "TestNet", 1);
    setenv("TW_WIFI_PASS", "secret123", 1);
    setenv("TW_HUB_ADDR", "coap://10.0.0.1:5684", 1);
    #endif

    tw_err_t err = svc_provision_run(&s_cfg);
    TEST_ASSERT_EQUAL(TW_OK, err);

    /* Verify WiFi credentials were stored. */
    char ssid[64];
    TEST_ASSERT_EQUAL(TW_OK, pal_nvs_get_str("wifi_ssid", ssid, sizeof(ssid)));
    TEST_ASSERT_EQUAL_STRING("TestNet", ssid);

    /* Clean up environment. */
    #ifdef _WIN32
    _putenv_s("TW_VENDOR_ID", "");
    _putenv_s("TW_PRODUCT_ID", "");
    _putenv_s("TW_WIFI_SSID", "");
    _putenv_s("TW_WIFI_PASS", "");
    _putenv_s("TW_HUB_ADDR", "");
    #else
    unsetenv("TW_VENDOR_ID");
    unsetenv("TW_PRODUCT_ID");
    unsetenv("TW_WIFI_SSID");
    unsetenv("TW_WIFI_PASS");
    unsetenv("TW_HUB_ADDR");
    #endif
}

void test_posix_provision_marks_hub_done(void)
{
    #ifdef _WIN32
    _putenv_s("TW_WIFI_SSID", "Net");
    _putenv_s("TW_HUB_ADDR", "coap://1.2.3.4:5684");
    #else
    setenv("TW_WIFI_SSID", "Net", 1);
    setenv("TW_HUB_ADDR", "coap://1.2.3.4:5684", 1);
    #endif

    svc_provision_run(&s_cfg);

    int32_t hub_prov = 0;
    pal_nvs_get_i32("id_hubprov", &hub_prov);
    TEST_ASSERT_EQUAL(1, hub_prov);
    TEST_ASSERT_FALSE(svc_provision_is_needed());

    #ifdef _WIN32
    _putenv_s("TW_WIFI_SSID", "");
    _putenv_s("TW_HUB_ADDR", "");
    #else
    unsetenv("TW_WIFI_SSID");
    unsetenv("TW_HUB_ADDR");
    #endif
}

/* ── ProvisionReply success / error ────────────────────────────────── */

void test_provision_reply_success(void)
{
    tw_ProvisionReply reply = tw_ProvisionReply_init_zero;
    reply.success = true;

    uint8_t buf[64];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    TEST_ASSERT_TRUE(pb_encode(&os, tw_ProvisionReply_fields, &reply));

    tw_ProvisionReply dec = tw_ProvisionReply_init_zero;
    pb_istream_t is = pb_istream_from_buffer(buf, os.bytes_written);
    TEST_ASSERT_TRUE(pb_decode(&is, tw_ProvisionReply_fields, &dec));

    TEST_ASSERT_TRUE(dec.success);
}

void test_provision_reply_with_error_message(void)
{
    tw_ProvisionReply reply = tw_ProvisionReply_init_zero;
    reply.success = false;
    strncpy(reply.error, "factory locked", sizeof(reply.error) - 1);

    uint8_t buf[128];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    TEST_ASSERT_TRUE(pb_encode(&os, tw_ProvisionReply_fields, &reply));

    tw_ProvisionReply dec = tw_ProvisionReply_init_zero;
    pb_istream_t is = pb_istream_from_buffer(buf, os.bytes_written);
    TEST_ASSERT_TRUE(pb_decode(&is, tw_ProvisionReply_fields, &dec));

    TEST_ASSERT_FALSE(dec.success);
    TEST_ASSERT_EQUAL_STRING("factory locked", dec.error);
}

/* ── ProvisionSetCmd with WiFi config ──────────────────────────────── */

void test_provision_set_cmd_wifi_fields(void)
{
    tw_ProvisionSetCmd msg = tw_ProvisionSetCmd_init_zero;
    msg.fields_count = 3;
    strncpy(msg.fields[0].key, "ssid", sizeof(msg.fields[0].key) - 1);
    strncpy(msg.fields[0].value, "HomeNet", sizeof(msg.fields[0].value) - 1);
    strncpy(msg.fields[1].key, "password", sizeof(msg.fields[1].key) - 1);
    strncpy(msg.fields[1].value, "pass1234", sizeof(msg.fields[1].value) - 1);
    strncpy(msg.fields[2].key, "hub-url", sizeof(msg.fields[2].key) - 1);
    strncpy(msg.fields[2].value, "coap://hub:5684",
            sizeof(msg.fields[2].value) - 1);

    uint8_t buf[512];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    TEST_ASSERT_TRUE(pb_encode(&os, tw_ProvisionSetCmd_fields, &msg));
    TEST_ASSERT_GREATER_THAN(0, os.bytes_written);

    tw_ProvisionSetCmd dec = tw_ProvisionSetCmd_init_zero;
    pb_istream_t is = pb_istream_from_buffer(buf, os.bytes_written);
    TEST_ASSERT_TRUE(pb_decode(&is, tw_ProvisionSetCmd_fields, &dec));

    TEST_ASSERT_EQUAL(3, dec.fields_count);
    TEST_ASSERT_EQUAL_STRING("ssid", dec.fields[0].key);
    TEST_ASSERT_EQUAL_STRING("HomeNet", dec.fields[0].value);
    TEST_ASSERT_EQUAL_STRING("password", dec.fields[1].key);
    TEST_ASSERT_EQUAL_STRING("pass1234", dec.fields[1].value);
    TEST_ASSERT_EQUAL_STRING("hub-url", dec.fields[2].key);
    TEST_ASSERT_EQUAL_STRING("coap://hub:5684", dec.fields[2].value);
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_provisioning_needed_when_fresh);
    RUN_TEST(test_provisioning_not_needed_when_hub_done);
    RUN_TEST(test_factory_done_flag);
    RUN_TEST(test_factory_finalized_flag);

    RUN_TEST(test_hub_provision_requires_16_byte_id);
    RUN_TEST(test_factory_provision_with_finalize);
    RUN_TEST(test_provision_info_encodes_status);
    RUN_TEST(test_provision_info_with_device_info);

    RUN_TEST(test_posix_provision_with_env_vars);
    RUN_TEST(test_posix_provision_marks_hub_done);

    RUN_TEST(test_provision_reply_success);
    RUN_TEST(test_provision_reply_with_error_message);
    RUN_TEST(test_provision_set_cmd_wifi_fields);

    return UNITY_END();
}
