/* See LICENSE file for license and copyright information */

#include "shortcuts.h"
#include "zathura.h"

void
config_load_default()
{
  if(!Zathura.UI.session)
    return;

  girara_shortcut_add(Zathura.UI.session, GDK_CONTROL_MASK, GDK_q, NULL, sc_quit, 0, 0, NULL);
}
