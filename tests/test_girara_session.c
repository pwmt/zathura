/* SPDX-License-Identifier: Zlib */

#include <girara-gtk/session.h>

#include "tests.h"

static void test_create(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);
  girara_session_destroy(session);
}

static void test_init(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);
  g_assert_true(girara_session_init(session, NULL));
  girara_session_destroy(session);
}

int main(int argc, char* argv[]) {
  setup_logger();

  gtk_init(NULL, NULL);
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/session/crate", test_create);
  g_test_add_func("/session/init", test_init);
  return g_test_run();
}
