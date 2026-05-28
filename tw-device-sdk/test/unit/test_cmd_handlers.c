/*
 * test_cmd_handlers.c -- Unit tests for hub-pushed command handlers.
 *
 * Tests svc_cmd_resources[] handlers with protobuf-encoded payloads.
 * Uses the exported resource table so we can call handlers without
 * making them non-static.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tw_cmd.h"
#include "tw_msg.h"
#include "tw_device.h"
#include "tw_config.h"
#include "mock_pal.h"

#include <pb_encode.h>
#include "tw_protocol.pb.h"

#include <string.h>

/* ── Test state ─────────────────────────────────────────────────────── */

static int s_on_command_count;
static char s_last_command[64];
static uint8_t s_last_payload[256];
static size_t s_last_payload_len;

static tw_err_t test_on_command(const tw_device_config_t *dev,
                                 const char *command,
                                 const uint8_t *payload, size_t payload_len)
{
    (void)dev;
    s_on_command_count++;
    strncpy(s_last_command, command, sizeof(s_last_command) - 1);
    if (payload && payload_len > 0 && payload_len <= sizeof(s_last_payload)) {
        memcpy(s_last_payload, payload, payload_len);
        s_last_payload_len = payload_len;
    } else {
        s_last_payload_len = 0;
    }
    return TW_OK;
}

static tw_device_config_t s_cfg = {
    .name       = "test",
    .fw_version = "0.1.0",
    .vendor_id  = 1,
    .product_id = 1,
    .on_command  = test_on_command,
};

/* Helper to find a handler by path in the resource table. */
static tw_msg_handler_t find_handler(const char *path)
{
    for (tw_msg_resource_t *r = svc_cmd_resources; r->path; r++) {
        if (strcmp(r->path, path) == 0) return r->handler;
    }
    return NULL;
}

/* Helper for creating request/response with protobuf payload. */
static uint8_t s_resp_buf[512];

static void make_request(tw_msg_request_t *req, tw_msg_response_t *resp,
                         const char *path, const uint8_t *payload, size_t len)
{
    memset(req, 0, sizeof(*req));
    memset(resp, 0, sizeof(*resp));
    req->method       = TW_MSG_POST;
    req->path         = path;
    req->payload      = payload;
    req->payload_len  = len;
    resp->payload          = s_resp_buf;
    resp->payload_capacity = sizeof(s_resp_buf);
}

/* ── setUp / tearDown ──────────────────────────────────────────────── */

void setUp(void)
{
    mock_reset();
    mock_config_reset();
    s_on_command_count = 0;
    memset(s_last_command, 0, sizeof(s_last_command));
    memset(s_last_payload, 0, sizeof(s_last_payload));
    s_last_payload_len = 0;
    svc_cmd_init(&s_cfg);
    pal_nvs_init();
}

void tearDown(void) {}

/* ── Resource table structure ──────────────────────────────────────── */

void test_resource_table_has_four_entries(void)
{
    int count = 0;
    for (tw_msg_resource_t *r = svc_cmd_resources; r->path; r++)
        count++;
    TEST_ASSERT_EQUAL(4, count);
}

void test_reboot_path_exists(void)
{
    TEST_ASSERT_NOT_NULL(find_handler("/tw/reboot"));
}

void test_set_config_path_exists(void)
{
    TEST_ASSERT_NOT_NULL(find_handler("/tw/set-config"));
}

void test_ota_available_path_exists(void)
{
    TEST_ASSERT_NOT_NULL(find_handler("/tw/ota-available"));
}

void test_app_path_exists(void)
{
    TEST_ASSERT_NOT_NULL(find_handler("/tw/app"));
}

/* ── /tw/reboot ────────────────────────────────────────────────────── */

void test_reboot_responds_204(void)
{
    tw_msg_handler_t h = find_handler("/tw/reboot");
    tw_msg_request_t req;
    tw_msg_response_t resp;
    make_request(&req, &resp, "/tw/reboot", NULL, 0);

    tw_err_t err = h(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_204_CHANGED, resp.code);
    TEST_ASSERT_TRUE(mock_count("pal_system_reboot") > 0);
}

/* ── /tw/set-config ────────────────────────────────────────────────── */

void test_set_config_empty_payload_returns_400(void)
{
    tw_msg_handler_t h = find_handler("/tw/set-config");
    tw_msg_request_t req;
    tw_msg_response_t resp;
    make_request(&req, &resp, "/tw/set-config", NULL, 0);

    tw_err_t err = h(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_400_BAD_REQ, resp.code);
}

