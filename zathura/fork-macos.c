#ifdef __APPLE__
#include <glib.h>
#include <unistd.h>

#include "fork-macos.h"

char** build_reexec_argv(char** orig_argv, int orig_argc, char** post_parse_files, int n_files, const char* specific_file) {

  GPtrArray* arr = g_ptr_array_new();

  g_ptr_array_add(arr, orig_argv[0]);

  for (int i = 1; i < orig_argc; i++) {
    if (g_strcmp0(orig_argv[i], "--fork") == 0) {
      continue;
    }

    bool is_file = false;
    for (int j = 0; j < n_files; j++) {
      if (g_strcmp0(orig_argv[i], post_parse_files[j]) == 0) {
        is_file = true;
        break;
      }
    }

    if (is_file) {
      continue;
    }

    g_ptr_array_add(arr, orig_argv[i]);
  }

  if (specific_file != NULL) {
    g_ptr_array_add(arr, (char*)specific_file);
  }

  g_ptr_array_add(arr, NULL);

  return (char**)g_ptr_array_free(arr, FALSE);
}
#endif
