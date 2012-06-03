/* See LICENSE file for license and copyright information */

#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stdlib.h>
#include <girara/types.h>

#include "zathura.h"
#include "callbacks.h"

/**
 * This function initializes a render thread
 *
 * @param zathura object
 * @return The render thread object or NULL if an error occured
 */
render_thread_t* render_init(zathura_t* zathura);

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
 *
 * @param zathura Zathura object
 */
void render_all(zathura_t* zathura);

/**
 * Lock the render thread. This is useful if you want to render on your own (e.g
 * for printing).
 *
 * @param render_thread The render thread object.
 */
void render_lock(render_thread_t* render_thread);

/**
 * Unlock the render thread.
 *
 * @param render_thread The render thread object.
 */
void render_unlock(render_thread_t* render_thread);

#endif // RENDER_H
