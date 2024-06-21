/* SPDX-License-Identifier: Zlib */

#include "zathura.h"

#include "tests.h"

static void test_create(void) {
  setup_logger();

  zathura_t* zathura = zathura_create();
  g_assert_nonnull(zathura);
  g_assert_nonnull(g_getenv("G_TEST_SRCDIR"));
  zathura_set_config_dir(zathura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(zathura));
  zathura_free(zathura);
}

int main(int argc, char* argv[]) {
  setup_logger();

  gtk_init(NULL, NULL);
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/session/create", test_create);
  return g_test_run();
}
