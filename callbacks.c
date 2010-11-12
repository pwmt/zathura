/* See LICENSE file for license and copyright information */

#include <callbacks.h>
#include <girara.h>

#include "zathura.h"

gboolean
cb_destroy(GtkWidget* widget, gpointer data)
{
  if(Zathura.UI.session)
    girara_session_destroy(Zathura.UI.session);

  return TRUE;
}
