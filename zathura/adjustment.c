/* SPDX-License-Identifier: Zlib */

#include "adjustment.h"
#include "utils.h"

#include <math.h>

double page_calc_height_width(zathura_document_t* document, double height, double width, unsigned int* page_height,
                              unsigned int* page_width, bool rotate) {
  g_return_val_if_fail(document != NULL && page_height != NULL && page_width != NULL, 0.0);

  double scale = zathura_document_get_scale(document);

  // TODO this just set all pages to the maximum.
  // needs to adjust cell size based on the page size itself.
  if (rotate == true && zathura_document_get_rotation(document) % 180 != 0) {
    *page_width  = round(height * scale);
    *page_height = round(width * scale);
    scale        = MAX(*page_width / height, *page_height / width);
  } else {
    *page_width  = round(width * scale);
    *page_height = round(height * scale);
    scale        = MAX(*page_width / width, *page_height / height);
  }

  return scale;
}

double individual_page_calc_height_width(zathura_document_t* document, zathura_page_t* page, unsigned int* page_height,
                              unsigned int* page_width, bool rotate) {
  g_return_val_if_fail(document != NULL && page_height != NULL && page_width != NULL, 0.0);

  double scale = zathura_document_get_scale(document);

  // TODO this just set all pages to the maximum.
  // needs to adjust cell size based on the page size itself.
  double height = zathura_page_get_height(page);
  double width  = zathura_page_get_width(page);
  if (rotate == true && (zathura_page_get_rotation(page) + zathura_document_get_rotation(document)) % 180 != 0) {
    *page_width  = round(height * scale);
    *page_height = round(width * scale);
    scale        = MAX(*page_width / height, *page_height / width);
  } else {
    *page_width  = round(width * scale);
    *page_height = round(height * scale);
    scale        = MAX(*page_width / width, *page_height / height);
  }

  return scale;
}

void page_calc_position(zathura_document_t* document, double x, double y, double* xn, double* yn) {
  g_return_if_fail(document != NULL && xn != NULL && yn != NULL);

  const unsigned int rot = zathura_document_get_rotation(document);
  if (rot == 90) {
    *xn = 1 - y;
    *yn = x;
  } else if (rot == 180) {
    *xn = 1 - x;
    *yn = 1 - y;
  } else if (rot == 270) {
    *xn = y;
    *yn = 1 - x;
  } else {
    *xn = x;
    *yn = y;
  }
}

unsigned int position_to_page_number(zathura_document_t* document, double pos_x, double pos_y) {
  g_return_val_if_fail(document != NULL, 0);

  unsigned int doc_width, doc_height;
  zathura_document_get_document_size(document, &doc_height, &doc_width);

  unsigned int cell_width, cell_height;
  zathura_document_get_cell_size(document, &cell_height, &cell_width);

  unsigned int c0        = zathura_document_get_first_page_column(document);
  unsigned int npag      = zathura_document_get_number_of_pages(document);
  unsigned int ncol      = zathura_document_get_pages_per_row(document);
  unsigned int nrow      = 0;
  unsigned int v_padding = zathura_document_get_page_v_padding(document);
  unsigned int h_padding = zathura_document_get_page_h_padding(document);

  if (c0 == 1) {
    /* There is no offset, so this is easy. */
    nrow = (npag + ncol - 1) / ncol;
  } else {
    /* If there is a offset, we handle the first row extra. */
    nrow = 1 + (npag - (ncol - c0 - 1) + (ncol - 1)) / ncol;
  }

  unsigned int col = floor(pos_x * (double)doc_width / (double)(cell_width + h_padding));
  unsigned int row = floor(pos_y * (double)doc_height / (double)(cell_height + v_padding));

  unsigned int page = ncol * (row % nrow) + (col % ncol);
  if (page < c0 - 1) {
    return 0;
  } else {
    return MIN(page - (c0 - 1), npag - 1);
  }
}

void page_number_to_position(zathura_document_t* document, unsigned int page_number, double xalign, double yalign,
                             double* pos_x, double* pos_y) {
  g_return_if_fail(document != NULL);

  unsigned int c0   = zathura_document_get_first_page_column(document);
  unsigned int ncol = zathura_document_get_pages_per_row(document);

  /* row and column for page_number indexed from 0 */
  unsigned int row = (page_number + c0 - 1) / ncol;
  unsigned int col = (page_number + c0 - 1) % ncol;

  /* sizes of page cell, viewport and document */
  unsigned int cell_height = 0, cell_width = 0;
  zathura_document_get_cell_size(document, &cell_height, &cell_width);

  unsigned int view_height = 0, view_width = 0;
  zathura_document_get_viewport_size(document, &view_height, &view_width);

  unsigned int doc_height = 0, doc_width = 0;
  zathura_document_get_document_size(document, &doc_height, &doc_width);

  /* compute the shift to align to the viewport. If the page fits to viewport, just center it. */
  double shift_x = 0.5, shift_y = 0.5;
  if (cell_width > view_width) {
    shift_x = 0.5 + (xalign - 0.5) * ((double)cell_width - (double)view_width) / (double)cell_width;
  }

  if (cell_height > view_height) {
    shift_y = 0.5 + (yalign - 0.5) * ((double)cell_height - (double)view_height) / (double)cell_height;
  }

  const unsigned int v_padding = zathura_document_get_page_v_padding(document);
  const unsigned int h_padding = zathura_document_get_page_h_padding(document);

  /* compute the position */
  *pos_x = ((double)col * (cell_width + h_padding) + shift_x * cell_width) / (double) doc_width;
  *pos_y = ((double)row * (cell_height + v_padding) + shift_y * cell_height) / (double) doc_height;
}

bool page_is_visible(zathura_document_t* document, unsigned int page_number) {
  g_return_val_if_fail(document != NULL, false);

  /* position at the center of the viewport */
  double pos_x = zathura_document_get_position_x(document);
  double pos_y = zathura_document_get_position_y(document);

  /* get the center of page page_number */
  double page_x, page_y;
  page_number_to_position(document, page_number, 0.5, 0.5, &page_x, &page_y);

  unsigned int cell_width, cell_height;
  zathura_document_get_cell_size(document, &cell_height, &cell_width);

  unsigned int doc_width, doc_height;
  zathura_document_get_document_size(document, &doc_height, &doc_width);

  unsigned int view_width, view_height;
  zathura_document_get_viewport_size(document, &view_height, &view_width);

  return (fabs(pos_x - page_x) < 0.5 * (double)(view_width + cell_width) / (double)doc_width &&
          fabs(pos_y - page_y) < 0.5 * (double)(view_height + cell_height) / (double)doc_height);
}
