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

static void cb_window_icon(girara_session_t* session, const char* UNUSED(name), girara_setting_type_t UNUSED(type),
                           const void* value, void* UNUSED(data)) {
  g_return_if_fail(session != NULL && value != NULL);

  if (session->gtk.window == NULL) {
    return;
  }

  girara_set_window_icon(session, value);
}

static void cb_font(girara_session_t* session, const char* UNUSED(name), girara_setting_type_t UNUSED(type),
                    const void* value, void* UNUSED(data)) {
  g_return_if_fail(session != NULL && value != NULL);

  css_template_fill_font(session->private_data->csstemplate, value);
}

static void cb_color(girara_session_t* session, const char* name, girara_setting_type_t UNUSED(type), const void* value,
                     void* UNUSED(data)) {
  g_return_if_fail(session != NULL && value != NULL);

  const char* str_value = value;

  GdkRGBA color = {0, 0, 0, 0};
  gdk_rgba_parse(&color, str_value);

  char* colorstr = gdk_rgba_to_string(&color);
  girara_template_set_variable_value(session->private_data->csstemplate, name, colorstr);
  g_free(colorstr);
}

static void cb_guioptions(girara_session_t* session, const char* UNUSED(name), girara_setting_type_t UNUSED(type),
                          const void* value, void* UNUSED(data)) {
  g_return_if_fail(session != NULL && value != NULL);

  /* set default values */
  bool show_commandline = false;
  bool show_statusbar   = false;

  /* evaluate input */
  const char* input         = value;
  const size_t input_length = strlen(input);

  for (size_t i = 0; i < input_length; i++) {
    switch (input[i]) {
    /* command line */
    case 'c':
      show_commandline = true;
      break;
    /* statusbar */
    case 's':
      show_statusbar = true;
      break;
    }
  }

  /* apply settings */
  if (show_commandline == true) {
    session->global.autohide_inputbar = false;
    gtk_widget_show(session->gtk.inputbar);
  } else {
    session->global.autohide_inputbar = true;
    gtk_widget_hide(session->gtk.inputbar);
  }

  if (show_statusbar == true) {
    session->global.hide_statusbar = false;
    gtk_widget_show(session->gtk.statusbar);
  } else {
    session->global.hide_statusbar = true;
    gtk_widget_hide(session->gtk.statusbar);
  }
}

