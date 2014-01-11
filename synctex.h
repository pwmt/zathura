/* See LICENSE file for license and copyright information */

#ifndef SYNCTEX_H
#define SYNCTEX_H

#include "types.h"

void synctex_edit(zathura_t* zathura, zathura_page_t* page, int x, int y);
girara_list_t* synctex_rectangles_from_position(const char* filename, const char* position, int* page);

#endif
