/* SPDX-License-Identifier: Zlib */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <girara/datastructures.h>
#include <girara/utils.h>

#include "completion.h"
#include "internal.h"
#include "session.h"
#include "settings.h"
#include "shortcuts.h"

static GtkEventBox* girara_completion_row_create(const char*, const char*, bool);
static void girara_completion_row_set_color(GtkEventBox*, int);

/* completion */
struct girara_internal_completion_entry_s {
  GtkEventBox* widget; /**< Eventbox widget */
  char* value;         /**< Name of the entry */
  bool group;          /**< The entry is a group */
};

/**
 * Structure of a completion element
 */
struct girara_completion_element_s {
  char* value;       /**> Name of the completion element */
  char* description; /**> Description of the completion element */
};

/**
 * Structure of a completion group
 */
struct girara_completion_group_s {
  char* value;             /**> Name of the completion element */
  girara_list_t* elements; /**> Elements of the completion group */
};

/**
 * Structure of a completion object
 */
struct girara_completion_s {
  girara_list_t* groups; /**> Containing completion groups */
};

typedef struct girara_internal_completion_entry_s girara_internal_completion_entry_t;

static void completion_element_free(void* data) {
  if (data != NULL) {
    girara_completion_element_t* element = data;

    g_free(element->description);
    g_free(element->value);
    g_free(element);
  }
}

girara_completion_t* girara_completion_init(void) {
  girara_completion_t* completion = g_malloc(sizeof(girara_completion_t));
  completion->groups              = girara_list_new_with_free((girara_free_function_t)girara_completion_group_free);

  return completion;
}

girara_completion_group_t* girara_completion_group_create(girara_session_t* UNUSED(session), const char* name) {
  girara_completion_group_t* group = g_malloc(sizeof(girara_completion_group_t));

  group->value    = g_strdup(name);
  group->elements = girara_list_new_with_free(completion_element_free);

  if (group->elements == NULL) {
    g_free(group);
    return NULL;
  }

  return group;
}

void girara_completion_add_group(girara_completion_t* completion, girara_completion_group_t* group) {
  g_return_if_fail(completion != NULL);
  g_return_if_fail(group != NULL);

  girara_list_append(completion->groups, group);
}

void girara_completion_group_free(girara_completion_group_t* group) {
  if (group != NULL) {
    girara_list_free(group->elements);
    g_free(group->value);
    g_free(group);
  }
}

void girara_completion_free(girara_completion_t* completion) {
  if (completion != NULL) {
    girara_list_free(completion->groups);
    g_free(completion);
  }
}

void girara_completion_group_add_element(girara_completion_group_t* group, const char* name, const char* description) {
  g_return_if_fail(group != NULL);
  g_return_if_fail(name != NULL);

  girara_completion_element_t* new_element = g_malloc(sizeof(girara_completion_element_t));

  new_element->value       = g_strdup(name);
  new_element->description = g_strdup(description);

  girara_list_append(group->elements, new_element);
}

static unsigned int find_completion_group_index(GList* current_entry, unsigned int current_index) {
  for (; current_entry != NULL; current_entry = current_entry->prev, --current_index) {
    girara_internal_completion_entry_t* tmp = current_entry->data;
    if (tmp->group) {
      return current_index;
    }
  }

  return UINT_MAX;
}

