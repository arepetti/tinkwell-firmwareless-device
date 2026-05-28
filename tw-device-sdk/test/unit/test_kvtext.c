/*
 * test_kvtext.c -- Unit tests for the kvtext parser and writer.
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "tw_kvtext.h"
#include <string.h>
#include <stdio.h>

/* ---- Parser tests ---- */

typedef struct {
    char keys[8][64];
    char vals[8][128];
    int  count;
} parsed_t;

static void on_kv(void *ctx, const char *key, const char *value)
{
    parsed_t *p = (parsed_t *)ctx;
    if (p->count < 8) {
        snprintf(p->keys[p->count], sizeof(p->keys[0]), "%s", key);
        snprintf(p->vals[p->count], sizeof(p->vals[0]), "%s", value);
        p->count++;
    }
}

void setUp(void) {}
void tearDown(void) {}

void test_parse_basic(void)
{
    const char *input = "vendor-id=42\nproduct-id=100\n";
    parsed_t p = {0};
    tw_kvtext_parse(input, strlen(input), on_kv, &p);

    TEST_ASSERT_EQUAL(2, p.count);
    TEST_ASSERT_EQUAL_STRING("vendor-id", p.keys[0]);
    TEST_ASSERT_EQUAL_STRING("42", p.vals[0]);
    TEST_ASSERT_EQUAL_STRING("product-id", p.keys[1]);
    TEST_ASSERT_EQUAL_STRING("100", p.vals[1]);
}

void test_parse_comments(void)
{
    const char *input =
        "# comment\n"
        "; another comment\n"
        "// C-style comment\n"
        "key=value\n";
    parsed_t p = {0};
    tw_kvtext_parse(input, strlen(input), on_kv, &p);

    TEST_ASSERT_EQUAL(1, p.count);
    TEST_ASSERT_EQUAL_STRING("key", p.keys[0]);
    TEST_ASSERT_EQUAL_STRING("value", p.vals[0]);
}

void test_parse_blank_lines(void)
{
    const char *input = "\n\nkey=val\n\n";
    parsed_t p = {0};
    tw_kvtext_parse(input, strlen(input), on_kv, &p);

    TEST_ASSERT_EQUAL(1, p.count);
    TEST_ASSERT_EQUAL_STRING("key", p.keys[0]);
}

void test_parse_ini_sections_ignored(void)
{
    const char *input = "[section]\nkey=val\n[other]\n";
    parsed_t p = {0};
    tw_kvtext_parse(input, strlen(input), on_kv, &p);

    TEST_ASSERT_EQUAL(1, p.count);
    TEST_ASSERT_EQUAL_STRING("key", p.keys[0]);
}

void test_parse_whitespace_trim(void)
{
    const char *input = "  key  =  value  \n";
    parsed_t p = {0};
    tw_kvtext_parse(input, strlen(input), on_kv, &p);

    TEST_ASSERT_EQUAL(1, p.count);
    TEST_ASSERT_EQUAL_STRING("key", p.keys[0]);
    TEST_ASSERT_EQUAL_STRING("value", p.vals[0]);
}

void test_parse_value_with_equals(void)
{
    const char *input = "hub-url=coap://192.168.1.1:5684/hub\n";
    parsed_t p = {0};
    tw_kvtext_parse(input, strlen(input), on_kv, &p);

    TEST_ASSERT_EQUAL(1, p.count);
    TEST_ASSERT_EQUAL_STRING("hub-url", p.keys[0]);
    TEST_ASSERT_EQUAL_STRING("coap://192.168.1.1:5684/hub", p.vals[0]);
}

void test_parse_no_trailing_newline(void)
{
    const char *input = "key=val";
    parsed_t p = {0};
    tw_kvtext_parse(input, strlen(input), on_kv, &p);

    TEST_ASSERT_EQUAL(1, p.count);
    TEST_ASSERT_EQUAL_STRING("val", p.vals[0]);
}

