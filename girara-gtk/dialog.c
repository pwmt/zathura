/* SPDX-License-Identifier: Zlib */

#include "dialog.h"

#include "internal.h"
#include "shortcuts.h"

struct _GiraraDialog {
  GiraraInputbar parent_instance;
  GtkLabel* prompt;
  girara_session_t* session;
};

G_DEFINE_TYPE(GiraraDialog, girara_dialog, GIRARA_TYPE_INPUTBAR)

static guint activate_signal;

static void dialog_activate(GtkEntry* entry, GiraraDialog* dialog) {
  gboolean handled      = FALSE;
  g_autofree char* text = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry)));
  g_signal_emit(dialog, activate_signal, 0, text, &handled);

  // hide the dialog as we are done
  gtk_widget_set_visible(GTK_WIDGET(dialog), FALSE);
}

static gboolean dialog_key_press(GtkEventControllerKey* controller, guint keyval, guint UNUSED(keycode),
                                 GdkModifierType state, GiraraDialog* dialog) {
  guint clean = 0;
  if (!girara_clean_key_mask(controller, state, &clean, &keyval)) {
    return FALSE;
  }
  return girara_process_inputbar_key(dialog->session, keyval, clean);
}

static void girara_dialog_class_init(GiraraDialogClass* klass) {
  activate_signal = g_signal_new("activate", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                                 g_signal_accumulator_true_handled, NULL, NULL, G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
}

static void girara_dialog_init(GiraraDialog* dialog) {
  GtkEntry* entry = girara_inputbar_get_entry(GIRARA_INPUTBAR(dialog));
  g_signal_connect(entry, "activate", G_CALLBACK(dialog_activate), dialog);
  GtkEventController* key = gtk_event_controller_key_new();
  g_signal_connect(key, "key-pressed", G_CALLBACK(dialog_key_press), dialog);
  gtk_widget_add_controller(GTK_WIDGET(entry), key);

  dialog->prompt = GTK_LABEL(gtk_label_new(NULL));
  gtk_widget_add_css_class(GTK_WIDGET(dialog->prompt), "inputbar");
  gtk_box_prepend(GTK_BOX(dialog), GTK_WIDGET(dialog->prompt));
}

GiraraDialog* girara_dialog_new(girara_session_t* session, const char* prompt, bool invisible) {
  g_return_val_if_fail(session != NULL, NULL);

  GiraraDialog* dialog = g_object_new(GIRARA_TYPE_DIALOG, NULL);
  dialog->session      = session;
  gtk_label_set_markup(dialog->prompt, prompt ? prompt : "");
  gtk_entry_set_visibility(girara_inputbar_get_entry(GIRARA_INPUTBAR(dialog)), !invisible);

  return dialog;
}
