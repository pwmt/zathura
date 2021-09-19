/* SPDX-License-Identifier: Zlib */

#include <check.h>

#include "utils.h"
#include "tests.h"

START_TEST(test_file_valid_extension_null) {
  ck_assert(file_valid_extension(NULL, NULL) == false);
  ck_assert(file_valid_extension((void*) 0xDEAD, NULL) == false);
  ck_assert(file_valid_extension(NULL, "pdf") == false);
} END_TEST

static Suite* suite_utils(void)
{
  TCase* tcase = NULL;
  Suite* suite = suite_create("Utils");

  /* file valid extension */
  tcase = tcase_create("file_valid_extension");
  tcase_add_test(tcase, test_file_valid_extension_null);
  suite_add_tcase(suite, tcase);

  return suite;
}

int main()
{
  return run_suite(suite_utils());
}
