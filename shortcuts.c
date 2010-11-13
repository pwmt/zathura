/* See LICENSE file for license and copyright information */

#include <girara.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "shortcuts.h"

bool
sc_abort(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_adjust_window(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_change_buffer(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_change_mode(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_follow(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_goto(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_navigate(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_recolor(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_reload(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_rotate(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_scroll(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_search(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_switch_goto_mode(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_navigate_index(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_toggle_index(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_toggle_inputbar(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_toggle_fullscreen(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_toggle_statusbar(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_quit(girara_session_t* session, girara_argument_t* argument)
{
  girara_argument_t arg = { GIRARA_HIDE, NULL };
  girara_isc_completion(session, &arg);

  cb_destroy(NULL, NULL);

  gtk_main_quit();

  return false;
}

bool
sc_zoom(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}