void test_set_config_decodes_entries(void)
{
    tw_SetConfigCmd msg = tw_SetConfigCmd_init_zero;
    msg.entries_count = 1;
    strncpy(msg.entries[0].key, "mode", sizeof(msg.entries[0].key) - 1);
    strncpy(msg.entries[0].value, "heat", sizeof(msg.entries[0].value) - 1);

    uint8_t buf[256];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    TEST_ASSERT_TRUE(pb_encode(&os, tw_SetConfigCmd_fields, &msg));

    tw_msg_handler_t h = find_handler("/tw/set-config");
    tw_msg_request_t req;
    tw_msg_response_t resp;
    make_request(&req, &resp, "/tw/set-config", buf, os.bytes_written);

    tw_err_t err = h(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_204_CHANGED, resp.code);

    /* Verify the config was written through to NVS. */
    char val[64];
    const char *got = tw_config_get_str("mode", "", val, sizeof(val));
    TEST_ASSERT_EQUAL_STRING("heat", got);
}

/* ── /tw/ota-available ─────────────────────────────────────────────── */

void test_ota_available_empty_payload_returns_400(void)
{
    tw_msg_handler_t h = find_handler("/tw/ota-available");
    tw_msg_request_t req;
    tw_msg_response_t resp;
    make_request(&req, &resp, "/tw/ota-available", NULL, 0);

    tw_err_t err = h(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_400_BAD_REQ, resp.code);
}

void test_ota_available_decodes_and_notifies(void)
{
    tw_OtaAvailableCmd msg = tw_OtaAvailableCmd_init_zero;
    strncpy(msg.url, "https://fw.example.com/v2.bin", sizeof(msg.url) - 1);
    msg.size_bytes = 100000;

    uint8_t buf[256];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    TEST_ASSERT_TRUE(pb_encode(&os, tw_OtaAvailableCmd_fields, &msg));

    tw_msg_handler_t h = find_handler("/tw/ota-available");
    tw_msg_request_t req;
    tw_msg_response_t resp;
    make_request(&req, &resp, "/tw/ota-available", buf, os.bytes_written);

    tw_err_t err = h(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_204_CHANGED, resp.code);

    /* Verify on_command was called. */
    TEST_ASSERT_EQUAL(1, s_on_command_count);
    TEST_ASSERT_EQUAL_STRING("ota-available", s_last_command);

    /* Verify OTA URL was stored in config. */
    char val[128];
    const char *got = tw_config_get_str("ota_url", "", val, sizeof(val));
    TEST_ASSERT_EQUAL_STRING("https://fw.example.com/v2.bin", got);
}

/* ── /tw/app ───────────────────────────────────────────────────────── */

void test_app_forwards_payload_to_callback(void)
{
    tw_AppCmd msg = tw_AppCmd_init_zero;
    uint8_t payload_data[] = {0x01, 0x02, 0x03};
    memcpy(msg.payload.bytes, payload_data, sizeof(payload_data));
    msg.payload.size = sizeof(payload_data);

    uint8_t buf[128];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    TEST_ASSERT_TRUE(pb_encode(&os, tw_AppCmd_fields, &msg));

    tw_msg_handler_t h = find_handler("/tw/app");
    tw_msg_request_t req;
    tw_msg_response_t resp;
    make_request(&req, &resp, "/tw/app", buf, os.bytes_written);

    tw_err_t err = h(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_204_CHANGED, resp.code);

    TEST_ASSERT_EQUAL(1, s_on_command_count);
    TEST_ASSERT_EQUAL_STRING("app", s_last_command);
    TEST_ASSERT_EQUAL(3, s_last_payload_len);
    TEST_ASSERT_EQUAL_MEMORY(payload_data, s_last_payload, 3);
}

void test_app_empty_payload_accepted(void)
{
    tw_msg_handler_t h = find_handler("/tw/app");
    tw_msg_request_t req;
    tw_msg_response_t resp;
    make_request(&req, &resp, "/tw/app", NULL, 0);

    tw_err_t err = h(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_204_CHANGED, resp.code);
}

/* ── Invalid protobuf ──────────────────────────────────────────────── */

void test_set_config_garbage_returns_400(void)
{
    uint8_t garbage[] = {0xFF, 0xFF, 0xFF, 0xFF};
    tw_msg_handler_t h = find_handler("/tw/set-config");
    tw_msg_request_t req;
    tw_msg_response_t resp;
    make_request(&req, &resp, "/tw/set-config", garbage, sizeof(garbage));

    tw_err_t err = h(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_400_BAD_REQ, resp.code);
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_resource_table_has_four_entries);
    RUN_TEST(test_reboot_path_exists);
    RUN_TEST(test_set_config_path_exists);
    RUN_TEST(test_ota_available_path_exists);
    RUN_TEST(test_app_path_exists);

    RUN_TEST(test_reboot_responds_204);

    RUN_TEST(test_set_config_empty_payload_returns_400);
    RUN_TEST(test_set_config_decodes_entries);
    RUN_TEST(test_set_config_garbage_returns_400);

    RUN_TEST(test_ota_available_empty_payload_returns_400);
    RUN_TEST(test_ota_available_decodes_and_notifies);

    RUN_TEST(test_app_forwards_payload_to_callback);
    RUN_TEST(test_app_empty_payload_accepted);

    return UNITY_END();
}
