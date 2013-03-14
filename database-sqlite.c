/* See LICENSE file for license and copyright information */

#include <sqlite3.h>
#include <girara/utils.h>
#include <girara/datastructures.h>
#include <string.h>

#include "database-sqlite.h"

static void zathura_database_interface_init(ZathuraDatabaseInterface* iface);

G_DEFINE_TYPE_WITH_CODE(ZathuraSQLDatabase, zathura_sqldatabase, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(ZATHURA_TYPE_DATABASE, zathura_database_interface_init))

static void sqlite_finalize(GObject* object);
static bool sqlite_add_bookmark(zathura_database_t* db, const char* file,
                                zathura_bookmark_t* bookmark);
static bool sqlite_remove_bookmark(zathura_database_t* db, const char* file,
                                   const char* id);
static girara_list_t* sqlite_load_bookmarks(zathura_database_t* db,
    const char* file);
static bool sqlite_set_fileinfo(zathura_database_t* db, const char* file,
                                zathura_fileinfo_t* file_info);
static bool sqlite_get_fileinfo(zathura_database_t* db, const char* file,
                                zathura_fileinfo_t* file_info);
static void sqlite_set_property(GObject* object, guint prop_id,
                                const GValue* value, GParamSpec* pspec);

typedef struct zathura_sqldatabase_private_s {
  sqlite3* session;
} zathura_sqldatabase_private_t;

#define ZATHURA_SQLDATABASE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ZATHURA_TYPE_SQLDATABASE, zathura_sqldatabase_private_t))

enum {
  PROP_0,
  PROP_PATH
};

static void
zathura_database_interface_init(ZathuraDatabaseInterface* iface)
{
  /* initialize interface */
  iface->add_bookmark    = sqlite_add_bookmark;
  iface->remove_bookmark = sqlite_remove_bookmark;
  iface->load_bookmarks  = sqlite_load_bookmarks;
  iface->set_fileinfo    = sqlite_set_fileinfo;
  iface->get_fileinfo    = sqlite_get_fileinfo;
}

static void
zathura_sqldatabase_class_init(ZathuraSQLDatabaseClass* class)
{
  /* add private members */
  g_type_class_add_private(class, sizeof(zathura_sqldatabase_private_t));

  /* override methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = sqlite_finalize;
  object_class->set_property = sqlite_set_property;

  g_object_class_install_property(object_class, PROP_PATH,
                                  g_param_spec_string("path", "path", "path to the database", NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
zathura_sqldatabase_init(ZathuraSQLDatabase* db)
{
  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);
  priv->session = NULL;
}

zathura_database_t*
zathura_sqldatabase_new(const char* path)
{
  g_return_val_if_fail(path != NULL && strlen(path) != 0, NULL);

  zathura_database_t* db = g_object_new(ZATHURA_TYPE_SQLDATABASE, "path", path, NULL);
  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);
  if (priv->session == NULL) {
    g_object_unref(G_OBJECT(db));
    return NULL;
  }

  return db;
}

static void
sqlite_finalize(GObject* object)
{
  ZathuraSQLDatabase* db = ZATHURA_SQLDATABASE(object);
  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);
  if (priv->session) {
    sqlite3_close(priv->session);
  }

  G_OBJECT_CLASS(zathura_sqldatabase_parent_class)->finalize(object);
}

static void
sqlite_db_init(ZathuraSQLDatabase* db, const char* path)
{
  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);

  /* create bookmarks database */
  static const char SQL_BOOKMARK_INIT[] =
    "CREATE TABLE IF NOT EXISTS bookmarks ("
    "file TEXT,"
    "id TEXT,"
    "page INTEGER,"
    "PRIMARY KEY(file, id));";

  static const char SQL_FILEINFO_INIT[] =
    "CREATE TABLE IF NOT EXISTS fileinfo ("
    "file TEXT PRIMARY KEY,"
    "page INTEGER,"
    "offset INTEGER,"
    "scale FLOAT,"
    "rotation INTEGER,"
    "pages_per_row INTEGER,"
    "first_page_column INTEGER,"
    "position_x FLOAT,"
    "position_y FLOAT"
    ");";

  static const char SQL_FILEINFO_ALTER[] =
    "ALTER TABLE fileinfo ADD COLUMN pages_per_row INTEGER;"
    "ALTER TABLE fileinfo ADD COLUMN position_x FLOAT;"
    "ALTER TABLE fileinfo ADD COLUMN position_y FLOAT;";

  static const char SQL_FILEINFO_ALTER2[] =
    "ALTER TABLE fileinfo ADD COLUMN first_page_column INTEGER;";

  sqlite3* session = NULL;
  if (sqlite3_open(path, &session) != SQLITE_OK) {
    girara_error("Could not open database: %s\n", path);
    return;
  }

  if (sqlite3_exec(session, SQL_BOOKMARK_INIT, NULL, 0, NULL) != SQLITE_OK) {
    girara_error("Failed to initialize database: %s\n", path);
    sqlite3_close(session);
    return;
  }

  if (sqlite3_exec(session, SQL_FILEINFO_INIT, NULL, 0, NULL) != SQLITE_OK) {
    girara_error("Failed to initialize database: %s\n", path);
    sqlite3_close(session);
    return;
  }

  const char* data_type = NULL;
  if (sqlite3_table_column_metadata(session, NULL, "fileinfo", "pages_per_row", &data_type, NULL, NULL, NULL, NULL) != SQLITE_OK) {
    girara_debug("old database table layout detected; updating ...");
    if (sqlite3_exec(session, SQL_FILEINFO_ALTER, NULL, 0, NULL) != SQLITE_OK) {
      girara_warning("failed to update database table layout");
    }
  }

  data_type = NULL;
  if (sqlite3_table_column_metadata(session, NULL, "fileinfo", "first_page_column", &data_type, NULL, NULL, NULL, NULL) != SQLITE_OK) {
    girara_debug("old database table layout detected; updating ...");
    if (sqlite3_exec(session, SQL_FILEINFO_ALTER2, NULL, 0, NULL) != SQLITE_OK) {
      girara_warning("failed to update database table layout");
    }
  }

  priv->session = session;
}

