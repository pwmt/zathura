/* See LICENSE file for license and copyright information */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "completion.h"
#include "utils.h"

girara_completion_t*
cc_print(girara_session_t* session, char* input)
{
  girara_completion_t* completion  = girara_completion_init();
  girara_completion_group_t* group = girara_completion_group_create(session, NULL);

  if (!session || !input || !completion || !group)
    return NULL;

  girara_completion_add_group(completion, group);

  char* const list_printers[] = {
    "lpstat", "-v", NULL
  };

  char* output;
  if (!execute_command(list_printers, &output)) {
    girara_completion_free(completion);
    return false;
  }

  /* parse single lines */
  unsigned int prefix_length = strlen("device for ");
  char* p = strtok(output, "\n");
  char* q;

  while (p) {
    if (!(p = strstr(p, "device for ")) || !(q = strchr(p, ':'))) {
      p = strtok(NULL, "\n");
      continue;
    }

    unsigned int printer_length = q - p - prefix_length;
    char* printer_name = malloc(sizeof(char) * printer_length);

    if (!printer_name) {
      p = strtok(NULL, "\n");
      continue;
    }

    strncpy(printer_name, p + prefix_length, printer_length - 1);
    printer_name[printer_length - 1] = '\0';

    /* add printer */
    girara_completion_group_add_element(group, printer_name, NULL);

    free(printer_name);

    /* next line */
    p = strtok(NULL, "\n");
  }

  free(output);

  return completion;
}

girara_completion_t*
cc_open(girara_session_t* session, char* input)
{
  g_return_val_if_fail(session != NULL, NULL);
  g_return_val_if_fail(session->global.data != NULL, NULL);
  zathura_t* zathura = session->global.data;

  girara_completion_t* completion  = girara_completion_init();
  girara_completion_group_t* group = girara_completion_group_create(session, NULL);

  if (!input || !completion || !group) {
    goto error_free;
  }

  gchar* path = girara_fix_path(input);
  if (path == NULL || strlen(path) == 0) {
    goto error_free;
  }

  /* If the path does not begin with a slash we update the path with the current
   * working directory */
  if (path[0] != '/') {
    size_t path_max;
#ifdef PATH_MAX
    path_max = PATH_MAX;
#else
    path_max = pathconf(path,_PC_PATH_MAX);
    if (path_max <= 0)
      path_max = 4096;
#endif

    char cwd[path_max];
    getcwd(cwd, path_max);

    char* tmp_path = g_strdup_printf("%s/%s", cwd, path);
    if (tmp_path == NULL) {
      goto error_free;
    }

    g_free(path);
    path = tmp_path;
  }

  /* read directory */
  if (g_file_test(path, G_FILE_TEST_IS_DIR) == TRUE) {
    GDir* dir = g_dir_open(path, 0, NULL);
    if (dir == NULL) {
      goto error_free;
    }

    /* read files */
    char* name = NULL;
    while ((name = (char*) g_dir_read_name(dir)) != NULL) {
      char* e_name    = g_filename_display_name(name);
      if (e_name == NULL) {
        goto error_free;
      }

      char* full_path = g_strdup_printf("%s/%s", path, e_name);
      if (full_path == NULL) {
        goto error_free;
      }

      if (file_valid_extension(zathura, full_path) == true) {
        girara_completion_group_add_element(group, full_path, NULL);
      }

      g_free(full_path);
      g_free(e_name);
    }

    g_dir_close(dir);
  /* given path is a file */
  } else if (g_file_test(path, G_FILE_TEST_IS_REGULAR) == TRUE) {
    if (file_valid_extension(zathura, path) == true) {
      girara_completion_group_add_element(group, path, NULL);
    }
  }

  g_free(path);

  girara_completion_add_group(completion, group);

  return completion;

error_free:

  if (completion) {
    girara_completion_free(completion);
  }
  if (group) {
    girara_completion_group_free(group);
  }

  g_free(path);

  return NULL;
}
