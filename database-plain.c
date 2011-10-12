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

#define BOOKMARKS "bookmarks"
#define HISTORY "history"

typedef struct zathura_lock_s
{
  int sock;
  struct sockaddr_un sun;
} zathura_lock_t;

typedef struct zathura_db_file_info_s
{
  char* file;
  int page;
  int offset;
  float scale;
} zathura_db_file_info_t;

/* forward declaration */
static zathura_lock_t* zathura_lock_new(const char* name);
static void zathura_lock_free(zathura_lock_t* lock);
static void zathura_lock_lock(zathura_lock_t* lock);
static void zathura_lock_unlock(zathura_lock_t* lock);
static bool zathura_db_check_file(const char* path);
static GKeyFile* zathura_db_read_bookmarks_from_file(char* path);
static void zathura_db_write_bookmarks_to_file(const char* file, GKeyFile* bookmarks);
static girara_list_t* zathura_db_read_history_from_file(char* path);
static void zathura_db_write_history_to_file(const char* file, girara_list_t* urls);
static void cb_zathura_db_watch_file(GFileMonitor* monitor, GFile* file, GFile*
    other_file, GFileMonitorEvent event, zathura_database_t* database);
static void zathura_db_file_info_free(zathura_db_file_info_t* file_info);

struct zathura_database_s
{
  char* bookmark_path;
  zathura_lock_t* bookmark_lock;
  GKeyFile* bookmarks;
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

  g_signal_connect(
      G_OBJECT(db->bookmark_monitor),
      "changed",
      G_CALLBACK(cb_zathura_db_watch_file),
      db
  );

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

  g_signal_connect(
      G_OBJECT(db->history_monitor),
      "changed",
      G_CALLBACK(cb_zathura_db_watch_file),
      db
  );

  db->history = zathura_db_read_history_from_file(db->history_path);
  if (db->history == NULL) {
    goto error_free;
  }

  girara_list_set_free_function(db->history, (girara_free_function_t) zathura_db_file_info_free);

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

  if (db->bookmarks != NULL) {
    g_key_file_free(db->bookmarks);
  }

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
  if (db == NULL || db->bookmarks == NULL || db->bookmark_path == NULL || file
      == NULL || bookmark == NULL || bookmark->id == NULL) {
    return false;
  }

  g_key_file_set_integer(db->bookmarks, file, bookmark->id, bookmark->page);

  zathura_lock_lock(db->bookmark_lock);
  zathura_db_write_bookmarks_to_file(db->bookmark_path, db->bookmarks);
  zathura_lock_unlock(db->bookmark_lock);

  return true;
}

bool
zathura_db_remove_bookmark(zathura_database_t* db, const char* file, const char*
    id)
{
  if (db == NULL || db->bookmarks == NULL || db->bookmark_path == NULL || file
      == NULL || id == NULL) {
    return false;
  }

  if (g_key_file_has_group(db->bookmarks, file) == TRUE) {
    g_key_file_remove_group(db->bookmarks, file, NULL);

    zathura_lock_lock(db->bookmark_lock);
    zathura_db_write_bookmarks_to_file(db->bookmark_path, db->bookmarks);
    zathura_lock_unlock(db->bookmark_lock);

    return true;
  }

  return false;
}

girara_list_t*
zathura_db_load_bookmarks(zathura_database_t* db, const char* file)
{
  if (db == NULL || db->bookmarks == NULL || file == NULL) {
    return NULL;
  }

  if (g_key_file_has_group(db->bookmarks, file) == FALSE) {
    return NULL;
  }

  girara_list_t* result = girara_list_new();
  if (result == NULL) {
    return NULL;
  }

  girara_list_set_free_function(result, (girara_free_function_t) zathura_bookmark_free);

  gsize length;
  char** keys = g_key_file_get_keys(db->bookmarks, file, &length, NULL);
  if (keys == NULL) {
    girara_list_free(result);
    return NULL;
  }

  for (gsize i = 0; i < length; i++) {
    zathura_bookmark_t* bookmark = g_malloc0(sizeof(zathura_bookmark_t));
    if (bookmark == NULL) {
      continue;
    }

    bookmark->id   = g_strdup(keys[i]);
    bookmark->page = g_key_file_get_integer(db->bookmarks, file, keys[i], NULL);

    girara_list_append(result, bookmark);
  }

  return result;
}

bool
zathura_db_set_fileinfo(zathura_database_t* db, const char* file, unsigned int
    page, int offset, float scale)
{
  if (db == NULL || db->history == NULL || file == NULL) {
    return false;
  }

  /* search for existing entry */
  if (girara_list_size(db->history) > 0) {
    girara_list_iterator_t* iter = girara_list_iterator(db->history);

    do {
     zathura_db_file_info_t* file_info = (zathura_db_file_info_t*) girara_list_iterator_data(iter);

      if (file_info != NULL && strcmp(file_info->file, file) == 0) {
        file_info->page   = page;
        file_info->offset = offset;
        file_info->scale  = scale;

        /* write new history list */
        zathura_lock_lock(db->history_lock);
        zathura_db_write_history_to_file(db->history_path, db->history);
        zathura_lock_unlock(db->history_lock);

        return true;
      }
    } while (girara_list_iterator_next(iter) != NULL);

    girara_list_iterator_free(iter);
  }

  /* add new entry */
  zathura_db_file_info_t* file_info = calloc(1, sizeof(zathura_db_file_info_t));
  if (file_info == NULL) {
    return false;
  }

  file_info->file   = g_strdup(file);
  file_info->page   = page;
  file_info->offset = offset;
  file_info->scale  = scale;

  girara_list_append(db->history, file_info);

  /* write new history list */
  zathura_lock_lock(db->history_lock);
  zathura_db_write_history_to_file(db->history_path, db->history);
  zathura_lock_unlock(db->history_lock);

  return true;
}

