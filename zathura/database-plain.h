/* See LICENSE file for license and copyright information */

#ifndef ZATHURA_DATABASE_PLAIN_H
#define ZATHURA_DATABASE_PLAIN_H

#include "database.h"

#define ZATHURA_TYPE_PLAINDATABASE \
  (zathura_plaindatabase_get_type ())
#define ZATHURA_PLAINDATABASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), ZATHURA_TYPE_PLAINDATABASE, ZathuraPlainDatabase))
#define ZATHURA_IS_PLAINDATABASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ZATHURA_TYPE_PLAINDATABASE))
#define ZATHURA_PLAINDATABASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), ZATHURA_TYPE_PLAINDATABASE, ZathuraPlainDatabaseClass))
#define ZATHURA_IS_PLAINDATABASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), ZATHURA_TYPE_PLAINDATABASE))
#define ZATHURA_PLAINDATABASE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), ZATHURA_TYPE_PLAINDATABASE, ZathuraPlainDatabaseClass))

typedef struct _ZathuraPlainDatabase        ZathuraPlainDatabase;
typedef struct _ZathuraPlainDatabaseClass   ZathuraPlainDatabaseClass;

struct _ZathuraPlainDatabase
{
  GObject parent_instance;
};

struct _ZathuraPlainDatabaseClass
{
  GObjectClass parent_class;
};

GType zathura_plaindatabase_get_type(void) G_GNUC_CONST;
/**
 * Initialize database system.
 *
 * @param dir Path to the directory where the database file should be located.
 * @return A valid zathura_database_t instance or NULL on failure
 */
zathura_database_t* zathura_plaindatabase_new(const char* dir);

#endif
