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
#include <girara/shortcuts.h>
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
#include "adjustment.h"

typedef struct zathura_document_info_s {
  zathura_t* zathura;
  const char* path;
  const char* password;
  int page_number;
} zathura_document_info_t;


static gboolean document_info_open(gpointer data);
static void zathura_jumplist_reset_current(zathura_t* zathura);
static void zathura_jumplist_append_jump(zathura_t* zathura);
static void zathura_jumplist_save(zathura_t* zathura);

/* function implementation */
zathura_t*
zathura_create(void)
{
  zathura_t* zathura = g_malloc0(sizeof(zathura_t));

  /* global settings */
  zathura->global.search_direction = FORWARD;

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

  /* zathura signals */
  zathura->signals.refresh_view = g_signal_new("refresh-view",
                                               GTK_TYPE_WIDGET,
                                               G_SIGNAL_RUN_LAST,
                                               0,
                                               NULL,
                                               NULL,
                                               g_cclosure_marshal_generic,
                                               G_TYPE_NONE,
                                               1,
                                               G_TYPE_POINTER);

  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.view), "refresh-view",
                   G_CALLBACK(cb_refresh_view), zathura);

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

  GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment(
                 GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

  /* Connect hadjustment signals */
  g_signal_connect(G_OBJECT(hadjustment), "value-changed",
      G_CALLBACK(cb_view_hadjustment_value_changed), zathura);
  g_signal_connect(G_OBJECT(hadjustment), "changed",
      G_CALLBACK(cb_view_hadjustment_changed), zathura);

  GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(
                 GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

  /* Connect vadjustment signals */
  g_signal_connect(G_OBJECT(vadjustment), "value-changed",
      G_CALLBACK(cb_view_vadjustment_value_changed), zathura);
  g_signal_connect(G_OBJECT(vadjustment), "changed",
      G_CALLBACK(cb_view_vadjustment_changed), zathura);

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

  /* database */
  char* database = NULL;
  girara_setting_get(zathura->ui.session, "database", &database);

  if (g_strcmp0(database, "plain") == 0) {
    girara_debug("Using plain database backend.");
    zathura->database = zathura_plaindatabase_new(zathura->config.data_dir);
#ifdef WITH_SQLITE
  } else if (g_strcmp0(database, "sqlite") == 0) {
    girara_debug("Using sqlite database backend.");
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
  } else {
    g_object_set(zathura->ui.session->command_history, "io", zathura->database, NULL);
  }

  /* bookmarks */
  zathura->bookmarks.bookmarks = girara_sorted_list_new2((girara_compare_function_t) zathura_bookmarks_compare,
                                 (girara_free_function_t) zathura_bookmark_free);

  /* jumplist */

  int jumplist_size = 20;
  girara_setting_get(zathura->ui.session, "jumplist-size", &jumplist_size);

  zathura->jumplist.max_size = jumplist_size < 0 ? 0 : jumplist_size;
  zathura->jumplist.list = NULL;
  zathura->jumplist.size = 0;
  zathura->jumplist.cur = NULL;

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
  if (zathura->database != NULL) {
    g_object_unref(G_OBJECT(zathura->database));
  }

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
zathura_set_synctex(zathura_t* zathura, bool value)
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
      document_open(document_info->zathura, file, document_info->password,
                    document_info->page_number);
      g_free(file);
    }
  }

  g_free(document_info);
  return FALSE;
}

