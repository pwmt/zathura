/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_STATUSBAR_H
#define GIRARA_STATUSBAR_H

#include "types.h"
#include <gtk/gtk.h>

/**
 * Function declaration for a statusbar event callback
 *
 * @param widget The statusbar item
 * @param event The occurred event
 * @param session The current girara session
 * @return TRUE No error occurred
 * @return FALSE Error occurred (and forward event)
 */
typedef bool (*girara_statusbar_event_t)(GtkWidget* widget, GdkEvent* event, girara_session_t* session);

/**
 * Creates an statusbar item
 *
 * @param session The used girara session
 * @param expand Expand attribute
 * @param fill Fill attribute
 * @param left True if it should be aligned to the left
 * @param callback Function that gets executed when an event occurs
 * @return The created statusbar item
 * @return NULL An error occurred
 */
girara_statusbar_item_t* girara_statusbar_item_add(girara_session_t* session, bool expand, bool fill, bool left,
                                                   girara_statusbar_event_t callback);

/**
 * Sets the shown text of an statusbar item
 *
 * @param session The used girara session
 * @param item The statusbar item
 * @param text Text that should be displayed
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
bool girara_statusbar_item_set_text(girara_session_t* session, girara_statusbar_item_t* item, const char* text);

#endif
