/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_VISUAL_H
#define ZATHURA_VISUAL_H

#include <stdbool.h>
#include <stddef.h>
#include <glib.h>

#include "types.h"
#include "zathura.h"

/**
 * Kind of keyboard selection
 */
typedef enum zathura_visual_kind_e {
  ZATHURA_VISUAL_CARET, /**< Only a caret, nothing is selected */
  ZATHURA_VISUAL_CHAR,  /**< Characterwise selection */
  ZATHURA_VISUAL_LINE,  /**< Linewise selection */
  ZATHURA_VISUAL_IMAGE, /**< An image is selected */
} zathura_visual_kind_t;

/**
 * Motions of the caret in visual mode
 */
typedef enum zathura_visual_motion_e {
  ZATHURA_VISUAL_MOTION_LEFT,                /**< h */
  ZATHURA_VISUAL_MOTION_RIGHT,               /**< l */
  ZATHURA_VISUAL_MOTION_UP,                  /**< k */
  ZATHURA_VISUAL_MOTION_DOWN,                /**< j */
  ZATHURA_VISUAL_MOTION_WORD_FORWARD,        /**< w */
  ZATHURA_VISUAL_MOTION_WORD_BACKWARD,       /**< b */
  ZATHURA_VISUAL_MOTION_WORD_END,            /**< e */
  ZATHURA_VISUAL_MOTION_BIGWORD_FORWARD,     /**< W */
  ZATHURA_VISUAL_MOTION_BIGWORD_BACKWARD,    /**< B */
  ZATHURA_VISUAL_MOTION_BIGWORD_END,         /**< E */
  ZATHURA_VISUAL_MOTION_LINE_START,          /**< 0 */
  ZATHURA_VISUAL_MOTION_LINE_FIRST_NONBLANK, /**< ^ */
  ZATHURA_VISUAL_MOTION_LINE_END,            /**< $ */
  ZATHURA_VISUAL_MOTION_TOP,                 /**< gg */
  ZATHURA_VISUAL_MOTION_BOTTOM,              /**< G */
} zathura_visual_motion_t;

/**
 * How y takes the text a motion moved over, as in vim
 */
typedef enum zathura_visual_operator_range_e {
  ZATHURA_VISUAL_OPERATOR_EXCLUSIVE, /**< Without the character the motion ends on: h, l, w, b, 0, ^, F, T */
  ZATHURA_VISUAL_OPERATOR_INCLUSIVE, /**< With the character the motion ends on: e, $, f, t */
  ZATHURA_VISUAL_OPERATOR_LINEWISE,  /**< Whole lines: j, k, gg, G */
} zathura_visual_operator_range_t;

/**
 * Text of a page prepared for caret motions
 */
typedef struct zathura_visual_text_s {
  gunichar* chars;                  /**< Characters of the page */
  const zathura_rectangle_t* rects; /**< Bounding box of each character (owned by the page) */
  size_t n;                         /**< Number of characters */
} zathura_visual_text_t;

/**
 * Move a caret position in a text.
 *
 * @param text The text
 * @param pos The current caret position
 * @param motion The motion to apply
 * @param curswant Horizontal position to keep for up and down motions. Pass a
 *   negative value to use the position of the caret. Set to the value to pass
 *   to the next motion.
 * @return The new caret position
 */
size_t zathura_visual_motion(const zathura_visual_text_t* text, size_t pos, zathura_visual_motion_t motion,
                             double* curswant);

/**
 * Get the first and last character covered by a selection.
 *
 * @param text The text
 * @param kind Kind of the selection
 * @param anchor The anchor of the selection
 * @param cursor The caret position
 * @param first Set to the first selected character
 * @param last Set to the last selected character
 */
void zathura_visual_range(const zathura_visual_text_t* text, zathura_visual_kind_t kind, size_t anchor, size_t cursor,
                          size_t* first, size_t* last);

