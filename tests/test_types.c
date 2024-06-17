/* SPDX-License-Identifier: Zlib */

#include <check.h>
#include <limits.h>

#include "types.h"

static void test_image_buffer_fail(void) {
  g_assert_null(zathura_image_buffer_create(UINT_MAX, UINT_MAX));
}

static void test_image_buffer(void) {
  zathura_image_buffer_t* buffer = zathura_image_buffer_create(1, 1);
  g_assert_nonnull(buffer);
  zathura_image_buffer_free(buffer);
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/types/image_buffer_fail", test_image_buffer_fail);
  g_test_add_func("/types/image_buffer", test_image_buffer);
  return g_test_run();
}
