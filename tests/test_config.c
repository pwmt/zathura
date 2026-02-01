/* SPDX-License-Identifier: Zlib */

#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>
#include <girara-gtk/config.h>

#include "tests.h"

static void test_config_parse(void) {
  setup_logger();

  girara_session_t* session = girara_session_create();
  g_assert_nonnull(session);

  int default_val = 1;
  g_assert_true(girara_setting_add(session, "test1", "default-string", STRING, false, NULL, NULL, NULL));
  g_assert_true(girara_setting_add(session, "test2", &default_val, INT, false, NULL, NULL, NULL));

  char* filename = NULL;
  int fd         = g_file_open_tmp(NULL, &filename, NULL);
  g_assert_cmpint(fd, !=, -1);
  g_assert_nonnull(filename);
  if (g_file_set_contents(filename,
                          "set test1 config-string\n"
                          "set test2 2\n",
                          -1, NULL) == FALSE) {
    g_assert_not_reached();
  }
  girara_config_parse(session, filename);

  char* ptr = NULL;
  g_assert_true(girara_setting_get(session, "test1", &ptr));
  g_assert_cmpstr(ptr, ==, "config-string");
  g_free(ptr);

  int real_val = 0;
  g_assert_true(girara_setting_get(session, "test2", &real_val));
  g_assert_cmpint(real_val, ==, 2);

  close(fd);
  g_remove(filename);
  g_free(filename);
  girara_session_destroy(session);
}

int main(int argc, char* argv[]) {
  setup_logger();

  gtk_init(NULL, NULL);
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/config/parse", test_config_parse);
  return g_test_run();
}
