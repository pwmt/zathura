/* See LICENSE file for license and copyright information */

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gtk/gtk.h>
#include <girara/types.h>
#include <girara/macros.h>

#include "internal.h"
#include "document.h"
#include "zathura.h"

/**
 * Quits the current zathura session
 *
 * @param widget The gtk window of zathura
 * @param zathura Correspondending zathura session
 * @return true if no error occured and the event has been handled
 */
gboolean cb_destroy(GtkWidget* widget, zathura_t* zathura);

/**
 * This function gets called when the buffer of girara changes
 *
 * @param session The girara session
 */
void cb_buffer_changed(girara_session_t* session);

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
 * @param name The name of the row
 * @param type The settings type
 * @param value The value
 * @param data Custom data
 */
void cb_pages_per_row_value_changed(girara_session_t* session, const char* name,
    girara_setting_type_t type, void* value, void* data);

/**
 * Called when an index element is activated (e.g.: double click)
 *
 * @param tree_view Tree view
 * @param path Path
 * @param column Column
 * @param zathura Zathura session
 */
void cb_index_row_activated(GtkTreeView* tree_view, GtkTreePath* path,
    GtkTreeViewColumn* column, void* zathura);

/**
 * Called when input has been passed to the sc_follow dialog
 *
 * @param entry The dialog inputbar
 * @param session The girara session
 * @return true if no error occured and the event has been handled
 */
bool cb_sc_follow(GtkEntry* entry, girara_session_t* session);

/**
 * Emitted when file has been changed
 *
 * @param monitor The file monitor
 * @param file The file
 * @param other_file A file or NULL
 * @param event The monitor event
 * @param session The girara session
 */
void cb_file_monitor(GFileMonitor* monitor, GFile* file, GFile* other_file,
    GFileMonitorEvent event, girara_session_t* session);

/**
 * Callback to read new password for file that should be opened
 *
 * @param entry The password entry
 * @param dialog The dialog information
 * @return true if input has been handled
 */
bool cb_password_dialog(GtkEntry* entry, zathura_password_dialog_info_t* dialog);

/**
 * Emitted when the view has been resized
 *
 * @param widget View
 * @param allocation Allocation
 * @param zathura Zathura session
 * @return true if signal has been handled successfully
 */
bool cb_view_resized(GtkWidget* widget, GtkAllocation* allocation, zathura_t* zathura);

/**
 * Emitted when the 'recolor' setting is changed
 *
 * @param session Girara session
 * @param name Name of the setting ("recolor")
 * @param type Type of the setting (BOOLEAN)
 * @param value New value
 * @param data Custom data
 */
void cb_setting_recolor_change(girara_session_t* session, const char* name,
    girara_setting_type_t type, void* value, void* data);

#endif // CALLBACKS_H
