/* SPDX-License-Identifier: Zlib */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/template.h>

#include "commands.h"
#include "internal.h"
#include "session.h"
#include "settings.h"
#include "shortcuts.h"

#define COMMENT_PREFIX "\"#"

bool girara_config_handle_add(girara_session_t* session, const char* identifier, girara_command_function_t handle) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(identifier != NULL, false);

  girara_session_private_t* session_private = session->private_data;
  bool found                                = false;

  /* search for existing config handle */
  for (size_t idx = 0; idx != girara_list_size(session_private->config.handles); ++idx) {
    girara_config_handle_t* data = girara_list_nth(session_private->config.handles, idx);
    if (g_strcmp0(data->identifier, identifier) == 0) {
      data->handle = handle;
      found        = true;
      break;
    }
  }

  if (found == false) {
    /* add new config handle */
    girara_config_handle_t* config_handle = g_malloc0(sizeof(girara_config_handle_t));

    config_handle->identifier = g_strdup(identifier);
    config_handle->handle     = handle;
    girara_list_append(session_private->config.handles, config_handle);
  }

  return true;
}

void girara_config_handle_free(girara_config_handle_t* handle) {
  if (handle == NULL) {
    return;
  }

  g_free(handle->identifier);
  g_free(handle);
}

static bool config_parse(girara_session_t* session, const char* path) {
  /* open file */
  g_autoptr(GFile) f = g_file_new_for_path(path);
  if (f == NULL) {
    girara_debug("failed to open config file '%s'", path);
    return false;
  }

  g_autoptr(GFileInputStream) stream = g_file_read(f, NULL, NULL);
  if (stream == NULL) {
    girara_debug("failed to open config file '%s'", path);
    return false;
  }

  g_autoptr(GDataInputStream) datastream = g_data_input_stream_new(G_INPUT_STREAM(stream));
  if (datastream == NULL) {
    girara_debug("failed to open config file '%s'", path);
    return false;
  }

  /* read lines */
  char* line               = NULL;
  gsize line_length        = 0;
  unsigned int line_number = 1;
  while ((line = g_data_input_stream_read_line(datastream, &line_length, NULL, NULL)) != NULL) {
    /* skip empty lines and comments */
    if (line_length == 0 || strchr(COMMENT_PREFIX, line[0]) != NULL) {
      g_free(line);
      continue;
    }

    g_autoptr(girara_list_t) argument_list = girara_list_new_with_free(g_free);
    if (argument_list == NULL) {
      g_free(line);
      return false;
    }

    gchar** argv            = NULL;
    gint argc               = 0;
    g_autoptr(GError) error = NULL;

    /* parse current line */
    if (g_shell_parse_argv(line, &argc, &argv, &error) != FALSE) {
      for (int i = 1; i < argc; i++) {
        char* argument = g_strdup(argv[i]);
        girara_list_append(argument_list, argument);
      }
    } else {
      if (error->code != G_SHELL_ERROR_EMPTY_STRING) {
        girara_error("Could not parse line %d in '%s': %s", line_number, path, error->message);
        g_free(line);
        return false;
      } else {
        g_free(line);
        continue;
      }
    }

    /* include gets a special treatment */
    if (g_strcmp0(argv[0], "include") == 0) {
      if (argc != 2) {
        girara_warning("Could not process line %d in '%s': usage: include path.", line_number, path);
      } else {
        g_autofree char* newpath = NULL;
        if (g_path_is_absolute(argv[1]) == TRUE) {
          newpath = g_strdup(argv[1]);
        } else {
          g_autofree char* basename = g_path_get_dirname(path);
          g_autofree char* tmp      = g_build_filename(basename, argv[1], NULL);
          newpath                   = girara_fix_path(tmp);
        }

        if (g_strcmp0(newpath, path) == 0) {
          girara_warning("Could not process line %d in '%s': trying to include itself.", line_number, path);
        } else {
          girara_debug("Loading config file '%s'.", newpath);
          if (config_parse(session, newpath) == false) {
            girara_warning("Could not process line %d in '%s': failed to load '%s'.", line_number, path, newpath);
          }
        }
      }
    } else {
      /* search for config handle */
      girara_session_private_t* session_private = session->private_data;
      bool found                                = false;
      for (size_t idx = 0; idx != girara_list_size(session_private->config.handles); ++idx) {
        girara_config_handle_t* handle = girara_list_nth(session_private->config.handles, idx);
        if (g_strcmp0(handle->identifier, argv[0]) == 0) {
          found = true;
          handle->handle(session, argument_list);
          break;
        }
      }

      if (found == false) {
        girara_warning("Could not process line %d in '%s': Unknown handle '%s'", line_number, path, argv[0]);
      }
    }

    line_number++;
    g_strfreev(argv);
    g_free(line);
  }

  return true;
}

bool girara_config_parse(girara_session_t* session, const char* path) {
  girara_debug("reading configuration file '%s'", path);
  return config_parse(session, path);
}
