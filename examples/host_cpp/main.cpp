#include "polyontest.hpp"

#include <cstdio>

typedef struct {
  int a;
  int b;
  int sum;
} AddRow;

static const AddRow k_rows[] = {{1, 1, 2}, {2, 2, 4}};

TEST(CppSugar, Smoke, Equals) { ASSERT_EQ(4, 2 + 2); }

TEST_TAGS(CppSugar, Smoke, Tagged, "cpp", "unit") { ASSERT_EQ(3, 1 + 2); }

#if POLYONTEST_CFG_HAS_FIXTURES
PARAM_TEST(CppSugar, Smoke, AddTable, AddRow, k_rows) {
  const AddRow row = PARAM_AS(AddRow);
  ASSERT_EQ(row.sum, row.a + row.b);
}
#endif

int main() {
  std::printf("polyontest C++ adapter smoke\n");
  return polyontest::run_from_env();
}
