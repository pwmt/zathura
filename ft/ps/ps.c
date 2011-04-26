/* See LICENSE file for license and copyright information */

#include <stdlib.h>

#include "ps.h"
#include "../../zathura.h"

void
plugin_register(zathura_document_plugin_t* plugin)
{
  plugin->file_extension = "ps";
  plugin->open_function  = ps_document_open;
}

bool
ps_document_open(zathura_document_t* document)
{
  if (!document) {
    goto error_ret;
  }

  document->functions.document_free             = ps_document_free;
  document->functions.page_get                  = ps_page_get;
  document->functions.page_render               = ps_page_render;
  document->functions.page_free                 = ps_page_free;

  document->data = malloc(sizeof(ps_document_t));
  if (!document->data) {
    goto error_ret;
  }

  ps_document_t* ps_document = (ps_document_t*) document->data;
  ps_document->document      = spectre_document_new();

  if (ps_document->document == NULL) {
    goto error_free;
  }

  spectre_document_load(ps_document->document, document->file_path);

  if (spectre_document_status(ps_document->document) != SPECTRE_STATUS_SUCCESS) {
    goto error_free;
  }

  document->number_of_pages = spectre_document_get_n_pages(ps_document->document);

  return true;

error_free:

  if (ps_document->document != NULL) {
    spectre_document_free(ps_document->document);
  }

  free(document->data);
  document->data = NULL;

error_ret:

  return false;
}

bool
ps_document_free(zathura_document_t* document)
{
  if (!document) {
    return false;
  }

  if (document->data != NULL) {
    ps_document_t* ps_document = (ps_document_t*) document->data;
    spectre_document_free(ps_document->document);
    free(document->data);
    document->data = NULL;
  }

  return true;
}

zathura_page_t*
ps_page_get(zathura_document_t* document, unsigned int page)
{
  if (!document || !document->data) {
    return NULL;
  }

  ps_document_t* ps_document    = (ps_document_t*) document->data;
  zathura_page_t* document_page = malloc(sizeof(zathura_page_t));

  if (document_page == NULL) {
    goto error_ret;
  }

  SpectrePage* ps_page = spectre_document_get_page(ps_document->document, page);

  if (ps_page == NULL) {
    goto error_free;
  }

  int page_width;
  int page_height;
  spectre_page_get_size(ps_page, &(page_width), &(page_height));

  document_page->width    = page_width;
  document_page->height   = page_height;
  document_page->document = document;
  document_page->data     = ps_page;

  return document_page;

error_free:

  free(document_page);

error_ret:

  return NULL;
}

bool
ps_page_free(zathura_page_t* page)
{
  if (page == NULL) {
    return false;
  }

  if (page->data != NULL) {
    SpectrePage* ps_page = (SpectrePage*) page->data;
    spectre_page_free(ps_page);
  }

  free(page);

  return true;
}

zathura_image_buffer_t*
ps_page_render(zathura_page_t* page)
{
  if (!page || !page->data || !page->document) {
    return NULL;
  }

  /* calculate sizes */
  unsigned int page_width  = page->document->scale * page->width;
  unsigned int page_height = page->document->scale * page->height;

  /* create image buffer */
  zathura_image_buffer_t* image_buffer = zathura_image_buffer_create(page_width, page_height);

  if (image_buffer == NULL) {
    return NULL;
  }

  SpectrePage* ps_page          = (SpectrePage*) page->data;
  SpectreRenderContext* context = spectre_render_context_new();

  spectre_render_context_set_scale(context, page->document->scale, page->document->scale);
  spectre_render_context_set_rotation(context, 0);

  unsigned char* page_data;
  int row_length;
  spectre_page_render(ps_page, context, &page_data, &row_length);

  for (unsigned int y = 0; y < page_height; y++) {
    for (unsigned int x = 0; x < page_width; x++) {
      unsigned char *s = page_data + y * row_length + x * 4;
      guchar* p = image_buffer->data + y * image_buffer->rowstride + x * 3;
      p[0] = s[0];
      p[1] = s[1];
      p[2] = s[2];
    }
  }

  spectre_render_context_free(context);

  ps_document_t* ps_document = (ps_document_t*) page->document->data;

  return image_buffer;
}
