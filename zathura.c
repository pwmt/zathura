/* See LICENSE file for license and copyright information */

#define _BSD_SOURCE
#define _XOPEN_SOURCE 700

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/session.h>
#include <girara/statusbar.h>
#include <girara/settings.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "bookmarks.h"
#include "callbacks.h"
#include "config.h"
#ifdef WITH_SQLITE
#include "database-sqlite.h"
#endif
#include "database-plain.h"
#include "document.h"
#include "shortcuts.h"
#include "zathura.h"
#include "utils.h"
#include "marks.h"
#include "render.h"
#include "page.h"
#include "page-widget.h"
#include "plugin.h"

typedef struct zathura_document_info_s {
  zathura_t* zathura;
  const char* path;
  const char* password;
} zathura_document_info_t;

typedef struct page_set_delayed_s {
  zathura_t* zathura;
  unsigned int page;
} page_set_delayed_t;

typedef struct position_set_delayed_s {
  zathura_t* zathura;
  double position_x;
  double position_y;
} position_set_delayed_t;

static gboolean document_info_open(gpointer data);
static gboolean purge_pages(gpointer data);

/* function implementation */
zathura_t*
zathura_create(void)
{
  zathura_t* zathura = g_malloc0(sizeof(zathura_t));

  /* global settings */
  zathura->global.recolor            = false;
  zathura->global.update_page_number = true;

  /* plugins */
  zathura->plugins.manager = zathura_plugin_manager_new();
  if (zathura->plugins.manager == NULL) {
    goto error_out;
  }

  /* UI */
  if ((zathura->ui.session = girara_session_create()) == NULL) {
    goto error_out;
  }

  zathura->ui.session->global.data = zathura;

  return zathura;

error_out:

  zathura_free(zathura);

  return NULL;
}

bool
zathura_init(zathura_t* zathura)
{
  if (zathura == NULL) {
    return false;
  }

  /* create zathura (config/data) directory */
  if (g_mkdir_with_parents(zathura->config.config_dir, 0771) == -1) {
    girara_error("Could not create '%s': %s", zathura->config.config_dir, strerror(errno));
  }

  if (g_mkdir_with_parents(zathura->config.data_dir, 0771) == -1) {
    girara_error("Could not create '%s': %s", zathura->config.data_dir, strerror(errno));
  }

  /* load plugins */
  zathura_plugin_manager_load(zathura->plugins.manager);

  /* configuration */
  config_load_default(zathura);

  /* load global configuration files */
  char* config_path = girara_get_xdg_path(XDG_CONFIG_DIRS);
  girara_list_t* config_dirs = girara_split_path_array(config_path);
  ssize_t size = girara_list_size(config_dirs) - 1;
  for (; size >= 0; --size) {
    const char* dir = girara_list_nth(config_dirs, size);
    char* file = g_build_filename(dir, ZATHURA_RC, NULL);
    config_load_file(zathura, file);
    g_free(file);
  }
  girara_list_free(config_dirs);
  g_free(config_path);

  config_load_file(zathura, GLOBAL_RC);

  /* load local configuration files */
  char* configuration_file = g_build_filename(zathura->config.config_dir, ZATHURA_RC, NULL);
  config_load_file(zathura, configuration_file);
  g_free(configuration_file);

  /* UI */
  if (girara_session_init(zathura->ui.session, "zathura") == false) {
    goto error_free;
  }

  /* girara events */
  zathura->ui.session->events.buffer_changed  = cb_buffer_changed;
  zathura->ui.session->events.unknown_command = cb_unknown_command;

  /* page view */
#if (GTK_MAJOR_VERSION == 3)
  zathura->ui.page_widget = gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(zathura->ui.page_widget), TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(zathura->ui.page_widget), TRUE);
#else
  zathura->ui.page_widget = gtk_table_new(0, 0, TRUE);
#endif
  if (zathura->ui.page_widget == NULL) {
    goto error_free;
  }

  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.window), "size-allocate", G_CALLBACK(cb_view_resized), zathura);

  /* callbacks */
  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  g_signal_connect(G_OBJECT(view_vadjustment), "value-changed", G_CALLBACK(cb_view_vadjustment_value_changed), zathura);
  GtkAdjustment* view_hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  g_signal_connect(G_OBJECT(view_hadjustment), "value-changed", G_CALLBACK(cb_view_vadjustment_value_changed), zathura);

  /* page view alignment */
  zathura->ui.page_widget_alignment = gtk_alignment_new(0.5, 0.5, 0, 0);
  if (zathura->ui.page_widget_alignment == NULL) {
    goto error_free;
  }
  gtk_container_add(GTK_CONTAINER(zathura->ui.page_widget_alignment), zathura->ui.page_widget);

