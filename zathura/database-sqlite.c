/* See LICENSE file for license and copyright information */

#include <sqlite3.h>
#include <girara/utils.h>
#include <girara/datastructures.h>
#include <girara/input-history.h>
#include <string.h>
#include <strings.h>

#include "database-sqlite.h"
#include "utils.h"

static void zathura_database_interface_init(ZathuraDatabaseInterface* iface);
static void io_interface_init(GiraraInputHistoryIOInterface* iface);

G_DEFINE_TYPE_WITH_CODE(ZathuraSQLDatabase, zathura_sqldatabase, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(ZATHURA_TYPE_DATABASE, zathura_database_interface_init)
                        G_IMPLEMENT_INTERFACE(GIRARA_TYPE_INPUT_HISTORY_IO, io_interface_init))

static bool           check_column(sqlite3* session, const char* table, const char* col, bool* result);
static bool           check_column_type(sqlite3* session, const char* table, const char* col, const char* type, bool* result);
static void           sqlite_finalize(GObject* object);
static bool           sqlite_add_bookmark(zathura_database_t* db, const char* file, zathura_bookmark_t* bookmark);
static bool           sqlite_remove_bookmark(zathura_database_t* db, const char* file, const char* id);
static girara_list_t* sqlite_load_bookmarks(zathura_database_t* db, const char* file);
static girara_list_t* sqlite_load_jumplist(zathura_database_t* db, const char* file);
static bool           sqlite_save_jumplist(zathura_database_t* db, const char* file, girara_list_t* jumplist);
static bool           sqlite_set_fileinfo(zathura_database_t* db, const char* file, zathura_fileinfo_t* file_info);
static bool           sqlite_get_fileinfo(zathura_database_t* db, const char* file, zathura_fileinfo_t* file_info);
static void           sqlite_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void           sqlite_io_append(GiraraInputHistoryIO* db, const char*);
static girara_list_t* sqlite_io_read(GiraraInputHistoryIO* db);
static girara_list_t* sqlite_get_recent_files(zathura_database_t* db, int max);

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
  iface->add_bookmark     = sqlite_add_bookmark;
  iface->remove_bookmark  = sqlite_remove_bookmark;
  iface->load_bookmarks   = sqlite_load_bookmarks;
  iface->load_jumplist    = sqlite_load_jumplist;
  iface->save_jumplist    = sqlite_save_jumplist;
  iface->set_fileinfo     = sqlite_set_fileinfo;
  iface->get_fileinfo     = sqlite_get_fileinfo;
  iface->get_recent_files = sqlite_get_recent_files;
}

