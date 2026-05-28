/*
 * test_proto_roundtrip.c -- Protobuf encode/decode roundtrip tests.
 *
 * Verifies that all tw_protocol.proto messages survive a nanopb
 * encode-then-decode cycle with correct field values, catching
 * schema-vs-options mismatches (max_size, fixed_length, max_count).
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include "tw_protocol.pb.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Helpers ────────────────────────────────────────────────────────── */

static uint8_t s_buf[1024];

#define ENCODE_OK(msg_type, msg_ptr) do {                           \
    pb_ostream_t _os = pb_ostream_from_buffer(s_buf, sizeof(s_buf));\
    TEST_ASSERT_TRUE_MESSAGE(                                       \
        pb_encode(&_os, msg_type##_fields, (msg_ptr)),              \
        PB_GET_ERROR(&_os));                                        \
    s_encoded_len = _os.bytes_written;                              \
} while (0)

#define DECODE_OK(msg_type, msg_ptr) do {                            \
    pb_istream_t _is = pb_istream_from_buffer(s_buf, s_encoded_len); \
    TEST_ASSERT_TRUE_MESSAGE(                                        \
        pb_decode(&_is, msg_type##_fields, (msg_ptr)),               \
        PB_GET_ERROR(&_is));                                         \
} while (0)

static size_t s_encoded_len;

/* ── HeartbeatPayload / HeartbeatReply ─────────────────────────────── */

void test_heartbeat_payload_roundtrip(void)
{
    tw_HeartbeatPayload orig = tw_HeartbeatPayload_init_zero;
    uint8_t id[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    memcpy(orig.id.bytes, id, 16);
    orig.id.size       = 16;
    orig.vendor_id     = 42;
    orig.product_id    = 100;
    orig.serial_number = 12345;
    strncpy(orig.fw_version, "1.2.3", sizeof(orig.fw_version) - 1);
    orig.uptime_ms     = 999888777;
    orig.free_heap     = 131072;
    orig.boot_reason   = 2;

    ENCODE_OK(tw_HeartbeatPayload, &orig);
    TEST_ASSERT_GREATER_THAN(0, s_encoded_len);

    tw_HeartbeatPayload dec = tw_HeartbeatPayload_init_zero;
    DECODE_OK(tw_HeartbeatPayload, &dec);

    TEST_ASSERT_EQUAL(16, dec.id.size);
    TEST_ASSERT_EQUAL_MEMORY(id, dec.id.bytes, 16);
    TEST_ASSERT_EQUAL(42, dec.vendor_id);
    TEST_ASSERT_EQUAL(100, dec.product_id);
    TEST_ASSERT_EQUAL(12345, dec.serial_number);
    TEST_ASSERT_EQUAL_STRING("1.2.3", dec.fw_version);
    TEST_ASSERT_EQUAL(999888777, dec.uptime_ms);
    TEST_ASSERT_EQUAL(131072, dec.free_heap);
    TEST_ASSERT_EQUAL(2, dec.boot_reason);
}

void test_heartbeat_payload_with_sensors(void)
{
    tw_HeartbeatPayload orig = tw_HeartbeatPayload_init_zero;
    orig.sensors_count = 2;
    strncpy(orig.sensors[0].name, "temperature", sizeof(orig.sensors[0].name) - 1);
    orig.sensors[0].value = 23.5;
    orig.sensors[0].timestamp_ms = 1000;
    strncpy(orig.sensors[1].name, "humidity", sizeof(orig.sensors[1].name) - 1);
    orig.sensors[1].value = 45.0;
    orig.sensors[1].timestamp_ms = 1001;

    ENCODE_OK(tw_HeartbeatPayload, &orig);

    tw_HeartbeatPayload dec = tw_HeartbeatPayload_init_zero;
    DECODE_OK(tw_HeartbeatPayload, &dec);

    TEST_ASSERT_EQUAL(2, dec.sensors_count);
    TEST_ASSERT_EQUAL_STRING("temperature", dec.sensors[0].name);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 23.5, dec.sensors[0].value);
    TEST_ASSERT_EQUAL_STRING("humidity", dec.sensors[1].name);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 45.0, dec.sensors[1].value);
}

void test_heartbeat_reply_roundtrip(void)
{
    tw_HeartbeatReply orig = tw_HeartbeatReply_init_zero;
    orig.pending = 7;

    ENCODE_OK(tw_HeartbeatReply, &orig);

    tw_HeartbeatReply dec = tw_HeartbeatReply_init_zero;
    DECODE_OK(tw_HeartbeatReply, &dec);

    TEST_ASSERT_EQUAL(7, dec.pending);
}

