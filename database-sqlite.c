/* See LICENSE file for license and copyright information */

#include <glib.h>
#include <sqlite3.h>
#include <girara.h>

#include "database.h"

struct zathura_database_s {
  sqlite3* session;
};

zathura_database_t*
zathura_db_init(const char* path) {
  zathura_database_t* db = g_malloc0(sizeof(zathura_database_t));
  
  /* create bookmarks database */
  static const char SQL_BOOKMARK_INIT[] =
    "CREATE TABLE IF NOT EXISTS bookmarks ("
      "file TEXT PRIMARY KEY,"
      "id TEXT,"
      "page INTEGER,"
      "UNIQUE(file, id));";

  static const char SQL_FILEINFO_INIT[] =
    "CREATE TABLE IF NOT EXISTS fileinfo ("
      "file TEXT PRIMARY KEY,"
      "page INTEGER,"
      "offset INTEGER,"
      "scale INTEGER);";

  if (sqlite3_open(path, &(db->session)) != SQLITE_OK) {
    girara_error("Could not open database: %s\n", path);
    zathura_db_free(db);
    return NULL;
  }

  if (sqlite3_exec(db->session, SQL_BOOKMARK_INIT, NULL, 0, NULL) != SQLITE_OK) {
    girara_error("Failed to initialize database: %s\n", path);
    zathura_db_free(db);
    return NULL;
  }

  if (sqlite3_exec(db->session, SQL_FILEINFO_INIT, NULL, 0, NULL) != SQLITE_OK) {
    girara_error("Failed to initialize database: %s\n", path);
    zathura_db_free(db);
    return NULL;
  }

  return db;
}

void
zathura_db_free(zathura_database_t* db) {
  if (db == NULL) {
    return;
  }

  sqlite3_close(db->session);
  g_free(db);
}

static sqlite3_stmt*
prepare_statement(sqlite3* session, const char* statement)
{
  if (session == NULL || statement == NULL) {
    return NULL;
  }

  const char* pz_tail   = NULL;
  sqlite3_stmt* pp_stmt = NULL;

  if (sqlite3_prepare(session, statement, -1, &pp_stmt, &pz_tail) != SQLITE_OK) {
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

bool
zathura_db_add_bookmark(zathura_database_t* db, const char* file, zathura_bookmark_t* bookmark)
{
  g_return_val_if_fail(db && file && bookmark, false);

  static const char SQL_BOOKMARK_ADD[] =
    "REPLACE INTO bookmarks (file, id, page) VALUES (?, ?, ?);";

  sqlite3_stmt* stmt = prepare_statement(db->session, SQL_BOOKMARK_ADD);
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
  return res == SQLITE_OK;
}

bool
zathura_db_remove_bookmark(zathura_database_t* db, const char* file, const char* id)
{
  g_return_val_if_fail(db && file && id, false);

  static const char SQL_BOOKMARK_ADD[] =
    "DELETE FROM bookmarks WHERE file = ? && id = ?;";

  sqlite3_stmt* stmt = prepare_statement(db->session, SQL_BOOKMARK_ADD);
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
  return res == SQLITE_OK;
}

girara_list_t*
zathura_db_load_bookmarks(zathura_database_t* db, const char* file)
{
  g_return_val_if_fail(db && file, NULL);

  static const char SQL_BOOKMARK_SELECT[] =
    "SELECT id, page FROM bookmarks WHERE file = ?;";

  sqlite3_stmt* stmt = prepare_statement(db->session, SQL_BOOKMARK_SELECT);
  if (stmt == NULL) {
    return NULL;
  }

  if (sqlite3_bind_text(stmt, 1, file, -1, NULL) != SQLITE_OK) {
    sqlite3_finalize(stmt);
    girara_error("Failed to bind arguments.");
    return NULL;
  }

  girara_list_t* result = girara_list_new();
  if (result == NULL) {
    sqlite3_finalize(stmt);
    return NULL;
  }
  girara_list_set_free_function(result, (girara_free_function_t) zathura_bookmark_free);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    zathura_bookmark_t* bookmark = g_malloc0(sizeof(zathura_bookmark_t));
    bookmark->id = g_strdup((const char*) sqlite3_column_text(stmt, 0));
    bookmark->page = sqlite3_column_int(stmt, 1);
    
    girara_list_append(result, bookmark);
  }
  sqlite3_finalize(stmt);
  return result;
}

