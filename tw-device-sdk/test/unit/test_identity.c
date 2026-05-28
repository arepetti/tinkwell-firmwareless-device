/*
 * test_identity.c -- Unit tests for the device identity service.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tw_identity.h"
#include "tw_device.h"
#include "tw_types.h"
#include "mock_pal.h"

#include <string.h>

static const tw_device_config_t test_cfg = {
    .name                 = "test-device",
    .fw_version           = "0.0.1",
    .vendor_id            = 42,
    .product_id           = 100,
    .vendor_display_name  = "TestVendor",
    .product_display_name = "TestProduct",
    .variant              = 3,
    .tick_interval_ms     = 1000,
};

void setUp(void)
{
    mock_reset();
    mock_config_reset();
    pal_nvs_init();
}

void tearDown(void) {}

/* ---- Init from compile-time defaults ---- */

void test_init_loads_defaults(void)
{
    tw_err_t err = tw_identity_init(&test_cfg);
    TEST_ASSERT_EQUAL(TW_OK, err);

    const tw_device_identity_t *id = tw_identity_get();
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_EQUAL(42, id->vendor_id);
    TEST_ASSERT_EQUAL(100, id->product_id);
    TEST_ASSERT_EQUAL_STRING("TestVendor", id->vendor_display_name);
    TEST_ASSERT_EQUAL_STRING("TestProduct", id->product_display_name);
    TEST_ASSERT_EQUAL(3, id->variant);
    TEST_ASSERT_EQUAL(0, id->serial_number);
    TEST_ASSERT_FALSE(id->uuid_valid);
    TEST_ASSERT_FALSE(id->factory_provisioned);
    TEST_ASSERT_FALSE(id->hub_provisioned);
    TEST_ASSERT_EQUAL(TW_KEY_NONE, id->key_type);
    TEST_ASSERT_FALSE(id->key_valid);
}

/* ---- NVS overrides ---- */

void test_init_nvs_overrides_vendor_id(void)
{
    pal_nvs_set_i32("id_vendor", 99);
    pal_nvs_commit();

    tw_identity_init(&test_cfg);
    const tw_device_identity_t *id = tw_identity_get();
    TEST_ASSERT_EQUAL(99, id->vendor_id);
}

void test_init_nvs_overrides_display_name(void)
{
    pal_nvs_set_str("id_vname", "OverriddenVendor");
    pal_nvs_commit();

    tw_identity_init(&test_cfg);
    const tw_device_identity_t *id = tw_identity_get();
    TEST_ASSERT_EQUAL_STRING("OverriddenVendor", id->vendor_display_name);
}

/* ---- UUID ---- */

void test_set_uuid(void)
{
    tw_identity_init(&test_cfg);

    uint8_t uuid[TW_UUID_SIZE] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
    };
    TEST_ASSERT_EQUAL(TW_OK, tw_identity_set_uuid(uuid));

    const tw_device_identity_t *id = tw_identity_get();
    TEST_ASSERT_TRUE(id->uuid_valid);
    TEST_ASSERT_EQUAL_MEMORY(uuid, id->uuid, TW_UUID_SIZE);

    uint8_t out[TW_UUID_SIZE];
    TEST_ASSERT_EQUAL(TW_OK, tw_identity_get_uuid(out));
    TEST_ASSERT_EQUAL_MEMORY(uuid, out, TW_UUID_SIZE);
}

void test_get_uuid_before_set_fails(void)
{
    tw_identity_init(&test_cfg);
    uint8_t out[TW_UUID_SIZE];
    TEST_ASSERT_EQUAL(TW_ERR_NOT_READY, tw_identity_get_uuid(out));
}

void test_set_uuid_null_rejected(void)
{
    tw_identity_init(&test_cfg);
    TEST_ASSERT_EQUAL(TW_ERR_INVAL, tw_identity_set_uuid(NULL));
}

/* ---- UUID string formatting ---- */

