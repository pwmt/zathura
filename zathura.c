/* See LICENSE file for license and copyright information */

#include "callbacks.h"
#include "config.h"
#include "ft/document.h"
#include "shortcuts.h"
#include "zathura.h"

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

  /* girara events */
  Zathura.UI.session->events.buffer_changed = buffer_changed;

  /* configuration */
  config_load_default();

  return true;

error_free:

  if(Zathura.UI.drawing_area) {
    g_object_unref(Zathura.UI.drawing_area);
  }

  girara_session_destroy(Zathura.UI.session);

error_out:

  return false;
}

bool
document_open(const char* path, const char* password)
{
  if(!path) {
    return false;
  }

  zathura_document_t* document = zathura_document_open(path, password);

  if(!document) {
    return false;
  }

  Zathura.document = document;

  if(!page_set(0)) {
    zathura_document_free(document);
    return false;
  }

  return true;
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
page_set(unsigned int page_id)
{
  if(!Zathura.document) {
    goto error_out;
  }

  if(page_id >= Zathura.document->number_of_pages) {
    goto error_out;
  }

  /* render page */
  zathura_page_t* page = zathura_page_get(Zathura.document, page_id);
  if(!page) {
    goto error_out;
  }

  GtkWidget* image = zathura_page_render(page);
  if(!image) {
    zathura_page_free(page);
    fprintf(stderr, "error: rendering failed\n");
    goto error_out;
  }

  zathura_page_free(page);

  /* draw new rendered page */
  if(!girara_set_view(Zathura.UI.session, image)) {
    goto error_out;
  }

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