bool girara_isc_completion(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
                           unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  girara_session_private_t* priv = session->private_data;

  /* get current text */
  g_autofree gchar* input = gtk_editable_get_chars(GTK_EDITABLE(session->gtk.inputbar_entry), 0, -1);
  if (input == NULL) {
    return false;
  }

  const size_t input_length = strlen(input);

  if (input_length == 0 || input[0] != ':') {
    return false;
  }

  gchar** elements = NULL;
  gint n_parameter = 0;
  if (input_length > 1) {
    if (g_shell_parse_argv(input + 1, &n_parameter, &elements, NULL) == FALSE) {
      return false;
    }
  } else {
    elements = g_try_malloc0(2 * sizeof(char*));
    if (elements == NULL) {
      return false;
    }
    elements[0] = g_strdup("");
  }

  if (n_parameter == 1 && input[input_length - 1] == ' ') {
    n_parameter += 1;
  }

  /* get current values */
  gchar* current_command   = (elements[0] != NULL && elements[0][0] != '\0') ? g_strdup(elements[0]) : NULL;
  gchar* current_parameter = (elements[0] != NULL && elements[1] != NULL) ? g_strdup(elements[1]) : NULL;

  size_t current_command_length = current_command ? strlen(current_command) : 0;

  const bool is_single_entry = 1 == g_list_length(priv->completion.entries);

  /* delete old list iff
   *   the completion should be hidden
   *   the current command differs from the previous one
   *   the current parameter differs from the previous one
   *   no current command is given
   *   there is only one completion entry
   */
  if ((argument->n == GIRARA_HIDE) ||
      (current_parameter && priv->completion.previous_parameter &&
       g_strcmp0(current_parameter, priv->completion.previous_parameter)) ||
      (current_command && priv->completion.previous_command &&
       g_strcmp0(current_command, priv->completion.previous_command)) ||
      (input_length != priv->completion.previous_length) || is_single_entry) {
    if (session->gtk.results != NULL) {
      /* destroy elements */
      for (GList* element = priv->completion.entries; element; element = g_list_next(element)) {
        girara_internal_completion_entry_t* entry = (girara_internal_completion_entry_t*)element->data;

        if (entry != NULL) {
          gtk_widget_destroy(GTK_WIDGET(entry->widget));
          g_free(entry->value);
          g_free(entry);
        }
      }

      g_list_free(priv->completion.entries);
      priv->completion.entries         = NULL;
      priv->completion.entries_current = NULL;

      /* delete row box */
      gtk_widget_destroy(GTK_WIDGET(session->gtk.results));
      session->gtk.results = NULL;
    }

    priv->completion.command_mode = true;

    if (argument->n == GIRARA_HIDE) {
      g_free(priv->completion.previous_command);
      priv->completion.previous_command = NULL;

      g_free(priv->completion.previous_parameter);
      priv->completion.previous_parameter = NULL;

      g_strfreev(elements);

      g_free(current_parameter);
      g_free(current_command);

      return false;
    }
  }

  /* create new list iff
   *  there is no current list
   */
  if (session->gtk.results == NULL) {
    session->gtk.results = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    widget_add_class(GTK_WIDGET(session->gtk.results), "completion-box");

    if (session->gtk.results == NULL) {
      g_free(current_parameter);
      g_free(current_command);

      g_strfreev(elements);
      return false;
    }

    if (n_parameter <= 1) {
      /* based on commands */
      priv->completion.command_mode = true;

      /* create command rows */
      for (size_t idx = 0; idx != girara_list_size(session->bindings.commands); ++idx) {
        girara_command_t* command = girara_list_nth(session->bindings.commands, idx);
        if (current_command == NULL ||
            (command->command != NULL && !strncmp(current_command, command->command, current_command_length)) ||
            (command->abbr != NULL && !strncmp(current_command, command->abbr, current_command_length))) {
          /* create entry */
          girara_internal_completion_entry_t* entry = g_malloc(sizeof(girara_internal_completion_entry_t));
          entry->group                              = FALSE;
          entry->value                              = g_strdup(command->command);
          entry->widget = girara_completion_row_create(command->command, command->description, FALSE);

          priv->completion.entries = g_list_append(priv->completion.entries, entry);

          /* show entry row */
          gtk_box_pack_start(session->gtk.results, GTK_WIDGET(entry->widget), FALSE, FALSE, 0);
        }
      }
    }

    /* based on parameters */
    if (n_parameter > 1 || g_list_length(priv->completion.entries) == 1) {
      /* if only one command exists try to run parameter completion */
      if (g_list_length(priv->completion.entries) == 1) {
        girara_internal_completion_entry_t* entry = g_list_first(priv->completion.entries)->data;

        /* unset command mode */
        priv->completion.command_mode = false;
        current_command               = entry->value;
        current_command_length        = strlen(current_command);

        /* clear list */
        gtk_widget_destroy(GTK_WIDGET(entry->widget));

        priv->completion.entries =
            g_list_remove(priv->completion.entries, g_list_first(priv->completion.entries)->data);
        g_free(entry);
      }

      /* search matching command */
      girara_command_t* command = NULL;
      for (size_t idx = 0; idx != girara_list_size(session->bindings.commands); ++idx) {
        girara_command_t* command_it = girara_list_nth(session->bindings.commands, idx);
        if ((current_command != NULL && command_it->command != NULL &&
             !strncmp(current_command, command_it->command, current_command_length)) ||
            (current_command != NULL && command_it->abbr != NULL &&
             !strncmp(current_command, command_it->abbr, current_command_length))) {
          g_free(priv->completion.previous_command);
          priv->completion.previous_command = g_strdup(command_it->command);
          command                           = command_it;
          break;
        }
      }

      if (command == NULL) {
        g_free(current_command);
        g_free(current_parameter);

        g_strfreev(elements);
        return false;
      }

      if (command->completion == NULL) {
        girara_internal_completion_entry_t* entry = g_malloc(sizeof(girara_internal_completion_entry_t));
        entry->group                              = FALSE;
        entry->value                              = g_strdup(command->command);
        entry->widget = girara_completion_row_create(command->command, command->description, FALSE);

        priv->completion.entries = g_list_append(priv->completion.entries, entry);

        gtk_box_pack_start(session->gtk.results, GTK_WIDGET(entry->widget), FALSE, FALSE, 0);
        priv->completion.command_mode = true;
      } else {
        /* generate completion result
         * XXX: the last argument should only be current_paramater ... but
         * therefore the completion functions would need to handle NULL correctly
         * (see cc_open in zathura). */
        girara_completion_t* result = command->completion(session, current_parameter ? current_parameter : "");

        if (result == NULL || result->groups == NULL) {
          g_free(current_parameter);
          g_free(current_command);

          g_strfreev(elements);
          return false;
        }

        for (size_t idx = 0; idx != girara_list_size(result->groups); ++idx) {
          girara_completion_group_t* group = girara_list_nth(result->groups, idx);
          if (group->elements == NULL || girara_list_size(group->elements) == 0) {
            continue;
          }

          /* create group entry */
          if (group->value != NULL) {
            girara_internal_completion_entry_t* entry = g_malloc(sizeof(girara_internal_completion_entry_t));
            entry->group                              = TRUE;
            entry->value                              = g_strdup(group->value);
            entry->widget                             = girara_completion_row_create(group->value, NULL, TRUE);

            priv->completion.entries = g_list_append(priv->completion.entries, entry);

            gtk_box_pack_start(session->gtk.results, GTK_WIDGET(entry->widget), FALSE, FALSE, 0);
          }

          for (size_t inner_idx = 0; inner_idx != girara_list_size(group->elements); ++inner_idx) {
            girara_completion_element_t* element = girara_list_nth(group->elements, inner_idx);

            girara_internal_completion_entry_t* entry = g_malloc(sizeof(girara_internal_completion_entry_t));
            entry->group                              = FALSE;
            entry->value                              = g_strdup(element->value);
            entry->widget = girara_completion_row_create(element->value, element->description, FALSE);

            priv->completion.entries = g_list_append(priv->completion.entries, entry);

            gtk_box_pack_start(session->gtk.results, GTK_WIDGET(entry->widget), FALSE, FALSE, 0);
          }
        }
        girara_completion_free(result);

        priv->completion.command_mode = false;
      }
    }

    if (priv->completion.entries != NULL) {
      priv->completion.entries_current =
          (argument->n == GIRARA_NEXT) ? g_list_last(priv->completion.entries) : priv->completion.entries;
      gtk_box_pack_start(priv->gtk.bottom_box, GTK_WIDGET(session->gtk.results), FALSE, FALSE, 0);
      gtk_widget_show(GTK_WIDGET(session->gtk.results));
    }
  }

  /* update entries */
  unsigned int n_elements = g_list_length(priv->completion.entries);
  if (priv->completion.entries != NULL && n_elements > 0) {
    if (n_elements > 1) {
      girara_completion_row_set_color(
          ((girara_internal_completion_entry_t*)priv->completion.entries_current->data)->widget, GIRARA_NORMAL);

      bool next_group = FALSE;

      for (unsigned int i = 0; i < n_elements; i++) {
        if (argument->n == GIRARA_NEXT || argument->n == GIRARA_NEXT_GROUP) {
          GList* entry = g_list_next(priv->completion.entries_current);
          if (entry == NULL) {
            entry = g_list_first(priv->completion.entries);
          }

          priv->completion.entries_current = entry;
        } else if (argument->n == GIRARA_PREVIOUS || argument->n == GIRARA_PREVIOUS_GROUP) {
          GList* entry = g_list_previous(priv->completion.entries_current);
          if (entry == NULL) {
            entry = g_list_last(priv->completion.entries);
          }

          priv->completion.entries_current = entry;
        }

        if (((girara_internal_completion_entry_t*)priv->completion.entries_current->data)->group) {
          if (priv->completion.command_mode == false &&
              (argument->n == GIRARA_NEXT_GROUP || argument->n == GIRARA_PREVIOUS_GROUP)) {
            next_group = TRUE;
          }
          continue;
        } else {
          if (priv->completion.command_mode == false && (next_group == 0) &&
              (argument->n == GIRARA_NEXT_GROUP || argument->n == GIRARA_PREVIOUS_GROUP)) {
            continue;
          }
          break;
        }
      }

      girara_completion_row_set_color(
          ((girara_internal_completion_entry_t*)priv->completion.entries_current->data)->widget, GIRARA_HIGHLIGHT);

      /* hide other items */
      unsigned int n_completion_items = 15;
      girara_setting_get(session, "n-completion-items", &n_completion_items);
      const unsigned int uh = ceil(n_completion_items / 2.0);
      const unsigned int lh = floor(n_completion_items / 2.0);

      const unsigned int current_item  = g_list_position(priv->completion.entries, priv->completion.entries_current);
      const unsigned int current_group = find_completion_group_index(priv->completion.entries_current, current_item);

      GList* tmpentry = priv->completion.entries;
      for (unsigned int i = 0; i < n_elements; i++) {
        girara_internal_completion_entry_t* tmp = tmpentry->data;
        /* If there is less than n-completion-items that need to be shown, show everything.
         * Else, show n-completion-items items
         * Additionally, the current group name is always shown */
        if ((n_elements <= n_completion_items) || (i >= (current_item - lh) && (i <= current_item + uh)) ||
            (i < n_completion_items && current_item < lh) ||
            (i >= (n_elements - n_completion_items) && (current_item >= (n_elements - uh))) || (i == current_group)) {
          gtk_widget_show(GTK_WIDGET(tmp->widget));
        } else {
          gtk_widget_hide(GTK_WIDGET(tmp->widget));
        }

        tmpentry = g_list_next(tmpentry);
      }
    } else {
      gtk_widget_hide(
          GTK_WIDGET(((girara_internal_completion_entry_t*)(g_list_nth(priv->completion.entries, 0))->data)->widget));
    }

    /* update text */
    char* temp;
    char* escaped_value =
        girara_escape_string(((girara_internal_completion_entry_t*)priv->completion.entries_current->data)->value);
    if (priv->completion.command_mode == true) {
      char* space = (n_elements == 1) ? " " : "";
      temp        = g_strconcat(":", escaped_value, space, NULL);
    } else {
      temp = g_strconcat(":", priv->completion.previous_command, " ", escaped_value, NULL);
    }

    gtk_entry_set_text(session->gtk.inputbar_entry, temp);
    gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar_entry), -1);
    g_free(escaped_value);

    /* update previous */
    g_free(priv->completion.previous_parameter);
    g_free(priv->completion.previous_command);

    priv->completion.previous_command =
        g_strdup((priv->completion.command_mode)
                     ? ((girara_internal_completion_entry_t*)priv->completion.entries_current->data)->value
                     : current_command);
    priv->completion.previous_parameter =
        g_strdup((priv->completion.command_mode)
                     ? current_parameter
                     : ((girara_internal_completion_entry_t*)priv->completion.entries_current->data)->value);
    priv->completion.previous_length = strlen(temp);
    g_free(temp);
  }

  g_free(current_parameter);
  g_free(current_command);

  g_strfreev(elements);

  return false;
}

