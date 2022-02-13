/* SPDX-License-Identifier: Zlib */

#include <gtk/gtk.h>
#include <stdlib.h>

#include "tests.h"

int run_suite(Suite* suite)
{
  SRunner* suite_runner = srunner_create(suite);
  srunner_run_all(suite_runner, CK_NORMAL);
  const int number_failed = srunner_ntests_failed(suite_runner);

  int ret = EXIT_SUCCESS;
  if (number_failed != 0) {
    ret = EXIT_FAILURE;
    TestResult** results = srunner_failures(suite_runner);

    for (int i = 0; i < number_failed; ++i) {
      if (tr_ctx(results[i]) == CK_CTX_SETUP) {
        /* mark tests as skipped */
        ret = 77;
        break;
      }
    }
  }

  srunner_free(suite_runner);

  return ret;
}

void setup(void)
{
  ck_assert_msg(gtk_init_check(NULL, NULL) == TRUE, "GTK+ initializitation failed");
}
