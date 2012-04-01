/* See LICENSE file for license and copyright information */

#define _POSIX_SOURCE

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <girara/utils.h>
#include <girara/datastructures.h>

#include "database-plain.h"

#define BOOKMARKS "bookmarks"
#define HISTORY "history"

#define KEY_PAGE "page"
#define KEY_OFFSET "offset"
#define KEY_SCALE "scale"
#define KEY_ROTATE "rotate"

#ifdef __GNU__
#include <sys/file.h>
#define file_lock_set(fd, cmd) flock(fd, cmd)
#else
#define file_lock_set(fd, cmd) \
  { \
  struct flock lock = { .l_type = cmd, .l_start = 0, .l_whence = SEEK_SET, .l_len = 0}; \
  fcntl(fd, F_SETLK, lock); \
  }
#endif

static void zathura_database_interface_init(ZathuraDatabaseInterface* iface);

G_DEFINE_TYPE_WITH_CODE(ZathuraPlainDatabase, zathura_plaindatabase, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE(ZATHURA_TYPE_DATABASE, zathura_database_interface_init))

static void plain_finalize(GObject* object);
static bool plain_add_bookmark(zathura_database_t* db, const char* file,
    zathura_bookmark_t* bookmark);
static bool plain_remove_bookmark(zathura_database_t* db, const char* file,
    const char* id);
static girara_list_t* plain_load_bookmarks(zathura_database_t* db,
    const char* file);
static bool plain_set_fileinfo(zathura_database_t* db, const char* file,
    unsigned int page, unsigned int offset, double scale, unsigned int rotation);
static bool plain_get_fileinfo(zathura_database_t* db, const char* file,
    unsigned int* page, unsigned int* offset, double* scale, unsigned int* rotation);
static void plain_set_property(GObject* object, guint prop_id,
    const GValue* value, GParamSpec* pspec);

/* forward declaration */
static bool zathura_db_check_file(const char* path);
static GKeyFile* zathura_db_read_key_file_from_file(const char* path);
static void zathura_db_write_key_file_to_file(const char* file, GKeyFile* key_file);
static void cb_zathura_db_watch_file(GFileMonitor* monitor, GFile* file, GFile*
    other_file, GFileMonitorEvent event, zathura_database_t* database);

typedef struct zathura_plaindatabase_private_s {
  char* bookmark_path;
  GKeyFile* bookmarks;
  GFileMonitor* bookmark_monitor;

  char* history_path;
  GKeyFile* history;
  GFileMonitor* history_monitor;
} zathura_plaindatabase_private_t;

#define ZATHURA_PLAINDATABASE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ZATHURA_TYPE_PLAINDATABASE, zathura_plaindatabase_private_t))

enum
{
  PROP_0,
  PROP_PATH
};

static char*
prepare_filename(const char* file)
{
  if (file == NULL) {
    return NULL;
  }

  if (strchr(file, '[') == NULL && strchr(file, ']') == NULL) {
    return g_strdup(file);
  }

  return g_base64_encode((const guchar*) file, strlen(file));
}

static void
zathura_database_interface_init(ZathuraDatabaseInterface* iface)
{
  /* initialize interface */
  iface->add_bookmark    = plain_add_bookmark;
  iface->remove_bookmark = plain_remove_bookmark;
  iface->load_bookmarks  = plain_load_bookmarks;
  iface->set_fileinfo    = plain_set_fileinfo;
  iface->get_fileinfo    = plain_get_fileinfo;
}

