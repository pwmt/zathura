/* See LICENSE file for license and copyright information */

#ifndef SYNCTEX_H
#define SYNCTEX_H

#include "types.h"

void synctex_edit(zathura_t* zathura, zathura_page_t* page, int x, int y);
bool synctex_view(zathura_t* zathura, const char* position);

#endif
