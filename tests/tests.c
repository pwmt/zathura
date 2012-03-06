/* See LICENSE file for license and copyright information */

#include <check.h>
#include <stdlib.h>

Suite* suite_utils();

int main()
{
  Suite* suite          = NULL;
  SRunner* suite_runner = NULL;
  int number_failed = 0;

  /* test utils */
  suite        = suite_utils();
  suite_runner = srunner_create(suite);
  srunner_run_all(suite_runner, CK_NORMAL);
  number_failed += srunner_ntests_failed(suite_runner);
  srunner_free(suite_runner);

  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
