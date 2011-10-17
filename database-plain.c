/* See LICENSE file for license and copyright information */

#include <girara.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "database.h"

#define BOOKMARKS "bookmarks"
#define HISTORY "history"

#define KEY_PAGE "page"
#define KEY_OFFSET "offset"
#define KEY_SCALE "scale"

#define file_lock_set(fd, cmd) \
  { \
  struct flock lock = { .l_type = cmd, .l_start = 0, .l_whence = SEEK_SET, .l_len = 0}; \
  fcntl(fd, F_SETLK, lock); \
  }

/* forward declaration */
static bool zathura_db_check_file(const char* path);
static GKeyFile* zathura_db_read_key_file_from_file(const char* path);
static void zathura_db_write_key_file_to_file(const char* file, GKeyFile* key_file);
static void cb_zathura_db_watch_file(GFileMonitor* monitor, GFile* file, GFile*
    other_file, GFileMonitorEvent event, zathura_database_t* database);

struct zathura_database_s
{
  char* bookmark_path;
  GKeyFile* bookmarks;
  GFileMonitor* bookmark_monitor;

  char* history_path;
  GKeyFile* history;
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

  db->bookmarks = zathura_db_read_key_file_from_file(db->bookmark_path);
  if (db->bookmarks == NULL) {
    goto error_free;
  }

  /* history */
  db->history_path = g_build_filename(dir, HISTORY, NULL);
  if (db->history_path == NULL ||
      zathura_db_check_file(db->history_path) == false) {
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

  db->history = zathura_db_read_key_file_from_file(db->history_path);
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

  if (db->bookmark_monitor != NULL) {
    g_object_unref(db->bookmark_monitor);
  }

  if (db->bookmarks != NULL) {
    g_key_file_free(db->bookmarks);
  }

  /* history */
  g_free(db->history_path);

  if (db->history_monitor != NULL) {
    g_object_unref(db->history_monitor);
  }

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

  zathura_db_write_key_file_to_file(db->bookmark_path, db->bookmarks);

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

    zathura_db_write_key_file_to_file(db->bookmark_path, db->bookmarks);

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

  girara_list_t* result = girara_sorted_list_new2((girara_compare_function_t) zathura_bookmarks_compare,
      (girara_free_function_t) zathura_bookmark_free);
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

  char* tmp = g_strdup_printf("%f", scale);
  if (tmp == NULL) {
    return false;
  }

  g_key_file_set_integer(db->history, file, KEY_PAGE,   page);
  g_key_file_set_integer(db->history, file, KEY_OFFSET, offset);
  g_key_file_set_string (db->history, file, KEY_SCALE,  tmp);

  g_free(tmp);

  zathura_db_write_key_file_to_file(db->history_path, db->history);

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

  if (g_key_file_has_group(db->history, file) == FALSE) {
    return false;
  }

  *page   = g_key_file_get_integer(db->history, file, KEY_PAGE, NULL);
  *offset = g_key_file_get_integer(db->history, file, KEY_OFFSET, NULL);
  *scale  = strtof(g_key_file_get_string(db->history, file, KEY_SCALE, NULL), NULL);

  return true;
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
zathura_db_read_key_file_from_file(const char* path)
{
  if (path == NULL) {
    return NULL;
  }

  /* open file */
  int fd = open(path, O_RDWR);
  if (fd == -1) {
    return NULL;
  }

  GKeyFile* key_file = g_key_file_new();
  if (key_file == NULL) {
    close(fd);
    return NULL;
  }

  /* read config file */
  file_lock_set(fd, F_WRLCK);
  char* content = girara_file_read_from_fd(fd);
  if (content == NULL) {
    file_lock_set(fd, F_UNLCK);
    close(fd);
    return NULL;
  }
  file_lock_set(fd, F_UNLCK);

  close(fd);

  /* parse config file */
  size_t contentlen = strlen(content);
  if (contentlen == 0) {
    static const char dummy_content[] = "# nothing";
    static const size_t dummy_len = sizeof(dummy_content) - 1;

    free(content);
    content = malloc(sizeof(char) * (dummy_len + 1));
    content = memcpy(content, dummy_content, dummy_len + 1);
    contentlen = dummy_len;
  }

  GError* error = NULL;
  if (g_key_file_load_from_data(key_file, content, contentlen,
        G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error) ==
      FALSE) {
    if (error->code != 1) /* ignore empty file */ {
      free(content);
      g_key_file_free(key_file);
      g_error_free(error);
      return NULL;
    }

    g_error_free(error);
  }

  free(content);

  return key_file;
}

static void
zathura_db_write_key_file_to_file(const char* file, GKeyFile* key_file)
{
  if (file == NULL || key_file == NULL) {
    return;
  }

  gchar* content = g_key_file_to_data(key_file, NULL, NULL);
  if (content == NULL) {
    return;
  }

  /* open file */
  int fd = open(file, O_RDWR);
  if (fd == -1) {
    g_free(content);
    return;
  }

  file_lock_set(fd, F_WRLCK);
  write(fd, content, strlen(content));
  file_lock_set(fd, F_UNLCK);

  close(fd);

  g_free(content);
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
    database->bookmarks = zathura_db_read_key_file_from_file(database->bookmark_path);
  } else if (database->history_path && strcmp(database->history_path, path) == 0) {
    database->history = zathura_db_read_key_file_from_file(database->history_path);
  }
}
