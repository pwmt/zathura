/* See LICENSE file for license and copyright information */

#ifndef PDF_H
#define PDF_H

#include <stdbool.h>
#include <fitz.h>
#include <mupdf.h>

#include "../document.h"

typedef struct pdf_document_s
{
	fz_glyphcache *glyphcache;
	pdf_xref *document;
} pdf_document_t;

typedef struct mupdf_page_s
{
  fz_obj* page_object;
  pdf_page* page;
} mupdf_page_t;

bool pdf_document_open(zathura_document_t* document);
bool pdf_document_free(zathura_document_t* document);
zathura_page_t* pdf_page_get(zathura_document_t* document, unsigned int page);
zathura_list_t* pdf_page_search_text(zathura_page_t* page, const char* text);
zathura_list_t* pdf_page_links_get(zathura_page_t* page);
zathura_list_t* pdf_page_form_fields_get(zathura_page_t* page);
GtkWidget* pdf_page_render(zathura_page_t* page);
bool pdf_page_free(zathura_page_t* page);

#endif // PDF_H