void test_uuid_str_format(void)
{
    uint8_t uuid[TW_UUID_SIZE] = {
        0xDE, 0xAD, 0xBE, 0xEF,
        0xCA, 0xFE,
        0xBA, 0xBE,
        0x01, 0x23,
        0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    };
    char buf[TW_UUID_STR_SIZE];
    tw_identity_uuid_to_str(uuid, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("deadbeef-cafe-babe-0123-456789abcdef", buf);
}

void test_uuid_str_short_buf(void)
{
    uint8_t uuid[TW_UUID_SIZE] = {0};
    char buf[10] = "unchanged";
    tw_identity_uuid_to_str(uuid, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

/* ---- Key type ---- */

void test_key_type_default_none(void)
{
    tw_identity_init(&test_cfg);
    TEST_ASSERT_EQUAL(TW_KEY_NONE, tw_identity_get_key_type());
    TEST_ASSERT_FALSE(tw_identity_has_key());
}

void test_key_type_from_nvs(void)
{
    pal_nvs_set_i32("id_key_type", TW_KEY_ED25519);
    uint8_t key[TW_KEY_SIZE];
    memset(key, 0x42, TW_KEY_SIZE);
    pal_nvs_set_blob("id_key", key, TW_KEY_SIZE);
    pal_nvs_commit();

    tw_identity_init(&test_cfg);
    TEST_ASSERT_EQUAL(TW_KEY_ED25519, tw_identity_get_key_type());
    TEST_ASSERT_TRUE(tw_identity_has_key());
}

/* ---- Ed25519 pubkey ---- */

void test_ed25519_pubkey_wrong_type(void)
{
    tw_identity_init(&test_cfg);
    uint8_t pub[TW_PUBKEY_SIZE];
    TEST_ASSERT_EQUAL(TW_ERR_INVAL, tw_identity_get_ed25519_pubkey(pub));
}

void test_ed25519_pubkey_no_key(void)
{
    pal_nvs_set_i32("id_key_type", TW_KEY_ED25519);
    pal_nvs_commit();

    tw_identity_init(&test_cfg);
    uint8_t pub[TW_PUBKEY_SIZE];
    TEST_ASSERT_EQUAL(TW_ERR_NOT_READY, tw_identity_get_ed25519_pubkey(pub));
}

void test_ed25519_pubkey_derivation(void)
{
    pal_nvs_set_i32("id_key_type", TW_KEY_ED25519);
    uint8_t key[TW_KEY_SIZE];
    memset(key, 0x42, TW_KEY_SIZE);
    pal_nvs_set_blob("id_key", key, TW_KEY_SIZE);
    pal_nvs_commit();

    tw_identity_init(&test_cfg);

    uint8_t pub[TW_PUBKEY_SIZE];
    tw_err_t err = tw_identity_get_ed25519_pubkey(pub);
    TEST_ASSERT_EQUAL(TW_OK, err);

    /* POSIX stub: pub[i] = key[i] ^ 0xA5 */
    for (int i = 0; i < TW_PUBKEY_SIZE; i++)
        TEST_ASSERT_EQUAL(0x42 ^ 0xA5, pub[i]);
}

/* ---- Factory provisioning ---- */

void test_factory_provisioning(void)
{
    tw_identity_init(&test_cfg);

    uint8_t uuid[TW_UUID_SIZE] = {0xAA};
    uint8_t key[TW_KEY_SIZE];
    memset(key, 0x55, TW_KEY_SIZE);

    tw_err_t err = tw_identity_set_factory(
        99, 200, "FactoryVendor", "FactoryProduct",
        7, 12345, uuid, TW_KEY_PSK, key, TW_KEY_SIZE);
    TEST_ASSERT_EQUAL(TW_OK, err);

    const tw_device_identity_t *id = tw_identity_get();
    TEST_ASSERT_EQUAL(99, id->vendor_id);
    TEST_ASSERT_EQUAL(200, id->product_id);
    TEST_ASSERT_EQUAL_STRING("FactoryVendor", id->vendor_display_name);
    TEST_ASSERT_EQUAL_STRING("FactoryProduct", id->product_display_name);
    TEST_ASSERT_EQUAL(7, id->variant);
    TEST_ASSERT_EQUAL(12345, id->serial_number);
    TEST_ASSERT_TRUE(id->factory_provisioned);
    TEST_ASSERT_FALSE(id->hub_provisioned);
    TEST_ASSERT_EQUAL(TW_KEY_PSK, id->key_type);
    TEST_ASSERT_TRUE(id->key_valid);
}

/* ---- set_field ---- */

void test_set_field_vendor_id(void)
{
    tw_identity_init(&test_cfg);
    int32_t val = 777;
    TEST_ASSERT_EQUAL(TW_OK,
        tw_identity_set_field("vendor_id", (const uint8_t *)&val, sizeof(val)));
    TEST_ASSERT_EQUAL(777, tw_identity_get()->vendor_id);
}

void test_set_field_unknown(void)
{
    tw_identity_init(&test_cfg);
    uint8_t val = 0;
    TEST_ASSERT_EQUAL(TW_ERR_NOT_FOUND,
        tw_identity_set_field("nonexistent", &val, 1));
}

/* ---- /tw/info ---- */

void test_msg_info_returns_content(void)
{
    tw_identity_init(&test_cfg);

    uint8_t payload_buf[512];
    tw_msg_request_t req = { .method = TW_MSG_GET, .path = "/tw/info" };
    tw_msg_response_t resp = {
        .payload = payload_buf,
        .payload_capacity = sizeof(payload_buf),
    };

    tw_err_t err = tw_identity_msg_info(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_205_CONTENT, resp.code);
    TEST_ASSERT_TRUE(resp.payload_len > 0);

    char *text = (char *)payload_buf;
    text[TW_MIN(resp.payload_len, sizeof(payload_buf) - 1)] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(text, "vendor_id=42"));
    TEST_ASSERT_NOT_NULL(strstr(text, "product_id=100"));
    TEST_ASSERT_NOT_NULL(strstr(text, "name=test-device"));
    TEST_ASSERT_NOT_NULL(strstr(text, "fw=0.0.1"));
    TEST_ASSERT_NOT_NULL(strstr(text, "key_type=0"));
    TEST_ASSERT_NOT_NULL(strstr(text, "key_valid=0"));
}

/* ---- /tw/identity/pubkey ---- */

void test_msg_pubkey_no_ed25519(void)
{
    tw_identity_init(&test_cfg);

    uint8_t payload_buf[128];
    tw_msg_request_t req = { .method = TW_MSG_GET, .path = "/tw/identity/pubkey" };
    tw_msg_response_t resp = {
        .payload = payload_buf,
        .payload_capacity = sizeof(payload_buf),
    };

    tw_err_t err = tw_identity_msg_pubkey(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_205_CONTENT, resp.code);
    TEST_ASSERT_TRUE(resp.payload_len > 0);
}

void test_msg_pubkey_with_ed25519(void)
{
    pal_nvs_set_i32("id_key_type", TW_KEY_ED25519);
    uint8_t key[TW_KEY_SIZE];
    memset(key, 0x42, TW_KEY_SIZE);
    pal_nvs_set_blob("id_key", key, TW_KEY_SIZE);
    pal_nvs_commit();

    tw_identity_init(&test_cfg);

    uint8_t payload_buf[128];
    tw_msg_request_t req = { .method = TW_MSG_GET, .path = "/tw/identity/pubkey" };
    tw_msg_response_t resp = {
        .payload = payload_buf,
        .payload_capacity = sizeof(payload_buf),
    };

    tw_err_t err = tw_identity_msg_pubkey(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(TW_MSG_205_CONTENT, resp.code);
    TEST_ASSERT_EQUAL(TW_PUBKEY_SIZE, resp.payload_len);

    /* POSIX stub: pub[i] = key[i] ^ 0xA5 */
    for (int i = 0; i < TW_PUBKEY_SIZE; i++)
        TEST_ASSERT_EQUAL(0x42 ^ 0xA5, payload_buf[i]);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_loads_defaults);
    RUN_TEST(test_init_nvs_overrides_vendor_id);
    RUN_TEST(test_init_nvs_overrides_display_name);

    RUN_TEST(test_set_uuid);
    RUN_TEST(test_get_uuid_before_set_fails);
    RUN_TEST(test_set_uuid_null_rejected);

    RUN_TEST(test_uuid_str_format);
    RUN_TEST(test_uuid_str_short_buf);

    RUN_TEST(test_key_type_default_none);
    RUN_TEST(test_key_type_from_nvs);

    RUN_TEST(test_ed25519_pubkey_wrong_type);
    RUN_TEST(test_ed25519_pubkey_no_key);
    RUN_TEST(test_ed25519_pubkey_derivation);

    RUN_TEST(test_factory_provisioning);

    RUN_TEST(test_set_field_vendor_id);
    RUN_TEST(test_set_field_unknown);

    RUN_TEST(test_msg_info_returns_content);
    RUN_TEST(test_msg_pubkey_no_ed25519);
    RUN_TEST(test_msg_pubkey_with_ed25519);

    return UNITY_END();
}
