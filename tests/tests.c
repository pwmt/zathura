/* See LICENSE file for license and copyright information */

#include <check.h>
#include <girara/macros.h>

#define UNUSED(x) GIRARA_UNUSED(x)

Suite* suite_utils();

int main(int UNUSED(argc), char *UNUSED(argv[]))
{
  Suite* suite          = NULL;
  SRunner* suite_runner = NULL;

  /* test utils */
  suite        = suite_utils();
  suite_runner = srunner_create(suite);
  srunner_run_all(suite_runner, CK_NORMAL);
  srunner_free(suite_runner);

  return 0;
}