void test_heartbeat_reply_zero_pending(void)
{
    tw_HeartbeatReply orig = tw_HeartbeatReply_init_zero;
    orig.pending = 0;

    ENCODE_OK(tw_HeartbeatReply, &orig);

    tw_HeartbeatReply dec = tw_HeartbeatReply_init_zero;
    DECODE_OK(tw_HeartbeatReply, &dec);

    TEST_ASSERT_EQUAL(0, dec.pending);
}

/* ── SetConfigCmd ──────────────────────────────────────────────────── */

void test_set_config_cmd_roundtrip(void)
{
    tw_SetConfigCmd orig = tw_SetConfigCmd_init_zero;
    orig.entries_count = 2;
    strncpy(orig.entries[0].key, "mode", sizeof(orig.entries[0].key) - 1);
    strncpy(orig.entries[0].value, "cool", sizeof(orig.entries[0].value) - 1);
    strncpy(orig.entries[1].key, "target_temp", sizeof(orig.entries[1].key) - 1);
    strncpy(orig.entries[1].value, "22", sizeof(orig.entries[1].value) - 1);

    ENCODE_OK(tw_SetConfigCmd, &orig);

    tw_SetConfigCmd dec = tw_SetConfigCmd_init_zero;
    DECODE_OK(tw_SetConfigCmd, &dec);

    TEST_ASSERT_EQUAL(2, dec.entries_count);
    TEST_ASSERT_EQUAL_STRING("mode", dec.entries[0].key);
    TEST_ASSERT_EQUAL_STRING("cool", dec.entries[0].value);
    TEST_ASSERT_EQUAL_STRING("target_temp", dec.entries[1].key);
    TEST_ASSERT_EQUAL_STRING("22", dec.entries[1].value);
}

/* ── OtaAvailableCmd ───────────────────────────────────────────────── */

void test_ota_available_cmd_roundtrip(void)
{
    tw_OtaAvailableCmd orig = tw_OtaAvailableCmd_init_zero;
    strncpy(orig.url, "https://fw.example.com/v2.bin", sizeof(orig.url) - 1);
    uint8_t sha[32];
    memset(sha, 0xAB, 32);
    memcpy(orig.sha256.bytes, sha, 32);
    orig.sha256.size = 32;
    orig.size_bytes  = 512000;

    ENCODE_OK(tw_OtaAvailableCmd, &orig);

    tw_OtaAvailableCmd dec = tw_OtaAvailableCmd_init_zero;
    DECODE_OK(tw_OtaAvailableCmd, &dec);

    TEST_ASSERT_EQUAL_STRING("https://fw.example.com/v2.bin", dec.url);
    TEST_ASSERT_EQUAL(32, dec.sha256.size);
    TEST_ASSERT_EQUAL_MEMORY(sha, dec.sha256.bytes, 32);
    TEST_ASSERT_EQUAL(512000, dec.size_bytes);
}

/* ── AppCmd ─────────────────────────────────────────────────────────── */

void test_app_cmd_roundtrip(void)
{
    tw_AppCmd orig = tw_AppCmd_init_zero;
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    memcpy(orig.payload.bytes, data, sizeof(data));
    orig.payload.size = sizeof(data);

    ENCODE_OK(tw_AppCmd, &orig);

    tw_AppCmd dec = tw_AppCmd_init_zero;
    DECODE_OK(tw_AppCmd, &dec);

    TEST_ASSERT_EQUAL(4, dec.payload.size);
    TEST_ASSERT_EQUAL_MEMORY(data, dec.payload.bytes, 4);
}

/* ── HubProvisionCmd ───────────────────────────────────────────────── */

void test_hub_provision_cmd_roundtrip(void)
{
    tw_HubProvisionCmd orig = tw_HubProvisionCmd_init_zero;
    uint8_t id[16] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22,
                      0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00};
    memcpy(orig.id.bytes, id, 16);
    orig.id.size = 16;

    ENCODE_OK(tw_HubProvisionCmd, &orig);

    tw_HubProvisionCmd dec = tw_HubProvisionCmd_init_zero;
    DECODE_OK(tw_HubProvisionCmd, &dec);

    TEST_ASSERT_EQUAL(16, dec.id.size);
    TEST_ASSERT_EQUAL_MEMORY(id, dec.id.bytes, 16);
}

/* ── FactoryProvisionCmd ───────────────────────────────────────────── */

