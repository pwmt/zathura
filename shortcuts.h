/* See LICENSE file for license and copyright information */

#ifndef SHORTCUTS_H
#define SHORTCUTS_H

#include <girara.h>

bool sc_abort(girara_session_t* session, girara_argument_t* argument);
bool sc_adjust_window(girara_session_t* session, girara_argument_t* argument);
bool sc_change_buffer(girara_session_t* session, girara_argument_t* argument);
bool sc_change_mode(girara_session_t* session, girara_argument_t* argument);
bool sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument);
bool sc_follow(girara_session_t* session, girara_argument_t* argument);
bool sc_goto(girara_session_t* session, girara_argument_t* argument);
bool sc_navigate(girara_session_t* session, girara_argument_t* argument);
bool sc_recolor(girara_session_t* session, girara_argument_t* argument);
bool sc_reload(girara_session_t* session, girara_argument_t* argument);
bool sc_rotate(girara_session_t* session, girara_argument_t* argument);
bool sc_scroll(girara_session_t* session, girara_argument_t* argument);
bool sc_search(girara_session_t* session, girara_argument_t* argument);
bool sc_switch_goto_mode(girara_session_t* session, girara_argument_t* argument);
bool sc_navigate_index(girara_session_t* session, girara_argument_t* argument);
bool sc_toggle_index(girara_session_t* session, girara_argument_t* argument);
bool sc_toggle_inputbar(girara_session_t* session, girara_argument_t* argument);
bool sc_toggle_fullscreen(girara_session_t* session, girara_argument_t* argument);
bool sc_toggle_statusbar(girara_session_t* session, girara_argument_t* argument);
bool sc_quit(girara_session_t* session, girara_argument_t* argument);
bool sc_zoom(girara_session_t* session, girara_argument_t* argument);

#endif // SHORTCUTS_H
