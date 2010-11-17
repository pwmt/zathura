/* See LICENSE file for license and copyright information */

#include <stdlib.h>

#include "document.h"

zathura_page_t*
zathura_page_get(zathura_document_t* document, unsigned int page)
{
  return NULL;
}

zathura_list_t*
zathura_page_search_text(zathura_page_t* page, const char* text)
{
  return NULL;
}

zathura_list_t*
zathura_page_links_get(zathura_page_t* page)
{
  return NULL;
}

bool
zathura_page_links_free(zathura_list_t* list)
{
  return false;
}

zathura_list_t*
zathura_page_form_fields_get(zathura_page_t* page)
{
  return NULL;
}

bool
zathura_page_form_fields_free(zathura_list_t* list)
{
  return false;
}

cairo_surface_t*
zathura_page_render(zathura_page_t* page)
{
  return NULL;
}

zathura_list_t*
zathura_document_index_generate(zathura_document_t* document)
{
  return NULL;
}

bool
zathura_document_index_free(zathura_list_t* list)
{
  return false;
}

bool
zathura_document_save_as(zathura_document_t* document, const char* path)
{
  return false;
}

zathura_list_t*
zathura_document_attachments_get(zathura_document_t* document)
{
  return NULL;
}

bool
zathura_document_attachments_free(zathura_list_t* list)
{
  return false;
}