#if (GTK_MAJOR_VERSION == 3)
  gtk_widget_set_hexpand_set(zathura->ui.page_widget_alignment, TRUE);
  gtk_widget_set_hexpand(zathura->ui.page_widget_alignment, FALSE);
  gtk_widget_set_vexpand_set(zathura->ui.page_widget_alignment, TRUE);
  gtk_widget_set_vexpand(zathura->ui.page_widget_alignment, FALSE);
#endif


  gtk_widget_show(zathura->ui.page_widget);

  /* statusbar */
  zathura->ui.statusbar.file = girara_statusbar_item_add(zathura->ui.session, TRUE, TRUE, TRUE, NULL);
  if (zathura->ui.statusbar.file == NULL) {
    goto error_free;
  }

  zathura->ui.statusbar.buffer = girara_statusbar_item_add(zathura->ui.session, FALSE, FALSE, FALSE, NULL);
  if (zathura->ui.statusbar.buffer == NULL) {
    goto error_free;
  }

  zathura->ui.statusbar.page_number = girara_statusbar_item_add(zathura->ui.session, FALSE, FALSE, FALSE, NULL);
  if (zathura->ui.statusbar.page_number == NULL) {
    goto error_free;
  }

  girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, _("[No name]"));

  /* signals */
  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.window), "destroy", G_CALLBACK(cb_destroy), zathura);

  /* set page padding */
  int page_padding = 1;
  girara_setting_get(zathura->ui.session, "page-padding", &page_padding);

#if (GTK_MAJOR_VERSION == 3)
  gtk_grid_set_row_spacing(GTK_GRID(zathura->ui.page_widget), page_padding);
  gtk_grid_set_column_spacing(GTK_GRID(zathura->ui.page_widget), page_padding);
#else
  gtk_table_set_row_spacings(GTK_TABLE(zathura->ui.page_widget), page_padding);
  gtk_table_set_col_spacings(GTK_TABLE(zathura->ui.page_widget), page_padding);
#endif

  /* database */
  char* database = NULL;
  girara_setting_get(zathura->ui.session, "database", &database);

  if (g_strcmp0(database, "plain") == 0) {
    girara_info("Using plain database backend.");
    zathura->database = zathura_plaindatabase_new(zathura->config.data_dir);
#ifdef WITH_SQLITE
  } else if (g_strcmp0(database, "sqlite") == 0) {
    girara_info("Using sqlite database backend.");
    char* tmp = g_build_filename(zathura->config.data_dir, "bookmarks.sqlite", NULL);
    zathura->database = zathura_sqldatabase_new(tmp);
    g_free(tmp);
#endif
  } else {
    girara_error("Database backend '%s' is not supported.", database);
  }
  g_free(database);

  if (zathura->database == NULL) {
    girara_error("Unable to initialize database. Bookmarks won't be available.");
  }

  /* bookmarks */
  zathura->bookmarks.bookmarks = girara_sorted_list_new2((girara_compare_function_t) zathura_bookmarks_compare,
                                 (girara_free_function_t) zathura_bookmark_free);

  /* add even to purge old pages */
  int interval = 30;
  girara_setting_get(zathura->ui.session, "page-store-interval", &interval);
  g_timeout_add_seconds(interval, purge_pages, zathura);

  /* jumplist */

  zathura->jumplist.max_size = 20;
  girara_setting_get(zathura->ui.session, "jumplist-size", &(zathura->jumplist.max_size));

  zathura->jumplist.list = girara_list_new2(g_free);
  zathura->jumplist.size = 0;
  zathura->jumplist.cur = NULL;
  zathura_jumplist_append_jump(zathura);
  zathura->jumplist.cur = girara_list_iterator(zathura->jumplist.list);
  return true;

error_free:

  if (zathura->ui.page_widget != NULL) {
    g_object_unref(zathura->ui.page_widget);
  }

  if (zathura->ui.page_widget_alignment != NULL) {
    g_object_unref(zathura->ui.page_widget_alignment);
  }

  return false;
}

