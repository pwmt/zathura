/* See LICENSE file for license and copyright information */

#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>
#include <girara.h>

#include "zathura.h"
#include "bookmarks.h"

zathura_database_t* zathura_db_init(const char* path);

void zathura_db_free(zathura_database_t* db);

bool zathura_db_add_bookmark(zathura_database_t* db, const char* file, zathura_bookmark_t* bookmark);

bool zathura_db_remove_bookmark(zathura_database_t* db, const char* file, const char* id);

girara_list_t* zathura_db_load_bookmarks(zathura_database_t* db, const char* file);

#endif // DATABASE_H
