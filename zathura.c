/* See LICENSE file for license and copyright information */

#define _BSD_SOURCE
#define _XOPEN_SOURCE 700

#include <stdlib.h>
#include <unistd.h>

#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/session.h>
#include <girara/statusbar.h>
#include <girara/settings.h>
#include <glib/gstdio.h>

#include "bookmarks.h"
#include "callbacks.h"
#include "config.h"
#include "database.h"
#include "document.h"
#include "shortcuts.h"
#include "zathura.h"
#include "utils.h"
#include "render.h"

typedef struct zathura_document_info_s
{
  zathura_t* zathura;
  const char* path;
  const char* password;
} zathura_document_info_t;

static gboolean document_info_open(gpointer data);

/* function implementation */
zathura_t*
zathura_init(int argc, char* argv[])
{
  /* parse command line options */
  GdkNativeWindow embed = 0;
  gchar* config_dir = NULL, *data_dir = NULL, *plugin_path = NULL;
  GOptionEntry entries[] =
  {
    { "reparent",    'e', 0, G_OPTION_ARG_INT,      &embed,       "Reparents to window specified by xid",       "xid"  },
    { "config-dir",  'c', 0, G_OPTION_ARG_FILENAME, &config_dir,  "Path to the config directory",               "path" },
    { "data-dir",    'd', 0, G_OPTION_ARG_FILENAME, &data_dir,    "Path to the data directory",                 "path" },
    { "plugins-dir", 'p', 0, G_OPTION_ARG_STRING,   &plugin_path, "Path to the directories containing plugins", "path" },
    { NULL, '\0', 0, 0, NULL, NULL, NULL }
  };

  GOptionContext* context = g_option_context_new(" [file] [password]");
  g_option_context_add_main_entries(context, entries, NULL);

  GError* error = NULL;
  if (!g_option_context_parse(context, &argc, &argv, &error))
  {
    printf("Error parsing command line arguments: %s\n", error->message);
    g_option_context_free(context);
    g_error_free(error);
    return NULL;
  }
  g_option_context_free(context);

  zathura_t* zathura = g_malloc0(sizeof(zathura_t));

  /* plugins */
  zathura->plugins.plugins = girara_list_new2(
      (girara_free_function_t)zathura_document_plugin_free);
  zathura->plugins.path    = girara_list_new2(g_free);
  zathura->plugins.type_plugin_mapping = girara_list_new2(
      (girara_free_function_t)zathura_type_plugin_mapping_free);

  if (config_dir) {
    zathura->config.config_dir = g_strdup(config_dir);
  } else {
    gchar* path = girara_get_xdg_path(XDG_CONFIG);
    zathura->config.config_dir = g_build_filename(path, "zathura", NULL);
    g_free(path);
  }

  if (data_dir) {
    zathura->config.data_dir = g_strdup(config_dir);
  } else {
    gchar* path = girara_get_xdg_path(XDG_DATA);
    zathura->config.data_dir = g_build_filename(path, "zathura", NULL);
    g_free(path);
  }

  /* create zathura (config/data) directory */
  g_mkdir_with_parents(zathura->config.config_dir, 0771);
  g_mkdir_with_parents(zathura->config.data_dir,   0771);

  if (plugin_path) {
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
  zathura->ui.statusbar.file        = NULL;
  zathura->ui.statusbar.buffer      = NULL;
  zathura->ui.statusbar.page_number = NULL;
  zathura->ui.page_view             = NULL;
  zathura->ui.index                 = NULL;

  /* print settings */
  zathura->print.settings   = NULL;
  zathura->print.page_setup = NULL;

  /* global settings */
  zathura->global.recolor = false;

  /* load plugins */
  zathura_document_plugins_load(zathura);

  /* load global configuration files */
  girara_list_t* config_dirs = girara_split_path_array(girara_get_xdg_path(XDG_CONFIG_DIRS));
  ssize_t size = girara_list_size(config_dirs) - 1;
  for (; size >= 0; --size) {
    const char* dir = girara_list_nth(config_dirs, size);
    char* file = g_build_filename(dir, ZATHURA_RC, NULL);
    config_load_file(zathura, file);
    g_free(file);
  }
  girara_list_free(config_dirs);

  config_load_file(zathura, GLOBAL_RC);

  /* configuration */
  config_load_default(zathura);


  /* load local configuration files */
  char* configuration_file = g_build_filename(zathura->config.config_dir, ZATHURA_RC, NULL);
  config_load_file(zathura, configuration_file);
  g_free(configuration_file);

  /* initialize girara */
  zathura->ui.session->gtk.embed = embed;
  if (girara_session_init(zathura->ui.session) == false) {
    goto error_out;
  }

  /* girara events */
  zathura->ui.session->events.buffer_changed = buffer_changed;

  /* page view */
  zathura->ui.page_view = gtk_table_new(0, 0, TRUE);
  if (!zathura->ui.page_view) {
    goto error_free;
  }

  /* callbacks */
  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  g_signal_connect(G_OBJECT(view_vadjustment), "value-changed", G_CALLBACK(cb_view_vadjustment_value_changed), zathura);
  GtkAdjustment* view_hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  g_signal_connect(G_OBJECT(view_hadjustment), "value-changed", G_CALLBACK(cb_view_vadjustment_value_changed), zathura);

  gtk_widget_show(zathura->ui.page_view);

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
  if (!zathura->ui.statusbar.page_number) {
    goto error_free;
  }

  girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, "[No Name]");

  /* signals */
  g_signal_connect(G_OBJECT(zathura->ui.session->gtk.window), "destroy", G_CALLBACK(cb_destroy), NULL);

  /* save page padding */
  int* page_padding = girara_setting_get(zathura->ui.session, "page-padding");
  zathura->global.page_padding = (page_padding) ? *page_padding : 1;
  g_free(page_padding);

  gtk_table_set_row_spacings(GTK_TABLE(zathura->ui.page_view), zathura->global.page_padding);
  gtk_table_set_col_spacings(GTK_TABLE(zathura->ui.page_view), zathura->global.page_padding);

  /* parse colors */
  char* string_value = girara_setting_get(zathura->ui.session, "recolor-darkcolor");
  if (string_value != NULL) {
    gdk_color_parse(string_value, &(zathura->ui.colors.recolor_dark_color));
    g_free(string_value);
  }

  string_value = girara_setting_get(zathura->ui.session, "recolor-lightcolor");
  if (string_value != NULL) {
    gdk_color_parse(string_value, &(zathura->ui.colors.recolor_light_color));
    g_free(string_value);
  }

  /* database */
  zathura->database = zathura_db_init(zathura->config.data_dir);
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
    g_idle_add(document_info_open, document_info);
  }

  return zathura;

