/* SPDX-License-Identifier: Zlib */

#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <stdbool.h>
#include "zathura.h"

struct zathura_bookmark_s {
  gchar* id;
  unsigned int page;
  double x;
  double y;
};

typedef struct zathura_bookmark_s zathura_bookmark_t;

/**
 * Create a bookmark and add it to the list of bookmarks.
 * @param zathura The zathura instance.
 * @param id The bookmark's id.
 * @param page The bookmark's page.
 * @return the bookmark instance or NULL on failure.
 */
zathura_bookmark_t* zathura_bookmark_add(zathura_t* zathura, const gchar* id, unsigned int page);

/**
 * Remove a bookmark from the list of bookmarks.
 * @param zathura The zathura instance.
 * @param id The bookmark's id.
 * @return true on success, false otherwise
 */
bool zathura_bookmark_remove(zathura_t* zathura, const gchar* id);

/**
 * Get bookmark from the list of bookmarks.
 * @param zathura The zathura instance.
 * @param id The bookmark's id.
 * @return The bookmark instance if it exists or NULL otherwise.
 */
zathura_bookmark_t* zathura_bookmark_get(zathura_t* zathura, const gchar* id);

/**
 * Initialize bookmark system
 * @param zathura The zathura instance.
 */
bool zathura_bookmarks_init(zathura_t* zathura);

/**
 * Load bookmarks for a specific file.
 * @param zathura The zathura instance.
 * @param file The file.
 * @return true on success, false otherwise
 */
bool zathura_bookmarks_load(zathura_t* zathura, const gchar* file);

/**
 * Free boomark system
 * @param zathura The zathura instance.
 */
void zathura_bookmarks_free(zathura_t* zathura);

#endif // BOOKMARKS_H
