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
    zathura_document_t* document = zathura_document_open(argv[1], NULL);

    if(!document) {
      return -1;
    }

    zathura_page_t* page = zathura_page_get(document, 0);

    if(!page) {
      zathura_document_free(document);
      return -2;
    }

    zathura_page_free(page);
    zathura_document_free(document);
  }

  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  return 0;
}