void girara_config_load_default(girara_session_t* session) {
  if (session == NULL) {
    return;
  }

  /* values */
  const int statusbar_h_padding = 8;
  const int statusbar_v_padding = 2;
  const int window_width        = 800;
  const int window_height       = 600;
  const int n_completion_items  = 15;
  girara_mode_t normal_mode     = session->modes.normal;

  /* clang-format off */
  /* settings */
  girara_setting_add(session, "font",                     "monospace normal 9", STRING,  FALSE, _("Font"), cb_font, NULL);
  girara_setting_add(session, "default-fg",               "#DDDDDD",            STRING,  FALSE,  _("Default foreground color"), cb_color, NULL);
  girara_setting_add(session, "default-bg",               "#000000",            STRING,  FALSE,  _("Default background color"), cb_color, NULL);
  girara_setting_add(session, "inputbar-fg",              "#9FBC00",            STRING,  FALSE,  _("Inputbar foreground color"), cb_color, NULL);
  girara_setting_add(session, "inputbar-bg",              "#131313",            STRING,  FALSE,  _("Inputbar background color"), cb_color, NULL);
  girara_setting_add(session, "statusbar-fg",             "#FFFFFF",            STRING,  FALSE,  _("Statusbar foreground color"), cb_color, NULL);
  girara_setting_add(session, "statusbar-bg",             "#000000",            STRING,  FALSE,  _("Statsubar background color"), cb_color, NULL);
  girara_setting_add(session, "completion-fg",            "#DDDDDD",            STRING,  FALSE,  _("Completion foreground color"), cb_color, NULL);
  girara_setting_add(session, "completion-bg",            "#232323",            STRING,  FALSE,  _("Completion background color"), cb_color, NULL);
  girara_setting_add(session, "completion-group-fg",      "#DEDEDE",            STRING,  FALSE,  _("Completion group foreground color"), cb_color, NULL);
  girara_setting_add(session, "completion-group-bg",      "#000000",            STRING,  FALSE,  _("Completion group background color"), cb_color, NULL);
  girara_setting_add(session, "completion-highlight-fg",  "#232323",            STRING,  FALSE,  _("Completion highlight foreground color"), cb_color, NULL);
  girara_setting_add(session, "completion-highlight-bg",  "#9FBC00",            STRING,  FALSE,  _("Completion highlight background color"), cb_color, NULL);
  girara_setting_add(session, "notification-error-fg",    "#FFFFFF",            STRING,  FALSE,  _("Error notification foreground color"), cb_color, NULL);
  girara_setting_add(session, "notification-error-bg",    "#FF1212",            STRING,  FALSE,  _("Error notification background color"), cb_color, NULL);
  girara_setting_add(session, "notification-warning-fg",  "#000000",            STRING,  FALSE,  _("Warning notification foreground color"), cb_color, NULL);
  girara_setting_add(session, "notification-warning-bg",  "#F3F000",            STRING,  FALSE,  _("Warning notifaction background color"), cb_color, NULL);
  girara_setting_add(session, "notification-fg",          "#000000",            STRING,  FALSE,  _("Notification foreground color"), cb_color, NULL);
  girara_setting_add(session, "notification-bg",          "#FFFFFF",            STRING,  FALSE,  _("Notification background color"), cb_color, NULL);
  girara_setting_add(session, "word-separator",           " /.-=&#?",           STRING,  TRUE,  NULL, NULL, NULL);
  girara_setting_add(session, "window-width",             &window_width,        INT,     TRUE,  _("Initial window width"), NULL, NULL);
  girara_setting_add(session, "window-height",            &window_height,       INT,     TRUE,  _("Initial window height"), NULL, NULL);
  girara_setting_add(session, "statusbar-h-padding",      &statusbar_h_padding, INT,     TRUE,  _("Horizontal padding for the status, input, and notification bars"), NULL, NULL);
  girara_setting_add(session, "statusbar-v-padding",      &statusbar_v_padding, INT,     TRUE,  _("Vertical padding for the status, input, and notification bars"), NULL, NULL);
  girara_setting_add(session, "n-completion-items",       &n_completion_items,  INT,     TRUE,  _("Number of completion items"), NULL, NULL);
  girara_setting_add(session, "window-icon",              "",                   STRING,  FALSE, _("Window icon"), cb_window_icon, NULL);
  girara_setting_add(session, "exec-command",             "",                   STRING,  FALSE, _("Command to execute in :exec"), NULL, NULL);
  girara_setting_add(session, "guioptions",               "s",                  STRING,  FALSE, _("Show or hide certain GUI elements"), cb_guioptions, NULL);

  /* shortcuts */
  girara_shortcut_add(session, 0,                GDK_KEY_Escape,      NULL, girara_sc_abort,           normal_mode, 0, NULL);
  girara_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_bracketleft, NULL, girara_sc_abort,           normal_mode, 0, NULL);
  girara_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_c,           NULL, girara_sc_abort,           normal_mode, 0, NULL);
  girara_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_q,           NULL, girara_sc_quit,            normal_mode, 0, NULL);
  girara_shortcut_add(session, 0,                GDK_KEY_colon,       NULL, girara_sc_focus_inputbar,  normal_mode, 0, ":");

  /* inputbar shortcuts */
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_j,            girara_isc_activate,            0,                           NULL);
  girara_inputbar_shortcut_add(session, 0,                GDK_KEY_Escape,       girara_isc_abort,               0,                           NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_bracketleft,  girara_isc_abort,               0,                           NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_c,            girara_isc_abort,               0,                           NULL);
  girara_inputbar_shortcut_add(session, 0,                GDK_KEY_Tab,          girara_isc_completion,          GIRARA_NEXT,                 NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_Tab,          girara_isc_completion,          GIRARA_NEXT_GROUP,           NULL);
  girara_inputbar_shortcut_add(session, 0,                GDK_KEY_ISO_Left_Tab, girara_isc_completion,          GIRARA_PREVIOUS,             NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_ISO_Left_Tab, girara_isc_completion,          GIRARA_PREVIOUS_GROUP,       NULL);
  girara_inputbar_shortcut_add(session, 0,                GDK_KEY_BackSpace,    girara_isc_string_manipulation, GIRARA_DELETE_LAST_CHAR,     NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_h,            girara_isc_string_manipulation, GIRARA_DELETE_LAST_CHAR,     NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_u,            girara_isc_string_manipulation, GIRARA_DELETE_TO_LINE_START, NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_k,            girara_isc_string_manipulation, GIRARA_DELETE_TO_LINE_END,   NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_d,            girara_isc_string_manipulation, GIRARA_DELETE_CURR_CHAR,     NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_w,            girara_isc_string_manipulation, GIRARA_DELETE_LAST_WORD,     NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_f,            girara_isc_string_manipulation, GIRARA_NEXT_CHAR,            NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_b,            girara_isc_string_manipulation, GIRARA_PREVIOUS_CHAR,        NULL);
  girara_inputbar_shortcut_add(session, 0,                GDK_KEY_Right,        girara_isc_string_manipulation, GIRARA_NEXT_CHAR,            NULL);
  girara_inputbar_shortcut_add(session, 0,                GDK_KEY_Left,         girara_isc_string_manipulation, GIRARA_PREVIOUS_CHAR,        NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_a,            girara_isc_string_manipulation, GIRARA_GOTO_START,           NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_e,            girara_isc_string_manipulation, GIRARA_GOTO_END,             NULL);
  girara_inputbar_shortcut_add(session, 0,                GDK_KEY_Up,           girara_isc_command_history,     GIRARA_PREVIOUS,             NULL);
  girara_inputbar_shortcut_add(session, 0,                GDK_KEY_Down,         girara_isc_command_history,     GIRARA_NEXT,                 NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_p,            girara_isc_command_history,     GIRARA_PREVIOUS,             NULL);
  girara_inputbar_shortcut_add(session, GDK_CONTROL_MASK, GDK_KEY_n,            girara_isc_command_history,     GIRARA_NEXT,                 NULL);

  /* commands */
  girara_inputbar_command_add(session, "exec",  NULL, girara_cmd_exec,        NULL,          _("Execute a command"));
  girara_inputbar_command_add(session, "map",   "m",  girara_cmd_map,         NULL,          _("Map a key sequence"));
  girara_inputbar_command_add(session, "quit",  "q",  girara_cmd_quit,        NULL,          _("Quit the program"));
  girara_inputbar_command_add(session, "set",   "s",  girara_cmd_set,         girara_cc_set, _("Set an option"));
  girara_inputbar_command_add(session, "unmap", NULL, girara_cmd_unmap,       NULL,          _("Unmap a key sequence"));
  girara_inputbar_command_add(session, "dump",  NULL, girara_cmd_dump_config, NULL,          _("Dump settings to a file"));

  /* config handle */
  girara_config_handle_add(session, "map",   girara_cmd_map);
  girara_config_handle_add(session, "set",   girara_cmd_set);
  girara_config_handle_add(session, "unmap", girara_cmd_unmap);

  /* shortcut mappings */
  girara_shortcut_mapping_add(session, "exec",             girara_sc_exec);
  girara_shortcut_mapping_add(session, "feedkeys",         girara_sc_feedkeys);
  girara_shortcut_mapping_add(session, "focus_inputbar",   girara_sc_focus_inputbar);
  girara_shortcut_mapping_add(session, "quit",             girara_sc_quit);
  girara_shortcut_mapping_add(session, "set",              girara_sc_set);
  girara_shortcut_mapping_add(session, "toggle_inputbar",  girara_sc_toggle_inputbar);
  girara_shortcut_mapping_add(session, "toggle_statusbar", girara_sc_toggle_statusbar);
  /* clang-format on */
}

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

void girara_config_parse(girara_session_t* session, const char* path) {
  girara_debug("reading configuration file '%s'", path);
  config_parse(session, path);
}
