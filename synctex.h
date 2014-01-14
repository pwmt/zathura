/* See LICENSE file for license and copyright information */

#ifndef SYNCTEX_H
#define SYNCTEX_H

#include "types.h"

typedef struct synctex_page_rect_s {
  int                 page;
  zathura_rectangle_t rect;
} synctex_page_rect_t;

void synctex_edit(zathura_t* zathura, zathura_page_t* page, int x, int y);
girara_list_t* synctex_rectangles_from_position(const char* filename,
    const char* position, int* page, girara_list_t** secondary_rects);

#endif