bool
document_open(zathura_t* zathura, const char* path, const char* password,
              int page_number)
{
  if (zathura == NULL || zathura->plugins.manager == NULL || path == NULL) {
    goto error_out;
  }

  gchar* file_uri = NULL;
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
    if (error == ZATHURA_ERROR_OK ) {
      girara_notify(zathura->ui.session, GIRARA_ERROR, _("Unsupported file type. Please install the necessary plugin."));
    }
    goto error_out;
  }

  const char* file_path        = zathura_document_get_path(document);
  unsigned int number_of_pages = zathura_document_get_number_of_pages(document);

  if (number_of_pages == 0) {
    girara_notify(zathura->ui.session, GIRARA_WARNING,
        _("Document does not contain any pages"));
    goto error_free;
  }

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
  /* if it wasn't specified on the command-line, get it from file_info */
  if (page_number == ZATHURA_PAGE_NUMBER_UNSPECIFIED)
    page_number = file_info.current_page;
  if (page_number < 0)
    page_number += number_of_pages;
  if ((unsigned)page_number > number_of_pages) {
    girara_warning("document info: '%s' has an invalid page number", file_path);
    zathura_document_set_current_page_number(document, 0);
  } else {
    zathura_document_set_current_page_number(document, page_number);
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

  /* initialize bisect state */
  zathura->bisect.start = 0;
  zathura->bisect.last_jump = zathura_document_get_current_page_number(document);
  zathura->bisect.end = number_of_pages - 1;

  /* update statusbar */
  bool basename_only = false;
  girara_setting_get(zathura->ui.session, "statusbar-basename", &basename_only);
  if (basename_only == false) {
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, file_path);
  } else {
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, zathura_document_get_basename(document));
  }

  /* install file monitor */
  file_uri = g_filename_to_uri(file_path, NULL, NULL);
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

  /* page cache size */
  int cache_size = 0;
  girara_setting_get(zathura->ui.session, "page-cache-size", &cache_size);
  if (cache_size <= 0) {
    girara_warning("page-cache-size is not positive, using %d instead",
        ZATHURA_PAGE_CACHE_DEFAULT_SIZE);
    cache_size = ZATHURA_PAGE_CACHE_DEFAULT_SIZE;
  }

  /* threads */
  zathura->sync.render_thread = zathura_renderer_new(cache_size);

  if (zathura->sync.render_thread == NULL) {
    goto error_free;
  }

  /* set up recolor info in ZathuraRenderer */
  char* recolor_dark = NULL;
  char* recolor_light = NULL;
  girara_setting_get(zathura->ui.session, "recolor-darkcolor", &recolor_dark);
  girara_setting_get(zathura->ui.session, "recolor-lightcolor", &recolor_light);
  zathura_renderer_set_recolor_colors_str(zathura->sync.render_thread,
      recolor_light, recolor_dark);
  g_free(recolor_dark);
  g_free(recolor_light);

  bool recolor = false;
  girara_setting_get(zathura->ui.session, "recolor", &recolor);
  zathura_renderer_enable_recolor(zathura->sync.render_thread, recolor);
  girara_setting_get(zathura->ui.session, "recolor-keephue", &recolor);
  zathura_renderer_enable_recolor_hue(zathura->sync.render_thread, recolor);

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

    double height = zathura_page_get_height(page);
    double width = zathura_page_get_width(page);
    page_calc_height_width(zathura->document, height, width, &page_height, &page_width, true);

    gtk_widget_set_size_request(page_widget, page_width, page_height);

    g_signal_connect(G_OBJECT(page_widget), "text-selected",
        G_CALLBACK(cb_page_widget_text_selected), zathura);
    g_signal_connect(G_OBJECT(page_widget), "image-selected",
        G_CALLBACK(cb_page_widget_text_selected), zathura);
  }

  /* view mode */
  unsigned int pages_per_row = 1;
  unsigned int first_page_column = 1;
  unsigned int page_padding = 1;

  girara_setting_get(zathura->ui.session, "page-padding", &page_padding);

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

  page_widget_set_mode(zathura, page_padding, pages_per_row, first_page_column);
  zathura_document_set_page_layout(zathura->document, page_padding, pages_per_row, first_page_column);

  girara_set_view(zathura->ui.session, zathura->ui.page_widget_alignment);

  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    gtk_widget_realize(zathura->pages[page_id]);
  }

  /* bookmarks */
  zathura_bookmarks_load(zathura, file_path);

  /* jumplist */
  if (zathura_jumplist_load(zathura, file_path) == false) {
    zathura->jumplist.list = girara_list_new2(g_free);
  }

  /* update title */
  basename_only = false;
  girara_setting_get(zathura->ui.session, "window-title-basename", &basename_only);
  if (basename_only == false) {
    girara_set_window_title(zathura->ui.session, file_path);
  } else {
    girara_set_window_title(zathura->ui.session, zathura_document_get_basename(document));
  }

  g_free(file_uri);


  /* adjust_view and set position*/
  adjust_view(zathura);

  /* set position */
  page_set(zathura, zathura_document_get_current_page_number(document));
  if (file_info.position_x != 0 || file_info.position_y != 0) {
    position_set(zathura, file_info.position_x, file_info.position_y);
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
document_open_idle(zathura_t* zathura, const char* path, const char* password,
                   int page_number)
{
  if (zathura == NULL || path == NULL) {
    return;
  }

  zathura_document_info_t* document_info = g_malloc0(sizeof(zathura_document_info_t));

  document_info->zathura     = zathura;
  document_info->path        = path;
  document_info->password    = password;
  document_info->page_number = page_number;

  gdk_threads_add_idle(document_info_open, document_info);
}

bool
document_save(zathura_t* zathura, const char* path, bool overwrite)
{
  g_return_val_if_fail(zathura, false);
  g_return_val_if_fail(zathura->document, false);
  g_return_val_if_fail(path, false);

  gchar* file_path = girara_fix_path(path);
  /* use current basename if path points to a directory  */
  if (g_file_test(file_path, G_FILE_TEST_IS_DIR) == TRUE) {
    char* basename = g_path_get_basename(zathura_document_get_path(zathura->document));
    char* tmp = file_path;
    file_path = g_strconcat(file_path, "/", basename, NULL);
    g_free(tmp);
    g_free(basename);
  }

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

  /* stop rendering */
  zathura_renderer_stop(zathura->sync.render_thread);

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
  file_info.position_x = zathura_document_get_position_x(zathura->document);
  file_info.position_y = zathura_document_get_position_y(zathura->document);

  /* save file info */
  zathura_db_set_fileinfo(zathura->database, path, &file_info);

  /* save jumplist */
  zathura_db_save_jumplist(zathura->database, path, zathura->jumplist.list);
  girara_list_iterator_free(zathura->jumplist.cur);
  zathura->jumplist.cur = NULL;
  girara_list_free(zathura->jumplist.list);
  zathura->jumplist.list = NULL;
  zathura->jumplist.size = 0;

  /* release render thread */
  g_object_unref(zathura->sync.render_thread);
  zathura->sync.render_thread = NULL;

  /* remove widgets */
  gtk_container_foreach(GTK_CONTAINER(zathura->ui.page_widget), remove_page_from_table, (gpointer) 1);
  for (unsigned int i = 0; i < zathura_document_get_number_of_pages(zathura->document); i++) {
    g_object_unref(zathura->pages[i]);
    g_object_unref(zathura->pages[i]); // FIXME
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

bool
page_set(zathura_t* zathura, unsigned int page_id)
{
  if (zathura == NULL || zathura->document == NULL) {
    goto error_out;
  }

  zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
  if (page == NULL) {
    goto error_out;
  }

  zathura_document_set_current_page_number(zathura->document, page_id);

  /* negative position means auto */
  return position_set(zathura, -1, -1);

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

    bool page_number_in_window_title = false;
    girara_setting_get(zathura->ui.session, "window-title-page", &page_number_in_window_title);

    if (page_number_in_window_title == true) {
      bool basename_only = false;
      girara_setting_get(zathura->ui.session, "window-title-basename", &basename_only);
      char* title = g_strdup_printf("%s %s",
        (basename_only == true)
          ? zathura_document_get_basename(zathura->document)
          : zathura_document_get_path(zathura->document),
        page_number_text);
      girara_set_window_title(zathura->ui.session, title);
      g_free(title);
    }

    g_free(page_number_text);
  } else {
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.page_number, "");
  }
}

void
page_widget_set_mode(zathura_t* zathura, unsigned int page_padding,
                     unsigned int pages_per_row, unsigned int first_page_column)
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
  gtk_grid_set_row_spacing(GTK_GRID(zathura->ui.page_widget), page_padding);
  gtk_grid_set_column_spacing(GTK_GRID(zathura->ui.page_widget), page_padding);

#else
  gtk_table_set_row_spacings(GTK_TABLE(zathura->ui.page_widget), page_padding);
  gtk_table_set_col_spacings(GTK_TABLE(zathura->ui.page_widget), page_padding);

  unsigned int ncol = pages_per_row;
  unsigned int nrow = (number_of_pages + first_page_column - 1 + ncol - 1) / ncol;
  gtk_table_resize(GTK_TABLE(zathura->ui.page_widget), nrow, ncol);
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

bool
position_set(zathura_t* zathura, double position_x, double position_y)
{
  if (zathura == NULL || zathura->document == NULL) {
    goto error_out;
  }

  double comppos_x, comppos_y;
  unsigned int page_id = zathura_document_get_current_page_number(zathura->document);

  /* xalign = 0.5: center horizontally (with the page, not the document) */
  /* yalign = 0.0: align page an viewport edges at the top               */
  page_number_to_position(zathura->document, page_id, 0.5, 0.0, &comppos_x, &comppos_y);

  /* automatic horizontal adjustment */
  zathura_adjust_mode_t adjust_mode = zathura_document_get_adjust_mode(zathura->document);

  /* negative position_x mean: use the computed value */
  if (position_x < 0) {
    position_x = comppos_x;
    bool zoom_center = false;
    girara_setting_get(zathura->ui.session, "zoom-center", &zoom_center);

    /* center horizontally */
    if (adjust_mode == ZATHURA_ADJUST_BESTFIT ||
      adjust_mode == ZATHURA_ADJUST_WIDTH ||
      zoom_center) {
        position_x = 0.5;
    }
  }

  if (position_y < 0) {
    position_y = comppos_y;
  }

  /* set the position */
  zathura_document_set_position_x(zathura->document, position_x);
  zathura_document_set_position_y(zathura->document, position_y);

  /* trigger a 'change' event for both adjustments */
  refresh_view(zathura);

  return true;

error_out:
  return false;
}


void
refresh_view(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);

  /* emit a custom refresh-view signal */
  g_signal_emit(zathura->ui.session->gtk.view, zathura->signals.refresh_view,
                0, zathura);
}


