/* See LICENSE file for license and copyright information */

#include <check.h>

#include "../utils.h"

START_TEST(test_file_exists_null) {
  fail_unless(file_exists(NULL) == false);
} END_TEST

START_TEST(test_file_get_extension_null) {
  fail_unless(file_get_extension(NULL) == false);
} END_TEST

Suite* suite_utils()
{
  TCase* tcase = NULL;
  Suite* suite = suite_create("Utils");

  /* file exists */
  tcase = tcase_create("file_exists");
  tcase_add_test(tcase, test_file_exists_null);
  suite_add_tcase(suite, tcase);

  /* file exists */
  tcase = tcase_create("file_get_extension");
  tcase_add_test(tcase, test_file_get_extension_null);
  suite_add_tcase(suite, tcase);

  return suite;
}
