/* SPDX-License-Identifier: Zlib */

#include "inputbar.h"

#include <girara/macros.h>
#include <girara/input-history.h>
#include <girara/datastructures.h>
#include <girara/log.h>
#include <string.h>

#include "commands.h"
#include "internal.h"
#include "session.h"
#include "shortcuts.h"

typedef struct {
  GtkEntry* entry;
} GiraraInputbarPrivate;

// TODO: decide if some of command history handling, command execution and so on should be handled here or in session

G_DEFINE_TYPE_WITH_PRIVATE(GiraraInputbar, girara_inputbar, GTK_TYPE_BOX)

static guint abort_signal;

static gboolean grab_focus(GtkWidget* widget) {
  GiraraInputbar* inputbar    = GIRARA_INPUTBAR(widget);
  GiraraInputbarPrivate* priv = girara_inputbar_get_instance_private(inputbar);

  girara_debug("focusing inputbar");

  // focus the entry
  if (!gtk_widget_grab_focus(GTK_WIDGET(priv->entry))) {
    return GTK_WIDGET_CLASS(girara_inputbar_parent_class)->grab_focus(widget);
  }

  return TRUE;
}

static void girara_inputbar_class_init(GiraraInputbarClass* klass) {
  GTK_WIDGET_CLASS(klass)->grab_focus = grab_focus;

  abort_signal =
      g_signal_new("abort", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void girara_inputbar_init(GiraraInputbar* inputbar) {
  GiraraInputbarPrivate* priv = girara_inputbar_get_instance_private(inputbar);
  gtk_orientable_set_orientation(GTK_ORIENTABLE(inputbar), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing(GTK_BOX(inputbar), 5);
  gtk_widget_add_css_class(GTK_WIDGET(inputbar), "inputbar");

  GtkWidget* entry = gtk_entry_new();
  priv->entry      = GTK_ENTRY(entry);
  gtk_entry_set_has_frame(priv->entry, FALSE);
  gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_widget_add_css_class(entry, "bottom_box");
  gtk_widget_add_css_class(entry, "inputbar");
  gtk_box_append(GTK_BOX(inputbar), entry);
}

GtkEntry* girara_inputbar_get_entry(GiraraInputbar* inputbar) {
  g_return_val_if_fail(GIRARA_IS_INPUTBAR(inputbar), NULL);
  GiraraInputbarPrivate* priv = girara_inputbar_get_instance_private(inputbar);
  return priv->entry;
}

gboolean girara_process_inputbar_key(girara_session_t* session, guint keyval, guint clean) {
  g_return_val_if_fail(session != NULL, FALSE);

  for (size_t idx = 0; idx != girara_list_size(session->bindings.inputbar_shortcuts); ++idx) {
    girara_inputbar_shortcut_t* inputbar_shortcut = girara_list_nth(session->bindings.inputbar_shortcuts, idx);
    if (inputbar_shortcut->key == keyval && inputbar_shortcut->mask == clean) {
      girara_debug("found shortcut for key %u and mask %x", keyval, clean);
      if (inputbar_shortcut->function) {
        inputbar_shortcut->function(session, &(inputbar_shortcut->argument), NULL, 0);
      }

      return TRUE;
    }
  }

  return FALSE;
}

static gboolean inputbar_activate(GtkEntry* entry, girara_session_t* session) {
  g_return_val_if_fail(session != NULL, FALSE);

  girara_debug("activating inputbar");

  g_autofree gchar* input = gtk_editable_get_chars(GTK_EDITABLE(entry), 1, -1);
  if (!input || !strlen(input)) {
    g_signal_emit(session->gtk.inputbar, abort_signal, 0);
    return FALSE;
  }

  /* append to command history */
  const char* command = gtk_editable_get_text(GTK_EDITABLE(entry));
  girara_input_history_append(session->command_history, command);

  /* special commands */
  g_autofree char* identifier_s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, 1);
  if (identifier_s == NULL) {
    return FALSE;
  }

  const char identifier = identifier_s[0];
  girara_debug("Processing special command with identifier '%c'.", identifier);
  for (size_t idx = 0; idx != girara_list_size(session->bindings.special_commands); ++idx) {
    girara_special_command_t* special_command = girara_list_nth(session->bindings.special_commands, idx);
    if (special_command->identifier == identifier) {
      girara_debug("Found special command.");
      if (special_command->always != true) {
        special_command->function(session, input, &special_command->argument);
      }

      g_signal_emit(session->gtk.inputbar, abort_signal, 0);
      return TRUE;
    }
  }

  return girara_command_run(session, input);
}

static gboolean inputbar_key_press_event(GtkEventControllerKey* controller, guint keyval_in, guint UNUSED(keycode),
                                         GdkModifierType state, girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  guint keyval = keyval_in;
  guint clean  = 0;
  if (!girara_clean_key_mask(controller, state, &clean, &keyval)) {
    girara_debug("clean_mask returned false.");
    return false;
  }
  girara_debug("Proccessing key %u with mask %x.", keyval, clean);

  if (girara_process_inputbar_key(session, keyval, clean)) {
    return TRUE;
  }

  if (session->gtk.results && gtk_widget_get_visible(GTK_WIDGET(session->gtk.results)) && (keyval == GDK_KEY_space)) {
    gtk_widget_set_visible(GTK_WIDGET(session->gtk.results), FALSE);
  }

  return FALSE;
}

static gboolean inputbar_changed_event(GtkEditable* entry, girara_session_t* session) {
  g_return_val_if_fail(session != NULL, FALSE);

  /* special commands */
  g_autofree char* identifier_s = gtk_editable_get_chars(entry, 0, 1);
  if (!identifier_s) {
    return FALSE;
  }

  char identifier = identifier_s[0];
  for (size_t idx = 0; idx != girara_list_size(session->bindings.special_commands); ++idx) {
    girara_special_command_t* special_command = girara_list_nth(session->bindings.special_commands, idx);
    if ((special_command->identifier == identifier) && (special_command->always == true)) {
      g_autofree gchar* input = gtk_editable_get_chars(GTK_EDITABLE(entry), 1, -1);
      special_command->function(session, input, &(special_command->argument));
      return TRUE;
    }
  }

  return FALSE;
}

GtkWidget* girara_inputbar_new(girara_session_t* session) {
  GiraraInputbar* inputbar = g_object_new(GIRARA_TYPE_INPUTBAR, NULL);

  GiraraInputbarPrivate* priv = girara_inputbar_get_instance_private(inputbar);

  GtkEventController* ib_key = gtk_event_controller_key_new();
  g_signal_connect(ib_key, "key-pressed", G_CALLBACK(inputbar_key_press_event), session);
  gtk_widget_add_controller(GTK_WIDGET(priv->entry), ib_key);

  g_signal_connect(G_OBJECT(priv->entry), "changed", G_CALLBACK(inputbar_changed_event), session);
  g_signal_connect(G_OBJECT(priv->entry), "activate", G_CALLBACK(inputbar_activate), session);

  return GTK_WIDGET(inputbar);
}