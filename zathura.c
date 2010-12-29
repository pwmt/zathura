/* See LICENSE file for license and copyright information */

#include <stdlib.h>

#include "callbacks.h"
#include "config.h"
#include "ft/document.h"
#include "shortcuts.h"
#include "zathura.h"
#include "utils.h"

/* function implementation */
bool
init_zathura()
{
  /* UI */
  if(!(Zathura.UI.session = girara_session_create())) {
    goto error_out;
  }

  if(!girara_session_init(Zathura.UI.session)) {
    goto error_out;
  }

  Zathura.UI.statusbar.file        = NULL;
  Zathura.UI.statusbar.buffer      = NULL;
  Zathura.UI.statusbar.page_number = NULL;
  Zathura.UI.drawing_area          = NULL;
  Zathura.UI.page_view             = NULL;

  /* page view */
  Zathura.UI.page_view = gtk_vbox_new(FALSE, 0);
  if(!Zathura.UI.page_view) {
    goto error_free;
  }

  gtk_widget_show(Zathura.UI.page_view);
  gtk_box_set_spacing(GTK_BOX(Zathura.UI.page_view), 0);

  /* statusbar */
  Zathura.UI.statusbar.file = girara_statusbar_item_add(Zathura.UI.session, TRUE, TRUE, TRUE, NULL);
  if(!Zathura.UI.statusbar.file) {
    goto error_free;
  }

  Zathura.UI.statusbar.buffer = girara_statusbar_item_add(Zathura.UI.session, FALSE, FALSE, FALSE, NULL);
  if(!Zathura.UI.statusbar.buffer) {
    goto error_free;
  }

  Zathura.UI.statusbar.page_number = girara_statusbar_item_add(Zathura.UI.session, FALSE, FALSE, FALSE, NULL);
  if(!Zathura.UI.statusbar.page_number) {
    goto error_free;
  }

  girara_statusbar_item_set_text(Zathura.UI.session, Zathura.UI.statusbar.file, "[No Name]");

  /* signals */
  g_signal_connect(G_OBJECT(Zathura.UI.session->gtk.window), "destroy", G_CALLBACK(cb_destroy), NULL);

  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(Zathura.UI.session->gtk.view));
  g_signal_connect(G_OBJECT(view_vadjustment), "value-changed", G_CALLBACK(cb_view_vadjustment_value_changed), NULL);

  /* girara events */
  Zathura.UI.session->events.buffer_changed = buffer_changed;

  /* configuration */
  config_load_default();

  return true;

error_free:

  if(Zathura.UI.drawing_area) {
    g_object_unref(Zathura.UI.drawing_area);
  }

  if(Zathura.UI.page_view) {
    g_object_unref(Zathura.UI.page_view);
  }

  girara_session_destroy(Zathura.UI.session);

error_out:

  return false;
}

bool
document_open(const char* path, const char* password)
{
  if(!path) {
    goto error_out;
  }

  zathura_document_t* document = zathura_document_open(path, password);

  if(!document) {
    goto error_out;
  }

  Zathura.document = document;

  /* create blank pages */
  for(unsigned int i = 0; i < document->number_of_pages; i++)
  {
    /* create blank page */
    GtkWidget* image = page_blank(document->pages[i]->width, document->pages[i]->height);

    if(!image) {
      goto error_free;
    }

    /* pack to page view */
    gtk_box_pack_start(GTK_BOX(Zathura.UI.page_view), image, TRUE,  TRUE, 0);
  }

  girara_set_view(Zathura.UI.session, Zathura.UI.page_view);

  /* first page */
  if(!page_set(0)) {
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
  if(!Zathura.document) {
    return false;
  }

  zathura_document_free(Zathura.document);

  return true;
}

bool
page_render(zathura_page_t* page)
{
  GtkWidget* image = zathura_page_render(page);
  if(!image) {
    goto error_out;
  }

  /* draw new rendered page */
  if(!girara_set_view(Zathura.UI.session, image)) {
    goto error_out;
  }

  return true;

error_out:

  return false;
}

bool
page_set(unsigned int page_id)
{
  if(!Zathura.document || !Zathura.document->pages) {
    goto error_out;
  }

  if(page_id >= Zathura.document->number_of_pages) {
    goto error_out;
  }

  /* render page */
  zathura_page_t* page = Zathura.document->pages[page_id];

  if(!page) {
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

/* main function */
int main(int argc, char* argv[])
{
  g_thread_init(NULL);
  gdk_threads_init();
  gtk_init(&argc, &argv);

  if(!init_zathura()) {
    printf("error: coult not initialize zathura\n");
    return -1;
  }

  if(argc > 1) {
    if(!document_open(argv[1], NULL)) {
      printf("error: could not open document\n");
      return -1;
    }
  }

  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  return 0;
}
