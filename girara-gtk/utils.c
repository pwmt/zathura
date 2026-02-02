/* SPDX-License-Identifier: Zlib */

#include "utils.h"

#include <girara/datastructures.h>
#include <girara/log.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "settings.h"
#include "session.h"

int girara_list_cmpstr(const void* lhs, const void* rhs) {
  return g_strcmp0(lhs, rhs);
}

bool girara_exec_with_argument_list(girara_session_t* session, girara_list_t* argument_list) {
  if (session == NULL || argument_list == NULL) {
    return false;
  }

  g_autofree char* cmd = NULL;
  girara_setting_get(session, "exec-command", &cmd);
  if (cmd == NULL || strlen(cmd) == 0) {
    girara_debug("exec-command is empty, executing directly.");
    g_free(cmd);
    cmd = NULL;
  }

  bool dont_append_first_space = cmd == NULL;
  g_autoptr(GString) command   = g_string_new(cmd ? cmd : "");

  for (size_t idx = 0; idx != girara_list_size(argument_list); ++idx) {
    if (dont_append_first_space == false) {
      g_string_append_c(command, ' ');
    }
    dont_append_first_space = false;
    g_autofree char* tmp    = g_shell_quote(girara_list_nth(argument_list, idx));
    g_string_append(command, tmp);
  };

  g_autoptr(GError) error = NULL;
  girara_info("executing: %s", command->str);
  gboolean ret = g_spawn_command_line_async(command->str, &error);
  if (error != NULL) {
    girara_warning("Failed to execute command: %s", error->message);
    girara_notify(session, GIRARA_ERROR, _("Failed to execute command: %s"), error->message);
  }

  return ret;
}

void widget_add_class(GtkWidget* widget, const char* styleclass) {
  if (widget == NULL || styleclass == NULL) {
    return;
  }

  GtkStyleContext* context = gtk_widget_get_style_context(widget);
  if (gtk_style_context_has_class(context, styleclass) == FALSE) {
    gtk_style_context_add_class(context, styleclass);
  }
}

void widget_remove_class(GtkWidget* widget, const char* styleclass) {
  if (widget == NULL || styleclass == NULL) {
    return;
  }

  GtkStyleContext* context = gtk_widget_get_style_context(widget);
  if (gtk_style_context_has_class(context, styleclass) == TRUE) {
    gtk_style_context_remove_class(context, styleclass);
  }
}
