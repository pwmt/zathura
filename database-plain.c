/* See LICENSE file for license and copyright information */

#include <glib.h>
#include <girara.h>

#include "database.h"

struct zathura_database_s
{
  void* tmp;
};

zathura_database_t*
zathura_db_init(const char* dir)
{
  return NULL;
}

void
zathura_db_free(zathura_database_t* db)
{
}

bool
zathura_db_add_bookmark(zathura_database_t* db, const char* file,
    zathura_bookmark_t* bookmark)
{
  return false;
}

bool
zathura_db_remove_bookmark(zathura_database_t* db, const char* file, const char*
    id)
{
  return false;
}

girara_list_t*
zathura_db_load_bookmarks(zathura_database_t* db, const char* file)
{
  return NULL;
}

bool
zathura_db_set_fileinfo(zathura_database_t* db, const char* file, unsigned int
    page, int offset, float scale)
{
  return false;
}

bool
zathura_db_get_fileinfo(zathura_database_t* db, const char* file, unsigned int*
    page, int* offset, float* scale)
{
  return false;
}
