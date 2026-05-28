/*
 * test_coap_dispatch.c -- Unit tests for the message resource dispatcher.
 *
 * Uses Unity test framework.  Link against the mock PAL backend.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tw_msg.h"
#include "tw_types.h"
#include "mock_pal.h"

#include <string.h>

/* --- Test resource handlers --- */

static int s_get_called;
static int s_put_called;

static tw_err_t on_test_get(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    (void)req;
    s_get_called++;
    tw_msg_respond_with_code(resp, TW_MSG_205_CONTENT, "hello");
    return TW_OK;
}

static tw_err_t on_test_put(tw_msg_request_t *req, tw_msg_response_t *resp)
{
    (void)req;
    s_put_called++;
    tw_msg_respond_with_code(resp, TW_MSG_204_CHANGED, NULL);
    return TW_OK;
}

static tw_msg_resource_t test_resources[] = {
    { "/test/value",  TW_MSG_GET,                on_test_get },
    { "/test/config", TW_MSG_GET | TW_MSG_PUT,   on_test_put },
    TW_MSG_RESOURCE_END
};

/* --- Tests --- */

void setUp(void)
{
    mock_reset();
    mock_config_reset();
    s_get_called = 0;
    s_put_called = 0;
}

void tearDown(void) {}

void test_resource_table_terminates(void)
{
    int count = 0;
    tw_msg_resource_t *r = test_resources;
    while (r->path) { count++; r++; }
    TEST_ASSERT_EQUAL(2, count);
}

void test_get_handler_invoked(void)
{
    tw_msg_request_t req = { .method = TW_MSG_GET, .path = "/test/value" };
    tw_msg_response_t resp = {0};

    tw_msg_resource_t *r = &test_resources[0];
    TEST_ASSERT_EQUAL_STRING("/test/value", r->path);

    tw_err_t err = r->handler(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(1, s_get_called);
}

void test_put_handler_invoked(void)
{
    tw_msg_request_t req = { .method = TW_MSG_PUT, .path = "/test/config" };
    tw_msg_response_t resp = {0};

    tw_msg_resource_t *r = &test_resources[1];
    tw_err_t err = r->handler(&req, &resp);
    TEST_ASSERT_EQUAL(TW_OK, err);
    TEST_ASSERT_EQUAL(1, s_put_called);
}

void test_method_bitmask(void)
{
    tw_msg_resource_t *r = &test_resources[1];
    TEST_ASSERT_TRUE(r->methods & TW_MSG_GET);
    TEST_ASSERT_TRUE(r->methods & TW_MSG_PUT);
    TEST_ASSERT_FALSE(r->methods & TW_MSG_POST);
    TEST_ASSERT_FALSE(r->methods & TW_MSG_DELETE);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_resource_table_terminates);
    RUN_TEST(test_get_handler_invoked);
    RUN_TEST(test_put_handler_invoked);
    RUN_TEST(test_method_bitmask);
    return UNITY_END();
}
