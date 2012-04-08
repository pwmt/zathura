/* See LICENSE file for license and copyright information */

#include <check.h>

#include "../document.h"

START_TEST(test_open) {
  fail_unless(zathura_document_open(NULL, NULL, NULL, NULL) == NULL, "Could create document", NULL);
  fail_unless(zathura_document_open(NULL, "fl", NULL, NULL) == NULL, "Could create document", NULL);
  fail_unless(zathura_document_open(NULL, "fl", "pw", NULL) == NULL, "Could create document", NULL);
} END_TEST

Suite* suite_document()
{
  TCase* tcase = NULL;
  Suite* suite = suite_create("Document");

  /* basic */
  tcase = tcase_create("basic");
  tcase_add_test(tcase, test_open);
  suite_add_tcase(suite, tcase);

  return suite;
}