/**
 * Get the first and last character of a number of lines.
 *
 * @param text The text
 * @param pos A character of the first line
 * @param count Number of lines, stopping at the end of the text
 * @param first Set to the first character of the first line
 * @param last Set to the last character of the last line
 */
void zathura_visual_lines(const zathura_visual_text_t* text, size_t pos, unsigned int count, size_t* first,
                          size_t* last);

/**
 * Find a character on the line of the caret, like f, F, t and T in vim.
 *
 * @param text The text
 * @param pos The current caret position
 * @param c The character to find
 * @param forward Search after the caret, otherwise before it
 * @param till Stop next to the character instead of on it
 * @param repeat The search is repeated, so a till search skips a match next to the caret
 * @param count Find the count-th occurrence
 * @return The new caret position, or pos if the character was not found
 */
size_t zathura_visual_find(const zathura_visual_text_t* text, size_t pos, gunichar c, bool forward, bool till,
                           bool repeat, unsigned int count);

/**
 * Get the word around a position, like the iw, aw, iW and aW text objects in vim.
 *
 * @param text The text
 * @param pos The position
 * @param big Words are separated by blanks only
 * @param around Include the blanks after the word, or before it if there are none after it
 * @param first Set to the first character of the word
 * @param last Set to the last character of the word
 */
void zathura_visual_word_object(const zathura_visual_text_t* text, size_t pos, bool big, bool around, size_t* first,
                                size_t* last);

/**
 * Get the text that y followed by a motion takes, as in vim. An exclusive
 * motion that ends at the start of a line takes the line before it up to its
 * end, without the newline.
 *
 * @param text The text
 * @param origin The caret position before the motion
 * @param target The caret position after the motion
 * @param range How the motion takes the text
 * @param first Set to the first character to take
 * @param last Set to the last character to take
 * @return false if the motion takes no text, like an exclusive motion that did
 *   not move
 */
bool zathura_visual_operator_range(const zathura_visual_text_t* text, size_t origin, size_t target,
                                   zathura_visual_operator_range_t range, size_t* first, size_t* last);

/**
 * Get the caret position when a motion continues on another page.
 *
 * @param text The text of the new page
 * @param motion The motion: down, word-forward and word-end continue on the
 *   next page, up and word-backward on the previous one
 * @param curswant Horizontal position to keep for up and down motions
 * @return The position, or text->n if the motion has no position on the page
 */
size_t zathura_visual_page_entry(const zathura_visual_text_t* text, zathura_visual_motion_t motion, double curswant);

/**
 * Bounding box to draw the caret on a character. Characters without a size,
 * like the newline of an empty line, get the size of the text around them.
 *
 * @param text The text
 * @param pos The position of the caret
 * @return The bounding box
 */
zathura_rectangle_t zathura_visual_caret_rectangle(const zathura_visual_text_t* text, size_t pos);

/**
 * Get the character at a point of the page, or the closest one: the closest
 * line first, then the closest character on it.
 *
 * @param text The text
 * @param x,y The point in page coordinates
 * @return The position of the character
 */
size_t zathura_visual_index_at(const zathura_visual_text_t* text, double x, double y);

/**
 * Enter visual mode on the current page
 *
 * @param zathura The zathura session
 * @param kind The kind of selection to start with
 * @return true if visual mode was entered
 */
bool zathura_visual_enter(zathura_t* zathura, zathura_visual_kind_t kind);

/**
 * Leave visual mode and clear the selection
 *
 * @param zathura The zathura session
 */
void zathura_visual_exit(zathura_t* zathura);

/**
 * Switch to another kind of selection. Switching to the current kind of a
 * selection goes back to a caret, like v and V in vim.
 *
 * @param zathura The zathura session
 * @param kind The kind of selection
 */
void zathura_visual_toggle(zathura_t* zathura, zathura_visual_kind_t kind);

/**
 * Move the caret
 *
 * @param zathura The zathura session
 * @param motion The motion
 * @param count How often to apply the motion
 */
void zathura_visual_move(zathura_t* zathura, zathura_visual_motion_t motion, unsigned int count);