void
zathura_free(zathura_t* zathura)
{
  if (zathura == NULL) {
    return;
  }

  document_close(zathura, false);

  if (zathura->ui.session != NULL) {
    girara_session_destroy(zathura->ui.session);
  }

  /* stdin support */
  if (zathura->stdin_support.file != NULL) {
    g_unlink(zathura->stdin_support.file);
    g_free(zathura->stdin_support.file);
  }

  /* bookmarks */
  girara_list_free(zathura->bookmarks.bookmarks);

  /* database */
  zathura_db_free(zathura->database);

  /* free print settings */
  if (zathura->print.settings != NULL) {
    g_object_unref(zathura->print.settings);
  }

  if (zathura->print.page_setup != NULL) {
    g_object_unref(zathura->print.page_setup);
  }

  /* free registered plugins */
  zathura_plugin_manager_free(zathura->plugins.manager);

  /* free config variables */
  g_free(zathura->config.config_dir);
  g_free(zathura->config.data_dir);

  /* free jumplist */
  if (zathura->jumplist.list != NULL) {
    girara_list_free(zathura->jumplist.list);
  }

  if (zathura->jumplist.cur != NULL) {
    girara_list_iterator_free(zathura->jumplist.cur);
  }

  g_free(zathura);
}

void
#if (GTK_MAJOR_VERSION == 2)
zathura_set_xid(zathura_t* zathura, GdkNativeWindow xid)
#else
zathura_set_xid(zathura_t* zathura, Window xid)
#endif
{
  g_return_if_fail(zathura != NULL);

  zathura->ui.session->gtk.embed = xid;
}

void
zathura_set_config_dir(zathura_t* zathura, const char* dir)
{
  g_return_if_fail(zathura != NULL);

  if (dir != NULL) {
    zathura->config.config_dir = g_strdup(dir);
  } else {
    gchar* path = girara_get_xdg_path(XDG_CONFIG);
    zathura->config.config_dir = g_build_filename(path, "zathura", NULL);
    g_free(path);
  }
}

void
zathura_set_data_dir(zathura_t* zathura, const char* dir)
{
  if (dir != NULL) {
    zathura->config.data_dir = g_strdup(dir);
  } else {
    gchar* path = girara_get_xdg_path(XDG_DATA);
    zathura->config.data_dir = g_build_filename(path, "zathura", NULL);
    g_free(path);
  }

  g_return_if_fail(zathura != NULL);
}

void
zathura_set_plugin_dir(zathura_t* zathura, const char* dir)
{
  g_return_if_fail(zathura != NULL);
  g_return_if_fail(zathura->plugins.manager != NULL);

  if (dir != NULL) {
    girara_list_t* paths = girara_split_path_array(dir);
    GIRARA_LIST_FOREACH(paths, char*, iter, path)
    zathura_plugin_manager_add_dir(zathura->plugins.manager, path);
    GIRARA_LIST_FOREACH_END(paths, char*, iter, path);
    girara_list_free(paths);
  } else {
#ifdef ZATHURA_PLUGINDIR
    girara_list_t* paths = girara_split_path_array(ZATHURA_PLUGINDIR);
    GIRARA_LIST_FOREACH(paths, char*, iter, path)
    zathura_plugin_manager_add_dir(zathura->plugins.manager, path);
    GIRARA_LIST_FOREACH_END(paths, char*, iter, path);
    girara_list_free(paths);
#endif
  }

}

void
zathura_set_synctex_editor_command(zathura_t* zathura, const char* command)
{
  g_return_if_fail(zathura != NULL);

  if (zathura->synctex.editor != NULL) {
    g_free(zathura->synctex.editor);
  }

  if (command != NULL) {
    zathura->synctex.editor = g_strdup(command);
  } else {
    zathura->synctex.editor = NULL;
  }
}

void
zathura_set_syntex(zathura_t* zathura, bool value)
{
  g_return_if_fail(zathura != NULL);
  g_return_if_fail(zathura->ui.session != NULL);

  girara_setting_set(zathura->ui.session, "synctex", &value);
}

void
zathura_set_argv(zathura_t* zathura, char** argv)
{
  g_return_if_fail(zathura != NULL);

  zathura->global.arguments = argv;
}

