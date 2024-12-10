/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_JUMPLIST_H
#define ZATHURA_JUMPLIST_H

#include <girara/datastructures.h>
#include "types.h"

typedef struct zathura_jumplist_s {
  girara_list_t* list;
  girara_list_iterator_t* cur;
  unsigned int size;
  unsigned int max_size;
} zathura_jumplist_t;

/**
 * Checks whether current jump has a previous jump
 *
 * @param zathura The zathura session
 * @return true if current jump has a previous jump
 */
bool zathura_jumplist_has_previous(zathura_t* jumplzathura);

/**
 * Checks whether current jump has a next jump
 *
 * @param zathura The zathura session
 * @return true if current jump has a next jump
 */
bool zathura_jumplist_has_next(zathura_t* zathura);

/**
 * Return current jump in the jumplist
 *
 * @param zathura The zathura session
 * @return current jump
 */
zathura_jump_t* zathura_jumplist_current(zathura_t* zathura);

/**
 * Move forward in the jumplist
 *
 * @param zathura The zathura session
 */
void zathura_jumplist_forward(zathura_t* zathura);

/**
 * Move backward in the jumplist
 *
 * @param zathura The zathura session
 */
void zathura_jumplist_backward(zathura_t* zathura);

/**
 * Add current page as a new item to the jumplist after current position
 *
 * @param zathura The zathura session
 */
void zathura_jumplist_add(zathura_t* zathura);

/**
 * Trim entries from the beginning of the jumplist to maintain it's maximum size constraint.
 *
 * @param zathura The zathura session
 */
void zathura_jumplist_trim(zathura_t* zathura);

/**
 * Load the jumplist of the specified file
 *
 * @param zathura The zathura session
 * @param file The file whose jumplist is to be loaded
 *
 * return A linked list of zathura_jump_t structures constituting the jumplist of the specified file, or NULL.
 */
bool zathura_jumplist_load(zathura_t* zathura, const char* file);

/**
 * Init jumplist with a maximum size
 *
 * @param zathura The zathura session
 * @param max_size maximum jumplist size (or 0 for unbounded lists)
 */
void zathura_jumplist_init(zathura_t* zathura, size_t max_size);

/**
 * Clear jumplist
 *
 * @param zathura The zathura session
 */
void zathura_jumplist_clear(zathura_t* zathura);

#endif
