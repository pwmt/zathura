/* See LICENSE file for license and copyright information */

#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <stdbool.h>
#include "zathura.h"

struct zathura_bookmark_s
{
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
 * Free a bookmark instance.
 * @param bookmark The bookmark instance.
 */
void zathura_bookmark_free(zathura_bookmark_t* bookmark);

/**
 * Load bookmarks for a specific file.
 * @param zathura The zathura instance.
 * @param file The file.
 * @return true on success, false otherwise
 */
bool zathura_bookmarks_load(zathura_t* zathura, const gchar* file);

/**
 * Compare two bookmarks.
 * @param lhs a bookmark
 * @param rhs a bookmark
 * @returns g_strcmp0(lhs->id, rhs->id)
 */
int zathura_bookmarks_compare(const zathura_bookmark_t* lhs, const zathura_bookmark_t* rhs);

#endif // BOOKMARKS_H
