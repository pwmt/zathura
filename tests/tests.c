/* See LICENSE file for license and copyright information */

#include <check.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))

extern Suite* suite_session();
extern Suite* suite_utils();
extern Suite* suite_document();

typedef Suite* (*suite_create_fnt_t)(void);

const suite_create_fnt_t suites[] = {
  suite_utils,
  suite_document,
  suite_session,
};

int
main(int argc, char* argv[])
{
  Suite* suite          = NULL;
  SRunner* suite_runner = NULL;
  int number_failed = 0;

  /* init gtk */
  gtk_init(&argc, &argv);

  /* run test suites */
  for (unsigned int i = 0; i < LENGTH(suites); i++) {
    suite = suites[i]();
    suite_runner = srunner_create(suite);
    srunner_run_all(suite_runner, CK_NORMAL);
    number_failed += srunner_ntests_failed(suite_runner);
    srunner_free(suite_runner);
  }

  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
