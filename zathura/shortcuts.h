/* SPDX-License-Identifier: Zlib */

#ifndef SHORTCUTS_H
#define SHORTCUTS_H

#include <girara/types.h>

/**
 * Abort the current action and return to normal mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_abort(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Adjust the rendered pages to the window
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_adjust_window(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Change the current mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_change_mode(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Cycle the column the first page is displayed in
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_cycle_first_column(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Display a link
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_display_link(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Copy a link
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_copy_link(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);


/**
 * Copy opened file path
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_copy_filepath(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Shortcut function to focus the inputbar
 *
 * @param session The used girara session
 * @param argument The argument
 * @param event Girara event
 * @param t Number of executions
 * @return true No error occurred
 * @return false An error occurred (abort execution)
 */
bool sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Follow a link
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_follow(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Go to a specific page or position
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_goto(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Handle mouse events
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_mouse_scroll(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Handle mouse zoom events
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_mouse_zoom(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Navigate through the document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_navigate(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Calls the print dialog
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_print(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Recolor the pages
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_recolor(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Reload the current document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_reload(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Rotate the pages
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_rotate(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Scroll through the pages
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_scroll(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Navigate through the jumplist
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_jumplist(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Bisect through the document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_bisect(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Search through the document for the latest search item
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_search(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Navigate through the index of the document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_navigate_index(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Show/Hide the index of the document
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_toggle_index(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Toggle multi page mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_toggle_page_mode(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Toggle fullscreen mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_toggle_fullscreen(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Toggle presentation mode
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_toggle_presentation(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Quit zathura
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_quit(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Change the zoom level
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_zoom(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Run external command.
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_exec(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Remove search highlights.
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_nohlsearch(girara_session_t* session, girara_argument_t* argument, girara_event_t* event, unsigned int t);

/**
 * Snaps the current view to the current page
 *
 * @param session The used girara session
 * @param argument The used argument
 * @param event Girara event
 * @param t Number of executions
 * @return true if no error occurred otherwise false
 */
bool sc_snap_to_page(girara_session_t *session, girara_argument_t *argument, girara_event_t *event, unsigned int t);

#endif // SHORTCUTS_H
