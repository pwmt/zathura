/* See LICENSE file for license and copyright information */

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gtk/gtk.h>
#include <girara.h>

#include "zathura.h"

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
 * This function gets called when the value of the vertical scrollbars
 * changes (e.g.: by scrolling, moving to another page)
 *
 * @param adjustment The vadjustment of the page view
 * @param data NULL
 */
void cb_view_vadjustment_value_changed(GtkAdjustment *adjustment, gpointer data);
/**
 * This function gets called when the value of the "pages-per-row"
 * variable changes
 *
 * @param session The current girara session
 * @param setting The "pages-per-row" setting
 */
void cb_pages_per_row_value_changed(girara_session_t* session, girara_setting_t* setting);

/**
 * Called when an index element is activated (e.g.: double click)
 *
 * @param tree_view Tree view
 * @param path Path
 * @param column Column
 * @param zathura Zathura session
 */
void cb_index_row_activated(GtkTreeView* tree_view, GtkTreePath* path,
    GtkTreeViewColumn* column, zathura_t* zathura);

#endif // CALLBACKS_H
