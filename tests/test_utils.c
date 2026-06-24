/* SPDX-License-Identifier: Zlib */

#include "utils.h"

static void test_file_valid_extension(void) {
  g_assert_false(file_valid_extension(NULL, NULL));
  g_assert_false(file_valid_extension((void*)0xDEAD, NULL));
  g_assert_false(file_valid_extension(NULL, "pdf"));
}

static void test_first_page_column(void) {
  g_assert_true(find_first_page_column("1:2", 1) == 1);
  g_assert_true(find_first_page_column("1:2", 2) == 2);
  g_assert_true(find_first_page_column("1:2", 3) == 2);
  g_assert_true(find_first_page_column("2", 1) == 1);
  g_assert_true(find_first_page_column("2", 2) == 2);
  g_assert_true(find_first_page_column("3:1:2", 1) == 1);
  g_assert_true(find_first_page_column("3:1:2", 2) == 1);
  g_assert_true(find_first_page_column("3:1:2", 7) == 2);
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/utils/file_valid_extension", test_file_valid_extension);
  g_test_add_func("/utils/first_page_column", test_first_page_column);
  return g_test_run();
}
