/* See LICENSE file for license and copyright information */

#define _POSIX_SOURCE
#define _XOPEN_SOURCE 500

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <girara/utils.h>
#include <girara/datastructures.h>
#include <girara/input-history.h>
#include <time.h>

#include "database-plain.h"
#include "utils.h"

#define BOOKMARKS                 "bookmarks"
#define HISTORY                   "history"
#define INPUT_HISTORY             "input-history"

#define KEY_PAGE                  "page"
#define KEY_OFFSET                "offset"
#define KEY_ZOOM                  "zoom"
#define KEY_ROTATE                "rotate"
#define KEY_PAGES_PER_ROW         "pages-per-row"
#define KEY_FIRST_PAGE_COLUMN     "first-page-column"
#define KEY_POSITION_X            "position-x"
#define KEY_POSITION_Y            "position-y"
#define KEY_JUMPLIST              "jumplist"
#define KEY_TIME                  "time"

#ifdef __GNU__
#include <sys/file.h>
#define file_lock_set(fd, cmd) flock(fd, cmd)
#else
static int
file_lock_set(int fd, short cmd)
{
  struct flock lock = { .l_type = cmd, .l_start = 0, .l_whence = SEEK_SET, .l_len = 0};
  return fcntl(fd, F_SETLK, lock);
}
#endif

static void zathura_database_interface_init(ZathuraDatabaseInterface* iface);
static void io_interface_init(GiraraInputHistoryIOInterface* iface);

G_DEFINE_TYPE_WITH_CODE(ZathuraPlainDatabase, zathura_plaindatabase, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(ZATHURA_TYPE_DATABASE, zathura_database_interface_init)
                        G_IMPLEMENT_INTERFACE(GIRARA_TYPE_INPUT_HISTORY_IO, io_interface_init))

static void           plain_dispose(GObject* object);
static void           plain_finalize(GObject* object);
static bool           plain_add_bookmark(zathura_database_t* db, const char* file, zathura_bookmark_t* bookmark);
static bool           plain_remove_bookmark(zathura_database_t* db, const char* file, const char* id);
static girara_list_t* plain_load_bookmarks(zathura_database_t* db, const char* file);
static girara_list_t* plain_load_jumplist(zathura_database_t* db, const char* file);
static bool           plain_save_jumplist(zathura_database_t* db, const char* file, girara_list_t* jumplist);
static bool           plain_set_fileinfo(zathura_database_t* db, const char* file, zathura_fileinfo_t* file_info);
static bool           plain_get_fileinfo(zathura_database_t* db, const char* file, zathura_fileinfo_t* file_info);
static void           plain_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void           plain_io_append(GiraraInputHistoryIO* db, const char*);
static girara_list_t* plain_io_read(GiraraInputHistoryIO* db);
static girara_list_t* plain_get_recent_files(zathura_database_t* db, int max, const char* basepath);

/* forward declaration */
static bool           zathura_db_check_file(const char* path);
static GKeyFile*      zathura_db_read_key_file_from_file(const char* path);
static void           zathura_db_write_key_file_to_file(const char* file, GKeyFile* key_file);
static void           cb_zathura_db_watch_file(GFileMonitor* monitor, GFile* file, GFile* other_file, GFileMonitorEvent event, zathura_database_t* database);

typedef struct zathura_plaindatabase_private_s {
  char* bookmark_path;
  GKeyFile* bookmarks;
  GFileMonitor* bookmark_monitor;

  char* history_path;
  GKeyFile* history;
  GFileMonitor* history_monitor;

  char* input_history_path;
} zathura_plaindatabase_private_t;

#define ZATHURA_PLAINDATABASE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ZATHURA_TYPE_PLAINDATABASE, zathura_plaindatabase_private_t))

enum {
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
  iface->add_bookmark     = plain_add_bookmark;
  iface->remove_bookmark  = plain_remove_bookmark;
  iface->load_bookmarks   = plain_load_bookmarks;
  iface->load_jumplist    = plain_load_jumplist;
  iface->save_jumplist    = plain_save_jumplist;
  iface->set_fileinfo     = plain_set_fileinfo;
  iface->get_fileinfo     = plain_get_fileinfo;
  iface->get_recent_files = plain_get_recent_files;
}

