/* See LICENSE file for license and copyright information */

#include <stdlib.h>

#include "pdf.h"
#include "../../zathura.h"

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

GtkWidget*
pdf_page_render(zathura_page_t* page)
{
  if (!Zathura.document || !page || !page->data || !page->document) {
    return NULL;
  }

  /* calculate sizes */
  unsigned int page_width  = Zathura.document->scale * page->width;
  unsigned int page_height = Zathura.document->scale * page->height;

  if (Zathura.document->rotate == 90 || Zathura.document->rotate == 270) {
      unsigned int dim_temp = 0;
      dim_temp    = page_width;
      page_width  = page_height;
      page_height = dim_temp;
  }

  /* create pixbuf */
  GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
      page_width, page_height);

  if (!pixbuf) {
    return NULL;
  }

  pdf_document_t* pdf_document = (pdf_document_t*) page->document->data;
  mupdf_page_t* mupdf_page = (mupdf_page_t*) page->data;

  /* render */
  fz_displaylist* list = fz_newdisplaylist();
  fz_device* dev       = fz_newlistdevice(list);

  if (pdf_runpage(pdf_document->document, mupdf_page->page, dev, fz_identity)) {
    return NULL;
  }

  fz_freedevice(dev);

  fz_matrix ctm = fz_translate(0, -mupdf_page->page->mediabox.y1);
  ctm = fz_concat(ctm, fz_scale(Zathura.document->scale, -Zathura.document->scale));

  fz_bbox bbox = fz_roundrect(fz_transformrect(ctm, mupdf_page->page->mediabox));

  guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
  int rowstride  = gdk_pixbuf_get_rowstride(pixbuf);
  int n_channels = gdk_pixbuf_get_n_channels(pixbuf);

  fz_pixmap* pix = fz_newpixmapwithrect(fz_devicergb, bbox);
  fz_clearpixmap(pix, 0xff);

  dev = fz_newdrawdevice(pdf_document->glyphcache, pix);
  fz_executedisplaylist(list, dev, ctm);
  fz_freedevice(dev);

  for (unsigned int y = 0; y < pix->h; y++) {
    for (unsigned int x = 0; x < pix->w; x++) {
      unsigned char *s = pix->samples + y * pix->w * 4 + x * 4;
      guchar* p = pixels + y * rowstride + x * n_channels;
      p[0] = s[0];
      p[1] = s[1];
      p[2] = s[2];
    }
  }

  fz_droppixmap(pix);
  fz_freedisplaylist(list);
  pdf_freepage(mupdf_page->page);
  pdf_agestore(pdf_document->document->store, 3);

  /* write pixbuf */
  GtkWidget* image = gtk_image_new();

  if (!image) {
    g_object_unref(pixbuf);
    return NULL;
  }

  gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
  gtk_widget_show(image);

  return image;
}