bool
adjust_view(zathura_t* zathura) {
    g_return_val_if_fail(zathura != NULL, false);

  if (zathura->ui.page_widget == NULL || zathura->document == NULL) {
    goto error_ret;
  }

  zathura_adjust_mode_t adjust_mode = zathura_document_get_adjust_mode(zathura->document);
  if (adjust_mode == ZATHURA_ADJUST_NONE) {
    /* there is nothing todo */
    goto error_ret;
  }

  unsigned int cell_height = 0, cell_width = 0;
  unsigned int document_height = 0, document_width = 0;
  unsigned int view_height = 0, view_width = 0;

  zathura_document_get_cell_size(zathura->document, &cell_height, &cell_width);
  zathura_document_get_document_size(zathura->document, &document_height, &document_width);
  zathura_document_get_viewport_size(zathura->document, &view_height, &view_width);

  double scale = zathura_document_get_scale(zathura->document);

  if (view_height == 0 || view_width == 0 || cell_height == 0 || cell_width == 0) {
    goto error_ret;
  }

  double page_ratio   = (double)cell_height / (double)document_width;
  double view_ratio = (double)view_height / (double)view_width;
  double newscale = scale;

  if (adjust_mode == ZATHURA_ADJUST_WIDTH ||
      (adjust_mode == ZATHURA_ADJUST_BESTFIT && page_ratio < view_ratio)) {
    newscale = scale * (double)view_width / (double)document_width;

  } else if (adjust_mode == ZATHURA_ADJUST_BESTFIT) {
    newscale = scale * (double)view_height / (double)cell_height;

  } else {
    goto error_ret;
  }

  /* save new scale and recompute cell size */
  zathura_document_set_scale(zathura->document, newscale);
  unsigned int new_cell_height = 0, new_cell_width = 0;
  zathura_document_get_cell_size(zathura->document, &new_cell_height, &new_cell_width);

  /* if the change in scale changes page cell dimensions by at least one pixel, render */
  if (abs(new_cell_width - cell_width) > 1 ||
      abs(new_cell_height - cell_height) > 1) {
    render_all(zathura);
    refresh_view(zathura);

 /* otherwise set the old scale and leave */
  } else {
    zathura_document_set_scale(zathura->document, scale);
  }

error_ret:
  return false;
}

