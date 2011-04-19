/* See LICENSE file for license and copyright information */

#include <stdlib.h>

#include "pdf.h"
#include "../../zathura.h"

void
plugin_register(zathura_document_plugin_t* plugin)
{
  plugin->file_extension = "pdf";
  plugin->open_function  = pdf_document_open;
}

bool
pdf_document_open(zathura_document_t* document)
{
  if (!document) {
    goto error_ret;
  }

  document->functions.document_free             = pdf_document_free;
  document->functions.page_get                  = pdf_page_get;
  document->functions.page_search_text          = pdf_page_search_text;
  document->functions.page_links_get            = pdf_page_links_get;
  document->functions.page_form_fields_get      = pdf_page_form_fields_get;
  document->functions.page_render               = pdf_page_render;
  document->functions.page_free                 = pdf_page_free;

  document->data = malloc(sizeof(pdf_document_t));
  if (!document->data) {
    goto error_ret;
  }

  pdf_document_t* pdf_document = (pdf_document_t*) document->data;

  fz_accelerate();
  pdf_document->glyphcache = fz_newglyphcache();

  if (pdf_openxref(&(pdf_document->document), document->file_path, NULL)) {
    fprintf(stderr, "error: could not open file\n");
    goto error_free;
  }

  if (pdf_loadpagetree(pdf_document->document)) {
    fprintf(stderr, "error: could not open file\n");
    goto error_free;
  }

  document->number_of_pages = pdf_getpagecount(pdf_document->document);

  return true;

error_free:

  if (pdf_document->document) {
    pdf_freexref(pdf_document->document);
  }

  if (pdf_document->glyphcache) {
    fz_freeglyphcache(pdf_document->glyphcache);
  }

  free(document->data);
  document->data = NULL;

error_ret:

  return false;
}

bool
pdf_document_free(zathura_document_t* document)
{
  if (!document) {
    return false;
  }

  if (document->data) {
    pdf_document_t* pdf_document = (pdf_document_t*) document->data;
    pdf_freexref(pdf_document->document);
    fz_freeglyphcache(pdf_document->glyphcache);
    free(document->data);
    document->data = NULL;
  }

  return true;
}

zathura_page_t*
pdf_page_get(zathura_document_t* document, unsigned int page)
{
  if (!document || !document->data) {
    return NULL;
  }

  pdf_document_t* pdf_document  = (pdf_document_t*) document->data;
  zathura_page_t* document_page = malloc(sizeof(zathura_page_t));

  if (document_page == NULL) {
    goto error_ret;
  }

  mupdf_page_t* mupdf_page = malloc(sizeof(mupdf_page_t));

  if (mupdf_page == NULL) {
    goto error_free;
  }

  document_page->document = document;
  document_page->data     = mupdf_page;

  mupdf_page->page_object = pdf_getpageobject(pdf_document->document, page + 1);

  if (pdf_loadpage(&(mupdf_page->page), pdf_document->document, mupdf_page->page_object)) {
    goto error_free;
  }

  document_page->width  = mupdf_page->page->mediabox.x1 - mupdf_page->page->mediabox.x0;
  document_page->height = mupdf_page->page->mediabox.y1 - mupdf_page->page->mediabox.y0;

  return document_page;

error_free:

  if (mupdf_page && mupdf_page->page_object) {
    fz_dropobj(mupdf_page->page_object);
  }

  if (mupdf_page && mupdf_page->page) {
    pdf_freepage(mupdf_page->page);
  }

  free(document_page);
  free(mupdf_page);

error_ret:

  return NULL;
}

bool
pdf_page_free(zathura_page_t* page)
{
  if (!page) {
    return false;
  }

  mupdf_page_t* mupdf_page = (mupdf_page_t*) page->data;
  pdf_freepage(mupdf_page->page);
  fz_dropobj(mupdf_page->page_object);
  free(mupdf_page);
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

zathura_image_buffer_t*
pdf_page_render(zathura_page_t* page)
{
  if (!page || !page->data || !page->document) {
    return NULL;
  }

  /* calculate sizes */
  unsigned int page_width  = page->document->scale * page->width;
  unsigned int page_height = page->document->scale * page->height;

  if (page->document->rotate == 90 || page->document->rotate == 270) {
      unsigned int dim_temp = 0;
      dim_temp    = page_width;
      page_width  = page_height;
      page_height = dim_temp;
  }

  /* create image buffer */
  zathura_image_buffer_t* image_buffer = zathura_image_buffer_create(page_width, page_height);

  if (image_buffer == NULL) {
    return NULL;
  }

  pdf_document_t* pdf_document = (pdf_document_t*) page->document->data;
  mupdf_page_t* mupdf_page     = (mupdf_page_t*) page->data;

  /* render */
  fz_displaylist* display_list = fz_newdisplaylist();
  fz_device* device            = fz_newlistdevice(display_list);

  if (pdf_runpage(pdf_document->document, mupdf_page->page, device, fz_identity)) {
    return NULL;
  }

  fz_freedevice(device);

  pdf_agestore(pdf_document->document->store, 3);

  fz_matrix ctm = fz_identity;
  ctm           = fz_concat(ctm, fz_translate(0, -mupdf_page->page->mediabox.y1));
  ctm           = fz_concat(ctm, fz_scale(page->document->scale, -page->document->scale));
  ctm           = fz_concat(ctm, fz_rotate(mupdf_page->page->rotate));
  ctm           = fz_concat(ctm, fz_rotate(page->document->rotate));
  fz_bbox bbox  = fz_roundrect(fz_transformrect(ctm, mupdf_page->page->mediabox));

  fz_pixmap* pixmap = fz_newpixmapwithrect(fz_devicergb, bbox);
  fz_clearpixmapwithcolor(pixmap, 0xFF);

  device = fz_newdrawdevice(pdf_document->glyphcache, pixmap);
  fz_executedisplaylist(display_list, device, ctm);
  fz_freedevice(device);

  for (unsigned int y = 0; y < pixmap->h; y++) {
    for (unsigned int x = 0; x < pixmap->w; x++) {
      unsigned char *s = pixmap->samples + y * pixmap->w * 4 + x * 4;
      guchar* p = image_buffer->data + y * image_buffer->rowstride + x * 3;
      p[0] = s[0];
      p[1] = s[1];
      p[2] = s[2];
    }
  }

  fz_droppixmap(pixmap);
  fz_freedisplaylist(display_list);

  return image_buffer;
}
