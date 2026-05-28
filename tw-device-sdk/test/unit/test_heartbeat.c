/*
 * test_heartbeat.c -- Unit tests for hub heartbeat service.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tw_hub.h"
#include "tw_types.h"
#include "tw_identity.h"
#include "mock_pal.h"

#include <string.h>

void setUp(void)
{
    mock_reset();
    mock_config_reset();
}

void tearDown(void) {}

void test_set_address_parses_coap_uri(void)
{
    tw_err_t err = tw_hub_set_address("coap://192.168.1.50:5684");
    TEST_ASSERT_EQUAL(TW_OK, err);

    char buf[128];
    err = tw_hub_get_address(buf, sizeof(buf));
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL_STRING("coap://192.168.1.50:5684", buf);
}

void test_set_address_without_scheme(void)
{
    tw_err_t err = tw_hub_set_address("10.0.0.1:9999");
    TEST_ASSERT_EQUAL(TW_OK, err);

    char buf[128];
    tw_hub_get_address(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("coap://10.0.0.1:9999", buf);
}

void test_set_address_null_rejected(void)
{
    TEST_ASSERT_EQUAL(TW_ERR_INVAL, tw_hub_set_address(NULL));
}

void test_uuid_to_str_formatting(void)
{
    uint8_t uuid[TW_UUID_SIZE] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
    };
    char buf[TW_UUID_STR_SIZE];
    tw_identity_uuid_to_str(uuid, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("01234567-89ab-cdef-fedc-ba9876543210", buf);
}

void test_uuid_to_str_zero(void)
{
    uint8_t uuid[TW_UUID_SIZE] = {0};
    char buf[TW_UUID_STR_SIZE];
    tw_identity_uuid_to_str(uuid, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("00000000-0000-0000-0000-000000000000", buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_set_address_parses_coap_uri);
    RUN_TEST(test_set_address_without_scheme);
    RUN_TEST(test_set_address_null_rejected);
    RUN_TEST(test_uuid_to_str_formatting);
    RUN_TEST(test_uuid_to_str_zero);
    return UNITY_END();
}
