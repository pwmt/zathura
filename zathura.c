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
  if(!(Zathura.UI.session = girara_session_create())) {
    return false;
  }

  if(!girara_session_init(Zathura.UI.session)) {
    return false;
  }

  /* UI */
  Zathura.UI.statusbar.file = girara_statusbar_item_add(Zathura.UI.session, TRUE, TRUE, TRUE, NULL);
  if(!Zathura.UI.statusbar.file) {
    girara_session_destroy(Zathura.UI.session);
    return false;
  }

  Zathura.UI.statusbar.buffer = girara_statusbar_item_add(Zathura.UI.session, FALSE, FALSE, FALSE, NULL);
  if(!Zathura.UI.statusbar.buffer) {
    girara_session_destroy(Zathura.UI.session);
    return false;
  }

  Zathura.UI.statusbar.page_number = girara_statusbar_item_add(Zathura.UI.session, FALSE, FALSE, FALSE, NULL);
  if(!Zathura.UI.statusbar.page_number) {
    girara_session_destroy(Zathura.UI.session);
    return false;
  }

  girara_statusbar_item_set_text(Zathura.UI.session, Zathura.UI.statusbar.file, "[No Name]");

  /* signals */
  g_signal_connect(G_OBJECT(Zathura.UI.session->gtk.window), "destroy", G_CALLBACK(cb_destroy), NULL);

  /* girara events */
  Zathura.UI.session->events.buffer_changed = buffer_changed;

  /* configuration */
  config_load_default();

  return true;
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
    return false;
  }

  if(page_id >= Zathura.document->number_of_pages) {
    return false;
  }

  Zathura.document->current_page_number = page_id;

  char* page_number = g_strdup_printf("[%d/%d]", page_id + 1, Zathura.document->number_of_pages);
  girara_statusbar_item_set_text(Zathura.UI.session, Zathura.UI.statusbar.page_number, page_number);
  g_free(page_number);

  return true;
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
