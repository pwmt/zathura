/* SPDX-License-Identifier: Zlib */

#include "zathura.h"
#ifdef WITH_SECCOMP
#include "seccomp-filters.h"
#endif
#ifdef WITH_LANDLOCK
#include "landlock.h"
#endif

static void test_create(void** GIRARA_UNUSED(fixture), gconstpointer GIRARA_UNUSED(user_data)) {
#ifdef GDK_WINDOWING_X11
  GdkDisplay* display = gdk_display_get_default();

  if (GDK_IS_X11_DISPLAY(display)) {
    g_test_skip("not running under X11");
  }
#endif

  zathura_t* zathura = zathura_create();
  g_assert_nonnull(zathura);
  g_assert_nonnull(g_getenv("G_TEST_SRCDIR"));
  zathura_set_config_dir(zathura, g_getenv("G_TEST_SRCDIR"));

#ifdef WITH_LANDLOCK
  landlock_drop_write();
#endif
#ifdef WITH_SECCOMP
  g_assert_cmpint(seccomp_enable_strict_filter(zathura), ==, 0);
#endif

  g_assert_true(zathura_init(zathura));
  zathura_free(zathura);
}

static void fixture_setup(void** GIRARA_UNUSED(fixture), gconstpointer GIRARA_UNUSED(user_data)) {
  g_assert_true(gtk_init_check(NULL, NULL));
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add("/sandbox/session_create", void*, NULL, fixture_setup, test_create, NULL);
  return g_test_run();
}