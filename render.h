/* See LICENSE file for license and copyright information */

#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stdlib.h>
#include <girara-datastructures.h>

#include "ft/document.h"

typedef struct render_thread_s
{
  girara_list_t* list;
  GThread* thread;
  GStaticMutex lock;
} render_thread_t;

render_thread_t* render_init(void);
void render_free(render_thread_t* render_thread);
bool render_page(render_thread_t* render_thread, zathura_page_t* page);

#endif // RENDER_H
