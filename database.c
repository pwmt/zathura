/* See LICENSE file for license and copyright information */

#include "database.h"

G_DEFINE_INTERFACE(ZathuraDatabase, zathura_database, G_TYPE_OBJECT)

static void
zathura_database_default_init(ZathuraDatabaseInterface* GIRARA_UNUSED(iface))
{
}

void
zathura_db_free(zathura_database_t* db)
{
  if (db == NULL) {
    return;
  }

  g_object_unref(db);
}

bool
zathura_db_add_bookmark(zathura_database_t* db, const char* file,
    zathura_bookmark_t* bookmark)
{
  g_return_val_if_fail(ZATHURA_IS_DATABASE(db) && file != NULL && bookmark != NULL, false);

  return ZATHURA_DATABASE_GET_INTERFACE(db)->add_bookmark(db, file, bookmark);
}

bool
zathura_db_remove_bookmark(zathura_database_t* db, const char* file, const char*
    id)
{
  g_return_val_if_fail(ZATHURA_IS_DATABASE(db) && file != NULL && id != NULL, false);

  return ZATHURA_DATABASE_GET_INTERFACE(db)->remove_bookmark(db, file, id);
}

girara_list_t*
zathura_db_load_bookmarks(zathura_database_t* db, const char* file)
{
  g_return_val_if_fail(ZATHURA_IS_DATABASE(db) && file != NULL, NULL);

  return ZATHURA_DATABASE_GET_INTERFACE(db)->load_bookmarks(db, file);
}

bool 
zathura_db_set_fileinfo(zathura_database_t* db, const char* file, unsigned int
    page, unsigned int offset, double scale, unsigned int rotation)
{
  g_return_val_if_fail(ZATHURA_IS_DATABASE(db) && file != NULL, false);

  return ZATHURA_DATABASE_GET_INTERFACE(db)->set_fileinfo(db, file, page, offset,
      scale, rotation);
}

bool
zathura_db_get_fileinfo(zathura_database_t* db, const char* file, unsigned int*
    page, unsigned int* offset, double* scale, unsigned int* rotation)
{
  g_return_val_if_fail(ZATHURA_IS_DATABASE(db) && file != NULL && page != NULL &&
      offset != NULL && scale != NULL && rotation != NULL, false);

  return ZATHURA_DATABASE_GET_INTERFACE(db)->get_fileinfo(db, file, page, offset,
      scale, rotation);
}