static void
io_interface_init(GiraraInputHistoryIOInterface* iface)
{
  /* initialize interface */
  iface->append = plain_io_append;
  iface->read = plain_io_read;
}

static void
zathura_plaindatabase_class_init(ZathuraPlainDatabaseClass* class)
{
  /* add private members */
  g_type_class_add_private(class, sizeof(zathura_plaindatabase_private_t));

  /* override methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->dispose      = plain_dispose;
  object_class->finalize     = plain_finalize;
  object_class->set_property = plain_set_property;

  g_object_class_install_property(object_class, PROP_PATH,
    g_param_spec_string("path", "path", "path to directory where the bookmarks and history are locates",
      NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
zathura_plaindatabase_init(ZathuraPlainDatabase* db)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);

  priv->bookmark_path         = NULL;
  priv->bookmark_monitor      = NULL;
  priv->bookmarks             = NULL;
  priv->history_path          = NULL;
  priv->history_monitor       = NULL;
  priv->history               = NULL;
  priv->input_history_path    = NULL;
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

  /* input history */
  priv->input_history_path = g_build_filename(dir, INPUT_HISTORY, NULL);
  if (zathura_db_check_file(priv->input_history_path) == false) {
    goto error_free;
  }

  return;

error_free:

  /* bookmarks */
  g_free(priv->bookmark_path);
  priv->bookmark_path = NULL;

  g_clear_object(&priv->bookmark_monitor);

  if (priv->bookmarks != NULL) {
    g_key_file_free(priv->bookmarks);
    priv->bookmarks = NULL;
  }

  /* history */
  g_free(priv->history_path);
  priv->history_path = NULL;

  g_clear_object(&priv->history_monitor);

  if (priv->history != NULL) {
    g_key_file_free(priv->history);
    priv->history = NULL;
  }

  /* input history */
  g_free(priv->input_history_path);
  priv->input_history_path = NULL;
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
plain_dispose(GObject* object)
{
  ZathuraPlainDatabase* db = ZATHURA_PLAINDATABASE(object);
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);

  g_clear_object(&priv->bookmark_monitor);
  g_clear_object(&priv->history_monitor);

  G_OBJECT_CLASS(zathura_plaindatabase_parent_class)->dispose(object);
}

static void
plain_finalize(GObject* object)
{
  ZathuraPlainDatabase* db = ZATHURA_PLAINDATABASE(object);
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);

  /* bookmarks */
  g_free(priv->bookmark_path);

  if (priv->bookmarks != NULL) {
    g_key_file_free(priv->bookmarks);
  }

  /* history */
  g_free(priv->history_path);

  if (priv->history != NULL) {
    g_key_file_free(priv->history);
  }

  /* input history */
  g_free(priv->input_history_path);

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
  char* val_list[] = {
    g_strdup_printf("%d", bookmark->page),
    g_try_malloc0(G_ASCII_DTOSTR_BUF_SIZE),
    g_try_malloc0(G_ASCII_DTOSTR_BUF_SIZE)
  };
  if (name == NULL || val_list[1] == NULL || val_list[2] == NULL) {
    g_free(name);
    for (unsigned int i = 0; i < LENGTH(val_list); ++i) {
      g_free(val_list[i]);
    }
    return false;
  }

  g_ascii_dtostr(val_list[1], G_ASCII_DTOSTR_BUF_SIZE, bookmark->x);
  g_ascii_dtostr(val_list[2], G_ASCII_DTOSTR_BUF_SIZE, bookmark->y);

  g_key_file_set_string_list(priv->bookmarks, name, bookmark->id, (const char**)val_list, LENGTH(val_list));

  for (unsigned int i = 0; i < LENGTH(val_list); ++i) {
    g_free(val_list[i]);
  }
  g_free(name);

  zathura_db_write_key_file_to_file(priv->bookmark_path, priv->bookmarks);

  return true;
}