void test_parse_cr_lf(void)
{
    const char *input = "key=val\r\n";
    parsed_t p = {0};
    tw_kvtext_parse(input, strlen(input), on_kv, &p);

    TEST_ASSERT_EQUAL(1, p.count);
    TEST_ASSERT_EQUAL_STRING("val", p.vals[0]);
}

/* ---- Writer tests ---- */

void test_write_str(void)
{
    char buf[256];
    size_t pos = 0;
    TEST_ASSERT_EQUAL(TW_OK, tw_kvtext_write_str(buf, sizeof(buf), &pos,
                                                   "device-name", "thermostat"));
    buf[pos] = '\0';
    TEST_ASSERT_EQUAL_STRING("device-name=thermostat\n", buf);
}

void test_write_i32(void)
{
    char buf[256];
    size_t pos = 0;
    TEST_ASSERT_EQUAL(TW_OK, tw_kvtext_write_i32(buf, sizeof(buf), &pos,
                                                   "vendor-id", -42));
    buf[pos] = '\0';
    TEST_ASSERT_EQUAL_STRING("vendor-id=-42\n", buf);
}

void test_write_hex(void)
{
    char buf[256];
    size_t pos = 0;
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_EQUAL(TW_OK, tw_kvtext_write_hex(buf, sizeof(buf), &pos,
                                                   "uuid", data, sizeof(data)));
    buf[pos] = '\0';
    TEST_ASSERT_EQUAL_STRING("uuid=deadbeef\n", buf);
}

void test_write_overflow(void)
{
    char buf[10];
    size_t pos = 0;
    TEST_ASSERT_EQUAL(TW_ERR_OVERFLOW,
        tw_kvtext_write_str(buf, sizeof(buf), &pos,
                            "very-long-key", "very-long-value"));
}

/* ---- Round-trip test ---- */

void test_round_trip(void)
{
    char buf[512];
    size_t pos = 0;

    tw_kvtext_write_str(buf, sizeof(buf), &pos, "device-name", "thermostat");
    tw_kvtext_write_i32(buf, sizeof(buf), &pos, "vendor-id", 42);
    tw_kvtext_write_i32(buf, sizeof(buf), &pos, "product-id", 100);

    uint8_t uuid[4] = {0x01, 0x02, 0x03, 0x04};
    tw_kvtext_write_hex(buf, sizeof(buf), &pos, "uuid", uuid, sizeof(uuid));

    parsed_t p = {0};
    tw_kvtext_parse(buf, pos, on_kv, &p);

    TEST_ASSERT_EQUAL(4, p.count);
    TEST_ASSERT_EQUAL_STRING("device-name", p.keys[0]);
    TEST_ASSERT_EQUAL_STRING("thermostat", p.vals[0]);
    TEST_ASSERT_EQUAL_STRING("vendor-id", p.keys[1]);
    TEST_ASSERT_EQUAL_STRING("42", p.vals[1]);
    TEST_ASSERT_EQUAL_STRING("product-id", p.keys[2]);
    TEST_ASSERT_EQUAL_STRING("100", p.vals[2]);
    TEST_ASSERT_EQUAL_STRING("uuid", p.keys[3]);
    TEST_ASSERT_EQUAL_STRING("01020304", p.vals[3]);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_parse_basic);
    RUN_TEST(test_parse_comments);
    RUN_TEST(test_parse_blank_lines);
    RUN_TEST(test_parse_ini_sections_ignored);
    RUN_TEST(test_parse_whitespace_trim);
    RUN_TEST(test_parse_value_with_equals);
    RUN_TEST(test_parse_no_trailing_newline);
    RUN_TEST(test_parse_cr_lf);

    RUN_TEST(test_write_str);
    RUN_TEST(test_write_i32);
    RUN_TEST(test_write_hex);
    RUN_TEST(test_write_overflow);

    RUN_TEST(test_round_trip);

    return UNITY_END();
}
