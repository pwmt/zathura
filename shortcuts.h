/* See LICENSE file for license and copyright information */

#ifndef SHORTCUTS_H
#define SHORTCUTS_H

#include <girara.h>

/**
 * Abort the current action and return to normal mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_abort(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Adjust the rendered pages to the window
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_adjust_window(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Modify the current buffer
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_change_buffer(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Change the current mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_change_mode(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Focus the inputbar
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Follow a link
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_follow(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Go to a specific page or position
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_goto(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Navigate through the document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_navigate(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Recolor the pages
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_recolor(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Reload the current document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_reload(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Rotate the pages
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_rotate(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Scroll through the pages
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_scroll(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Search through the document for the latest search item
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_search(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Switch go to mode (numeric, labels)
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_switch_goto_mode(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Navigate through the index of the document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_navigate_index(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Show/Hide the index of the document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_toggle_index(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Show/Hide the inputbar
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_toggle_inputbar(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Toggle fullscreen mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_toggle_fullscreen(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Show/Hide the statusbar
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_toggle_statusbar(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Quit zathura
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_quit(girara_session_t* session, girara_argument_t* argument, unsigned int t);

/**
 * Change the zoom level
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param t Number of executions
 * @return true if no error occured otherwise false
 */
bool sc_zoom(girara_session_t* session, girara_argument_t* argument, unsigned int t);

#endif // SHORTCUTS_H
