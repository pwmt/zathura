/* SPDX-License-Identifier: Zlib */

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
 * @return true if no error occurred and the event has been handled
 */
gboolean cb_destroy(GtkWidget* widget, zathura_t* zathura);

/**
 * This function gets called when the buffer of girara changes
 *
 * @param session The girara session
 */
void cb_buffer_changed(girara_session_t* session);

/**
 * This function gets called when the value of the horizontal scrollbars
 * changes (e.g.: by scrolling, moving to another page)
 *
 * @param adjustment The hadjustment of the page view
 * @param data NULL
 */
void cb_view_hadjustment_value_changed(GtkAdjustment *adjustment, gpointer data);

/**
 * This function gets called when the value of the vertical scrollbars
 * changes (e.g.: by scrolling, moving to another page)
 *
 * @param adjustment The vadjustment of the page view
 * @param data NULL
 */
void cb_view_vadjustment_value_changed(GtkAdjustment *adjustment, gpointer data);

/**
 * This function gets called when the bounds or the page_size of the horizontal
 * scrollbar change (e.g. when the zoom level is changed).
 *
 * It adjusts the value of the horizontal scrollbar
 *
 * @param adjustment The horizontal adjustment of a gtkScrolledWindow
 * @param data The zathura instance
 */
void cb_view_hadjustment_changed(GtkAdjustment *adjustment, gpointer data);

/**
 * This function gets called when the bounds or the page_size of the vertical
 * scrollbar change (e.g. when the zoom level is changed).
 *
 * It adjusts the value of the vertical scrollbar based on its previous
 * adjustment, stored in the tracking adjustment zathura->ui.hadjustment.
 *
 * @param adjustment The vertical adjustment of a gtkScrolledWindow
 * @param data The zathura instance
 */
void cb_view_vadjustment_changed(GtkAdjustment *adjustment, gpointer data);

/**
 * This function gets called when the program need to refresh the document view.
 *
 * It adjusts the value of the scrollbars, triggering a redraw in the new
 * position.
 *
 * @param view The view GtkWidget
 * @param data The zathura instance
 */
void cb_refresh_view(GtkWidget* view, gpointer data);

/**
 * This function gets called when the monitors associated with the GdkScreen
 * change.
 *
 * It checks for a change of monitor PPI, storing the new value and triggering
 * a refresh if appropriate.
 *
 * @param screen The GDK screen
 * @param gpointer The zathura instance
 */
void cb_monitors_changed(GdkScreen* screen, gpointer data);

/**
 * This function gets called when the screen associated with the view widget
 * changes.
 *
 * It updates the connection on the monitors-changed signal and checks for a
 * change of monitor PPI, storing the new value and triggering a refresh if
 * appropriate.
 *
 * @param widget The view widget
 * @param previous_screen The widget's previous screen
 * @param gpointer The zathura instance
 */
void cb_widget_screen_changed(GtkWidget* widget, GdkScreen* previous_screen, gpointer data);

/**
 * This function gets called when the main window's size, position or stacking
 * changes.
 *
 * It checks for a change of monitor PPI (due to the window moving between
 * different monitors), storing the new value and triggering a refresh if
 * appropriate.
 *
 * @param widget The main window widget
 * @param event The configure event
 * @param gpointer The zathura instance
 * @return true if no error occurred and the event has been handled
 */
gboolean cb_widget_configured(GtkWidget* widget, GdkEvent* event, gpointer data);

/**
 * This function gets called when the view widget scale factor changes (e.g.
 * when moving from a regular to a HiDPI screen).
 *
 * It records the new value and triggers a re-rendering of the document.
 *
 * @param object The view widget
 * @param pspec The GParamSpec for the scale-factor property
 * @param gpointer The zathura instance
 */
void cb_scale_factor(GObject* object, GParamSpec* pspec, gpointer data);

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
void cb_page_layout_value_changed(girara_session_t* session, const char* name,
    girara_setting_type_t type, const void* value, void* data);

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
 * @return true if no error occurred and the event has been handled
 */
gboolean cb_sc_follow(GtkEntry* entry, void* session);

/**
 * Called when input has been passed to the sc_display_link dialog
 *
 * @param entry The dialog inputbar
 * @param session The girara session
 * @return true if no error occurred and the event has been handled
 */
gboolean cb_sc_display_link(GtkEntry* entry, void* session);

/**
 * Called when input has been passed to the sc_copy_link dialog
 *
 * @param entry The dialog inputbar
 * @param session The girara session
 * @return true if no error occurred and the event has been handled
 */
gboolean cb_sc_copy_link(GtkEntry* entry, void* session);

/**
 * Emitted when file has been changed
 *
 * @param monitor The file monitor
 * @param session The girara session
 */
void cb_file_monitor(ZathuraFileMonitor* monitor, girara_session_t* session);

/**
 * Callback to read new password for file that should be opened
 *
 * @param entry The password entry
 * @param dialog The dialog information
 * @return true if input has been handled
 */
gboolean cb_password_dialog(GtkEntry* entry, void* dialog);

/**
 * Emitted when the view has been resized
 *
 * @param widget View
 * @param allocation Allocation
 * @param zathura Zathura session
 * @return true if signal has been handled successfully
 */
gboolean cb_view_resized(GtkWidget* widget, GtkAllocation* allocation, zathura_t* zathura);

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
    girara_setting_type_t type, const void* value, void* data);

/**
 * Emitted when the 'recolor-keephue' setting is changed
 *
 * @param session Girara session
 * @param name Name of the setting ("recolor")
 * @param type Type of the setting (BOOLEAN)
 * @param value New value
 * @param data Custom data
 */
void cb_setting_recolor_keep_hue_change(girara_session_t* session, const char* name,
    girara_setting_type_t type, const void* value, void* data);

/**
 * Emitted when the 'recolor-reverse-video' setting is changed
 *
 * @param session Girara session
 * @param name Name of the setting ("recolor")
 * @param type Type of the setting (BOOLEAN)
 * @param value New value
 * @param data Custom data
 */
void cb_setting_recolor_keep_reverse_video_change(girara_session_t* session, 
    const char* name, girara_setting_type_t type, const void* value, void* data);

/**
 * Unknown command handler which is used to handle the strict numeric goto
 * command
 *
 * @param session The girara session
 * @param input The command input
 * @return true if the input has been handled
 */
bool cb_unknown_command(girara_session_t* session, const char* input);

/**
 * Emitted when text has been selected in the page widget
 *
 * @param page page view widget
 * @param text selected text
 * @param data user data
 */
void cb_page_widget_text_selected(ZathuraPage* page, const char* text,
    void* data);

void cb_page_widget_image_selected(ZathuraPage* page, GdkPixbuf* pixbuf,
    void* data);

void cb_page_widget_scaled_button_release(ZathuraPage* page,
    GdkEventButton* event, void* data);

void cb_page_widget_link(ZathuraPage* page, void* data);

void update_visible_pages(zathura_t* zathura);

/**
 * Update window icon from cairo surface.
 */
void cb_window_update_icon(ZathuraRenderRequest* request, cairo_surface_t* surface, void* data);

void cb_gesture_zoom_begin(GtkGesture* self, GdkEventSequence* sequence, void* data);

void cb_gesture_zoom_scale_changed(GtkGestureZoom* self, gdouble scale, void* data);

/**
 * Clears all highlighted links when the inputbar gets closed
 *
 * @param GtkWidget* Inputbar widget
 * @param data The zathura instance
 */
void cb_hide_links(GtkWidget* widget, gpointer data);

#endif // CALLBACKS_H
