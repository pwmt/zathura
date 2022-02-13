/* SPDX-License-Identifier: Zlib */

#include <check.h>

#include "zathura.h"
#include "tests.h"

START_TEST(test_create) {
  zathura_t* zathura = zathura_create();
  ck_assert_ptr_nonnull(zathura);
  ck_assert(zathura_init(zathura));
  zathura_free(zathura);
} END_TEST

static Suite* suite_session(void)
{
  TCase* tcase = NULL;
  Suite* suite = suite_create("Session");

  /* basic */
  tcase = tcase_create("basic");
  tcase_add_checked_fixture(tcase, setup, NULL);
  tcase_add_test(tcase, test_create);
  suite_add_tcase(suite, tcase);

  return suite;
}

int main()
{
  return run_suite(suite_session());
}
