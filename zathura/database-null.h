/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_DATABASE_NULL_H
#define ZATHURA_DATABASE_NULL_H

#include "database.h"

#define ZATHURA_TYPE_NULLDATABASE (zathura_nulldatabase_get_type())
#define ZATHURA_NULLDATABASE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_NULLDATABASE, ZathuraNullDatabase))
#define ZATHURA_IS_NULLDATABASE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_NULLDATABASE))
#define ZATHURA_NULLDATABASE_CLASS(klass)                                                                              \
  (G_TYPE_CHECK_CLASS_CAST((klass), ZATHURA_TYPE_NULLDATABASE, ZathuraNullDatabaseClass))
#define ZATHURA_IS_NULLDATABASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZATHURA_TYPE_NULLDATABASE))
#define ZATHURA_NULLDATABASE_GET_CLASS(obj)                                                                            \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_NULLDATABASE, ZathuraNullDatabaseClass))

typedef struct _ZathuraNullDatabase ZathuraNullDatabase;
typedef struct _ZathuraNullDatabaseClass ZathuraNullDatabaseClass;

struct _ZathuraNullDatabase {
  GObject parent_instance;
};

struct _ZathuraNullDatabaseClass {
  GObjectClass parent_class;
};

GType zathura_nulldatabase_get_type(void) G_GNUC_CONST;

/**
 * Initialize database system.
 *
 * @return A valid zathura_database_t instance or NULL on failure
 */
zathura_database_t* zathura_nulldatabase_new(void);

#endif