void test_factory_provision_cmd_roundtrip(void)
{
    tw_FactoryProvisionCmd orig = tw_FactoryProvisionCmd_init_zero;
    orig.vendor_id = 42;
    strncpy(orig.vendor_display_name, "TestVendor",
            sizeof(orig.vendor_display_name) - 1);
    orig.product_id = 100;
    strncpy(orig.product_display_name, "TestProduct",
            sizeof(orig.product_display_name) - 1);
    orig.serial_number = 99999;
    orig.variant.bytes[0] = 7;
    orig.variant.size = 1;
    uint8_t id[16];
    memset(id, 0xDE, 16);
    memcpy(orig.id.bytes, id, 16);
    orig.id.size = 16;
    orig.finalize = true;

    ENCODE_OK(tw_FactoryProvisionCmd, &orig);

    tw_FactoryProvisionCmd dec = tw_FactoryProvisionCmd_init_zero;
    DECODE_OK(tw_FactoryProvisionCmd, &dec);

    TEST_ASSERT_EQUAL(42, dec.vendor_id);
    TEST_ASSERT_EQUAL_STRING("TestVendor", dec.vendor_display_name);
    TEST_ASSERT_EQUAL(100, dec.product_id);
    TEST_ASSERT_EQUAL_STRING("TestProduct", dec.product_display_name);
    TEST_ASSERT_EQUAL(99999, dec.serial_number);
    TEST_ASSERT_EQUAL(1, dec.variant.size);
    TEST_ASSERT_EQUAL(7, dec.variant.bytes[0]);
    TEST_ASSERT_EQUAL(16, dec.id.size);
    TEST_ASSERT_EQUAL_MEMORY(id, dec.id.bytes, 16);
    TEST_ASSERT_TRUE(dec.finalize);
}

void test_factory_provision_cmd_partial(void)
{
    tw_FactoryProvisionCmd orig = tw_FactoryProvisionCmd_init_zero;
    orig.vendor_id = 55;
    /* Leave all other fields at zero/empty. */

    ENCODE_OK(tw_FactoryProvisionCmd, &orig);

    tw_FactoryProvisionCmd dec = tw_FactoryProvisionCmd_init_zero;
    DECODE_OK(tw_FactoryProvisionCmd, &dec);

    TEST_ASSERT_EQUAL(55, dec.vendor_id);
    TEST_ASSERT_EQUAL(0, dec.product_id);
    TEST_ASSERT_EQUAL_STRING("", dec.vendor_display_name);
    TEST_ASSERT_FALSE(dec.finalize);
}

/* ── ProvisionInfo ─────────────────────────────────────────────────── */

void test_provision_info_roundtrip(void)
{
    tw_ProvisionInfo orig = tw_ProvisionInfo_init_zero;
    strncpy(orig.status, "hub-pending", sizeof(orig.status) - 1);
    orig.factory_done      = true;
    orig.factory_finalized = false;
    orig.has_device        = true;
    orig.device.vendor_id  = 42;
    orig.device.product_id = 100;

    ENCODE_OK(tw_ProvisionInfo, &orig);

    tw_ProvisionInfo dec = tw_ProvisionInfo_init_zero;
    DECODE_OK(tw_ProvisionInfo, &dec);

    TEST_ASSERT_EQUAL_STRING("hub-pending", dec.status);
    TEST_ASSERT_TRUE(dec.factory_done);
    TEST_ASSERT_FALSE(dec.factory_finalized);
    TEST_ASSERT_TRUE(dec.has_device);
    TEST_ASSERT_EQUAL(42, dec.device.vendor_id);
    TEST_ASSERT_EQUAL(100, dec.device.product_id);
}

/* ── ProvisionReply ────────────────────────────────────────────────── */

void test_provision_reply_roundtrip(void)
{
    tw_ProvisionReply orig = tw_ProvisionReply_init_zero;
    orig.success = true;

    ENCODE_OK(tw_ProvisionReply, &orig);

    tw_ProvisionReply dec = tw_ProvisionReply_init_zero;
    DECODE_OK(tw_ProvisionReply, &dec);

    TEST_ASSERT_TRUE(dec.success);
    TEST_ASSERT_EQUAL_STRING("", dec.error);
}

void test_provision_reply_error(void)
{
    tw_ProvisionReply orig = tw_ProvisionReply_init_zero;
    orig.success = false;
    strncpy(orig.error, "failed", sizeof(orig.error) - 1);

    ENCODE_OK(tw_ProvisionReply, &orig);

    tw_ProvisionReply dec = tw_ProvisionReply_init_zero;
    DECODE_OK(tw_ProvisionReply, &dec);

    TEST_ASSERT_FALSE(dec.success);
    TEST_ASSERT_EQUAL_STRING("failed", dec.error);
}