static void
io_interface_init(GiraraInputHistoryIOInterface* iface)
{
  /* initialize interface */
  iface->append = sqlite_io_append;
  iface->read = sqlite_io_read;
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
    g_param_spec_string("path", "path", "path to the database", NULL,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
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

  /* create bookmarks table */
  static const char SQL_BOOKMARK_INIT[] =
    "CREATE TABLE IF NOT EXISTS bookmarks ("
    "file TEXT,"
    "id TEXT,"
    "page INTEGER,"
    "hadj_ratio FLOAT,"
    "vadj_ratio FLOAT,"
    "PRIMARY KEY(file, id));";

  /* ceate jumplist table */
  static const char SQL_JUMPLIST_INIT[] =
    "CREATE TABLE IF NOT EXISTS jumplist ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "file TEXT,"
    "page INTEGER,"
    "hadj_ratio FLOAT,"
    "vadj_ratio FLOAT"
    ");";

  /* create fileinfo table */
  static const char SQL_FILEINFO_INIT[] =
    "CREATE TABLE IF NOT EXISTS fileinfo ("
    "file TEXT PRIMARY KEY,"
    "page INTEGER,"
    "offset INTEGER,"
    "scale FLOAT,"
    "rotation INTEGER,"
    "pages_per_row INTEGER,"
    "first_page_column TEXT,"
    "position_x FLOAT,"
    "position_y FLOAT,"
    "time TIMESTAMP"
    ");";

  /* create history table */
  static const char SQL_HISTORY_INIT[] =
    "CREATE TABLE IF NOT EXISTS history ("
    "time TIMESTAMP,"
    "line TEXT,"
    "PRIMARY KEY(line));";

  static const char* ALL_INIT[] = {
    SQL_BOOKMARK_INIT,
    SQL_JUMPLIST_INIT,
    SQL_FILEINFO_INIT,
    SQL_HISTORY_INIT
  };

  /* update fileinfo table (part 1) */
  static const char SQL_FILEINFO_ALTER[] =
    "ALTER TABLE fileinfo ADD COLUMN pages_per_row INTEGER;"
    "ALTER TABLE fileinfo ADD COLUMN position_x FLOAT;"
    "ALTER TABLE fileinfo ADD COLUMN position_y FLOAT;";

  /* update fileinfo table (part 2) */
  static const char SQL_FILEINFO_ALTER2[] =
    "ALTER TABLE fileinfo ADD COLUMN first_page_column TEXT;";

  /* update fileinfo table (part 3) */
  static const char SQL_FILEINFO_ALTER3[] =
    "ALTER TABLE fileinfo ADD COLUMN time TIMESTAMP;";

  /* update bookmark table */
  static const char SQL_BOOKMARK_ALTER[] =
    "ALTER TABLE bookmarks ADD COLUMN hadj_ratio FLOAT;"
    "ALTER TABLE bookmarks ADD COLUMN vadj_ratio FLOAT;";

  sqlite3* session = NULL;
  if (sqlite3_open(path, &session) != SQLITE_OK) {
    girara_error("Could not open database: %s\n", path);
    return;
  }

  /* create tables if they don't exist */
  for (size_t s = 0; s < LENGTH(ALL_INIT); ++s) {
    if (sqlite3_exec(session, ALL_INIT[s], NULL, 0, NULL) != SQLITE_OK) {
      girara_error("Failed to initialize database: %s\n", path);
      sqlite3_close(session);
      return;
    }
  }

  /* check existing tables for missing columns */
  bool res1, res2, ret1, ret2;

  ret1 = check_column(session, "fileinfo", "pages_per_row", &res1);

  if (ret1 == true && res1 == false) {
    girara_debug("old database table layout detected; updating ...");
    if (sqlite3_exec(session, SQL_FILEINFO_ALTER, NULL, 0, NULL) != SQLITE_OK) {
      girara_warning("failed to update database table layout");
    }
  }

  ret1 = check_column(session, "fileinfo", "first_page_column", &res1);

  if (ret1 == true && res1 == false) {
    girara_debug("old database table layout detected; updating ...");
    if (sqlite3_exec(session, SQL_FILEINFO_ALTER2, NULL, 0, NULL) != SQLITE_OK) {
      girara_warning("failed to update database table layout");
    }
  }

  ret1 = check_column(session, "fileinfo", "time", &res1);

  if (ret1 == true && res1 == false) {
    girara_debug("old database table layout detected; updating ...");
    if (sqlite3_exec(session, SQL_FILEINFO_ALTER3, NULL, 0, NULL) != SQLITE_OK) {
      girara_warning("failed to update database table layout");
    }
  }

  ret1 = check_column(session, "bookmarks", "hadj_ratio", &res1);
  ret2 = check_column(session, "bookmarks", "vadj_ratio", &res2);

  if (ret1 == true && ret2 == true && res1 == false && res2 == false) {
    girara_debug("old database table layout detected; updating ...");
    if (sqlite3_exec(session, SQL_BOOKMARK_ALTER, NULL, 0, NULL) != SQLITE_OK) {
      girara_warning("failed to update database table layout");
    }
  }

  /* check existing tables for correct column types */
  ret1 = check_column_type(session, "fileinfo", "first_page_column", "TEXT", &res1);

  if (ret1 == true && res1 == false) {
    girara_debug("old database table layout detected; updating ...");

    /* prepare transaction */
    char tx_begin[] = "BEGIN TRANSACTION;"
                      "ALTER TABLE fileinfo RENAME TO tmp;";
    char tx_end[] = "INSERT INTO fileinfo SELECT * FROM tmp;"
                    "DROP TABLE tmp;"
                    "COMMIT;";

    /* calculate requred buffer size */
    size_t tx_buffer_size = strlen(tx_begin);
    tx_buffer_size += strlen(SQL_FILEINFO_INIT);
    tx_buffer_size += strlen(tx_end);
    ++tx_buffer_size;

    /* assemble transaction */
    char transaction[tx_buffer_size];
    bzero(transaction, tx_buffer_size);
    strcat(transaction, tx_begin);
    strcat(transaction, SQL_FILEINFO_INIT);
    strcat(transaction, tx_end);

    if (sqlite3_exec(session, transaction, NULL, 0, NULL) != SQLITE_OK) {
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
check_column(sqlite3* session, const char* table, const char* col, bool* res)
{
  /* we can't actually bind the argument with sqlite3_bind_text because
   * sqlite3_prepare_v2 fails with "PRAGMA table_info(?);" */
  char* query = sqlite3_mprintf("PRAGMA table_info(%Q);", table);
  if (query == NULL) {
    return false;
  }

  sqlite3_stmt* stmt = prepare_statement(session, query);
  if (stmt == NULL) {
    return false;
  }

  *res = false;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (strcmp((const char*) sqlite3_column_text(stmt, 1), col) == 0) {
      *res = true;
      break;
    }
  }

  if (*res == false) {
    girara_debug("column %s in table %s is NOT found", col, table);
  }

  sqlite3_finalize(stmt);
  sqlite3_free(query);

  return true;
}

static bool
check_column_type(sqlite3* session, const char* table, const char* col, const char* type, bool* res)
{
  /* we can't actually bind the argument with sqlite3_bind_text because
   * sqlite3_prepare_v2 fails with "PRAGMA table_info(?);" */
  char* query = sqlite3_mprintf("PRAGMA table_info(%Q);", table);
  if (query == NULL) {
    return false;
  }

  sqlite3_stmt* stmt = prepare_statement(session, query);
  if (stmt == NULL) {
    return false;
  }

  *res = false;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (strcmp((const char*) sqlite3_column_text(stmt, 1), col) == 0) {
      if (strcmp((const char*) sqlite3_column_text(stmt, 2), type) == 0) {
        *res = true;
        break;
      }
    }
  }

  if (*res == false) {
    girara_debug("column %s in table %s has wrong type", col, table);
  }

  sqlite3_finalize(stmt);
  sqlite3_free(query);

  return true;
}

static bool
sqlite_add_bookmark(zathura_database_t* db, const char* file,
                    zathura_bookmark_t* bookmark)
{
  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);

  static const char SQL_BOOKMARK_ADD[] =
    "REPLACE INTO bookmarks (file, id, page, hadj_ratio, vadj_ratio) VALUES (?, ?, ?, ?, ?);";

  sqlite3_stmt* stmt = prepare_statement(priv->session, SQL_BOOKMARK_ADD);
  if (stmt == NULL) {
    return false;
  }

  if (sqlite3_bind_text(stmt, 1, file, -1, NULL) != SQLITE_OK ||
      sqlite3_bind_text(stmt, 2, bookmark->id, -1, NULL) != SQLITE_OK ||
      sqlite3_bind_int(stmt, 3, bookmark->page) != SQLITE_OK ||
      sqlite3_bind_double(stmt, 4, bookmark->x) != SQLITE_OK ||
      sqlite3_bind_double(stmt, 5, bookmark->y) != SQLITE_OK) {

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
    "SELECT id, page, hadj_ratio, vadj_ratio FROM bookmarks WHERE file = ?;";

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
  if (result == NULL) {
    sqlite3_finalize(stmt);
    return NULL;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    zathura_bookmark_t* bookmark = g_try_malloc0(sizeof(zathura_bookmark_t));
    if (bookmark == NULL) {
      continue;
    }

    bookmark->id   = g_strdup((const char*) sqlite3_column_text(stmt, 0));
    bookmark->page = sqlite3_column_int(stmt, 1);
    bookmark->x    = sqlite3_column_double(stmt, 2);
    bookmark->y    = sqlite3_column_double(stmt, 3);
    bookmark->x    = bookmark->x <= 0.0 ? DBL_MIN : bookmark->x;
    bookmark->y    = bookmark->y <= 0.0 ? DBL_MIN : bookmark->y;

    girara_list_append(result, bookmark);
  }

  sqlite3_finalize(stmt);

  return result;
}

static bool
sqlite_save_jumplist(zathura_database_t* db, const char* file, girara_list_t* jumplist)
{
  g_return_val_if_fail(db != NULL && file != NULL && jumplist != NULL, false);

  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);
  static const char SQL_INSERT_JUMP[] = "INSERT INTO jumplist (file, page, hadj_ratio, vadj_ratio) VALUES (?, ?, ?, ?);";
  static const char SQL_REMOVE_JUMPLIST[] = "DELETE FROM jumplist WHERE file = ?;";
  sqlite3_stmt* stmt = NULL;
  int res = 0;

  if (sqlite3_exec(priv->session, "BEGIN;", NULL, 0, NULL) != SQLITE_OK) {
    return false;
  }

  stmt = prepare_statement(priv->session, SQL_REMOVE_JUMPLIST);

  if (stmt == NULL) {
    sqlite3_exec(priv->session, "ROLLBACK;", NULL, 0, NULL);
    return false;
  }

  if (sqlite3_bind_text(stmt, 1, file, -1, NULL) != SQLITE_OK) {
    sqlite3_finalize(stmt);
    sqlite3_exec(priv->session, "ROLLBACK;", NULL, 0, NULL);
    girara_error("Failed to bind arguments.");
    return false;
  }

  res = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (res != SQLITE_DONE) {
    sqlite3_exec(priv->session, "ROLLBACK;", NULL, 0, NULL);
    return false;
  }

  if (girara_list_size(jumplist) == 0) {
    sqlite3_exec(priv->session, "COMMIT;", NULL, 0, NULL);
    return true;
  }

  girara_list_iterator_t* cur = girara_list_iterator(jumplist);
  bool status = true;

  while (true) {
    zathura_jump_t* jump = girara_list_iterator_data(cur);

    stmt = prepare_statement(priv->session, SQL_INSERT_JUMP);

    if (stmt == NULL) {
      status = false;
      break;
    }

    if (sqlite3_bind_text(stmt, 1, file, -1, NULL) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 2, jump->page)      != SQLITE_OK ||
        sqlite3_bind_double(stmt, 3, jump->x)      != SQLITE_OK ||
        sqlite3_bind_double(stmt, 4, jump->y)      != SQLITE_OK) {

      sqlite3_finalize(stmt);
      girara_error("Failed to bind arguments.");
      status = false;
      break;
    }

    res = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (res != SQLITE_DONE) {
      status = false;
      break;
    }

    if (girara_list_iterator_has_next(cur) == true) {
      girara_list_iterator_next(cur);
    } else {
      break;
    }
  }

  if (status == false) {
    sqlite3_exec(priv->session, "ROLLBACK;", NULL, 0, NULL);
    return false;
  } else {
    sqlite3_exec(priv->session, "COMMIT;", NULL, 0, NULL);
    return true;
  }
}