bool
zathura_jumplist_has_previous(zathura_t* zathura)
{
  return girara_list_iterator_has_previous(zathura->jumplist.cur);
}

bool
zathura_jumplist_has_next(zathura_t* zathura)
{
  return girara_list_iterator_has_next(zathura->jumplist.cur);
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

static void
zathura_jumplist_reset_current(zathura_t* zathura)
{
  g_return_if_fail(zathura != NULL && zathura->jumplist.cur != NULL);

  while (true) {
    if (girara_list_iterator_has_next(zathura->jumplist.cur) == false) {
      return;
    }

    girara_list_iterator_next(zathura->jumplist.cur);
  }
}

static void
zathura_jumplist_append_jump(zathura_t* zathura)
{
  g_return_if_fail(zathura != NULL && zathura->jumplist.list != NULL);

  zathura_jump_t *jump = g_malloc(sizeof(zathura_jump_t));
  jump->page = 0;
  jump->x = 0.0;
  jump->y = 0.0;
  girara_list_append(zathura->jumplist.list, jump);

  if (zathura->jumplist.size == 0) {
    zathura->jumplist.cur = girara_list_iterator(zathura->jumplist.list);
  }

  ++zathura->jumplist.size;
  zathura_jumplist_trim(zathura);
}

void
zathura_jumplist_trim(zathura_t* zathura)
{
  g_return_if_fail(zathura != NULL && zathura->jumplist.list != NULL && zathura->jumplist.size != 0);

  girara_list_iterator_t* cur = girara_list_iterator(zathura->jumplist.list);

  while (zathura->jumplist.size > zathura->jumplist.max_size) {
    if (girara_list_iterator_data(cur) == girara_list_iterator_data(zathura->jumplist.cur)) {
      girara_list_iterator_free(zathura->jumplist.cur);
      zathura->jumplist.cur = NULL;
    }

    girara_list_iterator_remove(cur);
    --zathura->jumplist.size;
  }

  if (zathura->jumplist.size == 0 || (zathura->jumplist.size != 0 && zathura->jumplist.cur != NULL)) {
    girara_list_iterator_free(cur);
  } else {
    zathura->jumplist.cur = cur;
  }
}

void
zathura_jumplist_add(zathura_t* zathura)
{
  g_return_if_fail(zathura != NULL && zathura->document != NULL && zathura->jumplist.list != NULL);

  unsigned int pagenum = zathura_document_get_current_page_number(zathura->document);
  double x = zathura_document_get_position_x(zathura->document);
  double y = zathura_document_get_position_y(zathura->document);

  if (zathura->jumplist.size != 0) {
    zathura_jumplist_reset_current(zathura);

    zathura_jump_t* cur = zathura_jumplist_current(zathura);

    if (cur != NULL) {
      if (cur->page == pagenum && cur->x == x && cur->y == y) {
        return;
      }
    }
  }

  zathura_jumplist_append_jump(zathura);
  zathura_jumplist_reset_current(zathura);
  zathura_jumplist_save(zathura);
}

bool
zathura_jumplist_load(zathura_t* zathura, const char* file)
{
  g_return_val_if_fail(zathura != NULL && zathura->database != NULL && file != NULL, false);

  zathura->jumplist.list = zathura_db_load_jumplist(zathura->database, file);

  if (zathura->jumplist.list == NULL) {
    girara_error("Failed to load the jumplist from the database");

    return false;
  }

  zathura->jumplist.size = girara_list_size(zathura->jumplist.list);

  if (zathura->jumplist.size != 0) {
    zathura->jumplist.cur = girara_list_iterator(zathura->jumplist.list);
    zathura_jumplist_reset_current(zathura);
    zathura_jumplist_trim(zathura);
    girara_debug("Loaded the jumplist from the database");
  } else {
    girara_debug("No jumplist for this file in the database yet");
  }

  return true;
}

static void
zathura_jumplist_save(zathura_t* zathura)
{
  g_return_if_fail(zathura != NULL && zathura->document != NULL);

  zathura_jump_t* cur = zathura_jumplist_current(zathura);

  unsigned int pagenum = zathura_document_get_current_page_number(zathura->document);

  if (cur) {
    cur->page = pagenum;
    cur->x = zathura_document_get_position_x(zathura->document);
    cur->y = zathura_document_get_position_y(zathura->document);
  }
}