static void
zathura_plaindatabase_class_init(ZathuraPlainDatabaseClass* class)
{
  /* add private members */
  g_type_class_add_private(class, sizeof(zathura_plaindatabase_private_t));

  /* override methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = plain_finalize;
  object_class->set_property = plain_set_property;

  g_object_class_install_property(object_class, PROP_PATH,
    g_param_spec_string("path", "path", "path to directory where the bookmarks and history are locates",
      NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
zathura_plaindatabase_init(ZathuraPlainDatabase* db)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);

  priv->bookmark_path    = NULL;
  priv->bookmark_monitor = NULL;
  priv->bookmarks        = NULL;
  priv->history_path     = NULL;
  priv->history_monitor  = NULL;
  priv->history          = NULL;
}

zathura_database_t*
zathura_plaindatabase_new(const char* path)
{
  g_return_val_if_fail(path != NULL && strlen(path) != 0, NULL);

  zathura_database_t* db = g_object_new(ZATHURA_TYPE_PLAINDATABASE, "path", path, NULL);
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);
  if (priv->bookmark_path == NULL) {
    g_object_unref(db);
    return NULL;
  }

  return db;
}

static void
plain_db_init(ZathuraPlainDatabase* db, const char* dir)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);

  /* bookmarks */
  priv->bookmark_path = g_build_filename(dir, BOOKMARKS, NULL);
  if (zathura_db_check_file(priv->bookmark_path) == false) {
    goto error_free;
  }

  GFile* bookmark_file = g_file_new_for_path(priv->bookmark_path);
  if (bookmark_file != NULL) {
    priv->bookmark_monitor = g_file_monitor(bookmark_file, G_FILE_MONITOR_NONE, NULL, NULL);
  } else {
    goto error_free;
  }

  g_object_unref(bookmark_file);

  g_signal_connect(
      G_OBJECT(priv->bookmark_monitor),
      "changed",
      G_CALLBACK(cb_zathura_db_watch_file),
      db
  );

  priv->bookmarks = zathura_db_read_key_file_from_file(priv->bookmark_path);
  if (priv->bookmarks == NULL) {
    goto error_free;
  }

  /* history */
  priv->history_path = g_build_filename(dir, HISTORY, NULL);
  if (zathura_db_check_file(priv->history_path) == false) {
    goto error_free;
  }

  GFile* history_file = g_file_new_for_path(priv->history_path);
  if (history_file != NULL) {
    priv->history_monitor = g_file_monitor(history_file, G_FILE_MONITOR_NONE, NULL, NULL);
  } else {
    goto error_free;
  }

  g_object_unref(history_file);

  g_signal_connect(
      G_OBJECT(priv->history_monitor),
      "changed",
      G_CALLBACK(cb_zathura_db_watch_file),
      db
  );

  priv->history = zathura_db_read_key_file_from_file(priv->history_path);
  if (priv->history == NULL) {
    goto error_free;
  }

  return;

error_free:

  /* bookmarks */
  g_free(priv->bookmark_path);
  priv->bookmark_path = NULL;

  if (priv->bookmark_monitor != NULL) {
    g_object_unref(priv->bookmark_monitor);
    priv->bookmark_monitor = NULL;
  }

  if (priv->bookmarks != NULL) {
    g_key_file_free(priv->bookmarks);
    priv->bookmarks = NULL;
  }

  /* history */
  g_free(priv->history_path);
  priv->history_path = NULL;

  if (priv->history_monitor != NULL) {
    g_object_unref(priv->history_monitor);
    priv->history_monitor = NULL;
  }

  if (priv->history != NULL) {
    g_key_file_free(priv->history);
    priv->history = NULL;
  }
}

static void
plain_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
  ZathuraPlainDatabase* db = ZATHURA_PLAINDATABASE(object);

  switch (prop_id) {
    case PROP_PATH:
      plain_db_init(db, g_value_get_string(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
plain_finalize(GObject* object)
{
  ZathuraPlainDatabase* db = ZATHURA_PLAINDATABASE(object);
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);

  /* bookmarks */
  g_free(priv->bookmark_path);

  if (priv->bookmark_monitor != NULL) {
    g_object_unref(priv->bookmark_monitor);
  }

  if (priv->bookmarks != NULL) {
    g_key_file_free(priv->bookmarks);
  }

  /* history */
  g_free(priv->history_path);

  if (priv->history_monitor != NULL) {
    g_object_unref(priv->history_monitor);
  }

  if (priv->history != NULL) {
    g_key_file_free(priv->history);
  }

  G_OBJECT_CLASS(zathura_plaindatabase_parent_class)->finalize(object);
}

static bool
plain_add_bookmark(zathura_database_t* db, const char* file,
    zathura_bookmark_t* bookmark)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);
  if (priv->bookmarks == NULL || priv->bookmark_path == NULL ||
      bookmark->id == NULL) {
    return false;
  }

  char* name = prepare_filename(file);
  g_key_file_set_integer(priv->bookmarks, name, bookmark->id, bookmark->page);
  g_free(name);

  zathura_db_write_key_file_to_file(priv->bookmark_path, priv->bookmarks);

  return true;
}

