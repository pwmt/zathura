/* See LICENSE file for license and copyright information */

#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stdlib.h>
#include <girara-datastructures.h>

#include "ft/document.h"
#include "callbacks.h"

typedef struct render_thread_s
{
  girara_list_t* list; /**> The list of pages */
  GThread* thread; /**> The thread object */
  GMutex* lock; /**> Lock */
  GCond* cond; /**> Condition */
} render_thread_t;

/**
 * This function initializes a render thread
 *
 * @return The render thread object or NULL if an error occured
 */
render_thread_t* render_init(void);

/**
 * This function destroys the render thread object
 *
 * @param render_thread The render thread object
 */
void render_free(render_thread_t* render_thread);

/**
 * This function is used to add a page to the render thread list
 * that should be rendered.
 *
 * @param render_thread The render thread object
 * @param page The page that should be rendered
 * @return true if no error occured
 */
bool render_page(render_thread_t* render_thread, zathura_page_t* page);

/**
 * This function is used to unmark all pages as not rendered. This should
 * be used if all pages should be rendered again (e.g.: the zoom level or the
 * colors have changed)
 */
void render_all(void);

#endif // RENDER_H
