/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_STATUSBAR_H
#define GIRARA_STATUSBAR_H

#include "types.h"
#include <gtk/gtk.h>

/**
 * Creates an statusbar item
 *
 * @param session The used girara session
 * @param expand Expand attribute
 * @param fill Fill attribute
 * @param left True if it should be aligned to the left
 * @return The created statusbar item
 * @return NULL An error occurred
 */
girara_statusbar_item_t* girara_statusbar_item_add(girara_session_t* session, bool expand, bool fill, bool left);

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
