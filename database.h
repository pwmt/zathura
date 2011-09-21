/* See LICENSE file for license and copyright information */

#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>
#include <girara.h>

#include "zathura.h"
#include "bookmarks.h"

/**
 * Initialize database system.
 * @param path Path to the database file.
 * @return A valid zathura_database_t instance or NULL on failure
 */
zathura_database_t* zathura_db_init(const char* path);

/**
 * Free database instance.
 * @param The database instance to free.
 */
void zathura_db_free(zathura_database_t* db);

/**
 * Add or update bookmark in the database.
 * @param db The database instance
 * @param file The file to which the bookmark belongs to.
 * @param bookmark The bookmark instance.
 * @return true on success, false otherwise
 */
bool zathura_db_add_bookmark(zathura_database_t* db, const char* file, zathura_bookmark_t* bookmark);

/**
 * Add or update bookmark in the database.
 * @param db The database instance
 * @param file The file to which the bookmark belongs to.
 * @param bookmark The bookmark instance.
 * @return true on success, false otherwise
 */
bool zathura_db_remove_bookmark(zathura_database_t* db, const char* file, const char* id);

/**
 * Loads all bookmarks from the database belonging to a specific file.
 * @param db The database instance.
 * @param file The file for which the bookmarks should be loaded.
 * @return List of zathura_bookmark_t* or NULL on failure.
 */
girara_list_t* zathura_db_load_bookmarks(zathura_database_t* db, const char* file);

/**
 * Set file info (last site, ...) in the database.
 * @param db The database instance
 * @param file The file to which the file info belongs to.
 * @param page The last page.
 * @param offset The last offset.
 * @param scale The last scale.
 * @return true on success, false otherwise.
 */
bool zathura_db_set_fileinfo(zathura_database_t* db, const char* file, unsigned int page, int offset, float scale);

#endif // DATABASE_H
