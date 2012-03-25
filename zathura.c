/* See LICENSE file for license and copyright information */

#define _BSD_SOURCE
#define _XOPEN_SOURCE 700

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
#include "render.h"
#include "page-widget.h"

typedef struct zathura_document_info_s
{
  zathura_t* zathura;
  const char* path;
  const char* password;
} zathura_document_info_t;

static gboolean document_info_open(gpointer data);
static gboolean purge_pages(gpointer data);

/* function implementation */
zathura_t*
zathura_init(int argc, char* argv[])
{
  /* parse command line options */
#if (GTK_MAJOR_VERSION == 2)
  GdkNativeWindow embed = 0;
#else
  Window embed = 0;
#endif

  gchar* config_dir = NULL, *data_dir = NULL, *plugin_path = NULL, *loglevel = NULL;
  bool forkback = false;
  GOptionEntry entries[] =
  {
    { "reparent",    'e', 0, G_OPTION_ARG_INT,      &embed,       _("Reparents to window specified by xid"),       "xid"  },
    { "config-dir",  'c', 0, G_OPTION_ARG_FILENAME, &config_dir,  _("Path to the config directory"),               "path" },
    { "data-dir",    'd', 0, G_OPTION_ARG_FILENAME, &data_dir,    _("Path to the data directory"),                 "path" },
    { "plugins-dir", 'p', 0, G_OPTION_ARG_STRING,   &plugin_path, _("Path to the directories containing plugins"), "path" },
    { "fork",        '\0', 0, G_OPTION_ARG_NONE,    &forkback,    _("Fork into the background"),                   NULL },
    { "debug",       'l', 0, G_OPTION_ARG_STRING,   &loglevel,    _("Log level (debug, info, warning, error)"),    "level" },
    { NULL, '\0', 0, 0, NULL, NULL, NULL }
  };

  GOptionContext* context = g_option_context_new(" [file] [password]");
  g_option_context_add_main_entries(context, entries, NULL);

  GError* error = NULL;
  if (g_option_context_parse(context, &argc, &argv, &error) == false)
  {
    printf("Error parsing command line arguments: %s\n", error->message);
    g_option_context_free(context);
    g_error_free(error);
    return NULL;
  }
  g_option_context_free(context);

  /* Fork into the background if the user really wants to ... */
  if (forkback == true)
  {
    int pid = fork();
    if (pid > 0) { /* parent */
      exit(0);
    } else if (pid < 0) { /* error */
      printf("Error: couldn't fork.");
    }

    setsid();
  }

  /* Set log level. */
  if (loglevel == NULL || g_strcmp0(loglevel, "info") == 0) {
    girara_set_debug_level(GIRARA_INFO);
  } else if (g_strcmp0(loglevel, "warning") == 0) {
    girara_set_debug_level(GIRARA_WARNING);
  } else if (g_strcmp0(loglevel, "error") == 0) {
    girara_set_debug_level(GIRARA_ERROR);
  }

  zathura_t* zathura = g_malloc0(sizeof(zathura_t));

  /* plugins */
  zathura->plugins.plugins = girara_list_new2(
      (girara_free_function_t)zathura_document_plugin_free);
  zathura->plugins.path    = girara_list_new2(g_free);
  zathura->plugins.type_plugin_mapping = girara_list_new2(
      (girara_free_function_t)zathura_type_plugin_mapping_free);

  if (config_dir != NULL) {
    zathura->config.config_dir = g_strdup(config_dir);
  } else {
    gchar* path = girara_get_xdg_path(XDG_CONFIG);
    zathura->config.config_dir = g_build_filename(path, "zathura", NULL);
    g_free(path);
  }

  if (data_dir != NULL) {
    zathura->config.data_dir = g_strdup(config_dir);
  } else {
    gchar* path = girara_get_xdg_path(XDG_DATA);
    zathura->config.data_dir = g_build_filename(path, "zathura", NULL);
    g_free(path);
  }

  /* create zathura (config/data) directory */
  g_mkdir_with_parents(zathura->config.config_dir, 0771);
  g_mkdir_with_parents(zathura->config.data_dir,   0771);

  if (plugin_path != NULL) {
    girara_list_t* paths = girara_split_path_array(plugin_path);
    girara_list_merge(zathura->plugins.path, paths);
    girara_list_free(paths);
  } else {
#ifdef ZATHURA_PLUGINDIR
    girara_list_t* paths = girara_split_path_array(ZATHURA_PLUGINDIR);
    girara_list_merge(zathura->plugins.path, paths);
    girara_list_free(paths);
#endif
  }

  /* UI */
  if ((zathura->ui.session = girara_session_create()) == NULL) {
    goto error_out;
  }

  zathura->ui.session->global.data  = zathura;

  /* global settings */
  zathura->global.recolor            = false;
  zathura->global.update_page_number = true;

  /* load plugins */
  zathura_document_plugins_load(zathura);

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

  /* initialize girara */
  zathura->ui.session->gtk.embed = embed;

  if (girara_session_init(zathura->ui.session, "zathura") == false) {
    goto error_out;
  }

  /* girara events */
  zathura->ui.session->events.buffer_changed = cb_buffer_changed;

  /* page view */
  zathura->ui.page_widget = gtk_table_new(0, 0, TRUE);
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

  gtk_table_set_row_spacings(GTK_TABLE(zathura->ui.page_widget), page_padding);
  gtk_table_set_col_spacings(GTK_TABLE(zathura->ui.page_widget), page_padding);

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

  /* open document if passed */
  if (argc > 1) {
    zathura_document_info_t* document_info = g_malloc0(sizeof(zathura_document_info_t));

    document_info->zathura  = zathura;
    document_info->path     = argv[1];
    document_info->password = (argc >= 2) ? argv[2] : NULL;
    gdk_threads_add_idle(document_info_open, document_info);
  }

  /* add even to purge old pages */
  int interval = 30;
  girara_setting_get(zathura->ui.session, "page-store-interval", &interval);
  g_timeout_add_seconds(interval, purge_pages, zathura);

  return zathura;

error_free:

  if (zathura->ui.page_widget != NULL) {
    g_object_unref(zathura->ui.page_widget);
  }

  if (zathura->ui.page_widget_alignment != NULL) {
    g_object_unref(zathura->ui.page_widget_alignment);
  }

error_out:

  zathura_free(zathura);

  return NULL;
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
  girara_list_free(zathura->plugins.type_plugin_mapping);
  girara_list_free(zathura->plugins.plugins);
  girara_list_free(zathura->plugins.path);

  /* free config variables */
  g_free(zathura->config.config_dir);
  g_free(zathura->config.data_dir);

  g_free(zathura);
}