static void
sqlite_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
  ZathuraSQLDatabase* db = ZATHURA_SQLDATABASE(object);
  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);

  switch (prop_id) {
    case PROP_PATH:
      g_return_if_fail(priv->session == NULL);
      sqlite_db_init(db, g_value_get_string(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static sqlite3_stmt*
prepare_statement(sqlite3* session, const char* statement)
{
  if (session == NULL || statement == NULL) {
    return NULL;
  }

  const char* pz_tail   = NULL;
  sqlite3_stmt* pp_stmt = NULL;

  if (sqlite3_prepare_v2(session, statement, -1, &pp_stmt, &pz_tail) != SQLITE_OK) {
    girara_error("Failed to prepare query: %s", statement);
    sqlite3_finalize(pp_stmt);
    return NULL;
  } else if (pz_tail && *pz_tail != '\0') {
    girara_error("Unused portion of statement: %s", pz_tail);
    sqlite3_finalize(pp_stmt);
    return NULL;
  }

  return pp_stmt;
}

static bool
sqlite_add_bookmark(zathura_database_t* db, const char* file,
                    zathura_bookmark_t* bookmark)
{
  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);

  static const char SQL_BOOKMARK_ADD[] =
    "REPLACE INTO bookmarks (file, id, page) VALUES (?, ?, ?);";

  sqlite3_stmt* stmt = prepare_statement(priv->session, SQL_BOOKMARK_ADD);
  if (stmt == NULL) {
    return false;
  }

  if (sqlite3_bind_text(stmt, 1, file, -1, NULL) != SQLITE_OK ||
      sqlite3_bind_text(stmt, 2, bookmark->id, -1, NULL) != SQLITE_OK ||
      sqlite3_bind_int(stmt, 3, bookmark->page) != SQLITE_OK) {
    sqlite3_finalize(stmt);
    girara_error("Failed to bind arguments.");
    return false;
  }

  int res = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return (res == SQLITE_DONE) ? true : false;
}

static bool
sqlite_remove_bookmark(zathura_database_t* db, const char* file, const char*
                       id)
{
  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);

  static const char SQL_BOOKMARK_ADD[] =
    "DELETE FROM bookmarks WHERE file = ? AND id = ?;";

  sqlite3_stmt* stmt = prepare_statement(priv->session, SQL_BOOKMARK_ADD);
  if (stmt == NULL) {
    return false;
  }

  if (sqlite3_bind_text(stmt, 1, file, -1, NULL) != SQLITE_OK ||
      sqlite3_bind_text(stmt, 2, id, -1, NULL) != SQLITE_OK) {
    sqlite3_finalize(stmt);
    girara_error("Failed to bind arguments.");
    return false;
  }

  int res = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return (res == SQLITE_DONE) ? true : false;
}

static girara_list_t*
sqlite_load_bookmarks(zathura_database_t* db, const char* file)
{
  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);

  static const char SQL_BOOKMARK_SELECT[] =
    "SELECT id, page FROM bookmarks WHERE file = ?;";

  sqlite3_stmt* stmt = prepare_statement(priv->session, SQL_BOOKMARK_SELECT);
  if (stmt == NULL) {
    return NULL;
  }

  if (sqlite3_bind_text(stmt, 1, file, -1, NULL) != SQLITE_OK) {
    sqlite3_finalize(stmt);
    girara_error("Failed to bind arguments.");
    return NULL;
  }

  girara_list_t* result = girara_sorted_list_new2((girara_compare_function_t) zathura_bookmarks_compare,
                          (girara_free_function_t) zathura_bookmark_free);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    zathura_bookmark_t* bookmark = g_malloc0(sizeof(zathura_bookmark_t));

    bookmark->id   = g_strdup((const char*) sqlite3_column_text(stmt, 0));
    bookmark->page = sqlite3_column_int(stmt, 1);

    girara_list_append(result, bookmark);
  }

  sqlite3_finalize(stmt);

  return result;
}

