/* SPDX-License-Identifier: Zlib */

#include "entry.h"

#include <girara/macros.h>

G_DEFINE_TYPE(GiraraEntry, girara_entry, GTK_TYPE_ENTRY)

static void girara_entry_paste_primary(GiraraEntry* self);

enum {
  PASTE_PRIMARY,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

static void girara_entry_class_init(GiraraEntryClass* klass) {
  klass->paste_primary = girara_entry_paste_primary;

  signals[PASTE_PRIMARY] = g_signal_new("paste-primary", GIRARA_TYPE_ENTRY, G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                        G_STRUCT_OFFSET(GiraraEntryClass, paste_primary), NULL, NULL,
                                        g_cclosure_marshal_generic, G_TYPE_NONE, 0);

  GtkBindingSet* binding_set = gtk_binding_set_by_class(klass);
  gtk_binding_entry_add_signal(binding_set, GDK_KEY_Insert, GDK_SHIFT_MASK, "paste-primary", 0);
}

static void girara_entry_init(GiraraEntry* GIRARA_UNUSED(self)) {}

GiraraEntry* girara_entry_new(void) {
  GObject* ret = g_object_new(GIRARA_TYPE_ENTRY, NULL);
  if (ret == NULL) {
    return NULL;
  }

  return GIRARA_ENTRY(ret);
}

static void girara_entry_paste_primary(GiraraEntry* self) {
  g_auto(GValue) editable = G_VALUE_INIT;
  g_value_init(&editable, G_TYPE_BOOLEAN);

  g_object_get_property(G_OBJECT(self), "editable", &editable);

  if (g_value_get_boolean(&editable) == TRUE) {
    gchar* text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY));

    if (text != NULL) {
      int pos = gtk_editable_get_position(GTK_EDITABLE(self));

      gtk_editable_insert_text(GTK_EDITABLE(self), text, -1, &pos);
      gtk_editable_set_position(GTK_EDITABLE(self), pos);
    }
  } else {
    gtk_widget_error_bell(GTK_WIDGET(self));
  }
}
