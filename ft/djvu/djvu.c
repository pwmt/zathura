/* See LICENSE file for license and copyright information */

#include <stdlib.h>
#include <cairo.h>

#include "djvu.h"
#include "../../zathura.h"

bool
djvu_document_open(zathura_document_t* document)
{
  if(!document) {
    goto error_out;
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
    goto error_out;
  }

  djvu_document_t* djvu_document = (djvu_document_t*) document->data;
  djvu_document->context  = NULL;
  djvu_document->document = NULL;
  djvu_document->format   = NULL;

  /* setup format */
  djvu_document->format = ddjvu_format_create(DDJVU_FORMAT_RGB24, 0, NULL);

  if(!djvu_document->format) {
    goto error_free;
  }

  ddjvu_format_set_row_order(djvu_document->format, TRUE);

  /* setup context */
  djvu_document->context = ddjvu_context_create("zathura");

  if(!djvu_document->context) {
    goto error_free;
  }

  /* setup document */
  djvu_document->document = ddjvu_document_create_by_filename(djvu_document->context, document->file_path, FALSE);

  if(!djvu_document->document) {
    goto error_free;
  }

  document->number_of_pages = ddjvu_document_get_pagenum(djvu_document->document);

  return true;

error_free:

  if(djvu_document->format) {
    ddjvu_format_release(djvu_document->format);
  }

  if(djvu_document->context) {
    ddjvu_context_release(djvu_document->context);
  }

  free(document->data);
  document->data = NULL;

error_out:

  return false;
}

bool
djvu_document_free(zathura_document_t* document)
{
  if(!document) {
    return false;
  }

  if(document->data) {
    djvu_document_t* djvu_document = (djvu_document_t*) document->data;
    ddjvu_context_release(djvu_document->context);
    ddjvu_document_release(djvu_document->document);
    ddjvu_format_release(djvu_document->format);
    free(document->data);
  }

  return true;
}

girara_tree_node_t*
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

  djvu_document_t* djvu_document = (djvu_document_t*) document->data;

  FILE* fp = fopen(path, "w");
  if(!fp) {
    return false;
  }

  ddjvu_document_save(djvu_document->document, fp, 0, NULL);
  fclose(fp);

  return true;
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

  djvu_document_t* djvu_document  = (djvu_document_t*) document->data;
  zathura_page_t* document_page = malloc(sizeof(zathura_page_t));

  if(!document_page) {
    return NULL;
  }

  document_page->document = document;
  document_page->data     = ddjvu_page_create_by_pageno(djvu_document->document, page);;

  if(!document_page->data) {
    free(document_page);
    return NULL;
  }

  document_page->width  = ddjvu_page_get_width(document_page->data);
  document_page->height = ddjvu_page_get_width(document_page->data);

  return document_page;
}

bool
djvu_page_free(zathura_page_t* page)
{
  if(!page) {
    return false;
  }

  ddjvu_page_release(page->data);
  free(page);

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
  if(Zathura.document || !page || !page->data || !page->document) {
    return NULL;
  }

  /* calculate sizes */
  unsigned int page_width  = page->width;
  unsigned int page_height = page->height;

  /* init ddjvu render data */
  ddjvu_rect_t rrect = { 0, 0, page_width, page_height };
  ddjvu_rect_t prect = { 0, 0, page_width, page_height };

  ddjvu_page_rotation_t rotation = DDJVU_ROTATE_0;

  unsigned int dim_temp = 0;

  switch(Zathura.document->rotate) {
    case 90:
      tmp         = page_width;
      page_width  = page_height;
      page_height = page_tmp;

      rotation = DDJVU_ROTATE_90;
      break;
    case 180:
      rotation = DDJVU_ROTATE_180;
      break;
    case 270:
      tmp         = page_width;
      page_width  = page_height;
      page_height = page_tmp;

      rotation = DDJVU_ROTATE_270;
      break;
  }

  djvu_document_t* djvu_document = (djvu_document_t*) page->document->data;

  /* create cairo data */
  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
      page->width, page->height);

  if(!surface) {
    return NULL;
  }

  int rowsize = cairo_image_surface_get_stride(surface);
  char* data  = (char*) cairo_image_surface_get_data(surface);

  /* render */
  ddjvu_page_set_rotation(page->data, rotation);

  ddjvu_page_render(page->data, DDJVU_RENDER_COLOR, &prect, &rrect,
      djvu_document->format, rowsize, data);

  cairo_surface_mark_dirty(surface);

  return surface;
}
