/* SPDX-License-Identifier: Zlib */

#ifndef HIGHLIGHTS_H
#define HIGHLIGHTS_H

#include <stdbool.h>
#include "zathura.h"

struct zathura_highlight_s {
  gchar* id;                 /**< Unique identifier */
  unsigned int page;         /**< Page index (0-based) the highlight belongs to */
  girara_list_t* rectangles; /**< List of zathura_rectangle_t*, one per line of highlighted text */
  GdkRGBA color;             /**< Fill color */
  gchar* text;               /**< Plain text covered by the highlight, cached at creation time */
  gint64 created;            /**< Creation time (microseconds since the epoch) */
};

typedef struct zathura_highlight_s zathura_highlight_t;

/**
 * Create a highlight and add it to the list of highlights. Takes ownership of
 * \p rectangles.
 *
 * @param zathura The zathura instance.
 * @param page The page the highlight belongs to (0-based).
 * @param rectangles List of zathura_rectangle_t* covered by the highlight.
 * @param color The highlight's color.
 * @param text The text covered by the highlight (may be NULL).
 * @return the highlight instance or NULL on failure.
 */
zathura_highlight_t* zathura_highlight_add(zathura_t* zathura, unsigned int page, girara_list_t* rectangles,
                                           GdkRGBA color, const gchar* text);

/**
 * Remove a highlight from the list of highlights.
 * @param zathura The zathura instance.
 * @param id The highlight's id (or an unambiguous prefix of it).
 * @param page Optional, set to the page the removed highlight belonged to.
 * @return true on success, false otherwise
 */
bool zathura_highlight_remove(zathura_t* zathura, const gchar* id, unsigned int* page);

/**
 * Get the list of highlights that belong to the given page.
 * @param zathura The zathura instance.
 * @param page The page (0-based).
 * @return A new (non-owning) list of zathura_highlight_t* or NULL on failure.
 */
girara_list_t* zathura_highlight_get_for_page(zathura_t* zathura, unsigned int page);

/**
 * Initialize highlight system
 * @param zathura The zathura instance.
 */
bool zathura_highlights_init(zathura_t* zathura);

/**
 * Load highlights for a specific file.
 * @param zathura The zathura instance.
 * @param file The file.
 * @return true on success, false otherwise
 */
bool zathura_highlights_load(zathura_t* zathura, const gchar* file);

/**
 * Free highlight system
 * @param zathura The zathura instance.
 */
void zathura_highlights_free(zathura_t* zathura);

/**
 * Get the currently active highlight color from the palette.
 * @param zathura The zathura instance.
 * @return The active color.
 */
GdkRGBA zathura_highlight_get_active_color(zathura_t* zathura);

/**
 * Cycle the active highlight color forward through the palette. The 4th
 * (user-defined) slot is skipped until it has actually been set.
 * @param zathura The zathura instance.
 */
void zathura_highlight_cycle_color(zathura_t* zathura);

/**
 * Set the 4th (user-defined) palette slot and make it the active color.
 * @param zathura The zathura instance.
 * @param color The color to store in the user-defined slot.
 */
void zathura_highlight_set_custom_color(zathura_t* zathura, GdkRGBA color);

/**
 * Get a short, human-readable name for the currently active color
 * ("yellow", "green", "red", or "custom").
 * @param zathura The zathura instance.
 * @return The name (not to be freed).
 */
const char* zathura_highlight_get_active_color_name(zathura_t* zathura);

#endif // HIGHLIGHTS_H
