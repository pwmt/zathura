/* See LICENSE file for license and copyright information */

#include <callbacks.h>
#include <girara.h>
#include <stdlib.h>

#include "zathura.h"

gboolean
cb_destroy(GtkWidget* widget, gpointer data)
{
  if(Zathura.UI.session) {
    girara_session_destroy(Zathura.UI.session);
  }

  return TRUE;
}

void
buffer_changed(girara_session_t* session)
{
  g_return_if_fail(session != NULL);

  char* buffer = girara_buffer_get(session);

  if(buffer) {
    girara_statusbar_item_set_text(session, Zathura.UI.statusbar.buffer, buffer);
    free(buffer);
  } else {
    girara_statusbar_item_set_text(session, Zathura.UI.statusbar.buffer, "");
  }
}
