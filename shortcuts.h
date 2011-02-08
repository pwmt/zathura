/* See LICENSE file for license and copyright information */

#ifndef SHORTCUTS_H
#define SHORTCUTS_H

#include <girara.h>

/**
 * Abort the current action and return to normal mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_abort(girara_session_t* session, girara_argument_t* argument);

/**
 * Adjust the rendered pages to the window
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_adjust_window(girara_session_t* session, girara_argument_t* argument);

/**
 * Modify the current buffer
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_change_buffer(girara_session_t* session, girara_argument_t* argument);

/**
 * Change the current mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_change_mode(girara_session_t* session, girara_argument_t* argument);

/**
 * Focus the inputbar
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument);

/**
 * Follow a link
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_follow(girara_session_t* session, girara_argument_t* argument);

/**
 * Go to a specific page or position
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_goto(girara_session_t* session, girara_argument_t* argument);

/**
 * Navigate through the document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_navigate(girara_session_t* session, girara_argument_t* argument);

/**
 * Recolor the pages
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_recolor(girara_session_t* session, girara_argument_t* argument);

/**
 * Reload the current document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_reload(girara_session_t* session, girara_argument_t* argument);

/**
 * Rotate the pages
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_rotate(girara_session_t* session, girara_argument_t* argument);

/**
 * Scroll through the pages
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_scroll(girara_session_t* session, girara_argument_t* argument);

/**
 * Search through the document for the latest search item
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_search(girara_session_t* session, girara_argument_t* argument);

/**
 * Switch go to mode (numeric, labels)
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_switch_goto_mode(girara_session_t* session, girara_argument_t* argument);

/**
 * Navigate through the index of the document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_navigate_index(girara_session_t* session, girara_argument_t* argument);

/**
 * Show/Hide the index of the document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_toggle_index(girara_session_t* session, girara_argument_t* argument);

/**
 * Show/Hide the inputbar
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_toggle_inputbar(girara_session_t* session, girara_argument_t* argument);

/**
 * Toggle fullscreen mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_toggle_fullscreen(girara_session_t* session, girara_argument_t* argument);

/**
 * Show/Hide the statusbar
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_toggle_statusbar(girara_session_t* session, girara_argument_t* argument);

/**
 * Quit zathura
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_quit(girara_session_t* session, girara_argument_t* argument);

/**
 * Change the zoom level
 *
 * @param session The used girara session
 * @param argument The used argument
 * @return true if no error occured otherwise false
 */
bool sc_zoom(girara_session_t* session, girara_argument_t* argument);

#endif // SHORTCUTS_H
