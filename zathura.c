/* See LICENSE file for license and copyright information */

#include <stdlib.h>

#include <girara.h>

#include "callbacks.h"
#include "config.h"
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

gboolean document_info_open(gpointer data);

/* function implementation */
zathura_t*
zathura_init(int argc, char* argv[])
{
  zathura_t* zathura = malloc(sizeof(zathura_t));

  if (zathura == NULL) {
    return NULL;
  }

  /* plugins */
  zathura->plugins.plugins = girara_list_new();
  zathura->plugins.path = girara_list_new();
  girara_list_set_free_function(zathura->plugins.path, g_free);

  /* parse command line options */
  GdkNativeWindow embed = 0;
  gchar* config_dir = NULL, *data_dir = NULL, *plugin_path = NULL;
  GOptionEntry entries[] = 
  {
    { "reparent",    'e', 0, G_OPTION_ARG_INT,      &embed,       "Reparents to window specified by xid",       "xid"  },
    { "config-dir",  'c', 0, G_OPTION_ARG_FILENAME, &config_dir,  "Path to the config directory",               "path" },
    { "data-dir",    'd', 0, G_OPTION_ARG_FILENAME, &data_dir,    "Path to the data directory",                 "path" },
    { "plugins-dir", 'p', 0, G_OPTION_ARG_STRING,   &plugin_path, "Path to the directories containing plugins", "path" },
    { NULL }
  };

  GOptionContext* context = g_option_context_new(" [file] [password]");
  g_option_context_add_main_entries(context, entries, NULL);

  GError* error = NULL;
  if (!g_option_context_parse(context, &argc, &argv, &error))
  {
    printf("Error parsing command line arguments: %s\n", error->message);
    g_option_context_free(context);
    g_error_free(error);
    goto error_free;
  }
  g_option_context_free(context);

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

  if (plugin_path) {
    gchar** paths = g_strsplit(plugin_path, ":", 0);
    for (unsigned int i = 0; paths[i] != '\0'; ++i) {
      girara_list_append(zathura->plugins.path, g_strdup(paths[i]));
    }
    g_strfreev(paths);
  } else {
    /* XXX: this shouldn't be hard coded! */
    girara_list_append(zathura->plugins.path, g_strdup("/usr/local/lib/zathura"));
    girara_list_append(zathura->plugins.path, g_strdup("/usr/lib/zathura"));
  }

  /* UI */
  if ((zathura->ui.session = girara_session_create()) == NULL) {
    goto error_out;
  }

  zathura->ui.session->gtk.embed = embed;
  if (girara_session_init(zathura->ui.session) == false) {
    goto error_out;
  }

  zathura->ui.session->global.data  = zathura;
  zathura->ui.statusbar.file        = NULL;
  zathura->ui.statusbar.buffer      = NULL;
  zathura->ui.statusbar.page_number = NULL;
  zathura->ui.page_view             = NULL;
  zathura->ui.index                 = NULL;

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

  /* Put the table in the main window */
  // gtk_container_add(GTK_CONTAINER (zathura->ui.page_view), table);

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

  /* girara events */
  zathura->ui.session->events.buffer_changed = buffer_changed;

  /* load plugins */
  zathura_document_plugins_load(zathura);

  /* configuration */
  config_load_default(zathura);

  /* load global configuration files */
  config_load_file(zathura, GLOBAL_RC);

  /* load local configuration files */
  char* configuration_file = g_build_filename(zathura->config.config_dir, ZATHURA_RC, NULL);
  config_load_file(zathura, configuration_file);
  free(configuration_file);

  /* save page padding */
  int* page_padding = girara_setting_get(zathura->ui.session, "page-padding");
  zathura->global.page_padding = (page_padding) ? *page_padding : 1;

  gtk_table_set_row_spacings(GTK_TABLE(zathura->ui.page_view), zathura->global.page_padding);
  gtk_table_set_col_spacings(GTK_TABLE(zathura->ui.page_view), zathura->global.page_padding);

  /* open document if passed */
  if (argc > 1) {
    zathura_document_info_t* document_info = malloc(sizeof(zathura_document_info_t));

    if (document_info != NULL) {
      document_info->zathura  = zathura;
      document_info->path     = argv[1];
      document_info->password = (argc >= 2) ? argv[2] : NULL;
      g_idle_add(document_info_open, document_info);
    }
  }

  return zathura;

error_free:

  if (zathura->ui.page_view) {
    g_object_unref(zathura->ui.page_view);
  }

  girara_session_destroy(zathura->ui.session);

error_out:

  return NULL;
}

void
zathura_free(zathura_t* zathura)
{
  if (zathura == NULL) {
    return;
  }

  if (zathura->ui.session != NULL) {
    girara_session_destroy(zathura->ui.session);
  }

  document_close(zathura);

  /* free registered plugins */
  zathura_document_plugins_free(zathura);
  girara_list_free(zathura->plugins.plugins);
  girara_list_free(zathura->plugins.path);

  /* free config variables */
  g_free(zathura->config.config_dir);
  g_free(zathura->config.data_dir);
}

gboolean
document_info_open(gpointer data)
{
  zathura_document_info_t* document_info = data;
  g_return_val_if_fail(document_info != NULL, FALSE);

  if (document_info->zathura == NULL || document_info->path == NULL) {
    free(document_info);
    return FALSE;
  }

  document_open(document_info->zathura, document_info->path, document_info->password);

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
  free(value);
  page_view_set_mode(zathura, pages_per_row);

  girara_set_view(zathura->ui.session, zathura->ui.page_view);

  /* threads */
  zathura->sync.render_thread = render_init(zathura);

  if (!zathura->sync.render_thread) {
    goto error_free;
  }

  /* create blank pages */
  for (int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zathura_page_t* page = document->pages[page_id];
    gtk_widget_realize(page->event_box);
  }

  return true;

error_free:

  zathura_document_free(document);

error_out:

  return false;
}

bool
document_close(zathura_t* zathura)
{
  if (!zathura->document) {
    return false;
  }

  if (zathura->sync.render_thread) {
    render_free(zathura->sync.render_thread);
  }

  zathura_document_free(zathura->document);

  return true;
}

bool
page_set(zathura_t* zathura, unsigned int page_id)
{
  if (!zathura->document || !zathura->document->pages) {
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

  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  cb_view_vadjustment_value_changed(view_vadjustment, zathura);

  /* update page number */
  zathura->document->current_page_number = page_id;

  char* page_number = g_strdup_printf("[%d/%d]", page_id + 1, zathura->document->number_of_pages);
  girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.page_number, page_number);
  g_free(page_number);

  return true;

error_out:

  return false;
}

void
page_view_set_mode(zathura_t* zathura, unsigned int pages_per_row)
{
  gtk_table_resize(GTK_TABLE(zathura->ui.page_view), zathura->document->number_of_pages / pages_per_row + 1, pages_per_row);
  for (unsigned int i = 0; i < zathura->document->number_of_pages; i++)
  {
    int x = i % pages_per_row;
    int y = i / pages_per_row;
    gtk_table_attach(GTK_TABLE(zathura->ui.page_view), zathura->document->pages[i]->event_box, x, x + 1, y, y + 1, GTK_EXPAND, GTK_EXPAND, 0, 0);
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
    printf("error: coult not initialize zathura\n");
    return -1;
  }

  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  return 0;
}
