/* See LICENSE file for license and copyright information */

#include <check.h>
#include <limits.h>

#include "types.h"
#include "tests.h"

START_TEST(test_image_buffer_fail) {
  fail_unless(zathura_image_buffer_create(UINT_MAX, UINT_MAX) == NULL, NULL);
} END_TEST

START_TEST(test_image_buffer) {
  zathura_image_buffer_t* buffer = zathura_image_buffer_create(1, 1);
  fail_unless(buffer != NULL, NULL);
  zathura_image_buffer_free(buffer);
} END_TEST

static Suite* suite_types(void)
{
  TCase* tcase = NULL;
  Suite* suite = suite_create("Types");

  /* file valid extension */
  tcase = tcase_create("Image buffer");
  tcase_add_test(tcase, test_image_buffer_fail);
  tcase_add_test(tcase, test_image_buffer);
  suite_add_tcase(suite, tcase);

  return suite;
}

int main()
{
  return run_suite(suite_types());
}
