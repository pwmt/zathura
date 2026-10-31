/* SPDX-License-Identifier: Zlib */

#include "completion.h"

#include <string.h>
#include <girara/datastructures.h>
#include <girara/utils.h>

#include "internal.h"
#include "session.h"
#include "settings.h"
#include "shortcuts.h"

// Model items for completion list view
typedef struct {
  GObject parent_instance;

  char* value;
  char* description;
  bool group;
} GiraraCompletionItem;

typedef GObjectClass GiraraCompletionItemClass;

G_DEFINE_TYPE(GiraraCompletionItem, girara_completion_item, G_TYPE_OBJECT)

#define GIRARA_COMPLETION_ITEM_TYPE (girara_completion_item_get_type())
#define GIRARA_COMPLETION_ITEM(obj)                                                                                    \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GIRARA_COMPLETION_ITEM_TYPE, GiraraCompletionItem))

static void girara_completion_item_finalize(GObject* object) {
  GiraraCompletionItem* item = GIRARA_COMPLETION_ITEM(object);
  g_free(item->description);
  g_free(item->value);

  G_OBJECT_CLASS(girara_completion_item_parent_class)->finalize(object);
}

static void girara_completion_item_class_init(GiraraCompletionItemClass* klass) {
  G_OBJECT_CLASS(klass)->finalize = girara_completion_item_finalize;
}

static void girara_completion_item_init(GiraraCompletionItem* item) {
  item->value       = NULL;
  item->description = NULL;
  item->group       = false;
}

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

static void completion_element_free(void* data) {
  if (data) {
    girara_completion_element_t* element = data;

    g_free(element->description);
    g_free(element->value);
    g_free(element);
  }
}

girara_completion_t* girara_completion_init(void) {
  g_autoptr(girara_completion_t) completion = g_try_malloc0(sizeof(girara_completion_t));
  if (!completion) {
    return NULL;
  }

  completion->groups = girara_list_new_with_free((girara_free_function_t)girara_completion_group_free);
  if (!completion->groups) {
    return NULL;
  }

  return g_steal_pointer(&completion);
}

girara_completion_group_t* girara_completion_group_create(const char* name) {
  g_autoptr(girara_completion_group_t) group = g_try_malloc0(sizeof(girara_completion_group_t));
  if (!group) {
    return NULL;
  }

  group->value    = g_strdup(name);
  group->elements = girara_list_new_with_free(completion_element_free);
  if (!group->elements) {
    return NULL;
  }

  return g_steal_pointer(&group);
}

void girara_completion_add_group(girara_completion_t* completion, girara_completion_group_t* group) {
  g_return_if_fail(completion != NULL);
  g_return_if_fail(group != NULL);

  girara_list_append(completion->groups, group);
}

void girara_completion_group_free(girara_completion_group_t* group) {
  if (group) {
    girara_list_free(group->elements);
    g_free(group->value);
    g_free(group);
  }
}

void girara_completion_free(girara_completion_t* completion) {
  if (completion) {
    girara_list_free(completion->groups);
    g_free(completion);
  }
}

void girara_completion_group_add_element(girara_completion_group_t* group, const char* name, const char* description) {
  g_return_if_fail(group != NULL);
  g_return_if_fail(name != NULL);

  girara_completion_element_t* new_element = g_try_malloc0(sizeof(girara_completion_element_t));
  g_return_if_fail(new_element);

  new_element->value       = g_strdup(name);
  new_element->description = g_strdup(description);

  girara_list_append(group->elements, new_element);
}

static void completion_append(GListStore* model, const char* value, const char* description, bool group) {
  g_autoptr(GObject) object = g_object_new(GIRARA_COMPLETION_ITEM_TYPE, NULL);

  GiraraCompletionItem* item = GIRARA_COMPLETION_ITEM(object);
  item->value                = g_strdup(value);
  item->description          = g_strdup(description);
  item->group                = group;

  g_list_store_append(model, object);
}

static void completion_row_setup(GtkSignalListItemFactory* UNUSED(factory), GtkListItem* item, gpointer UNUSED(data)) {
  GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  for (unsigned int i = 0; i < 2; ++i) {
    GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(label), i == 0 ? 0 : 1);
    gtk_widget_set_valign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(row), label);
  }

  gtk_list_item_set_focusable(item, FALSE);
  gtk_list_item_set_activatable(item, FALSE);
  gtk_list_item_set_child(item, row);
}