/**
 * Swap the caret and the anchor of the selection
 *
 * @param zathura The zathura session
 */
void zathura_visual_swap(zathura_t* zathura);

/**
 * Copy the selected text to the selection clipboard and leave visual mode.
 * Without a selection, the first call waits for what to copy, like the y
 * operator in vim: the next motion, f, F, t, T, ;, ",", text object or label
 * copies the text it covers, and a second call copies the line of the caret
 * and the lines after it, like yy.
 *
 * @param zathura The zathura session
 * @param count How often to apply the motion, or the number of lines for yy
 */
void zathura_visual_yank(zathura_t* zathura, unsigned int count);

/**
 * Copy to the end of the line and stay in visual mode, like Y in vim. With a
 * selection, copy its whole lines and leave visual mode.
 *
 * @param zathura The zathura session
 * @param count Copy to the end of the count-th line
 */
void zathura_visual_yank_line_end(zathura_t* zathura, unsigned int count);

/**
 * Cancel a y that waits for what to copy
 *
 * @param zathura The zathura session
 * @return true if a y was cancelled
 */
bool zathura_visual_cancel(zathura_t* zathura);

/**
 * Move the caret to a character on the line, or repeat the last search
 *
 * @param zathura The zathura session
 * @param c The character, or 0 to repeat the last search
 * @param forward Search forward, or, when repeating, in the direction of the last search
 * @param till Stop next to the character
 * @param count Find the count-th occurrence
 */
void zathura_visual_find_char(zathura_t* zathura, gunichar c, bool forward, bool till, unsigned int count);

/**
 * Select the word under the caret
 *
 * @param zathura The zathura session
 * @param big Words are separated by blanks only
 * @param around Include the blanks around the word
 */
void zathura_visual_select_word(zathura_t* zathura, bool big, bool around);

/**
 * Enter visual mode with the last selection, like gv in vim
 *
 * @param zathura The zathura session
 * @return true if there was a selection to restore
 */
bool zathura_visual_reselect(zathura_t* zathura);

/**
 * Show labels on the visible words and move the caret to the word whose label
 * is typed, like flash.nvim
 *
 * @param zathura The zathura session
 */
void zathura_visual_jump(zathura_t* zathura);

/**
 * Start tracking a mouse selection that may continue on other pages
 *
 * @param zathura The zathura session
 * @param page_index The page where the button was pressed
 * @param x,y The point where the button was pressed, in page coordinates
 */
void zathura_visual_mouse_press(zathura_t* zathura, unsigned int page_index, double x, double y);

/**
 * Update the mouse selection while the button is held. On the page where the
 * selection started, the page selects as before; on other pages, the text
 * from the start to the pointer is selected.
 *
 * @param zathura The zathura session
 * @param widget The page widget that received the motion
 * @param x,y The pointer, relative to the widget
 * @return true if the motion was handled, false if the page should handle it
 */
bool zathura_visual_mouse_motion(zathura_t* zathura, ZathuraPageWidget* widget, double x, double y);

/**
 * Finish the mouse selection and copy it if it spans pages
 *
 * @param zathura The zathura session
 * @return true if a selection over several pages was finished
 */
bool zathura_visual_mouse_release(zathura_t* zathura);

/**
 * Stop tracking the mouse selection without copying it, e.g. when the press
 * was used by a mouse binding
 *
 * @param zathura The zathura session
 */
void zathura_visual_mouse_cancel(zathura_t* zathura);

/**
 * Clear the mouse selection over several pages
 *
 * @param zathura The zathura session
 */
void zathura_visual_mouse_clear(zathura_t* zathura);

/**
 * Leave visual mode and drop everything that refers to the pages of the
 * document: the selection, the last selection for gv, the mouse selection and
 * the flash. Called when the document is closed.
 *
 * @param zathura The zathura session
 */
void zathura_visual_reset(zathura_t* zathura);

#endif // ZATHURA_VISUAL_H