static bool
plain_remove_bookmark(zathura_database_t* db, const char* file, const char* id)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);
  if (priv->bookmarks == NULL || priv->bookmark_path == NULL) {
    return false;
  }

  char* name = prepare_filename(file);
  if (g_key_file_has_group(priv->bookmarks, name) == TRUE) {
    if (g_key_file_remove_key(priv->bookmarks, name, id, NULL) == TRUE) {

      zathura_db_write_key_file_to_file(priv->bookmark_path, priv->bookmarks);
      g_free(name);

      return true;
    }
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

  girara_list_t* result = bookmarks_list_new();

  gsize num_keys;
  char** keys = g_key_file_get_keys(priv->bookmarks, name, &num_keys, NULL);
  if (keys == NULL) {
    girara_list_free(result);
    g_free(name);
    return NULL;
  }

  gsize num_vals = 0;

  for (gsize i = 0; i < num_keys; i++) {
    zathura_bookmark_t* bookmark = g_try_malloc0(sizeof(zathura_bookmark_t));
    if (bookmark == NULL) {
      continue;
    }

    bookmark->id    = g_strdup(keys[i]);
    char** val_list = g_key_file_get_string_list(priv->bookmarks, name, keys[i],
                                                 &num_vals, NULL);

    if (num_vals != 1 && num_vals != 3) {
      girara_error("Unexpected number of values.");
      g_free(bookmark);
      g_strfreev(val_list);
      continue;
    }

    bookmark->page = atoi(val_list[0]);

    if (num_vals == 3) {
      bookmark->x = g_ascii_strtod(val_list[1], NULL);
      bookmark->y = g_ascii_strtod(val_list[2], NULL);
    } else if (num_vals == 1) {
       bookmark->x = DBL_MIN;
       bookmark->y = DBL_MIN;
    }

    girara_list_append(result, bookmark);
    g_strfreev(val_list);
  }

  g_free(name);
  g_strfreev(keys);

  return result;
}

static girara_list_t*
get_jumplist_from_str(const char* str)
{
  g_return_val_if_fail(str != NULL, NULL);

  if (*str == '\0') {
    return girara_list_new2(g_free);
  }

  girara_list_t* result = girara_list_new2(g_free);
  char* copy = g_strdup(str);
  char* saveptr = NULL;
  char* token = strtok_r(copy, " ", &saveptr);

  while (token != NULL) {
    zathura_jump_t* jump = g_try_malloc0(sizeof(zathura_jump_t));
    if (jump == NULL) {
      continue;
    }

    jump->page = strtoul(token, NULL, 0);
    token = strtok_r(NULL, " ", &saveptr);
    if (token == NULL) {
      girara_warning("Could not parse jumplist information.");
      g_free(jump);
      break;
    }
    jump->x = g_ascii_strtod(token, NULL);

    token = strtok_r(NULL, " ", &saveptr);
    if (token == NULL) {
      girara_warning("Could not parse jumplist information.");
      g_free(jump);
      break;
    }
    jump->y = g_ascii_strtod(token, NULL);

    girara_list_append(result, jump);
    token = strtok_r(NULL, " ", &saveptr);
  }

  g_free(copy);

  return result;
}

static girara_list_t*
plain_load_jumplist(zathura_database_t* db, const char* file)
{
  g_return_val_if_fail(db != NULL && file != NULL, NULL);

  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);
  char* str_value = g_key_file_get_string(priv->history, file, KEY_JUMPLIST, NULL);

  if (str_value == NULL) {
    return girara_list_new2(g_free);
  }

  girara_list_t* list = get_jumplist_from_str(str_value);
  g_free(str_value);
  return list;
}

static void
jump_to_str(void* data, void* userdata)
{
  const zathura_jump_t* jump = data;
  GString* str_val           = userdata;

  char buffer[G_ASCII_DTOSTR_BUF_SIZE] = { '\0' };

  g_string_append_printf(str_val, "%d ", jump->page);
  g_string_append(str_val, g_ascii_dtostr(buffer, G_ASCII_DTOSTR_BUF_SIZE, jump->x));
  g_string_append_c(str_val, ' ');
  g_string_append(str_val, g_ascii_dtostr(buffer, G_ASCII_DTOSTR_BUF_SIZE, jump->y));
  g_string_append_c(str_val, ' ');
}

