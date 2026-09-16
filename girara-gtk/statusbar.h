/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_STATUSBAR_H
#define GIRARA_STATUSBAR_H

#include "types.h"
#include <gtk/gtk.h>

#define GIRARA_TYPE_STATUSBAR (girara_statusbar_get_type())
G_DECLARE_FINAL_TYPE(GiraraStatusbar, girara_statusbar, GIRARA, STATUSBAR, GtkBox)

/**
 * Creates a statusbar widget.
 *
 * @return The created statusbar widget
 */
GtkWidget* girara_statusbar_new(void);

/**
 * Creates an statusbar item
 *
 * @param statusbar The statusbar widget
 * @param expand Expand attribute
 * @param left True if it should be aligned to the left
 * @return The created label, owned by the statusbar
 * @return NULL An error occurred
 */
GtkLabel* girara_statusbar_item_add(GiraraStatusbar* statusbar, bool expand, bool left);

/**
 * Sets the shown text of an statusbar item
 *
 * @param item The statusbar item
 * @param text Text that should be displayed
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
bool girara_statusbar_item_set_text(GtkLabel* item, const char* text);

#endif