static bool
plain_remove_bookmark(zathura_database_t* db, const char* file, const char*
    GIRARA_UNUSED(id))
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);
  if (priv->bookmarks == NULL || priv->bookmark_path == NULL) {
    return false;
  }

  char* name = prepare_filename(file);
  if (g_key_file_has_group(priv->bookmarks, name) == TRUE) {
    g_key_file_remove_group(priv->bookmarks, name, NULL);

    zathura_db_write_key_file_to_file(priv->bookmark_path, priv->bookmarks);
    g_free(name);

    return true;
  }
  g_free(name);

  return false;
}

static girara_list_t*
plain_load_bookmarks(zathura_database_t* db, const char* file)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);
  if (priv->bookmarks == NULL) {
    return NULL;
  }

  char* name = prepare_filename(file);
  if (g_key_file_has_group(priv->bookmarks, name) == FALSE) {
    g_free(name);
    return NULL;
  }

  girara_list_t* result = girara_sorted_list_new2((girara_compare_function_t)
      zathura_bookmarks_compare, (girara_free_function_t)
      zathura_bookmark_free);

  gsize length;
  char** keys = g_key_file_get_keys(priv->bookmarks, name, &length, NULL);
  if (keys == NULL) {
    girara_list_free(result);
    g_free(name);
    return NULL;
  }

  for (gsize i = 0; i < length; i++) {
    zathura_bookmark_t* bookmark = g_malloc0(sizeof(zathura_bookmark_t));

    bookmark->id   = g_strdup(keys[i]);
    bookmark->page = g_key_file_get_integer(priv->bookmarks, name, keys[i], NULL);

    girara_list_append(result, bookmark);
  }

  g_free(name);
  g_strfreev(keys);

  return result;
}

static bool
plain_set_fileinfo(zathura_database_t* db, const char* file, unsigned int
    page, unsigned int offset, double scale, unsigned int rotation)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);
  if (priv->history == NULL) {
    return false;
  }

  char* tmp = g_strdup_printf("%f", scale);
  char* name = prepare_filename(file);

  g_key_file_set_integer(priv->history, name, KEY_PAGE,   page);
  g_key_file_set_integer(priv->history, name, KEY_OFFSET, offset);
  g_key_file_set_string (priv->history, name, KEY_SCALE,  tmp);
  g_key_file_set_integer(priv->history, name, KEY_ROTATE, rotation);

  g_free(name);
  g_free(tmp);

  zathura_db_write_key_file_to_file(priv->history_path, priv->history);

  return true;
}

static bool
plain_get_fileinfo(zathura_database_t* db, const char* file, unsigned int*
    page, unsigned int* offset, double* scale, unsigned int* rotation)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);
  if (priv->history == NULL) {
    return false;
  }

  char* name = prepare_filename(file);
  if (g_key_file_has_group(priv->history, name) == FALSE) {
    return false;
  }

  *page     = g_key_file_get_integer(priv->history, name, KEY_PAGE, NULL);
  *offset   = g_key_file_get_integer(priv->history, name, KEY_OFFSET, NULL);
  *rotation = g_key_file_get_integer(priv->history, name, KEY_ROTATE, NULL);

  char* scale_string = g_key_file_get_string(priv->history, name, KEY_SCALE, NULL);
  *scale  = strtod(scale_string, NULL);
  g_free(scale_string);
  g_free(name);

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
  FILE* file = fopen(path, "rw");
  if (file == NULL) {
    return NULL;
  }

  GKeyFile* key_file = g_key_file_new();
  if (key_file == NULL) {
    fclose(file);
    return NULL;
  }

  /* read config file */
  file_lock_set(fileno(file), F_WRLCK);
  char* content = girara_file_read2(file);
  file_lock_set(fileno(file), F_UNLCK);
  fclose(file);
  if (content == NULL) {
    g_key_file_free(key_file);
    return NULL;
  }

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
  if (write(fd, content, strlen(content)) == 0) {
    girara_error("Failed to write to %s", file);
  }
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

  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(database);
  if (priv->bookmark_path && strcmp(priv->bookmark_path, path) == 0) {
    if (priv->bookmarks != NULL) {
      g_key_file_free(priv->bookmarks);
    }

    priv->bookmarks = zathura_db_read_key_file_from_file(priv->bookmark_path);
  } else if (priv->history_path && strcmp(priv->history_path, path) == 0) {
    if (priv->history != NULL) {
      g_key_file_free(priv->history);
    }

    priv->history = zathura_db_read_key_file_from_file(priv->history_path);
  }

  g_free(path);
}
