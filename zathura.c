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
  zathura->plugins.path = NULL;

  /* UI */
  if ((zathura->ui.session = girara_session_create()) == NULL) {
    goto error_out;
  }

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
  zathura->ui.page_view = gtk_vbox_new(FALSE, 0);
  if (!zathura->ui.page_view) {
    goto error_free;
  }

  gtk_widget_show(zathura->ui.page_view);
  gtk_box_set_spacing(GTK_BOX(zathura->ui.page_view), 0);

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

  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  g_signal_connect(G_OBJECT(view_vadjustment), "value-changed", G_CALLBACK(cb_view_vadjustment_value_changed), zathura);

  /* girara events */
  zathura->ui.session->events.buffer_changed = buffer_changed;

  /* load plugins */
  zathura_document_plugins_load(zathura);

  /* configuration */
  config_load_default(zathura);

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

  /* first page */
  if (!page_set(zathura, 0)) {
    goto error_free;
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
  /* empty page view */
  GList* container = gtk_container_get_children(GTK_CONTAINER(zathura->ui.page_view));
  for (GList* child = container; child; child = g_list_next(child)) {
    gtk_container_remove(GTK_CONTAINER(zathura->ui.page_view), child->data);
  }

  GtkWidget* row = NULL;

  /* create blank pages */
  for (unsigned int i = 0; i < zathura->document->number_of_pages; i++)
  {
    if (i % pages_per_row == 0) {
      row = gtk_hbox_new(FALSE, 0);
    }

    /* pack row */
    gtk_box_pack_start(GTK_BOX(row), zathura->document->pages[i]->event_box, FALSE, FALSE, 1);

    /* pack row to page view */
    if ((i + 1) % pages_per_row == 0) {
      gtk_box_pack_start(GTK_BOX(zathura->ui.page_view), row, FALSE,  FALSE, 1);
    }
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