error_free:

  if (zathura->ui.page_view) {
    g_object_unref(zathura->ui.page_view);
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

  document_close(zathura);

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
  if(zathura->print.settings != NULL) {
    g_object_unref(zathura->print.settings);
  }

  if (zathura->print.page_setup != NULL) {
    g_object_unref(zathura->print.page_setup);
  }

  /* free registered plugins */
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
    girara_error("Can not create temporary file: %s", error->message);
    g_error_free(error);
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
  if (!path) {
    goto error_out;
  }

  zathura_document_t* document = zathura_document_open(zathura, path, password);

  if (!document) {
    goto error_out;
  }

  zathura->document = document;

  /* view mode */
  int* value = girara_setting_get(zathura->ui.session, "pages-per-row");
  int pages_per_row = (value) ? *value : 1;
  g_free(value);
  page_view_set_mode(zathura, pages_per_row);

  girara_set_view(zathura->ui.session, zathura->ui.page_view);

  /* threads */
  zathura->sync.render_thread = render_init(zathura);

  if (!zathura->sync.render_thread) {
    goto error_free;
  }

  /* create blank pages */
  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zathura_page_t* page = document->pages[page_id];
    gtk_widget_realize(page->event_box);
  }

  /* bookmarks */
  if (!zathura_bookmarks_load(zathura, zathura->document->file_path)) {
    girara_warning("Failed to load bookmarks for %s.\n", zathura->document->file_path);
  }

  page_set_delayed(zathura, document->current_page_number - 1);

  return true;