static bool
plain_save_jumplist(zathura_database_t* db, const char* file, girara_list_t* jumplist)
{
  g_return_val_if_fail(db != NULL && file != NULL && jumplist != NULL, false);

  GString* str_val = g_string_new(NULL);
  girara_list_foreach(jumplist, jump_to_str, str_val);

  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);

  g_key_file_set_string(priv->history, file, KEY_JUMPLIST, str_val->str);
  zathura_db_write_key_file_to_file(priv->history_path, priv->history);
  g_string_free(str_val, TRUE);

  return true;
}

static bool
plain_set_fileinfo(zathura_database_t* db, const char* file, zathura_fileinfo_t*
                   file_info)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);
  if (priv->history == NULL || file_info == NULL || file == NULL) {
    return false;
  }

  char* name = prepare_filename(file);

  g_key_file_set_integer(priv->history, name, KEY_PAGE,              file_info->current_page);
  g_key_file_set_integer(priv->history, name, KEY_OFFSET,            file_info->page_offset);
  g_key_file_set_double (priv->history, name, KEY_ZOOM,              file_info->zoom);
  g_key_file_set_integer(priv->history, name, KEY_ROTATE,            file_info->rotation);
  g_key_file_set_integer(priv->history, name, KEY_PAGES_PER_ROW,     file_info->pages_per_row);
  g_key_file_set_string(priv->history, name, KEY_FIRST_PAGE_COLUMN,  file_info->first_page_column_list);
  g_key_file_set_double (priv->history, name, KEY_POSITION_X,        file_info->position_x);
  g_key_file_set_double (priv->history, name, KEY_POSITION_Y,        file_info->position_y);
  g_key_file_set_integer(priv->history, name, KEY_TIME,              time(NULL));

  g_free(name);

  zathura_db_write_key_file_to_file(priv->history_path, priv->history);

  return true;
}

static bool
plain_get_fileinfo(zathura_database_t* db, const char* file, zathura_fileinfo_t*
                   file_info)
{
  if (db == NULL || file == NULL || file_info == NULL) {
    return false;
  }

  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);
  if (priv->history == NULL) {
    return false;
  }

  char* name = prepare_filename(file);
  if (g_key_file_has_group(priv->history, name) == FALSE) {
    g_free(name);
    return false;
  }

  file_info->current_page      = g_key_file_get_integer(priv->history, name, KEY_PAGE, NULL);
  file_info->page_offset       = g_key_file_get_integer(priv->history, name, KEY_OFFSET, NULL);
  file_info->zoom              = g_key_file_get_double (priv->history, name, KEY_ZOOM, NULL);
  file_info->rotation          = g_key_file_get_integer(priv->history, name, KEY_ROTATE, NULL);

  /* the following flags got introduced at a later point */
  if (g_key_file_has_key(priv->history, name, KEY_PAGES_PER_ROW, NULL) == TRUE) {
    file_info->pages_per_row     = g_key_file_get_integer(priv->history, name, KEY_PAGES_PER_ROW, NULL);
  }
  if (g_key_file_has_key(priv->history, name, KEY_FIRST_PAGE_COLUMN, NULL) == TRUE) {
    file_info->first_page_column_list = g_key_file_get_string(priv->history, name, KEY_FIRST_PAGE_COLUMN, NULL);
  }
  if (g_key_file_has_key(priv->history, name, KEY_POSITION_X, NULL) == TRUE) {
    file_info->position_x        = g_key_file_get_double(priv->history, name, KEY_POSITION_X, NULL);
  }
  if (g_key_file_has_key(priv->history, name, KEY_POSITION_Y, NULL) == TRUE) {
    file_info->position_y        = g_key_file_get_double(priv->history, name, KEY_POSITION_Y, NULL);
  }

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
    if (content == NULL)
    {
      g_key_file_free(key_file);
      return NULL;
    }
    g_strlcat(content, dummy_content, dummy_len + 1);
    contentlen = dummy_len;
  }

  GError* error = NULL;
  if (g_key_file_load_from_data(key_file, content, contentlen,
                                G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error) ==
      FALSE) {
    if (error->code != 1) { /* ignore empty file */
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
  int fd = open(file, O_RDWR | O_TRUNC);
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

static girara_list_t*
plain_io_read(GiraraInputHistoryIO* db)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);

  /* open file */
  FILE* file = fopen(priv->input_history_path, "r");
  if (file == NULL) {
    return NULL;
  }

  /* read input history file */
  file_lock_set(fileno(file), F_RDLCK);
  char* content = girara_file_read2(file);
  file_lock_set(fileno(file), F_UNLCK);
  fclose(file);

  girara_list_t* res = girara_list_new2(g_free);
  char** tmp = g_strsplit(content, "\n", 0);
  for (size_t i = 0; tmp[i] != NULL; ++i) {
    if (strlen(tmp[i]) == 0 || strchr(":/?", tmp[i][0]) == NULL) {
      continue;
    }
    girara_list_append(res, g_strdup(tmp[i]));
  }
  g_strfreev(tmp);
  free(content);

  return res;
}