static gchar*
prepare_document_open_from_stdin(zathura_t* zathura)
{
  g_return_val_if_fail(zathura, NULL);

  GError* error = NULL;
  gchar* file = NULL;
  gint handle = g_file_open_tmp("zathura.stdin.XXXXXX", &file, &error);
  if (handle == -1)
  {
    if (error != NULL) {
      girara_error("Can not create temporary file: %s", error->message);
      g_error_free(error);
    }
    return NULL;
  }

  // read from stdin and dump to temporary file
  int stdinfno = fileno(stdin);
  if (stdinfno == -1)
  {
    girara_error("Can not read from stdin.");
    close(handle);
    g_unlink(file);
    g_free(file);
    return NULL;
  }

  char buffer[BUFSIZ];
  ssize_t count = 0;
  while ((count = read(stdinfno, buffer, BUFSIZ)) > 0)
  {
    if (write(handle, buffer, count) != count)
    {
      girara_error("Can not write to temporary file: %s", file);
      close(handle);
      g_unlink(file);
      g_free(file);
      return NULL;
    }
  }
  close(handle);

  if (count != 0)
  {
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
  if (path == NULL) {
    goto error_out;
  }

  zathura_document_t* document = zathura_document_open(zathura, path, password);

  if (document == NULL) {
    goto error_out;
  }

  /* install file monitor */
  gchar* file_uri = g_filename_to_uri(document->file_path, NULL, NULL);
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
    zathura->file_monitor.file_path = g_strdup(document->file_path);
    if (zathura->file_monitor.file_path == NULL) {
      goto error_free;
    }
  }

  if (document->password != NULL) {
    g_free(zathura->file_monitor.password);
    zathura->file_monitor.password = g_strdup(document->password);
    if (zathura->file_monitor.password == NULL) {
      goto error_free;
    }
  }

  zathura->document = document;

  /* view mode */
  int pages_per_row = 1;
  girara_setting_get(zathura->ui.session, "pages-per-row", &pages_per_row);
  page_widget_set_mode(zathura, pages_per_row);

  girara_set_view(zathura->ui.session, zathura->ui.page_widget_alignment);

  /* threads */
  zathura->sync.render_thread = render_init(zathura);

  if (zathura->sync.render_thread == NULL) {
    goto error_free;
  }

  /* create blank pages */
  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zathura_page_t* page = document->pages[page_id];
    gtk_widget_realize(page->drawing_area);
  }

  /* bookmarks */
  zathura_bookmarks_load(zathura, zathura->document->file_path);

  page_set_delayed(zathura, document->current_page_number);
  cb_view_vadjustment_value_changed(NULL, zathura);

  /* update title */
  girara_set_window_title(zathura->ui.session, document->file_path);

  free(file_uri);

  /* adjust window */
  girara_argument_t argument = { zathura->document->adjust_mode, NULL };
  sc_adjust_window(zathura->ui.session, &argument, NULL, 0);

  return true;

