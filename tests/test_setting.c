/* SPDX-License-Identifier: Zlib */

#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>

#include "tests.h"

static void test_settings_basic(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);

  g_assert_true(girara_setting_add(session, "test", NULL, STRING, false, NULL, NULL, NULL));
  char* ptr = NULL;
  g_assert_true(girara_setting_get(session, "test", &ptr));
  g_assert_null(ptr);

  g_assert_true(girara_setting_set(session, "test", "value"));
  g_assert_true(girara_setting_get(session, "test", &ptr));
  g_assert_cmpstr(ptr, ==, "value");
  g_free(ptr);

  ptr = NULL;
  g_assert_false(girara_setting_get(session, "does-not-exist", &ptr));
  g_assert_null(ptr);

  g_assert_true(girara_setting_add(session, "test2", "value", STRING, false, NULL, NULL, NULL));
  g_assert_true(girara_setting_get(session, "test2", &ptr));
  g_assert_cmpstr(ptr, ==, "value");
  g_free(ptr);

  ptr = NULL;
  g_assert_false(girara_setting_add(session, "test3", NULL, INT, false, NULL, NULL, NULL));
  g_assert_false(girara_setting_get(session, "test3", &ptr));
  g_assert_null(ptr);

  float val = 0.0, rval = 0.0;
  g_assert_true(girara_setting_add(session, "test4", &val, FLOAT, false, NULL, NULL, NULL));
  g_assert_true(girara_setting_get(session, "test4", &rval));
  g_assert_cmpfloat(val, ==, rval);

  girara_session_destroy(session);
}

static int callback_called = 0;

static void setting_callback(girara_session_t* session, const char* name, girara_setting_type_t type, const void* value,
                             void* data) {
  g_assert_cmpuint(callback_called, ==, 0);
  g_assert_nonnull(session);
  g_assert_cmpstr(name, ==, "test");
  g_assert_cmpuint(type, ==, STRING);
  g_assert_cmpstr(value, ==, "value");
  g_assert_cmpstr(data, ==, "data");
  callback_called++;
}

static void test_settings_callback(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);

  g_assert_true(girara_setting_add(session, "test", "oldvalue", STRING, false, NULL, setting_callback, "data"));
  g_assert_true(girara_setting_set(session, "test", "value"));
  g_assert_cmpuint(callback_called, ==, 1);

  girara_session_destroy(session);
}

int main(int argc, char* argv[]) {
  setup_logger();

  gtk_init(NULL, NULL);
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/settings/basic", test_settings_basic);
  g_test_add_func("/settings/callback", test_settings_callback);
  return g_test_run();
}
