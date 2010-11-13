/* See LICENSE file for license and copyright information */

#ifndef ZATHURA_H
#define ZATHURA_H

#include <stdbool.h>
#include <girara.h>

enum { NEXT, PREVIOUS, LEFT, RIGHT, UP, DOWN, BOTTOM, TOP, HIDE, HIGHLIGHT,
  DELETE_LAST_WORD, DELETE_LAST_CHAR, DEFAULT, ERROR, WARNING, NEXT_GROUP,
  PREVIOUS_GROUP, ZOOM_IN, ZOOM_OUT, ZOOM_ORIGINAL, ZOOM_SPECIFIC, FORWARD,
  BACKWARD, ADJUST_BESTFIT, ADJUST_WIDTH, ADJUST_NONE, CONTINUOUS, DELETE_LAST,
  ADD_MARKER, EVAL_MARKER, EXPAND, COLLAPSE, SELECT, GOTO_DEFAULT, GOTO_LABELS,
  GOTO_OFFSET, HALF_UP, HALF_DOWN, FULL_UP, FULL_DOWN, NEXT_CHAR, PREVIOUS_CHAR,
  DELETE_TO_LINE_START, APPEND_FILEPATH };

/* define modes */
#define ALL        (1 << 0)
#define FULLSCREEN (1 << 1)
#define INDEX      (1 << 2)
#define NORMAL     (1 << 3)
#define INSERT     (1 << 4)

struct
{
  struct 
  {
    girara_session_t* session; /**> girara interface session */
  } UI;
} Zathura;

/**
 * Initializes zathura
 *
 * @return If no error occured true, otherwise false, is returned.
 */

bool init_zathura();

#endif // ZATHURA_H
