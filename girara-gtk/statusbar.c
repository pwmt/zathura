/* SPDX-License-Identifier: Zlib */

#include "statusbar.h"

struct _GiraraStatusbar {
  GtkBox parent_instance;
  GtkBox* entries;
};

G_DEFINE_TYPE(GiraraStatusbar, girara_statusbar, GTK_TYPE_BOX)

static void girara_statusbar_class_init(GiraraStatusbarClass* GIRARA_UNUSED(klass)) {}

static void girara_statusbar_init(GiraraStatusbar* statusbar) {
  gtk_orientable_set_orientation(GTK_ORIENTABLE(statusbar), GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_add_css_class(GTK_WIDGET(statusbar), "statusbar");
  statusbar->entries = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
  gtk_widget_add_css_class(GTK_WIDGET(statusbar->entries), "bottom_box");
  gtk_box_append(GTK_BOX(statusbar), GTK_WIDGET(statusbar->entries));
}

GtkWidget* girara_statusbar_new(void) {
  return g_object_new(GIRARA_TYPE_STATUSBAR, NULL);
}

GtkLabel* girara_statusbar_item_add(GiraraStatusbar* statusbar, bool expand, bool left) {
  g_return_val_if_fail(GIRARA_IS_STATUSBAR(statusbar), NULL);

  GtkWidget* item = gtk_label_new(NULL);
  GtkLabel* label = GTK_LABEL(item);

  /* set style */
  gtk_widget_add_css_class(item, "statusbar-item");
  gtk_widget_add_css_class(item, "statusbar");

  /* set properties */
  gtk_widget_set_halign(item, left ? GTK_ALIGN_START : GTK_ALIGN_END);
  gtk_widget_set_valign(item, GTK_ALIGN_CENTER);
  gtk_label_set_use_markup(label, TRUE);

  /* add ellipsis if item is on the left side */
  if (left) {
    gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_END);
  }

  /* add it to the statusbar */
  gtk_widget_set_hexpand(item, expand);
  gtk_box_append(statusbar->entries, item);
  gtk_widget_set_visible(item, TRUE);

  return label;
}

bool girara_statusbar_item_set_text(GtkLabel* item, const char* text) {
  g_return_val_if_fail(GTK_IS_LABEL(item), false);

  g_autofree char* escaped_text = g_markup_escape_text(text, -1);
  gtk_label_set_markup(item, escaped_text);

  return true;
}