static void
plain_io_append(GiraraInputHistoryIO* db, const char* input)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);

  /* open file */
  FILE* file = fopen(priv->input_history_path, "r+");
  if (file == NULL) {
    return;
  }

  /* read input history file */
  file_lock_set(fileno(file), F_WRLCK);
  char* content = girara_file_read2(file);

  rewind(file);
  if (ftruncate(fileno(file), 0) != 0) {
    free(content);
    file_lock_set(fileno(file), F_UNLCK);
    fclose(file);
    return;
  }

  char** tmp = g_strsplit(content, "\n", 0);
  free(content);

  /* write input history file */
  for (size_t i = 0; tmp[i] != NULL; ++i) {
    if (strlen(tmp[i]) == 0 || strchr(":/?", tmp[i][0]) == NULL || strcmp(tmp[i], input) == 0) {
      continue;
    }
    fprintf(file, "%s\n", tmp[i]);
  }
  g_strfreev(tmp);
  fprintf(file, "%s\n", input);

  file_lock_set(fileno(file), F_UNLCK);
  fclose(file);
}

static int
compare_time(const void* l, const void* r, void* data)
{
  const gchar* lhs = *(const gchar**) l;
  const gchar* rhs = *(const gchar**) r;
  GKeyFile* keyfile = data;

  time_t lhs_time = 0;
  time_t rhs_time = 0;

  if (g_key_file_has_key(keyfile, lhs, KEY_TIME, NULL) == TRUE) {
    lhs_time = g_key_file_get_uint64(keyfile, lhs, KEY_TIME, NULL);
  }
  if (g_key_file_has_key(keyfile, rhs, KEY_TIME, NULL) == TRUE) {
    rhs_time = g_key_file_get_uint64(keyfile, rhs, KEY_TIME, NULL);
  }

  if (lhs_time < rhs_time) {
    return 1;
  } else if (lhs_time > rhs_time) {
    return -1;
  }
  return 0;
}

static girara_list_t*
plain_get_recent_files(zathura_database_t* db, int max, const char* basepath)
{
  zathura_plaindatabase_private_t* priv = ZATHURA_PLAINDATABASE_GET_PRIVATE(db);

  girara_list_t* result = girara_list_new2(g_free);
  if (result == NULL) {
    return NULL;
  }

  gsize groups_size = 0;
  gchar** groups = g_key_file_get_groups(priv->history, &groups_size);

  if (groups_size > 0) {
    g_qsort_with_data(groups, groups_size, sizeof(gchar*), compare_time, priv->history);
  }

  const size_t basepath_len = basepath != NULL ? strlen(basepath) : 0;

  for (gsize s = 0; s != groups_size && max != 0; ++s) {
    if (basepath != NULL && strncmp(groups[s], basepath, basepath_len) != 0) {
      continue;
    }

    girara_list_append(result, g_strdup(groups[s]));
    --max;
  }
  g_strfreev(groups);

  return result;
}
