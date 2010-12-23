/* See LICENSE file for license and copyright information */

#include <stdlib.h>

#include "djvu.h"

bool
djvu_document_open(zathura_document_t* document)
{
  if(!document) {
    return false;
  }

  document->functions.document_free             = djvu_document_free;
  document->functions.document_index_generate   = djvu_document_index_generate;;
  document->functions.document_save_as          = djvu_document_save_as;
  document->functions.document_attachments_get  = djvu_document_attachments_get;
  document->functions.page_get                  = djvu_page_get;
  document->functions.page_search_text          = djvu_page_search_text;
  document->functions.page_links_get            = djvu_page_links_get;
  document->functions.page_form_fields_get      = djvu_page_form_fields_get;
  document->functions.page_render               = djvu_page_render;
  document->functions.page_free                 = djvu_page_free;

  document->data = malloc(sizeof(djvu_document_t));
  if(!document->data) {
    return false;
  }

  djvu_document_t* djvu_document = (djvu_document_t*) document->data;
  djvu_document->context         = ddjvu_context_create("zathura");

  if(!djvu_document->context) {
    free(document->data);
    document->data = NULL;
    return false;
  }

  djvu_document->document = ddjvu_document_create_by_filename(djvu_document->context, document->file_path, FALSE);

  if(!djvu_document->document) {
    ddjvu_context_release(djvu_document->context);
    free(document->data);
    document->data = NULL;
    return false;
  }

  document->number_of_pages = ddjvu_document_get_pagenum(djvu_document->document);

  return true;
}

bool
djvu_document_free(zathura_document_t* document)
{
  if(!document) {
    return false;
  }

  return true;
}

zathura_list_t*
djvu_document_index_generate(zathura_document_t* document)
{
  return NULL;
}

bool
djvu_document_save_as(zathura_document_t* document, const char* path)
{
  if(!document || !document->data || !path) {
    return false;
  }

  return false;
}

zathura_list_t*
djvu_document_attachments_get(zathura_document_t* document)
{
  return NULL;
}

zathura_page_t*
djvu_page_get(zathura_document_t* document, unsigned int page)
{
  if(!document || !document->data) {
    return NULL;
  }

  return NULL;
}

bool
djvu_page_free(zathura_page_t* page)
{
  if(!page) {
    return false;
  }

  return true;
}

zathura_list_t*
djvu_page_search_text(zathura_page_t* page, const char* text)
{
  return NULL;
}

zathura_list_t*
djvu_page_links_get(zathura_page_t* page)
{
  return NULL;
}

zathura_list_t*
djvu_page_form_fields_get(zathura_page_t* page)
{
  return NULL;
}

cairo_surface_t*
djvu_page_render(zathura_page_t* page)
{
  return NULL;
}
