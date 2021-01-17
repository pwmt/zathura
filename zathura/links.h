/* SPDX-License-Identifier: Zlib */

#ifndef LINK_H
#define LINK_H

#include "types.h"

#include <gtk/gtk.h>

/**
 * Creates a new zathura link
 *
 * @param type Type of the link
 * @param position Position of the link
 * @param target Target
 * @return New zathura link
 */
ZATHURA_PLUGIN_API zathura_link_t*
zathura_link_new(zathura_link_type_t type, zathura_rectangle_t position,
    zathura_link_target_t target);

/**
 * Free link
 *
 * @param link The link
 */
ZATHURA_PLUGIN_API void zathura_link_free(zathura_link_t* link);

/**
 * Returns the type of the link
 *
 * @param link The link
 * @return The target type of the link
 */
ZATHURA_PLUGIN_API zathura_link_type_t zathura_link_get_type(zathura_link_t* link);

/**
 * Returns the position of the link
 *
 * @param link The link
 * @return The position of the link
 */
ZATHURA_PLUGIN_API zathura_rectangle_t zathura_link_get_position(zathura_link_t* link);

/**
 * The target value of the link
 *
 * @param link The link
 * @return Returns the target of the link (depends on the link type)
 */
ZATHURA_PLUGIN_API zathura_link_target_t zathura_link_get_target(zathura_link_t* link);

/**
 * Evaluate link
 *
 * @param zathura Zathura instance
 * @param link The link
 */
void zathura_link_evaluate(zathura_t* zathura, zathura_link_t* link);

/**
 * Display a link using girara_notify
 *
 * @param zathura Zathura instance
 * @param link The link
 */
void zathura_link_display(zathura_t* zathura, zathura_link_t* link);

/**
 * Copy a link into the clipboard using and display it using girara_notify
 *
 * @param zathura Zathura instance
 * @param link The link
 * @param selection target clipboard
 */
void zathura_link_copy(zathura_t* zathura, zathura_link_t* link, GdkAtom* selection);

#endif // LINK_H