static gchar*
prepare_document_open_from_stdin(zathura_t* zathura)
{
  g_return_val_if_fail(zathura, NULL);

  GError* error = NULL;
  gchar* file = NULL;
  gint handle = g_file_open_tmp("zathura.stdin.XXXXXX", &file, &error);
  if (handle == -1) {
    if (error != NULL) {
      girara_error("Can not create temporary file: %s", error->message);
      g_error_free(error);
    }
    return NULL;
  }

  // read from stdin and dump to temporary file
  int stdinfno = fileno(stdin);
  if (stdinfno == -1) {
    girara_error("Can not read from stdin.");
    close(handle);
    g_unlink(file);
    g_free(file);
    return NULL;
  }

  char buffer[BUFSIZ];
  ssize_t count = 0;
  while ((count = read(stdinfno, buffer, BUFSIZ)) > 0) {
    if (write(handle, buffer, count) != count) {
      girara_error("Can not write to temporary file: %s", file);
      close(handle);
      g_unlink(file);
      g_free(file);
      return NULL;
    }
  }

  close(handle);

  if (count != 0) {
    girara_error("Can not read from stdin.");
    g_unlink(file);
    g_free(file);
    return NULL;
  }

  return file;
}

static gboolean
document_info_open(gpointer data)
{
  zathura_document_info_t* document_info = data;
  g_return_val_if_fail(document_info != NULL, FALSE);

  if (document_info->zathura != NULL && document_info->path != NULL) {
    char* file = NULL;
    if (g_strcmp0(document_info->path, "-") == 0) {
      file = prepare_document_open_from_stdin(document_info->zathura);
      if (file == NULL) {
        girara_notify(document_info->zathura->ui.session, GIRARA_ERROR,
                      "Could not read file from stdin and write it to a temporary file.");
      } else {
        document_info->zathura->stdin_support.file = g_strdup(file);
      }
    } else {
      file = g_strdup(document_info->path);
    }

    if (file != NULL) {
      document_open(document_info->zathura, file, document_info->password);
      g_free(file);
    }
  }

  g_free(document_info);
  return FALSE;
}