static bool
sqlite_set_fileinfo(zathura_database_t* db, const char* file,
                    zathura_fileinfo_t* file_info)
{
  if (db == NULL || file == NULL || file_info == NULL) {
    return false;
  }

  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);

  static const char SQL_FILEINFO_SET[] =
    "REPLACE INTO fileinfo (file, page, offset, scale, rotation, pages_per_row, first_page_column, position_x, position_y) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

  sqlite3_stmt* stmt = prepare_statement(priv->session, SQL_FILEINFO_SET);
  if (stmt == NULL) {
    return false;
  }

  if (sqlite3_bind_text(stmt,   1, file, -1, NULL)               != SQLITE_OK ||
      sqlite3_bind_int(stmt,    2, file_info->current_page)      != SQLITE_OK ||
      sqlite3_bind_int(stmt,    3, file_info->page_offset)       != SQLITE_OK ||
      sqlite3_bind_double(stmt, 4, file_info->scale)             != SQLITE_OK ||
      sqlite3_bind_int(stmt,    5, file_info->rotation)          != SQLITE_OK ||
      sqlite3_bind_int(stmt,    6, file_info->pages_per_row)     != SQLITE_OK ||
      sqlite3_bind_int(stmt,    7, file_info->first_page_column) != SQLITE_OK ||
      sqlite3_bind_double(stmt, 8, file_info->position_x)        != SQLITE_OK ||
      sqlite3_bind_double(stmt, 9, file_info->position_y)        != SQLITE_OK) {
    sqlite3_finalize(stmt);
    girara_error("Failed to bind arguments.");
    return false;
  }

  int res = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return (res == SQLITE_DONE) ? true : false;
}

static bool
sqlite_get_fileinfo(zathura_database_t* db, const char* file,
                    zathura_fileinfo_t* file_info)
{
  if (db == NULL || file == NULL || file_info == NULL) {
    return false;
  }

  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);

  static const char SQL_FILEINFO_GET[] =
    "SELECT page, offset, scale, rotation, pages_per_row, first_page_column, position_x, position_y FROM fileinfo WHERE file = ?;";

  sqlite3_stmt* stmt = prepare_statement(priv->session, SQL_FILEINFO_GET);
  if (stmt == NULL) {
    return false;
  }

  if (sqlite3_bind_text(stmt, 1, file, -1, NULL) != SQLITE_OK) {
    sqlite3_finalize(stmt);
    girara_error("Failed to bind arguments.");
    return false;
  }

  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    girara_info("No info for file %s available.", file);
    return false;
  }

  file_info->current_page      = sqlite3_column_int(stmt, 0);
  file_info->page_offset       = sqlite3_column_int(stmt, 1);
  file_info->scale             = sqlite3_column_double(stmt, 2);
  file_info->rotation          = sqlite3_column_int(stmt, 3);
  file_info->pages_per_row     = sqlite3_column_int(stmt, 4);
  file_info->first_page_column = sqlite3_column_int(stmt, 5);
  file_info->position_x        = sqlite3_column_double(stmt, 6);
  file_info->position_y        = sqlite3_column_double(stmt, 7);

  sqlite3_finalize(stmt);

  return true;
}
