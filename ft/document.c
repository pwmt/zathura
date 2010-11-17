/* See LICENSE file for license and copyright information */

#include <stdlib.h>
#include <stdio.h>

#include "document.h"

zathura_document_t*
zathura_document_create(const char* path)
{
  return NULL;
}

bool
zathura_document_free(zathura_document_t* document)
{
  if(!document) {
    return NULL;
  }

  if(!document->functions.document_free(document)) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_free(document);
}

bool
zathura_document_save_as(zathura_document_t* document, const char* path)
{
  if(!document || !path) {
    return false;
  }

  if(!document->functions.document_save_as) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return false;
  }

  return document->functions.document_save_as(document, path);
}

zathura_list_t*
zathura_document_index_generate(zathura_document_t* document)
{
  if(!document) {
    return NULL;
  }

  if(!document->functions.document_index_generate) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_index_generate(document);
}

bool
zathura_document_index_free(zathura_list_t* list)
{
  return false;
}

zathura_list_t*
zathura_document_attachments_get(zathura_document_t* document)
{
  if(!document) {
    return NULL;
  }

  if(!document->functions.document_attachments_get) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_attachments_get(document);
}

bool
zathura_document_attachments_free(zathura_list_t* list)
{
  return false;
}

zathura_page_t*
zathura_page_get(zathura_document_t* document, unsigned int page)
{
  if(!document) {
    return NULL;
  }

  if(!document->functions.page_get) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return document->functions.page_get(document, page);
}

zathura_list_t*
zathura_page_search_text(zathura_page_t* page, const char* text)
{
  if(!page || !page->document || !text) {
    return NULL;
  }

  if(!page->document->functions.page_search_text) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_search_text(page, text);
}

zathura_list_t*
zathura_page_links_get(zathura_page_t* page)
{
  if(!page || !page->document) {
    return NULL;
  }

  if(!page->document->functions.page_links_get) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_links_get(page);
}

bool
zathura_page_links_free(zathura_list_t* list)
{
  return false;
}

zathura_list_t*
zathura_page_form_fields_get(zathura_page_t* page)
{
  if(!page || !page->document) {
    return NULL;
  }

  if(!page->document->functions.page_form_fields_get) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_form_fields_get(page);
}

bool
zathura_page_form_fields_free(zathura_list_t* list)
{
  return false;
}

cairo_surface_t*
zathura_page_render(zathura_page_t* page)
{
  if(!page || !page->document) {
    return NULL;
  }

  if(!page->document->functions.page_render) {
    fprintf(stderr, "error: %s not implemented\n", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_render(page);
}
