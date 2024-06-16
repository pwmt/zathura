/* SPDX-License-Identifier: Zlib */

#include <check.h>

#include "tests.h"
#include "zathura.h"
#ifdef WITH_SECCOMP
#include "seccomp-filters.h"
#endif
#ifdef WITH_LANDLOCK
#include "landlock.h"
#endif

START_TEST(test_create) {
  zathura_t* zathura = zathura_create();
  ck_assert_ptr_nonnull(zathura);

#ifdef WITH_LANDLOCK
  landlock_drop_write();
#endif
#ifdef WITH_SECCOMP
  ck_assert_int_eq(seccomp_enable_strict_filter(zathura), 0);
#endif

  ck_assert(zathura_init(zathura));
  zathura_free(zathura);
}
END_TEST

static void sandbox_setup(void) {
  setup();

#ifdef GDK_WINDOWING_X11
  GdkDisplay* display = gdk_display_get_default();

  ck_assert_msg(!GDK_IS_X11_DISPLAY(display), "Running under X11.");
#endif
}

static Suite* suite_sandbox(void) {
  TCase* tcase = NULL;
  Suite* suite = suite_create("Sandbox");

  /* basic */
  tcase = tcase_create("basic");
  tcase_add_checked_fixture(tcase, sandbox_setup, NULL);
  tcase_add_test(tcase, test_create);
  suite_add_tcase(suite, tcase);

  return suite;
}

int main() {
  return run_suite(suite_sandbox());
}
