/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_NOTIFICATION_AREA_H
#define GIRARA_NOTIFICATION_AREA_H

#include <girara/log.h>
#include <gtk/gtk.h>

#define GIRARA_TYPE_NOTIFICATION_AREA (girara_notification_area_get_type())
G_DECLARE_FINAL_TYPE(GiraraNotificationArea, girara_notification_area, GIRARA, NOTIFICATION_AREA, GtkBox)

/**
 * Creates a notification area widget, initially hidden.
 *
 * @return The created notification area widget
 */
GtkWidget* girara_notification_area_new(void);

/**
 * Sets the notification message and its severity styling.
 *
 * @param area The notification area
 * @param level The girara notification level
 * @param message The message as Pango markup
 */
void girara_notification_area_set_message(GiraraNotificationArea* area, girara_log_level_t level, const char* message);

#endif