static void completion_row_bind(GtkSignalListItemFactory* UNUSED(factory), GtkListItem* list_item,
                                gpointer UNUSED(data)) {
  GiraraCompletionItem* item = gtk_list_item_get_item(list_item);
  GtkWidget* row             = gtk_list_item_get_child(list_item);
  GtkWidget* command         = gtk_widget_get_first_child(row);
  GtkWidget* description     = gtk_widget_get_next_sibling(command);

  g_autofree char* cmd  = g_markup_printf_escaped(FORMAT_COMMAND, item->value);
  g_autofree char* desc = item->description ? g_markup_printf_escaped(FORMAT_DESCRIPTION, item->description) : NULL;
  gtk_label_set_markup(GTK_LABEL(command), cmd);
  gtk_label_set_markup(GTK_LABEL(description), desc ? desc : "");

  // Row reuse requires update of widget classes.
  const char* style     = item->group ? "completion-group" : "completion";
  const char* old_style = item->group ? "completion" : "completion-group";

  GtkWidget* widgets[] = {row, command, description};
  for (size_t i = 0; i < G_N_ELEMENTS(widgets); ++i) {
    gtk_widget_remove_css_class(widgets[i], old_style);
    gtk_widget_add_css_class(widgets[i], style);
  }
  gtk_list_item_set_selectable(list_item, !item->group);
}

void girara_completion_clear(girara_session_t* session) {
  girara_session_private_t* priv = session->private_data;
  if (priv->completion.selection) {
    g_signal_handlers_disconnect_by_data(priv->completion.selection, session);
  }
  if (priv->completion.view_selection) {
    g_signal_handlers_disconnect_by_data(priv->completion.view_selection, session);
  }

  if (session->gtk.results) {
    g_signal_handlers_disconnect_by_data(session->gtk.results, session);
    gtk_list_view_set_factory(session->gtk.results, NULL);
    gtk_list_view_set_model(session->gtk.results, NULL);
  }

  if (priv->completion.scroll) {
    GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(priv->completion.scroll));
    if (parent) {
      gtk_box_remove(GTK_BOX(parent), GTK_WIDGET(priv->completion.scroll));
    }

    gtk_scrolled_window_set_child(priv->completion.scroll, NULL);
    g_clear_object(&priv->completion.scroll);
  }

  session->gtk.results = NULL;
  g_clear_object(&priv->completion.view_selection);
  g_clear_object(&priv->completion.visible_model);
  g_clear_object(&priv->completion.selection);
  g_clear_object(&priv->completion.model);
  g_clear_pointer(&priv->completion.previous_command, g_free);
  g_clear_pointer(&priv->completion.previous_input, g_free);
  priv->completion.command_mode = true;
}

static void completion_view_selected(GObject* UNUSED(object), GParamSpec* UNUSED(pspec), girara_session_t* session) {
  girara_session_private_t* priv = session->private_data;

  const unsigned int selected = gtk_single_selection_get_selected(priv->completion.view_selection);
  if (selected == GTK_INVALID_LIST_POSITION) {
    return;
  }

  GiraraCompletionItem* item = gtk_single_selection_get_selected_item(priv->completion.view_selection);
  if (!item->group) {
    const unsigned int offset = gtk_slice_list_model_get_offset(priv->completion.visible_model);
    gtk_single_selection_set_selected(priv->completion.selection, offset + selected);
  }
}

static void completion_update_view(girara_session_t* session) {
  girara_session_private_t* priv = session->private_data;
  const unsigned int position    = gtk_single_selection_get_selected(priv->completion.selection);
  const unsigned int total       = g_list_model_get_n_items(G_LIST_MODEL(priv->completion.model));

  unsigned int max_items = 15;
  girara_setting_get(session, "n-completion-items", &max_items);

  const unsigned int size   = MIN(total, max_items);
  const unsigned int offset = size ? MIN(position > size / 2 ? position - size / 2 : 0, total - size) : 0;

  /* Moving the slice changes its local selection; only the final selection should
   * be mapped back to the complete model. */
  g_signal_handlers_block_matched(priv->completion.view_selection, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, session);
  gtk_slice_list_model_set_size(priv->completion.visible_model, size);
  gtk_slice_list_model_set_offset(priv->completion.visible_model, offset);
  gtk_single_selection_set_selected(priv->completion.view_selection,
                                    size ? position - offset : GTK_INVALID_LIST_POSITION);
  g_signal_handlers_unblock_matched(priv->completion.view_selection, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, session);

  gtk_widget_set_visible(GTK_WIDGET(priv->completion.scroll), total > 1 && size > 0);
  if (size) {
    gtk_list_view_scroll_to(session->gtk.results, position - offset, GTK_LIST_SCROLL_NONE, NULL);
  }
}