bool
zathura_db_get_fileinfo(zathura_database_t* db, const char* file, unsigned int*
    page, int* offset, float* scale)
{
  if (db == NULL || db->history == NULL || file == NULL || page == NULL ||
      offset == NULL || scale == NULL) {
    return false;
  }

  if (girara_list_size(db->history) > 0) {
    girara_list_iterator_t* iter = girara_list_iterator(db->history);

    do {
     zathura_db_file_info_t* file_info = (zathura_db_file_info_t*) girara_list_iterator_data(iter);

      if (file_info != NULL && strcmp(file_info->file, file) == 0) {
        *page   = file_info->page;
        *offset = file_info->offset;
        *scale  = file_info->scale;
        return true;
      }
    } while (girara_list_iterator_next(iter) != NULL);

    girara_list_iterator_free(iter);
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

static GKeyFile*
zathura_db_read_bookmarks_from_file(char* path)
{
  if (path == NULL) {
    return NULL;
  }

  GKeyFile* key_file = g_key_file_new();
  if (key_file == NULL) {
    return NULL;
  }

  GError* error = NULL;
  if (g_key_file_load_from_file(key_file, path,
        G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error) ==
      FALSE) {

    if (error->code != 1) /* ignore empty file */ {
      g_key_file_free(key_file);
      return NULL;
      g_error_free(error);
    }

    g_error_free(error);
  }

  return key_file;
}

static void
zathura_db_write_bookmarks_to_file(const char* file, GKeyFile* bookmarks)
{
  if (file == NULL || bookmarks == NULL) {
    return;
  }

  gchar* content = g_key_file_to_data(bookmarks, NULL, NULL);
  if (content == NULL) {
    return;
  }

  if (g_file_set_contents(file, content, -1, NULL) == FALSE) {
    g_free(content);
    return;
  }

  g_free(content);
}

static girara_list_t*
zathura_db_read_history_from_file(char* path)
{
  if (path == NULL) {
    return NULL;
  }

  /* open file */
  FILE* file = girara_file_open(path, "r");
  if (file == NULL) {
    return NULL;
  }

  girara_list_t* list = girara_list_new();
  if (list == NULL) {
    fclose(file);
    return NULL;
  }

  /* read lines */
  char* line = NULL;
  while ((line = girara_file_read_line(file)) != NULL) {
    /* skip empty lines */
    if (strlen(line) == 0) {
      free(line);
      continue;
    }

    /* parse line */
    gchar** argv = NULL;
    gint    argc = 0;

    if (g_shell_parse_argv(line, &argc, &argv, NULL) != FALSE) {
      if (argc < 4) {
        g_strfreev(argv);
        free(line);
        continue;
      }

      zathura_db_file_info_t* file_info = calloc(1, sizeof(zathura_db_file_info_t));
      if (file_info == NULL) {
        g_strfreev(argv);
        free(line);
        continue;
      }

      file_info->file   = g_strdup(argv[0]);
      file_info->page   = atoi(argv[1]);
      file_info->offset = atoi(argv[2]);
      file_info->scale  = strtof(argv[3], NULL);

      girara_list_append(list, file_info);
    }

    g_strfreev(argv);
    free(line);
  }

  fclose(file);

  return list;
}

static void
zathura_db_write_history_to_file(const char* file, girara_list_t* history)
{
  if (file == NULL || history == NULL) {
    return;
  }

  /* open file */
  FILE* f = girara_file_open(file, "w");
  if (f == NULL) {
    return;
  }

  if (girara_list_size(history) > 0) {
    girara_list_iterator_t* iter = girara_list_iterator(history);
    do {
      zathura_db_file_info_t* file_info = (zathura_db_file_info_t*) girara_list_iterator_data(iter);
      if (file_info == NULL || file_info->file == NULL) {
        continue;
      }

      /* write path */
      char* tmp = g_shell_quote(file_info->file);
      fwrite(tmp, sizeof(char), strlen(tmp), f);
      g_free(tmp);

      /* write page number */
      tmp = g_strdup_printf(" %d %d %f", file_info->page, file_info->offset, file_info->scale);
      fwrite(tmp, sizeof(char), strlen(tmp), f);
      g_free(tmp);

      fwrite("\n", sizeof(char), 1, f);
    } while (girara_list_iterator_next(iter) != NULL);
    girara_list_iterator_free(iter);
  }

  fclose(f);
}

static void
cb_zathura_db_watch_file(GFileMonitor* UNUSED(monitor), GFile* file, GFile* UNUSED(other_file),
    GFileMonitorEvent event, zathura_database_t* database)
{
  if (event != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT || database == NULL) {
    return;
  }

  char* path = g_file_get_path(file);
  if (path == NULL) {
    return;
  }

  if (database->bookmark_path && strcmp(database->bookmark_path, path) == 0) {
    zathura_lock_lock(database->bookmark_lock);
    database->bookmarks = zathura_db_read_bookmarks_from_file(database->history_path);
    zathura_lock_unlock(database->bookmark_lock);
  } else if (database->history_path && strcmp(database->history_path, path) == 0) {
    girara_list_free(database->history);
    zathura_lock_lock(database->history_lock);
    database->history = zathura_db_read_history_from_file(database->history_path);
    zathura_lock_unlock(database->history_lock);
    girara_list_set_free_function(database->history, (girara_free_function_t) zathura_db_file_info_free);
  }
}

static void
zathura_db_file_info_free(zathura_db_file_info_t* file_info)
{
  if (file_info == NULL) {
    return;
  }

  g_free(file_info->file);
  free(file_info);
}
