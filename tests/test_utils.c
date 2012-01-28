/* See LICENSE file for license and copyright information */

#include <check.h>

#include "../utils.h"

START_TEST(test_file_exists_null) {
  fail_unless(file_exists(NULL) == false);
} END_TEST

START_TEST(test_file_get_extension_null) {
  fail_unless(file_get_extension(NULL) == NULL);
} END_TEST

START_TEST(test_file_get_extension_none) {
  const char* path = "test";
  fail_unless(file_get_extension(path) == NULL);
} END_TEST

START_TEST(test_file_get_extension_single) {
  const char* path = "test.pdf";
  const char* extension = file_get_extension(path);
  fail_unless(strcmp(extension, "pdf") == 0);
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
  tcase_add_test(tcase, test_file_get_extension_none);
  tcase_add_test(tcase, test_file_get_extension_single);
  suite_add_tcase(suite, tcase);

  return suite;
}