static void completion_selected(GObject* UNUSED(object), GParamSpec* UNUSED(pspec), girara_session_t* session) {
  girara_session_private_t* priv = session->private_data;
  GiraraCompletionItem* item     = gtk_single_selection_get_selected_item(priv->completion.selection);
  if (!item || item->group) {
    return;
  }

  g_autofree char* escaped = girara_escape_string(item->value);
  g_autofree char* input   = NULL;
  if (priv->completion.command_mode) {
    const char* space = g_list_model_get_n_items(G_LIST_MODEL(priv->completion.model)) == 1 ? " " : "";
    input             = g_strconcat(":", escaped, space, NULL);

    g_free(priv->completion.previous_command);
    priv->completion.previous_command = g_strdup(item->value);
  } else {
    input = g_strconcat(":", priv->completion.previous_command, " ", escaped, NULL);
  }

  g_free(priv->completion.previous_input);
  priv->completion.previous_input = g_strdup(input);

  GtkEditable* entry = GTK_EDITABLE(girara_inputbar_get_entry(GIRARA_INPUTBAR(session->gtk.inputbar)));
  gtk_editable_set_text(entry, input);
  gtk_editable_set_position(entry, -1);
  completion_update_view(session);
}

static void completion_view_create(girara_session_t* session) {
  girara_session_private_t* priv = session->private_data;

  priv->completion.selection = gtk_single_selection_new(G_LIST_MODEL(g_object_ref(priv->completion.model)));
  gtk_single_selection_set_autoselect(priv->completion.selection, FALSE);
  gtk_single_selection_set_can_unselect(priv->completion.selection, TRUE);
  gtk_single_selection_set_selected(priv->completion.selection, GTK_INVALID_LIST_POSITION);

  priv->completion.visible_model = gtk_slice_list_model_new(G_LIST_MODEL(g_object_ref(priv->completion.model)), 0, 0);
  priv->completion.view_selection =
      gtk_single_selection_new(G_LIST_MODEL(g_object_ref(priv->completion.visible_model)));
  gtk_single_selection_set_autoselect(priv->completion.view_selection, FALSE);
  gtk_single_selection_set_can_unselect(priv->completion.view_selection, TRUE);

  priv->completion.scroll = GTK_SCROLLED_WINDOW(g_object_ref_sink(gtk_scrolled_window_new()));
  gtk_scrolled_window_set_policy(priv->completion.scroll, GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_scrolled_window_set_propagate_natural_height(priv->completion.scroll, TRUE);
  // TODO: 300 is an arbitrary value
  gtk_scrolled_window_set_max_content_height(priv->completion.scroll, 300);

  GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(completion_row_setup), NULL);
  g_signal_connect(factory, "bind", G_CALLBACK(completion_row_bind), NULL);

  session->gtk.results =
      GTK_LIST_VIEW(gtk_list_view_new(GTK_SELECTION_MODEL(g_object_ref(priv->completion.view_selection)), factory));
  gtk_widget_add_css_class(GTK_WIDGET(session->gtk.results), "completion-box");
  gtk_widget_set_focusable(GTK_WIDGET(session->gtk.results), FALSE);

  gtk_scrolled_window_set_child(priv->completion.scroll, GTK_WIDGET(session->gtk.results));
  gtk_box_append(priv->gtk.bottom_box, GTK_WIDGET(priv->completion.scroll));
  g_signal_connect(priv->completion.selection, "notify::selected", G_CALLBACK(completion_selected), session);
  g_signal_connect(priv->completion.view_selection, "notify::selected", G_CALLBACK(completion_view_selected), session);
}