static girara_list_t*
sqlite_load_jumplist(zathura_database_t* db, const char* file)
{
  g_return_val_if_fail(db != NULL && file != NULL, NULL);

  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);
  static const char SQL_GET_JUMPLIST[] = "SELECT page, hadj_ratio, vadj_ratio FROM jumplist WHERE file = ? ORDER BY id ASC;";
  sqlite3_stmt* stmt = prepare_statement(priv->session, SQL_GET_JUMPLIST);

  if (stmt == NULL) {
    return NULL;
  }

  if (sqlite3_bind_text(stmt, 1, file, -1, NULL) != SQLITE_OK) {
    sqlite3_finalize(stmt);
    girara_error("Failed to bind arguments.");

    return NULL;
  }

  girara_list_t* jumplist = girara_list_new2(g_free);
  if (jumplist == NULL) {
    sqlite3_finalize(stmt);
    return NULL;
  }

  int res = 0;
  while ((res = sqlite3_step(stmt)) == SQLITE_ROW) {
    zathura_jump_t* jump = g_try_malloc0(sizeof(zathura_jump_t));
    if (jump == NULL) {
      continue;
    }

    jump->page = sqlite3_column_int(stmt, 0);
    jump->x    = sqlite3_column_double(stmt, 1);
    jump->y    = sqlite3_column_double(stmt, 2);
    girara_list_append(jumplist, jump);
  }

  sqlite3_finalize(stmt);

  if (res != SQLITE_DONE) {
    girara_list_free(jumplist);

    return NULL;
  }

  return jumplist;
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
    "REPLACE INTO fileinfo (file, page, offset, scale, rotation, pages_per_row, first_page_column, position_x, position_y, time) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, DATETIME('now'));";

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
      sqlite3_bind_text(stmt,   7, file_info->first_page_column_list, -1, NULL)
                                                                 != SQLITE_OK ||
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
    girara_debug("No info for file %s available.", file);
    return false;
  }

  file_info->current_page           = sqlite3_column_int(stmt, 0);
  file_info->page_offset            = sqlite3_column_int(stmt, 1);
  file_info->scale                  = sqlite3_column_double(stmt, 2);
  file_info->rotation               = sqlite3_column_int(stmt, 3);
  file_info->pages_per_row          = sqlite3_column_int(stmt, 4);
  file_info->first_page_column_list = g_strdup((const char*) sqlite3_column_text(stmt, 5));
  file_info->position_x             = sqlite3_column_double(stmt, 6);
  file_info->position_y             = sqlite3_column_double(stmt, 7);

  sqlite3_finalize(stmt);

  return true;
}