static GtkEventBox* girara_completion_row_create(const char* command, const char* description, bool group) {
  GtkBox* col = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

  GtkEventBox* row = GTK_EVENT_BOX(gtk_event_box_new());

  GtkLabel* show_command     = GTK_LABEL(gtk_label_new(NULL));
  GtkLabel* show_description = GTK_LABEL(gtk_label_new(NULL));

  gtk_widget_set_halign(GTK_WIDGET(show_command), GTK_ALIGN_START);
  gtk_widget_set_valign(GTK_WIDGET(show_command), GTK_ALIGN_START);
  gtk_widget_set_halign(GTK_WIDGET(show_description), GTK_ALIGN_END);
  gtk_widget_set_valign(GTK_WIDGET(show_description), GTK_ALIGN_START);

  gtk_label_set_use_markup(show_command, TRUE);
  gtk_label_set_use_markup(show_description, TRUE);

  gtk_label_set_ellipsize(show_command, PANGO_ELLIPSIZE_END);
  gtk_label_set_ellipsize(show_description, PANGO_ELLIPSIZE_END);

  g_autofree gchar* c = command ? g_markup_printf_escaped(FORMAT_COMMAND, command) : NULL;
  g_autofree gchar* d = description ? g_markup_printf_escaped(FORMAT_DESCRIPTION, description) : NULL;
  gtk_label_set_markup(show_command, command ? c : "");
  gtk_label_set_markup(show_description, description ? d : "");

  const char* class = group == true ? "completion-group" : "completion";
  widget_add_class(GTK_WIDGET(show_command), class);
  widget_add_class(GTK_WIDGET(show_description), class);
  widget_add_class(GTK_WIDGET(row), class);
  widget_add_class(GTK_WIDGET(col), class);

  gtk_box_pack_start(GTK_BOX(col), GTK_WIDGET(show_command), TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(col), GTK_WIDGET(show_description), TRUE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(row), GTK_WIDGET(col));
  gtk_widget_show_all(GTK_WIDGET(row));

  return row;
}

static void girara_completion_row_set_color(GtkEventBox* row, int mode) {
  g_return_if_fail(row != NULL);

  GtkBox* col            = GTK_BOX(gtk_bin_get_child(GTK_BIN(row)));
  g_autoptr(GList) items = gtk_container_get_children(GTK_CONTAINER(col));
  GtkWidget* cmd         = GTK_WIDGET(g_list_nth_data(items, 0));
  GtkWidget* desc        = GTK_WIDGET(g_list_nth_data(items, 1));

  if (mode == GIRARA_HIGHLIGHT) {
    gtk_widget_set_state_flags(cmd, GTK_STATE_FLAG_SELECTED, false);
    gtk_widget_set_state_flags(desc, GTK_STATE_FLAG_SELECTED, false);
    gtk_widget_set_state_flags(GTK_WIDGET(row), GTK_STATE_FLAG_SELECTED, false);
  } else {
    gtk_widget_unset_state_flags(cmd, GTK_STATE_FLAG_SELECTED);
    gtk_widget_unset_state_flags(desc, GTK_STATE_FLAG_SELECTED);
    gtk_widget_unset_state_flags(GTK_WIDGET(row), GTK_STATE_FLAG_SELECTED);
  }
}