static void completion_build(girara_session_t* session, const char* current_command, const char* parameter,
                             bool complete_parameter) {
  girara_session_private_t* priv = session->private_data;
  priv->completion.model         = g_list_store_new(girara_completion_item_get_type());

  girara_command_t* matching_command = NULL;
  for (size_t i = 0; i < girara_list_size(session->bindings.commands); ++i) {
    girara_command_t* command = girara_list_nth(session->bindings.commands, i);
    if (!current_command || g_str_has_prefix(command->command, current_command) ||
        (command->abbr && g_str_has_prefix(command->abbr, current_command))) {
      if (!complete_parameter) {
        completion_append(priv->completion.model, command->command, command->description, false);
      }

      if (!matching_command) {
        matching_command = command;
      }
    }
  }

  if (complete_parameter || g_list_model_get_n_items(G_LIST_MODEL(priv->completion.model)) == 1) {
    g_list_store_remove_all(priv->completion.model);
    if (!matching_command) {
      return;
    }

    if (!matching_command->completion) {
      completion_append(priv->completion.model, matching_command->command, matching_command->description, false);
    } else {
      priv->completion.command_mode     = false;
      priv->completion.previous_command = g_strdup(matching_command->command);

      g_autoptr(girara_completion_t) result = matching_command->completion(session, parameter ? parameter : "");
      if (!result) {
        return;
      }

      for (size_t i = 0; i < girara_list_size(result->groups); ++i) {
        girara_completion_group_t* group = girara_list_nth(result->groups, i);
        if (!girara_list_size(group->elements)) {
          continue;
        }
        if (group->value) {
          completion_append(priv->completion.model, group->value, NULL, true);
        }

        for (size_t j = 0; j < girara_list_size(group->elements); ++j) {
          girara_completion_element_t* element = girara_list_nth(group->elements, j);
          completion_append(priv->completion.model, element->value, element->description, false);
        }
      }
    }
  }
}

static void completion_cycle(girara_session_t* session, int direction) {
  girara_session_private_t* priv = session->private_data;
  GListModel* model              = G_LIST_MODEL(priv->completion.model);
  const unsigned int count       = g_list_model_get_n_items(model);
  const bool forward             = direction == GIRARA_NEXT || direction == GIRARA_NEXT_GROUP;

  unsigned int position = gtk_single_selection_get_selected(priv->completion.selection);
  bool skip_group =
      position != GTK_INVALID_LIST_POSITION && (direction == GIRARA_NEXT_GROUP || direction == GIRARA_PREVIOUS_GROUP);
  if (skip_group) {
    /* Group shortcuts also work for completions without headings. */
    skip_group = false;
    for (unsigned int i = 0; i < count; ++i) {
      g_autoptr(GObject) item_object = g_list_model_get_item(model, i);
      GiraraCompletionItem* item     = GIRARA_COMPLETION_ITEM(item_object);
      if (item->group) {
        skip_group = true;
        break;
      }
    }
  }

  if (position == GTK_INVALID_LIST_POSITION) {
    position = forward ? count - 1 : 0;
  }

  for (unsigned int i = 0; i < count; ++i) {
    position = forward ? (position + 1) % count : (position + count - 1) % count;

    g_autoptr(GObject) item_object = g_list_model_get_item(model, position);
    GiraraCompletionItem* item     = GIRARA_COMPLETION_ITEM(item_object);
    if (item->group) {
      skip_group = false;
    } else if (!skip_group) {
      gtk_single_selection_set_selected(priv->completion.selection, position);
      return;
    }
  }
}

bool girara_isc_completion(girara_session_t* session, girara_argument_t* argument, girara_event_t* UNUSED(event),
                           unsigned int UNUSED(t)) {
  g_return_val_if_fail(session != NULL, false);
  g_return_val_if_fail(argument != NULL, false);

  if (argument->n == GIRARA_HIDE) {
    girara_completion_clear(session);
    return false;
  }

  girara_session_private_t* priv = session->private_data;
  GtkEditable* entry             = GTK_EDITABLE(girara_inputbar_get_entry(GIRARA_INPUTBAR(session->gtk.inputbar)));
  g_autofree char* input         = g_strdup(gtk_editable_get_text(entry));
  if (input[0] != ':') {
    return false;
  }

  if (!priv->completion.model || g_strcmp0(input, priv->completion.previous_input) != 0 ||
      g_list_model_get_n_items(G_LIST_MODEL(priv->completion.model)) == 1) {
    girara_completion_clear(session);

    g_auto(GStrv) elements = NULL;
    int n_parameters       = 0;
    if (input[1] && !g_shell_parse_argv(input + 1, &n_parameters, &elements, NULL)) {
      return false;
    }

    const char* command           = n_parameters ? elements[0] : NULL;
    const char* parameter         = n_parameters > 1 ? elements[1] : NULL;
    const bool complete_parameter = n_parameters > 1 || (n_parameters == 1 && g_str_has_suffix(input, " "));
    completion_build(session, command, parameter, complete_parameter);

    if (!g_list_model_get_n_items(G_LIST_MODEL(priv->completion.model))) {
      girara_completion_clear(session);
      return false;
    }

    completion_view_create(session);
  }

  completion_cycle(session, argument->n);
  return false;
}
