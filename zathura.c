/* See LICENSE file for license and copyright information */

#include <stdlib.h>

#include <girara.h>

#include "callbacks.h"
#include "config.h"
#include "document.h"
#include "shortcuts.h"
#include "zathura.h"
#include "utils.h"

/* function implementation */
bool
init_zathura()
{
  /* UI */
  if (!(Zathura.UI.session = girara_session_create())) {
    goto error_out;
  }

  if (!girara_session_init(Zathura.UI.session)) {
    goto error_out;
  }

  Zathura.UI.statusbar.file        = NULL;
  Zathura.UI.statusbar.buffer      = NULL;
  Zathura.UI.statusbar.page_number = NULL;
  Zathura.UI.page_view             = NULL;
  Zathura.UI.index                 = NULL;

  /* page view */
  Zathura.UI.page_view = gtk_vbox_new(FALSE, 0);
  if (!Zathura.UI.page_view) {
    goto error_free;
  }

  gtk_widget_show(Zathura.UI.page_view);
  gtk_box_set_spacing(GTK_BOX(Zathura.UI.page_view), 0);

  /* statusbar */
  Zathura.UI.statusbar.file = girara_statusbar_item_add(Zathura.UI.session, TRUE, TRUE, TRUE, NULL);
  if (!Zathura.UI.statusbar.file) {
    goto error_free;
  }

  Zathura.UI.statusbar.buffer = girara_statusbar_item_add(Zathura.UI.session, FALSE, FALSE, FALSE, NULL);
  if (!Zathura.UI.statusbar.buffer) {
    goto error_free;
  }

  Zathura.UI.statusbar.page_number = girara_statusbar_item_add(Zathura.UI.session, FALSE, FALSE, FALSE, NULL);
  if (!Zathura.UI.statusbar.page_number) {
    goto error_free;
  }

  girara_statusbar_item_set_text(Zathura.UI.session, Zathura.UI.statusbar.file, "[No Name]");

  /* signals */
  g_signal_connect(G_OBJECT(Zathura.UI.session->gtk.window), "destroy", G_CALLBACK(cb_destroy), NULL);

  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(Zathura.UI.session->gtk.view));
  g_signal_connect(G_OBJECT(view_vadjustment), "value-changed", G_CALLBACK(cb_view_vadjustment_value_changed), NULL);

  /* girara events */
  Zathura.UI.session->events.buffer_changed = buffer_changed;

  /* load plugins */
  zathura_document_plugins_load();

  /* configuration */
  config_load_default();

  return true;

error_free:

  if (Zathura.UI.page_view) {
    g_object_unref(Zathura.UI.page_view);
  }

  girara_session_destroy(Zathura.UI.session);

error_out:

  return false;
}

bool
document_open(const char* path, const char* password)
{
  if (!path) {
    goto error_out;
  }

  zathura_document_t* document = zathura_document_open(path, password);

  if (!document) {
    goto error_out;
  }

  Zathura.document = document;

  /* init view */
  if (create_blank_pages() == false) {
    return false;
  }

  /* view mode */
  int* value = girara_setting_get(Zathura.UI.session, "pages-per-row");
  int pages_per_row = (value) ? *value : 1;
  free(value);
  page_view_set_mode(pages_per_row);

  girara_set_view(Zathura.UI.session, Zathura.UI.page_view);

  /* threads */
  Zathura.Sync.render_thread = render_init();

  if (!Zathura.Sync.render_thread) {
    goto error_free;
  }

  /* first page */
  if (!page_set(0)) {
    goto error_free;
  }

  return true;

error_free:

  zathura_document_free(document);

error_out:

  return false;
}

bool
document_close()
{
  if (!Zathura.document) {
    return false;
  }

  if (Zathura.Sync.render_thread) {
    render_free(Zathura.Sync.render_thread);
  }

  zathura_document_free(Zathura.document);

  return true;
}

bool
page_set(unsigned int page_id)
{
  if (!Zathura.document || !Zathura.document->pages) {
    goto error_out;
  }

  if (page_id >= Zathura.document->number_of_pages) {
    goto error_out;
  }

  /* render page */
  zathura_page_t* page = Zathura.document->pages[page_id];

  if (!page) {
    goto error_out;
  }

  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(Zathura.UI.session->gtk.view));
  cb_view_vadjustment_value_changed(view_vadjustment, NULL);

  /* update page number */
  Zathura.document->current_page_number = page_id;

  char* page_number = g_strdup_printf("[%d/%d]", page_id + 1, Zathura.document->number_of_pages);
  girara_statusbar_item_set_text(Zathura.UI.session, Zathura.UI.statusbar.page_number, page_number);
  g_free(page_number);

  return true;

error_out:

  return false;
}

void
page_view_set_mode(unsigned int pages_per_row)
{
  /* empty page view */
  GList* container = gtk_container_get_children(GTK_CONTAINER(Zathura.UI.page_view));
  for (GList* child = container; child; child = g_list_next(child)) {
    gtk_container_remove(GTK_CONTAINER(Zathura.UI.page_view), child->data);
  }

  GtkWidget* row = NULL;

  /* create blank pages */
  for (unsigned int i = 0; i < Zathura.document->number_of_pages; i++)
  {
    if (i % pages_per_row == 0) {
      row = gtk_hbox_new(FALSE, 0);
    }

    /* pack row */
    gtk_box_pack_start(GTK_BOX(row), Zathura.document->pages[i]->event_box, FALSE, FALSE, 1);

    /* pack row to page view */
    if ((i + 1) % pages_per_row == 0) {
      gtk_box_pack_start(GTK_BOX(Zathura.UI.page_view), row, FALSE,  FALSE, 1);
    }
  }

  gtk_widget_show_all(Zathura.UI.page_view);
}

bool
create_blank_pages()
{
  /* create blank pages */
  for (unsigned int i = 0; i < Zathura.document->number_of_pages; i++)
  {
    zathura_page_t* page = Zathura.document->pages[i];
    g_static_mutex_lock(&(page->lock));

    cairo_t* cairo = gdk_cairo_create(page->drawing_area->window);

    if (cairo == NULL) {
      girara_error("Could not create blank page");
      g_static_mutex_unlock(&(page->lock));
      return false;
    }

    cairo_set_source_rgb(cairo, 1, 1, 1);
    cairo_rectangle(cairo, 0, 0, page->width, page->height);
    cairo_fill(cairo);
    cairo_destroy(cairo);

    g_static_mutex_unlock(&(page->lock));
  }

  return true;
}

/* main function */
int main(int argc, char* argv[])
{
  g_thread_init(NULL);
  gdk_threads_init();
  gtk_init(&argc, &argv);

  if (!init_zathura()) {
    printf("error: coult not initialize zathura\n");
    return -1;
  }

  if (argc > 1) {
    if (!document_open(argv[1], NULL)) {
      printf("error: could not open document\n");
      return -1;
    }
  }

  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  return 0;
}