bool
document_open(zathura_t* zathura, const char* path, const char* password)
{
  if (zathura == NULL || zathura->plugins.manager == NULL || path == NULL) {
    goto error_out;
  }

  zathura_error_t error = ZATHURA_ERROR_OK;
  zathura_document_t* document = zathura_document_open(zathura->plugins.manager, path, password, &error);

  if (document == NULL) {
    if (error == ZATHURA_ERROR_INVALID_PASSWORD) {
      zathura_password_dialog_info_t* password_dialog_info = malloc(sizeof(zathura_password_dialog_info_t));
      if (password_dialog_info != NULL) {
        password_dialog_info->zathura = zathura;

        if (path != NULL) {
          password_dialog_info->path = g_strdup(path);
          girara_dialog(zathura->ui.session, "Enter password:", true, NULL,
                        (girara_callback_inputbar_activate_t) cb_password_dialog, password_dialog_info);
          goto error_out;
        } else {
          free(password_dialog_info);
        }
      }
      goto error_out;
    }
    goto error_out;
  }

  const char* file_path        = zathura_document_get_path(document);
  unsigned int number_of_pages = zathura_document_get_number_of_pages(document);

  /* read history file */
  zathura_fileinfo_t file_info = { 0, 0, 1, 0, 0, 0, 0, 0 };
  bool known_file = zathura_db_get_fileinfo(zathura->database, file_path, &file_info);

  /* set page offset */
  zathura_document_set_page_offset(document, file_info.page_offset);

  /* check for valid scale value */
  if (file_info.scale <= FLT_EPSILON) {
    girara_warning("document info: '%s' has non positive scale", file_path);
    zathura_document_set_scale(document, 1);
  } else {
    zathura_document_set_scale(document, file_info.scale);
  }

  /* check current page number */
  if (file_info.current_page > number_of_pages) {
    girara_warning("document info: '%s' has an invalid page number", file_path);
    zathura_document_set_current_page_number(document, 0);
  } else {
    zathura_document_set_current_page_number(document, file_info.current_page);
  }

  /* check for valid rotation */
  if (file_info.rotation % 90 != 0) {
    girara_warning("document info: '%s' has an invalid rotation", file_path);
    zathura_document_set_rotation(document, 0);
  } else {
    zathura_document_set_rotation(document, file_info.rotation % 360);
  }

  /* jump to first page if setting enabled */
  bool always_first_page = false;
  girara_setting_get(zathura->ui.session, "open-first-page", &always_first_page);
  if (always_first_page == true) {
    zathura_document_set_current_page_number(document, 0);
  }

  /* apply open adjustment */
  char* adjust_open = "best-fit";
  if (known_file == false && girara_setting_get(zathura->ui.session, "adjust-open", &(adjust_open)) == true) {
    if (g_strcmp0(adjust_open, "best-fit") == 0) {
      zathura_document_set_adjust_mode(document, ZATHURA_ADJUST_BESTFIT);
    } else if (g_strcmp0(adjust_open, "width") == 0) {
      zathura_document_set_adjust_mode(document, ZATHURA_ADJUST_WIDTH);
    } else {
      zathura_document_set_adjust_mode(document, ZATHURA_ADJUST_NONE);
    }
    g_free(adjust_open);
  } else {
    zathura_document_set_adjust_mode(document, ZATHURA_ADJUST_NONE);
  }

  /* update statusbar */
  girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, file_path);

  /* install file monitor */
  gchar* file_uri = g_filename_to_uri(file_path, NULL, NULL);
  if (file_uri == NULL) {
    goto error_free;
  }

  if (zathura->file_monitor.file == NULL) {
    zathura->file_monitor.file = g_file_new_for_uri(file_uri);
    if (zathura->file_monitor.file == NULL) {
      goto error_free;
    }
  }

  if (zathura->file_monitor.monitor == NULL) {
    zathura->file_monitor.monitor = g_file_monitor_file(zathura->file_monitor.file, G_FILE_MONITOR_NONE, NULL, NULL);
    if (zathura->file_monitor.monitor == NULL) {
      goto error_free;
    }
    g_signal_connect(G_OBJECT(zathura->file_monitor.monitor), "changed", G_CALLBACK(cb_file_monitor), zathura->ui.session);
  }

  if (zathura->file_monitor.file_path == NULL) {
    zathura->file_monitor.file_path = g_strdup(file_path);
    if (zathura->file_monitor.file_path == NULL) {
      goto error_free;
    }
  }

  if (password != NULL) {
    g_free(zathura->file_monitor.password);
    zathura->file_monitor.password = g_strdup(password);
    if (zathura->file_monitor.password == NULL) {
      goto error_free;
    }
  }

  /* create marks list */
  zathura->global.marks = girara_list_new2((girara_free_function_t) mark_free);
  if (zathura->global.marks == NULL) {
    goto error_free;
  }

  zathura->document = document;

  /* create blank pages */
  zathura->pages = calloc(number_of_pages, sizeof(GtkWidget*));
  if (zathura->pages == NULL) {
    goto error_free;
  }

  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(document, page_id);
    if (page == NULL) {
      goto error_free;
    }

    GtkWidget* page_widget = zathura_page_widget_new(zathura, page);
    if (page_widget == NULL) {
      goto error_free;
    }

    zathura->pages[page_id] = page_widget;

    /* set widget size */
    unsigned int page_height = 0;
    unsigned int page_width  = 0;
    page_calc_height_width(page, &page_height, &page_width, true);

    gtk_widget_set_size_request(page_widget, page_width, page_height);
  }

  /* view mode */
  int pages_per_row = 1;
  int first_page_column = 1;
  if (file_info.pages_per_row > 0) {
    pages_per_row = file_info.pages_per_row;
  } else {
    girara_setting_get(zathura->ui.session, "pages-per-row", &pages_per_row);
  }

  if (file_info.first_page_column > 0) {
    first_page_column = file_info.first_page_column;
  } else {
    girara_setting_get(zathura->ui.session, "first-page-column", &first_page_column);
  }

  girara_setting_set(zathura->ui.session, "pages-per-row", &pages_per_row);
  girara_setting_set(zathura->ui.session, "first-page-column", &first_page_column);
  page_widget_set_mode(zathura, pages_per_row, first_page_column);

  girara_set_view(zathura->ui.session, zathura->ui.page_widget_alignment);

  /* threads */
  zathura->sync.render_thread = render_init(zathura);

  if (zathura->sync.render_thread == NULL) {
    goto error_free;
  }

  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    gtk_widget_realize(zathura->pages[page_id]);
  }

  /* bookmarks */
  zathura_bookmarks_load(zathura, file_path);

  /* update title */
  bool basename_only = false;
  girara_setting_get(zathura->ui.session, "window-title-basename", &basename_only);
  if (basename_only == false) {
    girara_set_window_title(zathura->ui.session, file_path);
  } else {
    char* tmp = g_path_get_basename(file_path);
    girara_set_window_title(zathura->ui.session, tmp);
    g_free(tmp);
  }

  g_free(file_uri);

  /* adjust window */
  girara_argument_t argument = { zathura_document_get_adjust_mode(document), NULL };
  sc_adjust_window(zathura->ui.session, &argument, NULL, 0);

  /* set position */
  if (file_info.position_x != 0 || file_info.position_y != 0) {
    position_set_delayed(zathura, file_info.position_x, file_info.position_y);
  } else {
    page_set_delayed(zathura, zathura_document_get_current_page_number(document));
    cb_view_vadjustment_value_changed(NULL, zathura);
  }

  return true;