error_free:

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
  if (!overwrite && g_file_test(file_path, G_FILE_TEST_EXISTS))
  {
    girara_error("File already exists: %s. Use :write! to overwrite it.", file_path);
    g_free(file_path);
    return false;
  }

  bool res = zathura_document_save_as(zathura->document, file_path);
  g_free(file_path);
  return res;
}

static void
remove_page_from_table(GtkWidget* page, gpointer permanent)
{
  if (!permanent) {
    g_object_ref(G_OBJECT(page));
  }

  gtk_container_remove(GTK_CONTAINER(page->parent), page);
}

bool
document_close(zathura_t* zathura)
{
  if (!zathura->document) {
    return false;
  }

  /* store last seen page */
  zathura_db_set_fileinfo(zathura->database, zathura->document->file_path, zathura->document->current_page_number + 1,
      /* zathura->document->offset TODO */ 0, zathura->document->scale);

  render_free(zathura->sync.render_thread);
  zathura->sync.render_thread = NULL;

  gtk_container_foreach(GTK_CONTAINER(zathura->ui.page_view), remove_page_from_table, (gpointer)1);

  zathura_document_free(zathura->document);
  zathura->document = NULL;

  gtk_widget_hide(zathura->ui.page_view);

  statusbar_page_number_update(zathura);

  if (zathura->ui.session != NULL && zathura->ui.statusbar.file != NULL) {
    girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, "[No name]");
  }

  return true;
}

typedef struct page_set_delayed_s {
  zathura_t* zathura;
  unsigned int page;
} page_set_delayed_t;

static gboolean
page_set_delayed_impl(gpointer data) {
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
  g_idle_add(page_set_delayed_impl, p);
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

  if (!page) {
    goto error_out;
  }

  page_offset_t* offset = page_calculate_offset(page);
  if (offset == NULL) {
    goto error_out;
  }

  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  GtkAdjustment* view_hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  gtk_adjustment_set_value(view_hadjustment, offset->x);
  gtk_adjustment_set_value(view_vadjustment, offset->y);

  /* update page number */
  zathura->document->current_page_number = page_id;
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
page_view_set_mode(zathura_t* zathura, unsigned int pages_per_row)
{
  /* show at least one page */
  if (pages_per_row == 0) {
    pages_per_row = 1;
  }

  if (zathura->document == NULL) {
    return;
  }

  gtk_container_foreach(GTK_CONTAINER(zathura->ui.page_view), remove_page_from_table, (gpointer)0);

  gtk_table_resize(GTK_TABLE(zathura->ui.page_view), zathura->document->number_of_pages / pages_per_row + 1, pages_per_row);
  for (unsigned int i = 0; i < zathura->document->number_of_pages; i++)
  {
    int x = i % pages_per_row;
    int y = i / pages_per_row;
    gtk_table_attach(GTK_TABLE(zathura->ui.page_view), zathura->document->pages[i]->event_box, x, x + 1, y, y + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
  }

  gtk_widget_show_all(zathura->ui.page_view);
}

/* main function */
int main(int argc, char* argv[])
{
  g_thread_init(NULL);
  gdk_threads_init();
  gtk_init(&argc, &argv);

  zathura_t* zathura = zathura_init(argc, argv);
  if (zathura == NULL) {
    printf("error: could not initialize zathura\n");
    return -1;
  }

  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  zathura_free(zathura);

  return 0;
}

