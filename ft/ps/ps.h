/* See LICENSE file for license and copyright information */

#ifndef PS_H
#define PS_H

#include <stdbool.h>
#include <libspectre/spectre.h>

#include "../../document.h"

typedef struct ps_document_s
{
  SpectreDocument* document;
} ps_document_t;

bool ps_document_open(zathura_document_t* document);
bool ps_document_free(zathura_document_t* document);
zathura_page_t* ps_page_get(zathura_document_t* document, unsigned int page);
zathura_image_buffer_t* ps_page_render(zathura_page_t* page);
bool ps_page_free(zathura_page_t* page);

#endif // PS_H
