/* See LICENSE file for license and copyright information */

#ifndef ZATHURA_DATABASE_SQLITE_H
#define ZATHURA_DATABASE_SQLITE_H

#include "database.h"

#define ZATHURA_TYPE_SQLDATABASE \
  (zathura_sqldatabase_get_type ())
#define ZATHURA_SQLDATABASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), ZATHURA_TYPE_SQLDATABASE, ZathuraSQLDatabase))
#define ZATHURA_IS_SQLDATABASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ZATHURA_TYPE_SQLDATABASE))
#define ZATHURA_SQLDATABASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), ZATHURA_TYPE_SQLDATABASE, ZathuraSQLDatabaseClass))
#define ZATHURA_IS_SQLDATABASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), ZATHURA_TYPE_SQLDATABASE))
#define ZATHURA_SQLDATABASE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), ZATHURA_TYPE_SQLDATABASE, ZathuraSQLDatabaseClass))

typedef struct _ZathuraSQLDatabase        ZathuraSQLDatabase;
typedef struct _ZathuraSQLDatabaseClass   ZathuraSQLDatabaseClass;

struct _ZathuraSQLDatabase
{
  GObject parent_instance;
};

struct _ZathuraSQLDatabaseClass
{
  GObjectClass parent_class;
};

GType zathura_sqldatabase_get_type(void);
/**
 * Initialize database system.
 *
 * @param dir Path to the sqlite database.
 * @return A valid zathura_database_t instance or NULL on failure
 */
zathura_database_t* zathura_sqldatabase_new(const char* path);

#endif
