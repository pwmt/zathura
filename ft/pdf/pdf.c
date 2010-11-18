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

  document->data = malloc(sizeof(pdf_document_t));
  if(!document->data) {
    return false;
  }

  /* format path */
  GError* error  = NULL;
  char* file_uri = g_filename_to_uri(document->file_path, NULL, &error);

  if(!file_uri) {
    fprintf(stderr, "error: could not open file: %s\n", error->message);
    g_error_free(error);
    free(document->data);
    return false;
  }

  pdf_document_t* pdf_document = (pdf_document_t*) document->data;
  pdf_document->document       = poppler_document_new_from_file(file_uri, document->password, &error);

  if(!pdf_document->document) {
    fprintf(stderr, "error: could not open file: %s\n", error->message);
    g_error_free(error);
    free(document->data);
    return false;
  }

  document->number_of_pages = poppler_document_get_n_pages(pdf_document->document);

  return true;
}

bool
pdf_document_free(zathura_document_t* document)
{
  if(!document) {
    return false;
  }

  if(document->data) {
    pdf_document_t* pdf_document = (pdf_document_t*) document->data;
    g_object_unref(pdf_document->document);
    free(document->data);
  }

  return true;
}

zathura_list_t*
pdf_document_index_generate(zathura_document_t* document)
{
  return NULL;
}

bool
pdf_document_save_as(zathura_document_t* document, const char* path)
{
  if(!document || !document->data || !path) {
    return false;
  }

  pdf_document_t* pdf_document = (pdf_document_t*) document->data;

  char* file_path = g_strdup_printf("file://%s", path);
  poppler_document_save(pdf_document->document, file_path, NULL);
  g_free(file_path);

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
  if(!document || !document->data) {
    return NULL;
  }

  pdf_document_t* pdf_document  = (pdf_document_t*) document->data;
  zathura_page_t* document_page = malloc(sizeof(zathura_page_t));

  if(!document_page) {
    return NULL;
  }

  document_page->document = document;
  document_page->data     = poppler_document_get_page(pdf_document->document, page);

  if(!document_page->data) {
    free(document_page);
    return NULL;
  }

  poppler_page_get_size(document_page->data, &(document_page->width), &(document_page->height));

  return document_page;
}

bool
pdf_page_free(zathura_page_t* page)
{
  if(!page) {
    return false;
  }

  g_object_unref(page->data);
  free(page);

  return true;
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
