/* SPDX-License-Identifier: Zlib */

#include "document.h"

static void test_open(void) {
  g_assert_null(zathura_document_open(NULL, NULL, NULL, NULL, NULL));
  g_assert_null(zathura_document_open(NULL, "fl", NULL, NULL, NULL));
  g_assert_null(zathura_document_open(NULL, "fl", "ur", NULL, NULL));
  g_assert_null(zathura_document_open(NULL, "fl", NULL, "pw", NULL));
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/document/open", test_open);
  return g_test_run();
}