/* ── OtaBeginCmd / OtaStatus / OtaCommitReply ──────────────────────── */

void test_ota_begin_cmd_roundtrip(void)
{
    tw_OtaBeginCmd orig = tw_OtaBeginCmd_init_zero;
    orig.size_bytes = 256000;
    uint8_t sha[32];
    memset(sha, 0x42, 32);
    memcpy(orig.sha256.bytes, sha, 32);
    orig.sha256.size = 32;
    strncpy(orig.version, "2.0.0", sizeof(orig.version) - 1);

    ENCODE_OK(tw_OtaBeginCmd, &orig);

    tw_OtaBeginCmd dec = tw_OtaBeginCmd_init_zero;
    DECODE_OK(tw_OtaBeginCmd, &dec);

    TEST_ASSERT_EQUAL(256000, dec.size_bytes);
    TEST_ASSERT_EQUAL(32, dec.sha256.size);
    TEST_ASSERT_EQUAL_MEMORY(sha, dec.sha256.bytes, 32);
    TEST_ASSERT_EQUAL_STRING("2.0.0", dec.version);
}

void test_ota_status_roundtrip(void)
{
    tw_OtaStatus orig = tw_OtaStatus_init_zero;
    strncpy(orig.state, "downloading", sizeof(orig.state) - 1);
    orig.progress_percent = 75;

    ENCODE_OK(tw_OtaStatus, &orig);

    tw_OtaStatus dec = tw_OtaStatus_init_zero;
    DECODE_OK(tw_OtaStatus, &dec);

    TEST_ASSERT_EQUAL_STRING("downloading", dec.state);
    TEST_ASSERT_EQUAL(75, dec.progress_percent);
}

void test_ota_commit_reply_roundtrip(void)
{
    tw_OtaCommitReply orig = tw_OtaCommitReply_init_zero;
    orig.success = true;

    ENCODE_OK(tw_OtaCommitReply, &orig);

    tw_OtaCommitReply dec = tw_OtaCommitReply_init_zero;
    DECODE_OK(tw_OtaCommitReply, &dec);

    TEST_ASSERT_TRUE(dec.success);
}

/* ── AppletStatus / AppletCommitReply ──────────────────────────────── */

void test_applet_status_roundtrip(void)
{
    tw_AppletStatus orig = tw_AppletStatus_init_zero;
    strncpy(orig.state, "running", sizeof(orig.state) - 1);
    orig.size_bytes = 4096;

    ENCODE_OK(tw_AppletStatus, &orig);

    tw_AppletStatus dec = tw_AppletStatus_init_zero;
    DECODE_OK(tw_AppletStatus, &dec);

    TEST_ASSERT_EQUAL_STRING("running", dec.state);
    TEST_ASSERT_EQUAL(4096, dec.size_bytes);
}

/* ── TelemetryPush / TelemetryReply ───────────────────────────────── */

void test_telemetry_push_roundtrip(void)
{
    tw_TelemetryPush orig = tw_TelemetryPush_init_zero;
    orig.readings_count = 1;
    strncpy(orig.readings[0].name, "temp", sizeof(orig.readings[0].name) - 1);
    orig.readings[0].value = 21.7;
    orig.readings[0].timestamp_ms = 500;

    ENCODE_OK(tw_TelemetryPush, &orig);

    tw_TelemetryPush dec = tw_TelemetryPush_init_zero;
    DECODE_OK(tw_TelemetryPush, &dec);

    TEST_ASSERT_EQUAL(1, dec.readings_count);
    TEST_ASSERT_EQUAL_STRING("temp", dec.readings[0].name);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 21.7, dec.readings[0].value);
    TEST_ASSERT_EQUAL(500, dec.readings[0].timestamp_ms);
}

void test_telemetry_reply_roundtrip(void)
{
    tw_TelemetryReply orig = tw_TelemetryReply_init_zero;
    orig.next_interval_s = 30;

    ENCODE_OK(tw_TelemetryReply, &orig);

    tw_TelemetryReply dec = tw_TelemetryReply_init_zero;
    DECODE_OK(tw_TelemetryReply, &dec);

    TEST_ASSERT_EQUAL(30, dec.next_interval_s);
}

/* ── DeviceInfo ────────────────────────────────────────────────────── */

