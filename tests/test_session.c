/* SPDX-License-Identifier: Zlib */

#include "zathura.h"

static void fixture_setup(void** GIRARA_UNUSED(fixture), gconstpointer GIRARA_UNUSED(user_data)) {
  g_assert_true(gtk_init_check(NULL, NULL));
}

static void test_create(void** GIRARA_UNUSED(fixture), gconstpointer GIRARA_UNUSED(user_data)) {
  zathura_t* zathura = zathura_create();
  g_assert_nonnull(zathura);
  g_assert_nonnull(g_getenv("G_TEST_SRCDIR"));
  zathura_set_config_dir(zathura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(zathura));
  zathura_free(zathura);
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add("/session/create", void*, NULL, fixture_setup, test_create, NULL);
  return g_test_run();
}