error_free:

  if (file_uri != NULL) {
    g_free(file_uri);
  }

  zathura_document_free(document);

error_out:

  return false;
}

void
document_open_idle(zathura_t* zathura, const char* path, const char* password)
{
  if (zathura == NULL || path == NULL) {
    return;
  }

  zathura_document_info_t* document_info = g_malloc0(sizeof(zathura_document_info_t));

  document_info->zathura  = zathura;
  document_info->path     = path;
  document_info->password = password;

  gdk_threads_add_idle(document_info_open, document_info);
}

bool
document_save(zathura_t* zathura, const char* path, bool overwrite)
{
  g_return_val_if_fail(zathura, false);
  g_return_val_if_fail(zathura->document, false);
  g_return_val_if_fail(path, false);

  gchar* file_path = girara_fix_path(path);
  if ((overwrite == false) && g_file_test(file_path, G_FILE_TEST_EXISTS)) {
    girara_error("File already exists: %s. Use :write! to overwrite it.", file_path);
    g_free(file_path);
    return false;
  }

  zathura_error_t error = zathura_document_save_as(zathura->document, file_path);
  g_free(file_path);

  return (error == ZATHURA_ERROR_OK) ? true : false;
}

static void
remove_page_from_table(GtkWidget* page, gpointer permanent)
{
  if (permanent == false) {
    g_object_ref(G_OBJECT(page));
  }

  gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(page)), page);
}

bool
document_close(zathura_t* zathura, bool keep_monitor)
{
  if (zathura == NULL || zathura->document == NULL) {
    return false;
  }

  /* remove monitor */
  if (keep_monitor == false) {
    if (zathura->file_monitor.monitor != NULL) {
      g_file_monitor_cancel(zathura->file_monitor.monitor);
      g_object_unref(zathura->file_monitor.monitor);
      zathura->file_monitor.monitor = NULL;
    }

    if (zathura->file_monitor.file != NULL) {
      g_object_unref(zathura->file_monitor.file);
      zathura->file_monitor.file = NULL;
    }

    if (zathura->file_monitor.file_path != NULL) {
      g_free(zathura->file_monitor.file_path);
      zathura->file_monitor.file_path = NULL;
    }

    if (zathura->file_monitor.password != NULL) {
      g_free(zathura->file_monitor.password);
      zathura->file_monitor.password = NULL;
    }
  }

  /* remove marks */
  if (zathura->global.marks != NULL) {
    girara_list_free(zathura->global.marks);
    zathura->global.marks = NULL;
  }

  /* store file information */
  const char* path = zathura_document_get_path(zathura->document);

  zathura_fileinfo_t file_info = { 0, 0, 1, 0, 1, 1, 0, 0 };
  file_info.current_page = zathura_document_get_current_page_number(zathura->document);
  file_info.page_offset  = zathura_document_get_page_offset(zathura->document);
  file_info.scale        = zathura_document_get_scale(zathura->document);
  file_info.rotation     = zathura_document_get_rotation(zathura->document);

  girara_setting_get(zathura->ui.session, "pages-per-row", &(file_info.pages_per_row));
  girara_setting_get(zathura->ui.session, "first-page-column", &(file_info.first_page_column));

  /* get position */
  GtkScrolledWindow *window = GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view);
  GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(window);
  GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment(window);

  file_info.position_x = gtk_adjustment_get_value(hadjustment);
  file_info.position_y = gtk_adjustment_get_value(vadjustment);

  /* save file info */
  zathura_db_set_fileinfo(zathura->database, path, &file_info);

  /* release render thread */
  render_free(zathura->sync.render_thread);
  zathura->sync.render_thread = NULL;

  /* remove widgets */
  gtk_container_foreach(GTK_CONTAINER(zathura->ui.page_widget), remove_page_from_table, (gpointer) 1);
  for (unsigned int i = 0; i < zathura_document_get_number_of_pages(zathura->document); i++) {
    g_object_unref(zathura->pages[i]);
  }
  free(zathura->pages);
  zathura->pages = NULL;

  /* remove document */
  zathura_document_free(zathura->document);
  zathura->document = NULL;

  /* remove index */
  if (zathura->ui.index != NULL) {
    g_object_ref_sink(zathura->ui.index);
    zathura->ui.index = NULL;
  }

  gtk_widget_hide(zathura->ui.page_widget);

  statusbar_page_number_update(zathura);

  if (zathura->ui.session != NULL && zathura->ui.statusbar.file != NULL) {
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, _("[No name]"));
  }

  /* update title */
  girara_set_window_title(zathura->ui.session, "zathura");

  return true;
}