void test_device_info_roundtrip(void)
{
    tw_DeviceInfo orig = tw_DeviceInfo_init_zero;
    uint8_t id[16];
    memset(id, 0xCC, 16);
    memcpy(orig.id.bytes, id, 16);
    orig.id.size   = 16;
    orig.vendor_id = 42;
    orig.product_id = 100;
    orig.serial_number = 1;
    strncpy(orig.fw_version, "1.0.0", sizeof(orig.fw_version) - 1);
    strncpy(orig.device_name, "test-device", sizeof(orig.device_name) - 1);

    ENCODE_OK(tw_DeviceInfo, &orig);

    tw_DeviceInfo dec = tw_DeviceInfo_init_zero;
    DECODE_OK(tw_DeviceInfo, &dec);

    TEST_ASSERT_EQUAL(16, dec.id.size);
    TEST_ASSERT_EQUAL_MEMORY(id, dec.id.bytes, 16);
    TEST_ASSERT_EQUAL(42, dec.vendor_id);
    TEST_ASSERT_EQUAL(100, dec.product_id);
    TEST_ASSERT_EQUAL_STRING("1.0.0", dec.fw_version);
    TEST_ASSERT_EQUAL_STRING("test-device", dec.device_name);
}

/* ── ProvisionSetCmd ───────────────────────────────────────────────── */

void test_provision_set_cmd_roundtrip(void)
{
    tw_ProvisionSetCmd orig = tw_ProvisionSetCmd_init_zero;
    orig.fields_count = 2;
    strncpy(orig.fields[0].key, "ssid", sizeof(orig.fields[0].key) - 1);
    strncpy(orig.fields[0].value, "MyWiFi", sizeof(orig.fields[0].value) - 1);
    strncpy(orig.fields[1].key, "hub-url", sizeof(orig.fields[1].key) - 1);
    strncpy(orig.fields[1].value, "coap://10.0.0.1:5684",
            sizeof(orig.fields[1].value) - 1);

    ENCODE_OK(tw_ProvisionSetCmd, &orig);

    tw_ProvisionSetCmd dec = tw_ProvisionSetCmd_init_zero;
    DECODE_OK(tw_ProvisionSetCmd, &dec);

    TEST_ASSERT_EQUAL(2, dec.fields_count);
    TEST_ASSERT_EQUAL_STRING("ssid", dec.fields[0].key);
    TEST_ASSERT_EQUAL_STRING("MyWiFi", dec.fields[0].value);
    TEST_ASSERT_EQUAL_STRING("hub-url", dec.fields[1].key);
    TEST_ASSERT_EQUAL_STRING("coap://10.0.0.1:5684", dec.fields[1].value);
}

/* ── Empty messages encode to zero bytes ───────────────────────────── */

void test_empty_heartbeat_reply(void)
{
    tw_HeartbeatReply orig = tw_HeartbeatReply_init_zero;
    ENCODE_OK(tw_HeartbeatReply, &orig);
    /* proto3: all-default fields produce zero-length encoding. */
    TEST_ASSERT_EQUAL(0, s_encoded_len);
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_heartbeat_payload_roundtrip);
    RUN_TEST(test_heartbeat_payload_with_sensors);
    RUN_TEST(test_heartbeat_reply_roundtrip);
    RUN_TEST(test_heartbeat_reply_zero_pending);

    RUN_TEST(test_set_config_cmd_roundtrip);
    RUN_TEST(test_ota_available_cmd_roundtrip);
    RUN_TEST(test_app_cmd_roundtrip);

    RUN_TEST(test_hub_provision_cmd_roundtrip);
    RUN_TEST(test_factory_provision_cmd_roundtrip);
    RUN_TEST(test_factory_provision_cmd_partial);
    RUN_TEST(test_provision_info_roundtrip);
    RUN_TEST(test_provision_reply_roundtrip);
    RUN_TEST(test_provision_reply_error);
    RUN_TEST(test_provision_set_cmd_roundtrip);

    RUN_TEST(test_ota_begin_cmd_roundtrip);
    RUN_TEST(test_ota_status_roundtrip);
    RUN_TEST(test_ota_commit_reply_roundtrip);
    RUN_TEST(test_applet_status_roundtrip);

    RUN_TEST(test_telemetry_push_roundtrip);
    RUN_TEST(test_telemetry_reply_roundtrip);

    RUN_TEST(test_device_info_roundtrip);
    RUN_TEST(test_empty_heartbeat_reply);

    return UNITY_END();
}
