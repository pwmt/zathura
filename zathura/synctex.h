/* SPDX-License-Identifier: Zlib */

#ifndef SYNCTEX_H
#define SYNCTEX_H

#include "types.h"

typedef struct synctex_page_rect_s {
  int                 page;
  zathura_rectangle_t rect;
} synctex_page_rect_t;

bool synctex_get_input_line_column(const char* filename, unsigned int page,
    int x, int y, char** input_file, unsigned int* line, unsigned int* column);

void synctex_edit(const char* editor, zathura_page_t* page, int x, int y);

bool synctex_parse_input(const char* synctex, char** input_file, int* line,
                         int* column);

girara_list_t* synctex_rectangles_from_position(const char* filename,
    const char* input_file, int line, int column, unsigned int* page,
    girara_list_t** secondary_rects);

void synctex_highlight_rects(zathura_t* zathura, unsigned int page,
                             girara_list_t** rectangles);

bool synctex_view(zathura_t* zathura, const char* input_file,
                  unsigned int line, unsigned int column);

#endif
