/* See LICENSE file for license and copyright information */

#include <check.h>

#include "../utils.h"

START_TEST(test_file_valid_extension_null) {
  fail_unless(file_valid_extension(NULL, NULL) == false, NULL);
  fail_unless(file_valid_extension((void*) 0xDEAD, NULL) == false, NULL);
  fail_unless(file_valid_extension(NULL, "pdf") == false, NULL);
} END_TEST

START_TEST(test_strings_replace_substrings_invalid) {
  fail_unless(replace_substring(NULL, NULL, NULL) == NULL);
  fail_unless(replace_substring("", NULL, NULL) == NULL);
  fail_unless(replace_substring("", "", NULL) == NULL);
} END_TEST

START_TEST(test_strings_replace_substrings_nothing_to_replace) {
  fail_unless(replace_substring("test", "n", "y") == NULL);
} END_TEST

START_TEST(test_strings_replace_substrings_1) {
  char* result = replace_substring("test", "e", "f");
  fail_unless(result != NULL);
  fail_unless(strncmp(result, "tfst", 5) == 0);
  g_free(result);
} END_TEST

START_TEST(test_strings_replace_substrings_2) {
  char* result = replace_substring("test", "es", "f");
  fail_unless(result != NULL);
  fail_unless(strncmp(result, "tft", 4) == 0);
  g_free(result);
} END_TEST

START_TEST(test_strings_replace_substrings_3) {
  char* result = replace_substring("test", "e", "fg");
  fail_unless(result != NULL);
  fail_unless(strncmp(result, "tfgst", 6) == 0);
  g_free(result);
} END_TEST


Suite* suite_utils()
{
  TCase* tcase = NULL;
  Suite* suite = suite_create("Utils");

  /* file valid extension */
  tcase = tcase_create("file_valid_extension");
  tcase_add_test(tcase, test_file_valid_extension_null);
  suite_add_tcase(suite, tcase);

  /* strings */
  tcase = tcase_create("strings");
  tcase_add_test(tcase, test_strings_replace_substrings_invalid);
  tcase_add_test(tcase, test_strings_replace_substrings_nothing_to_replace);
  tcase_add_test(tcase, test_strings_replace_substrings_1);
  tcase_add_test(tcase, test_strings_replace_substrings_2);
  tcase_add_test(tcase, test_strings_replace_substrings_3);
  suite_add_tcase(suite, tcase);

  return suite;
}
