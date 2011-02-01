/* See LICENSE file for license and copyright information */

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gtk/gtk.h>
#include <girara.h>

/**
 * Quits the current zathura session
 *
 * @param widget The gtk window of zathura
 * @param data NULL
 * @return TRUE
 */
gboolean cb_destroy(GtkWidget* widget, gpointer data);

/**
 * This function gets called when the buffer of girara changes
 *
 * @param session The girara session
 */
void buffer_changed(girara_session_t* session);

/**
 * This function gets called when the the value of the vertical scrollbars
 * changes (e.g.: by scrolling, moving to another page)
 *
 * @param adjustment The vadjustment of the page view
 * @param data NULL
 */
void cb_view_vadjustment_value_changed(GtkAdjustment *adjustment, gpointer data);

#endif // CALLBACKS_H
