/* SPDX-License-Identifier: Zlib */

#include "utils.h"

static void test_file_valid_extension(void) {
  g_assert_false(file_valid_extension(NULL, NULL));
  g_assert_false(file_valid_extension((void*)0xDEAD, NULL));
  g_assert_false(file_valid_extension(NULL, "pdf"));
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/utils/file_valid_extension", test_file_valid_extension);
  return g_test_run();
}
