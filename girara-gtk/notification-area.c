/* SPDX-License-Identifier: Zlib */

#include "notification-area.h"

#include <girara/macros.h>

#include "internal.h"

struct _GiraraNotificationArea {
  GtkBox parent_instance;
  GtkLabel* text;
};

G_DEFINE_TYPE(GiraraNotificationArea, girara_notification_area, GTK_TYPE_BOX)

static void girara_notification_area_class_init(GiraraNotificationAreaClass* GIRARA_UNUSED(klass)) {}

static void girara_notification_area_init(GiraraNotificationArea* area) {
  gtk_orientable_set_orientation(GTK_ORIENTABLE(area), GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_add_css_class(GTK_WIDGET(area), "notification");

  GtkWidget* text = gtk_label_new(NULL);
  area->text      = GTK_LABEL(text);
  gtk_label_set_selectable(area->text, TRUE);
  gtk_label_set_ellipsize(area->text, PANGO_ELLIPSIZE_END);
  gtk_label_set_use_markup(area->text, TRUE);
  gtk_widget_set_halign(text, GTK_ALIGN_START);
  gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class(text, "bottom_box");
  gtk_widget_add_css_class(text, "notification");
  gtk_box_append(GTK_BOX(area), text);
}

GtkWidget* girara_notification_area_new(void) {
  return g_object_new(GIRARA_TYPE_NOTIFICATION_AREA, NULL);
}

void girara_notification_area_set_message(GiraraNotificationArea* area, girara_log_level_t level, const char* message) {
  g_return_if_fail(GIRARA_IS_NOTIFICATION_AREA(area));

  GtkWidget* area_widget = GTK_WIDGET(area);
  GtkWidget* text_widget = GTK_WIDGET(area->text);

  if (level == GIRARA_ERROR) {
    widget_add_class(area_widget, "notification-error");
    widget_add_class(text_widget, "notification-error");
  } else {
    widget_remove_class(area_widget, "notification-error");
    widget_remove_class(text_widget, "notification-error");
  }

  if (level == GIRARA_WARNING) {
    widget_add_class(area_widget, "notification-warning");
    widget_add_class(text_widget, "notification-warning");
  } else {
    widget_remove_class(area_widget, "notification-warning");
    widget_remove_class(text_widget, "notification-warning");
  }

  gtk_label_set_markup(area->text, message);
}
