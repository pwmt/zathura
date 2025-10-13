/* SPDX-License-Identifier: Zlib */

#include "zathura.h"
#include "document.h"
#ifdef WITH_SECCOMP
#include "seccomp-filters.h"
#endif
#ifdef WITH_LANDLOCK
#include "landlock.h"
#endif

#include "tests.h"

static void test_create(void) {
  setup_logger();

#ifdef GDK_WINDOWING_X11
  GdkDisplay* display = gdk_display_get_default();

  if (GDK_IS_X11_DISPLAY(display)) {
    g_test_skip("not running under X11");
    return;
  }
#endif

  zathura_t* zathura = zathura_create();
  g_assert_nonnull(zathura);
  g_assert_nonnull(g_getenv("G_TEST_SRCDIR"));
  zathura_set_config_dir(zathura, g_getenv("G_TEST_SRCDIR"));
  g_assert_true(zathura_init(zathura));

#ifdef WITH_LANDLOCK
  landlock_drop_write();
#endif
#ifdef WITH_SECCOMP
  g_assert_cmpint(seccomp_enable_strict_filter(zathura), ==, 0);
#endif

  g_assert_null(zathura_document_open(zathura, NULL, NULL, NULL, NULL));
  g_assert_null(zathura_document_open(zathura, "fl", NULL, NULL, NULL));
  g_assert_null(zathura_document_open(zathura, "fl", "ur", NULL, NULL));
  g_assert_null(zathura_document_open(zathura, "fl", NULL, "pw", NULL));

  zathura_free(zathura);
}

int main(int argc, char* argv[]) {
  setup_logger();

  gtk_init(NULL, NULL);
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/sandbox/session_create", test_create);
  return g_test_run();
}
