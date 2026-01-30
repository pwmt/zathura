/* SPDX-License-Identifier: Zlib */

#include "adjustment.h"
#include "document-widget.h"
#include "zathura.h"

#include <math.h>

double page_calc_height_width(zathura_document_t* document, double height, double width, unsigned int* page_height,
                              unsigned int* page_width, bool rotate) {
  g_return_val_if_fail(document != NULL && page_height != NULL && page_width != NULL, 0.0);

  double scale = zathura_document_get_scale(document);

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

unsigned int position_to_page_number(zathura_t* zathura, double pos_x, double pos_y) {
  g_return_val_if_fail(zathura != NULL, 0);

  zathura_document_t* document = zathura_get_document(zathura);
  g_return_val_if_fail(document != NULL, 0);

  ZathuraDocumentWidget* doc_widget = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
 
  unsigned int doc_width, doc_height;
  zathura_document_widget_get_document_size(doc_widget, &doc_height, &doc_width);
 
  unsigned int c0        = zathura_document_get_first_page_column(document);
  unsigned int npag      = zathura_document_get_number_of_pages(document);
  unsigned int ncol      = zathura_document_get_pages_per_row(document);
  unsigned int nrow      = (npag + c0 - 1 + ncol - 1) / ncol;

  // This could be done using binary search if linear is too slow
  unsigned int row = 0;
  for (unsigned int i = 0; i < nrow; i++) {
    unsigned int row_pos, row_height;
    zathura_document_widget_get_row(doc_widget, i, &row_pos, &row_height); 

    if (pos_y * doc_height <= row_pos + row_height) {
      row = i;
      break;
    }
  }

  unsigned int col = 0;
  for (unsigned int i = 0; i < ncol; i++) {
    unsigned int col_pos, col_width;
    zathura_document_widget_get_col(doc_widget, i, &col_pos, &col_width); 

    if (pos_x * doc_width <= col_pos + col_width) {
      col = i;
      break;
    }
  }

  unsigned int page = ncol * (row % nrow) + (col % ncol);
  if (page < c0 - 1) {
    return 0;
  } else {
    return MIN(page - (c0 - 1), npag - 1);
  }
}

void page_number_to_position(zathura_t* zathura, unsigned int page_number, double xalign, double yalign,
                             double* pos_x, double* pos_y) {
  g_return_if_fail(zathura != NULL);

  zathura_document_t* document = zathura_get_document(zathura);
  g_return_if_fail(document != NULL);


  /* sizes of page cell, viewport and document */
  unsigned int cell_height = 0, cell_width = 0;
  zathura_document_widget_get_cell_size(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget), page_number, &cell_height,
                                        &cell_width);

  unsigned int cell_pos_x = 0, cell_pos_y = 0;
  zathura_document_widget_get_cell_pos(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget), page_number, &cell_pos_x,
                                       &cell_pos_y);

  unsigned int view_height = 0, view_width = 0;
  zathura_document_get_viewport_size(document, &view_height, &view_width);

  unsigned int doc_height = 0, doc_width = 0;
  zathura_document_widget_get_document_size(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget), &doc_height,
                                            &doc_width);

  /* compute the shift to align to the viewport. If the page fits to viewport, just center it. */
  double shift_x = 0.5, shift_y = 0.5;
  if (cell_width > view_width) {
    shift_x = 0.5 + (xalign - 0.5) * ((double)cell_width - (double)view_width) / (double)cell_width;
  }

  if (cell_height > view_height) {
    shift_y = 0.5 + (yalign - 0.5) * ((double)cell_height - (double)view_height) / (double)cell_height;
  }

  /* compute the position */
  *pos_x = ((double)cell_pos_x + shift_x * cell_width) / (double) doc_width;
  *pos_y = ((double)cell_pos_y + shift_y * cell_height) / (double) doc_height;
}

bool page_is_visible(zathura_t* zathura, unsigned int page_number) {
  g_return_val_if_fail(zathura != NULL, false);
  zathura_document_t* document = zathura_get_document(zathura);

  g_return_val_if_fail(document != NULL, false);

  /* position at the center of the viewport */
  double pos_x = zathura_document_get_position_x(document);
  double pos_y = zathura_document_get_position_y(document);

  /* get the center of page page_number */
  double page_x, page_y;
  page_number_to_position(zathura, page_number, 0.5, 0.5, &page_x, &page_y);

  unsigned int cell_width, cell_height;
  zathura_document_widget_get_cell_size(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget), page_number, &cell_height,
                                        &cell_width);

  unsigned int doc_width, doc_height;
  zathura_document_widget_get_document_size(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget), &doc_height,
                                            &doc_width);

  unsigned int view_width, view_height;
  zathura_document_get_viewport_size(document, &view_height, &view_width);

  return (fabs(pos_x - page_x) < 0.5 * (double)(view_width + cell_width) / (double)doc_width &&
          fabs(pos_y - page_y) < 0.5 * (double)(view_height + cell_height) / (double)doc_height);
}

gdouble zathura_adjustment_get_ratio(GtkAdjustment* adjustment) {
  gdouble lower     = gtk_adjustment_get_lower(adjustment);
  gdouble upper     = gtk_adjustment_get_upper(adjustment);
  gdouble page_size = gtk_adjustment_get_page_size(adjustment);
  gdouble value     = gtk_adjustment_get_value(adjustment);

  return (value - lower + page_size / 2.0) / (upper - lower);
}

void zathura_adjustment_set_value(GtkAdjustment* adjustment, gdouble value) {
  const gdouble lower        = gtk_adjustment_get_lower(adjustment);
  const gdouble upper_m_size = gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment);

  gtk_adjustment_set_value(adjustment, MAX(lower, MIN(upper_m_size, value)));
}

void zathura_adjustment_set_value_from_ratio(GtkAdjustment* adjustment, gdouble ratio) {
  if (ratio == 0.0) {
    return;
  }

  gdouble lower     = gtk_adjustment_get_lower(adjustment);
  gdouble upper     = gtk_adjustment_get_upper(adjustment);
  gdouble page_size = gtk_adjustment_get_page_size(adjustment);

  gdouble value = (upper - lower) * ratio + lower - page_size / 2.0;

  zathura_adjustment_set_value(adjustment, value);
}