static gboolean
page_set_delayed_impl(gpointer data)
{
  page_set_delayed_t* p = data;
  page_set(p->zathura, p->page);

  g_free(p);
  return FALSE;
}

bool
page_set_delayed(zathura_t* zathura, unsigned int page_id)
{
  if (zathura == NULL || zathura->document == NULL ||
      (page_id >= zathura_document_get_number_of_pages(zathura->document))) {
    return false;
  }

  page_set_delayed_t* p = g_malloc(sizeof(page_set_delayed_t));
  p->zathura = zathura;
  p->page = page_id;
  gdk_threads_add_idle(page_set_delayed_impl, p);
  return true;
}

bool
page_set(zathura_t* zathura, unsigned int page_id)
{
  if (zathura == NULL || zathura->document == NULL) {
    goto error_out;
  }

  /* render page */
  zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);

  if (page == NULL) {
    goto error_out;
  }

  zathura_document_set_current_page_number(zathura->document, page_id);
  zathura->global.update_page_number = false;

  page_offset_t offset;
  page_calculate_offset(zathura, page, &offset);

  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  GtkAdjustment* view_hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  set_adjustment(view_hadjustment, offset.x);
  set_adjustment(view_vadjustment, offset.y);

  statusbar_page_number_update(zathura);

  return true;

error_out:

  return false;
}

void
statusbar_page_number_update(zathura_t* zathura)
{
  if (zathura == NULL || zathura->ui.statusbar.page_number == NULL) {
    return;
  }

  unsigned int number_of_pages     = zathura_document_get_number_of_pages(zathura->document);
  unsigned int current_page_number = zathura_document_get_current_page_number(zathura->document);

  if (zathura->document != NULL) {
    char* page_number_text = g_strdup_printf("[%d/%d]", current_page_number + 1, number_of_pages);
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.page_number, page_number_text);
    g_free(page_number_text);
  } else {
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.page_number, "");
  }
}

void
page_widget_set_mode(zathura_t* zathura, unsigned int pages_per_row, unsigned int first_page_column)
{
  /* show at least one page */
  if (pages_per_row == 0) {
    pages_per_row = 1;
  }

  /* ensure: 0 < first_page_column <= pages_per_row */
  if (first_page_column < 1) {
    first_page_column = 1;
  }

  if (first_page_column > pages_per_row) {
    first_page_column = ((first_page_column - 1) % pages_per_row) + 1;
  }

  if (zathura->document == NULL) {
    return;
  }

  gtk_container_foreach(GTK_CONTAINER(zathura->ui.page_widget), remove_page_from_table, (gpointer)0);

  unsigned int number_of_pages     = zathura_document_get_number_of_pages(zathura->document);
#if (GTK_MAJOR_VERSION == 3)
#else
  gtk_table_resize(GTK_TABLE(zathura->ui.page_widget), ceil((number_of_pages + first_page_column - 1) / pages_per_row), pages_per_row);
#endif

  for (unsigned int i = 0; i < number_of_pages; i++) {
    int x = (i + first_page_column - 1) % pages_per_row;
    int y = (i + first_page_column - 1) / pages_per_row;

    zathura_page_t* page   = zathura_document_get_page(zathura->document, i);
    GtkWidget* page_widget = zathura_page_get_widget(zathura, page);
#if (GTK_MAJOR_VERSION == 3)
    gtk_grid_attach(GTK_GRID(zathura->ui.page_widget), page_widget, x, y, 1, 1);
#else
    gtk_table_attach(GTK_TABLE(zathura->ui.page_widget), page_widget, x, x + 1, y, y + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
#endif
  }

  gtk_widget_show_all(zathura->ui.page_widget);
}

static
gboolean purge_pages(gpointer data)
{
  zathura_t* zathura = data;
  if (zathura == NULL || zathura->document == NULL) {
    return TRUE;
  }

  int threshold = 0;
  girara_setting_get(zathura->ui.session, "page-store-threshold", &threshold);
  if (threshold <= 0) {
    return TRUE;
  }

  girara_debug("purging pages ...");
  unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page   = zathura_document_get_page(zathura->document, page_id);
    GtkWidget* page_widget = zathura_page_get_widget(zathura, page);
    zathura_page_widget_purge_unused(ZATHURA_PAGE(page_widget), threshold);
  }
  return TRUE;
}

