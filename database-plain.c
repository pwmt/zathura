/* See LICENSE file for license and copyright information */

#include <girara.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <sched.h>

#include "database.h"

typedef struct zathura_lock_s
{
  int sock;
  struct sockaddr_un sun;
} zathura_lock_t;

#define BOOKMARKS "bookmarks"
#define HISTORY "history"

/* forward declaration */
static zathura_lock_t* zathura_lock_new(const char* name);
static void zathura_lock_free(zathura_lock_t* lock);
static void zathura_lock_lock(zathura_lock_t* lock);
static void zathura_lock_unlock(zathura_lock_t* lock);
static bool zathura_db_check_file(const char* path);
static girara_list_t* zathura_db_read_bookmarks_from_file(char* path);
static girara_list_t* zathura_db_read_history_from_file(char* path);

struct zathura_database_s
{
  char* bookmark_path;
  zathura_lock_t* bookmark_lock;
  girara_list_t* bookmarks;
  GFileMonitor* bookmark_monitor;

  char* history_path;
  zathura_lock_t* history_lock;
  girara_list_t* history;
  GFileMonitor* history_monitor;
};

zathura_database_t*
zathura_db_init(const char* dir)
{
  if (dir == NULL) {
    goto error_ret;
  }

  zathura_database_t* db = calloc(1, sizeof(zathura_database_t));
  if (db == NULL) {
    goto error_ret;
  }

  /* bookmarks */
  db->bookmark_path = g_build_filename(dir, BOOKMARKS, NULL);
  if (db->bookmark_path == NULL ||
      zathura_db_check_file(db->bookmark_path) == false) {
    goto error_free;
  }

  db->bookmark_lock = zathura_lock_new("zathura-bookmarks");
  if (db->bookmark_lock == NULL) {
    goto error_free;
  }

  GFile* bookmark_file = g_file_new_for_path(db->bookmark_path);
  if (bookmark_file != NULL) {
    db->bookmark_monitor = g_file_monitor(bookmark_file, G_FILE_MONITOR_NONE, NULL, NULL);
  } else {
    goto error_free;
  }
  g_object_unref(bookmark_file);

  db->bookmarks = zathura_db_read_bookmarks_from_file(db->bookmark_path);
  if (db->bookmarks == NULL) {
    goto error_free;
  }

  /* history */
  db->history_path = g_build_filename(dir, HISTORY, NULL);
  if (db->history_path == NULL ||
      zathura_db_check_file(db->history_path) == false) {
    goto error_free;
  }

  db->history_lock = zathura_lock_new("zathura-history");
  if (db->history_lock == NULL) {
    goto error_free;
  }

  GFile* history_file = g_file_new_for_path(db->history_path);
  if (history_file != NULL) {
    db->history_monitor = g_file_monitor(history_file, G_FILE_MONITOR_NONE, NULL, NULL);
  } else {
    goto error_free;
  }
  g_object_unref(history_file);

  db->history = zathura_db_read_history_from_file(db->history_path);
  if (db->history == NULL) {
    goto error_free;
  }

  return db;

error_free:

  zathura_db_free(db);

error_ret:

  return NULL;
}

void
zathura_db_free(zathura_database_t* db)
{
  if (db == NULL) {
    return;
  }

  /* bookmarks */
  g_free(db->bookmark_path);
  zathura_lock_free(db->bookmark_lock);

  if (db->bookmark_monitor != NULL) {
    g_object_unref(db->bookmark_monitor);
  }

  girara_list_free(db->bookmarks);

  /* history */
  g_free(db->history_path);
  zathura_lock_free(db->history_lock);

  if (db->history_monitor != NULL) {
    g_object_unref(db->history_monitor);
  }

  girara_list_free(db->history);

  /* database */
  free(db);
}

bool
zathura_db_add_bookmark(zathura_database_t* db, const char* file,
    zathura_bookmark_t* bookmark)
{
  if (db == NULL || file == NULL || bookmark == NULL) {
    return false;
  }

  return false;
}

bool
zathura_db_remove_bookmark(zathura_database_t* db, const char* file, const char*
    id)
{
  if (db == NULL || file == NULL || id == NULL) {
    return false;
  }

  return false;
}

girara_list_t*
zathura_db_load_bookmarks(zathura_database_t* db, const char* file)
{
  if (db == NULL || file == NULL) {
    return NULL;
  }

  return NULL;
}

bool
zathura_db_set_fileinfo(zathura_database_t* db, const char* file, unsigned int
    page, int offset, float scale)
{
  if (db == NULL || file == NULL) {
    return NULL;
  }

  return false;
}

bool
zathura_db_get_fileinfo(zathura_database_t* db, const char* file, unsigned int*
    page, int* offset, float* scale)
{
  if (db == NULL || file == NULL || page == NULL || offset == NULL || scale == NULL) {
    return false;
  }

  return false;
}

static zathura_lock_t*
zathura_lock_new(const char* name)
{
  if (name == NULL) {
    return NULL;
  }

  zathura_lock_t* lock = calloc(1, sizeof(zathura_lock_t));
  if (lock == NULL) {
    return NULL;
  }

  strncpy(&(lock->sun).sun_path[1], name, sizeof(lock->sun.sun_path) - 2);
  lock->sun.sun_family = AF_UNIX;

  if ((lock->sock = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0) {
    free(lock);
    return NULL;
  }

  return lock;
}

static void
zathura_lock_free(zathura_lock_t* lock)
{
  if (lock == NULL) {
    return;
  }

  close(lock->sock);
  free(lock);
}

static void
zathura_lock_lock(zathura_lock_t* lock)
{
  if (lock == NULL) {
    return;
  }

  while (bind(lock->sock, (struct sockaddr*) &(lock->sun), sizeof(lock->sun)) < 0) {
    sched_yield();
  }
}

static void
zathura_lock_unlock(zathura_lock_t* lock)
{
  if (lock == NULL) {
    return;
  }

  close(lock->sock);
  lock->sock = socket(PF_UNIX, SOCK_DGRAM, 0);
}

static bool
zathura_db_check_file(const char* path)
{
  if (path == NULL) {
    return false;
  }

  if (g_file_test(path, G_FILE_TEST_EXISTS) == false) {
    FILE* file = fopen(path, "w");
    if (file != NULL) {
      fclose(file);
    } else {
      return false;
    }
  } else if (g_file_test(path, G_FILE_TEST_IS_REGULAR) == false) {
    return false;
  }

  return true;
}

static girara_list_t*
zathura_db_read_bookmarks_from_file(char* path)
{
  if (path == NULL) {
    return NULL;
  }

  return NULL;
}

static girara_list_t*
zathura_db_read_history_from_file(char* path)
{
  if (path == NULL) {
    return NULL;
  }

  return NULL;
}
