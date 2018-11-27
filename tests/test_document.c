/* SPDX-License-Identifier: Zlib */

#include <check.h>

#include "document.h"
#include "tests.h"

START_TEST(test_open) {
  fail_unless(zathura_document_open(NULL, NULL, NULL, NULL, NULL) == NULL, "Could create document", NULL);
  fail_unless(zathura_document_open(NULL, "fl", NULL, NULL, NULL) == NULL, "Could create document", NULL);
  fail_unless(zathura_document_open(NULL, "fl", "ur", NULL, NULL) == NULL, "Could create document", NULL);
  fail_unless(zathura_document_open(NULL, "fl", NULL, "pw", NULL) == NULL, "Could create document", NULL);
} END_TEST

static Suite* suite_document(void)
{
  TCase* tcase = NULL;
  Suite* suite = suite_create("Document");

  /* basic */
  tcase = tcase_create("basic");
  tcase_add_test(tcase, test_open);
  suite_add_tcase(suite, tcase);

  return suite;
}

int main()
{
  return run_suite(suite_document());
}