static void
sqlite_io_append(GiraraInputHistoryIO* db, const char* input)
{
  static const char SQL_HISTORY_SET[] =
    "REPLACE INTO history (line, time) VALUES (?, DATETIME('now'));";

  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);
  sqlite3_stmt* stmt = prepare_statement(priv->session, SQL_HISTORY_SET);
  if (stmt == NULL) {
    return;
  }

  if (sqlite3_bind_text(stmt, 1, input, -1, NULL) != SQLITE_OK) {
    sqlite3_finalize(stmt);
    girara_error("Failed to bind arguments.");
    return;
  }

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

static girara_list_t*
sqlite_io_read(GiraraInputHistoryIO* db)
{
  static const char SQL_HISTORY_GET[] =
    "SELECT line FROM history ORDER BY time";

  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);
  sqlite3_stmt* stmt = prepare_statement(priv->session, SQL_HISTORY_GET);
  if (stmt == NULL) {
    return NULL;
  }

  girara_list_t* list = girara_list_new2((girara_free_function_t) g_free);
  if (list == NULL) {
    sqlite3_finalize(stmt);
    return NULL;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    girara_list_append(list, g_strdup((const char*) sqlite3_column_text(stmt, 0)));
  }

  sqlite3_finalize(stmt);
  return list;
}

static girara_list_t*
sqlite_get_recent_files(zathura_database_t* db, int max)
{
  static const char SQL_HISTORY_GET[] =
    "SELECT file FROM fileinfo ORDER BY time DESC LIMIT ?";

  zathura_sqldatabase_private_t* priv = ZATHURA_SQLDATABASE_GET_PRIVATE(db);
  sqlite3_stmt* stmt = prepare_statement(priv->session, SQL_HISTORY_GET);
  if (stmt == NULL) {
    return NULL;
  }

  if (max < 0) {
    max = INT_MAX;
  }

  if (sqlite3_bind_int(stmt, 1, max) != SQLITE_OK) {
    sqlite3_finalize(stmt);
    girara_error("Failed to bind arguments.");
    return false;
  }

  girara_list_t* list = girara_list_new2((girara_free_function_t) g_free);
  if (list == NULL) {
    sqlite3_finalize(stmt);
    return NULL;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    girara_list_append(list, g_strdup((const char*) sqlite3_column_text(stmt, 0)));
  }

  sqlite3_finalize(stmt);
  return list;
}