error_free:

  if (file_uri != NULL) {
    g_free(file_uri);
  }

  zathura_document_free(document);

error_out:

  return false;
}

bool
document_save(zathura_t* zathura, const char* path, bool overwrite)
{
  g_return_val_if_fail(zathura, false);
  g_return_val_if_fail(zathura->document, false);
  g_return_val_if_fail(path, false);

  gchar* file_path = girara_fix_path(path);
  if ((overwrite == false) && g_file_test(file_path, G_FILE_TEST_EXISTS))
  {
    girara_error("File already exists: %s. Use :write! to overwrite it.", file_path);
    g_free(file_path);
    return false;
  }

  zathura_plugin_error_t error = zathura_document_save_as(zathura->document, file_path);
  g_free(file_path);

  return (error == ZATHURA_PLUGIN_ERROR_OK) ? true : false;
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

  /* store last seen page */
  zathura_db_set_fileinfo(zathura->database, zathura->document->file_path,
      zathura->document->current_page_number, zathura->document->page_offset,
      zathura->document->scale, zathura->document->rotate);

  render_free(zathura->sync.render_thread);
  zathura->sync.render_thread = NULL;

  gtk_container_foreach(GTK_CONTAINER(zathura->ui.page_widget), remove_page_from_table, (gpointer)1);

  zathura_document_free(zathura->document);
  zathura->document = NULL;

  gtk_widget_hide(zathura->ui.page_widget);

  statusbar_page_number_update(zathura);

  if (zathura->ui.session != NULL && zathura->ui.statusbar.file != NULL) {
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, _("[No name]"));
  }

  /* update title */
  girara_set_window_title(zathura->ui.session, "zathura");

  return true;
}

typedef struct page_set_delayed_s
{
  zathura_t* zathura;
  unsigned int page;
} page_set_delayed_t;

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
  if (zathura == NULL || zathura->document == NULL || zathura->document->pages == NULL ||
      page_id >= zathura->document->number_of_pages) {
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
  if (zathura == NULL || zathura->document == NULL || zathura->document->pages == NULL) {
    goto error_out;
  }

  if (page_id >= zathura->document->number_of_pages) {
    goto error_out;
  }

  /* render page */
  zathura_page_t* page = zathura->document->pages[page_id];

  if (page == NULL) {
    goto error_out;
  }

  zathura->document->current_page_number = page_id;
  zathura->global.update_page_number = false;

  page_offset_t offset;
  page_calculate_offset(page, &offset);

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

  if (zathura->document != NULL) {
    char* page_number_text = g_strdup_printf("[%d/%d]", zathura->document->current_page_number + 1, zathura->document->number_of_pages);
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.page_number, page_number_text);
    g_free(page_number_text);
  } else {
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.page_number, "");
  }
}

void
page_widget_set_mode(zathura_t* zathura, unsigned int pages_per_row)
{
  /* show at least one page */
  if (pages_per_row == 0) {
    pages_per_row = 1;
  }

  if (zathura->document == NULL) {
    return;
  }

  gtk_container_foreach(GTK_CONTAINER(zathura->ui.page_widget), remove_page_from_table, (gpointer)0);

  gtk_table_resize(GTK_TABLE(zathura->ui.page_widget), ceil(zathura->document->number_of_pages / pages_per_row), pages_per_row);
  for (unsigned int i = 0; i < zathura->document->number_of_pages; i++)
  {
    int x = i % pages_per_row;
    int y = i / pages_per_row;
    gtk_table_attach(GTK_TABLE(zathura->ui.page_widget), zathura->document->pages[i]->drawing_area, x, x + 1, y, y + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
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

  for (unsigned int page_id = 0; page_id < zathura->document->number_of_pages; page_id++) {
    zathura_page_t* page = zathura->document->pages[page_id];
    zathura_page_widget_purge_unused(ZATHURA_PAGE(page->drawing_area), threshold);
  }
  return TRUE;
}
