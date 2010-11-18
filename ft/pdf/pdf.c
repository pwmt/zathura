/* See LICENSE file for license and copyright information */

#include <stdlib.h>

#include "pdf.h"

bool
pdf_document_open(zathura_document_t* document)
{
  if(!document)
    return false;

  document->functions.document_free             = pdf_document_free;
  document->functions.document_index_generate   = pdf_document_index_generate;;
  document->functions.document_save_as          = pdf_document_save_as;
  document->functions.document_attachments_get  = pdf_document_attachments_get;
  document->functions.page_get                  = pdf_page_get;
  document->functions.page_search_text          = pdf_page_search_text;
  document->functions.page_links_get            = pdf_page_links_get;
  document->functions.page_form_fields_get      = pdf_page_form_fields_get;
  document->functions.page_render               = pdf_page_render;
  document->functions.page_free                 = pdf_page_free;

  return true;
}

bool
pdf_document_free(zathura_document_t* document)
{
  return false;
}

zathura_list_t*
pdf_document_index_generate(zathura_document_t* document)
{
  return NULL;
}

bool
pdf_document_save_as(zathura_document_t* document, const char* path)
{
  return false;
}

zathura_list_t*
pdf_document_attachments_get(zathura_document_t* document)
{
  return NULL;
}

zathura_page_t*
pdf_page_get(zathura_document_t* document, unsigned int page)
{
  return NULL;
}

zathura_list_t*
pdf_page_search_text(zathura_page_t* page, const char* text)
{
  return NULL;
}

zathura_list_t*
pdf_page_links_get(zathura_page_t* page)
{
  return NULL;
}

zathura_list_t*
pdf_page_form_fields_get(zathura_page_t* page)
{
  return NULL;
}

cairo_surface_t*
pdf_page_render(zathura_page_t* page)
{
  return NULL;
}

bool
pdf_page_free(zathura_page_t* page)
{
  return false;
}
