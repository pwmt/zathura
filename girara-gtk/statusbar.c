/* SPDX-License-Identifier: Zlib */

#include "statusbar.h"

#include <girara/datastructures.h>

#include "internal.h"
#include "settings.h"

struct _GiraraStatusbar {
  GtkBox parent_instance;
  GtkBox* entries;
  girara_list_t* statusbar_items;
};

G_DEFINE_TYPE(GiraraStatusbar, girara_statusbar, GTK_TYPE_BOX)

static void girara_statusbar_finalize(GObject* object) {
  GiraraStatusbar* statusbar = GIRARA_STATUSBAR(object);
  girara_list_free(statusbar->statusbar_items);

  G_OBJECT_CLASS(girara_statusbar_parent_class)->finalize(object);
}

static void girara_statusbar_class_init(GiraraStatusbarClass* klass) {
  G_OBJECT_CLASS(klass)->finalize = girara_statusbar_finalize;
}

static void girara_statusbar_init(GiraraStatusbar* statusbar) {
  statusbar->statusbar_items = girara_list_new_with_free(g_free);
  gtk_orientable_set_orientation(GTK_ORIENTABLE(statusbar), GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_add_css_class(GTK_WIDGET(statusbar), "statusbar");
  statusbar->entries = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));
  gtk_widget_add_css_class(GTK_WIDGET(statusbar->entries), "bottom_box");
  gtk_box_append(GTK_BOX(statusbar), GTK_WIDGET(statusbar->entries));
}

GtkWidget* girara_statusbar_new(void) {
  return g_object_new(GIRARA_TYPE_STATUSBAR, NULL);
}

girara_statusbar_item_t* girara_statusbar_item_add(GiraraStatusbar* statusbar, bool expand, bool UNUSED(fill),
                                                   bool left) {
  g_return_val_if_fail(GIRARA_IS_STATUSBAR(statusbar), NULL);

  girara_statusbar_item_t* item = g_malloc0(sizeof(girara_statusbar_item_t));

  item->text = GTK_LABEL(gtk_label_new(NULL));

  /* set style */
  widget_add_class(GTK_WIDGET(item->text), "statusbar-item");
  widget_add_class(GTK_WIDGET(item->text), "statusbar");

  /* set properties */
  gtk_widget_set_halign(GTK_WIDGET(item->text), left ? GTK_ALIGN_START : GTK_ALIGN_END);
  gtk_widget_set_valign(GTK_WIDGET(item->text), GTK_ALIGN_CENTER);
  gtk_label_set_use_markup(item->text, TRUE);

  /* add ellipsis if item is on the left side */
  if (left) {
    gtk_label_set_ellipsize(item->text, PANGO_ELLIPSIZE_END);
  }

  /* add it to the list */
  gtk_widget_set_hexpand(GTK_WIDGET(item->text), expand);
  gtk_box_append(statusbar->entries, GTK_WIDGET(item->text));
  gtk_widget_set_visible(GTK_WIDGET(item->text), TRUE);

  girara_list_append(statusbar->statusbar_items, item);
  return item;
}

bool girara_statusbar_item_set_text(girara_statusbar_item_t* item, const char* text) {
  g_return_val_if_fail(item != NULL, false);

  g_autofree char* escaped_text = g_markup_escape_text(text, -1);
  gtk_label_set_markup(item->text, escaped_text);

  return true;
}
