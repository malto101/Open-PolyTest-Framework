/**
 * Host C smoke tests — suite/group fixtures, tags, asserts, IGNORE, PROTECT,
 * parameterized cases.
 */
#include "polyontest/polyontest.h"

static int add(int a, int b) { return a + b; }

static int g_suite_ready;
static int g_group_value;

typedef struct {
    int a;
    int b;
    int sum;
} add_case_t;

static const add_case_t k_add_cases[] = {
    {2, 3, 5},
    {0, 0, 0},
    {-1, 1, 0},
};

POLYONTEST_SUITE_SETUP(Math) { g_suite_ready = 1; }
POLYONTEST_SUITE_TEARDOWN(Math) { g_suite_ready = 0; }
POLYONTEST_SUITE_TAGS(Math, "host", "smoke");

POLYONTEST_GROUP_SETUP(Math, Basic) { g_group_value = 40; }
POLYONTEST_GROUP_TEARDOWN(Math, Basic) { g_group_value = 0; }
POLYONTEST_GROUP_TAGS(Math, Basic, "unit");

TEST(Math, Basic, AddPositive) {
    ASSERT_EQ(5, add(2, 3));
#if POLYONTEST_CFG_HAS_FIXTURES
    ASSERT_TRUE(g_suite_ready);
#endif
}

TEST(Math, Basic, AddZero) {
    ASSERT_EQ(2, add(2, 0));
}

TEST(Math, Basic, UsesGroupSetup) {
#if POLYONTEST_CFG_HAS_FIXTURES
    ASSERT_EQ(42, g_group_value + 2);
#else
    ASSERT_EQ(2, add(1, 1));
#endif
}

TEST(Math, Basic, TypedAndBits) {
    ASSERT_EQUAL_HEX32(0xA5u, 0xA5u);
#if POLYONTEST_CFG_HAS_EXTENDED_ASSERTS
    ASSERT_BITS(0x0Fu, 0x05u, 0x15u);
    ASSERT_BITS_HIGH(0x01u, 0xF1u);
    ASSERT_BITS_LOW(0x02u, 0xF1u);
#endif
    ASSERT_GREATER_THAN(3, 10);
    ASSERT_INT_WITHIN(2, 10, 11);
#if POLYONTEST_CFG_HAS_EXTENDED_ASSERTS
    ASSERT_EQUAL_STRING("hi", "hi");
#endif
#ifndef POLYONTEST_EXCLUDE_FLOAT
    ASSERT_EQUAL_FLOAT(1.0f, 1.0f);
#endif
}

TEST_TAGS(Math, Basic, SkipMe, "skipdemo") {
    IGNORE_MESSAGE("demonstrating IGNORE");
    ASSERT_TRUE(0);
}

#ifdef POLYONTEST_TEST_CRASH
TEST(Math, Basic, CrashMe) {
    volatile int *p = NULL;
    (void)p;
    *p = 42;
}
#endif

#ifdef POLYONTEST_TEST_HANG
TEST(HangSuite, HangGroup, HangMe) {
    while (1) {
        // infinite loop for timeout testing
    }
}
#endif


TEST(Math, Basic, ProtectRegion) {
    int entered = 0;
    if (TEST_PROTECT()) {
        entered = 1;
        /* TEST_ABORT() would longjmp here and mark the case failed. */
    }
    ASSERT_TRUE(entered);
}

#if POLYONTEST_CFG_HAS_FIXTURES
PARAM_TEST(Math, Basic, AddTable, add_case_t, k_add_cases) {
    const add_case_t row = PARAM_AS(add_case_t);
    ASSERT_EQ(row.sum, add(row.a, row.b));
}
#endif

TEST(Expect, Pointers, NotNull) {
    int x = 1;
    ASSERT_NOT_NULL(&x);
}

int main(void) { return polyontest_run_from_env(); }