static gboolean
position_set_delayed_impl(gpointer data)
{
  position_set_delayed_t* p = (position_set_delayed_t*) data;

  GtkScrolledWindow *window = GTK_SCROLLED_WINDOW(p->zathura->ui.session->gtk.view);
  GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(window);
  GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment(window);

  set_adjustment(hadjustment, p->position_x);
  set_adjustment(vadjustment, p->position_y);

  g_free(p);

  return FALSE;
}

bool
position_set_delayed(zathura_t* zathura, double position_x, double position_y)
{
  position_set_delayed_t* p = g_malloc0(sizeof(position_set_delayed_t));

  p->zathura    = zathura;
  p->position_x = position_x;
  p->position_y = position_y;

  gdk_threads_add_idle(position_set_delayed_impl, p);

  return FALSE;
}


zathura_jump_t*
zathura_jumplist_current(zathura_t* zathura)
{
  if (zathura->jumplist.cur != NULL) {
    return girara_list_iterator_data(zathura->jumplist.cur);
  } else {
    return NULL;
  }
}

void
zathura_jumplist_forward(zathura_t* zathura)
{
  if (girara_list_iterator_has_next(zathura->jumplist.cur)) {
    girara_list_iterator_next(zathura->jumplist.cur);
  }
}

void
zathura_jumplist_backward(zathura_t* zathura)
{
  if (girara_list_iterator_has_previous(zathura->jumplist.cur)) {
    girara_list_iterator_previous(zathura->jumplist.cur);
  }
}

void
zathura_jumplist_append_jump(zathura_t* zathura)
{
  zathura_jump_t *jump = g_malloc(sizeof(zathura_jump_t));
  if (jump != NULL) {
    jump->page = 0;
    jump->x = 0;
    jump->y = 0;

    /* remove right tail after current */
    if (zathura->jumplist.cur != NULL) {
      girara_list_iterator_t *it = girara_list_iterator_copy(zathura->jumplist.cur);
      girara_list_iterator_next(it);
      while (girara_list_iterator_is_valid(it)) {
        girara_list_iterator_remove(it);
        zathura->jumplist.size = zathura->jumplist.size - 1;
      }
      g_free(it);
    }

    /* trim from beginning until max_size */
    girara_list_iterator_t *it = girara_list_iterator(zathura->jumplist.list);
    while (zathura->jumplist.size >= zathura->jumplist.max_size && girara_list_iterator_is_valid(it)) {
      girara_list_iterator_remove(it);
      zathura->jumplist.size = zathura->jumplist.size - 1;
    }
    g_free(it);

    girara_list_append(zathura->jumplist.list, jump);
    zathura->jumplist.size = zathura->jumplist.size + 1;
  }
}

void
zathura_jumplist_add(zathura_t* zathura)
{
  if (zathura->jumplist.list != NULL) {
    unsigned int pagenum = zathura_document_get_current_page_number(zathura->document);
    zathura_jump_t* cur = zathura_jumplist_current(zathura);
    if (cur && cur->page == pagenum) {
      return;
    }

    zathura_jumplist_append_jump(zathura);
    girara_list_iterator_next(zathura->jumplist.cur);
    zathura_jumplist_save(zathura);
  }
}


void
zathura_jumplist_save(zathura_t* zathura)
{
  zathura_jump_t* cur = zathura_jumplist_current(zathura);

  unsigned int pagenum = zathura_document_get_current_page_number(zathura->document);

  if (cur) {
    /* get position */
    GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
    GtkAdjustment* view_hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

    cur->page = pagenum;
    cur->x = gtk_adjustment_get_value(view_hadjustment) / zathura_document_get_scale(zathura->document);
    cur->y = gtk_adjustment_get_value(view_vadjustment) / zathura_document_get_scale(zathura->document);;
  }
}
