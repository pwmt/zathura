/* SPDX-License-Identifier: Zlib */

#include <girara/log.h>
#include <stdbool.h>

#include "document-widget.h"

#include "adjustment.h"
#include "document.h"
#include "page.h"
#include "types.h"
#include "zathura.h"

static const unsigned int cairo_max_size = INT16_MAX;

typedef struct zathura_document_widget_private_s {
  unsigned int v_spacing;
  unsigned int h_spacing;
  unsigned int row_count;
  unsigned int start_row;
  bool page_right_to_left;
  bool do_render;
} ZathuraDocumentWidgetPrivate;

G_DEFINE_TYPE_WITH_CODE(ZathuraDocumentWidget, zathura_document_widget, GTK_TYPE_GRID,
                        G_ADD_PRIVATE(ZathuraDocumentWidget))

static void zathura_document_widget_class_init(ZathuraDocumentWidgetClass* GIRARA_UNUSED(class)) {}

static void zathura_document_widget_init(ZathuraDocumentWidget* widget) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);

  priv->v_spacing          = 0;
  priv->h_spacing          = 0;
  priv->row_count          = 0;
  priv->start_row          = 0;
  priv->page_right_to_left = false;
  priv->do_render          = true;
}

GtkWidget* zathura_document_widget_new(void) {
  GObject* ret = g_object_new(ZATHURA_TYPE_DOCUMENT_WIDGET, NULL);
  if (ret == NULL) {
    return NULL;
  }

  ZathuraDocumentWidget* widget = ZATHURA_DOCUMENT_WIDGET(ret);
  GtkWidget* gtk_widget         = GTK_WIDGET(widget);

  gtk_grid_set_row_homogeneous(GTK_GRID(widget), TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(widget), TRUE);

  gtk_widget_set_halign(gtk_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(gtk_widget, GTK_ALIGN_CENTER);

  gtk_widget_set_hexpand_set(gtk_widget, TRUE);
  gtk_widget_set_hexpand(gtk_widget, FALSE);
  gtk_widget_set_vexpand_set(gtk_widget, TRUE);
  gtk_widget_set_vexpand(gtk_widget, FALSE);

  return gtk_widget;
}

static void remove_page_from_table(GtkWidget* page, gpointer UNUSED(permanent)) {
  gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(page)), page);
}

void zathura_document_widget_clear_pages(GtkWidget* widget) {
  gtk_container_foreach(GTK_CONTAINER(widget), remove_page_from_table, NULL);
}

static void zathura_document_widget_view_range(zathura_t* zathura, unsigned int* start_row, unsigned int* end_row) {
  zathura_document_t* document  = zathura_get_document(zathura);

  const unsigned int c0   = zathura_document_get_first_page_column(document);
  const unsigned int ncol = zathura_document_get_pages_per_row(document);
  const unsigned int npag = zathura_document_get_number_of_pages(document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol; /* number of rows */

  unsigned int start = nrow;
  unsigned int end   = 0;

  for (unsigned int i = 0; i < nrow; i++) {
    if (!page_is_visible(document, i * ncol)) {
      continue;
    }

    start = MIN(start, i);
    end   = MAX(end, i);
  }

  *start_row = start;
  *end_row   = end;
}

static void zathura_document_render_range(zathura_t* zathura, unsigned int* start_row, unsigned int* row_count) {
  g_return_if_fail(zathura != NULL && zathura->document != NULL);
  zathura_document_t* document       = zathura_get_document(zathura);

  const double pos_x = zathura_document_get_position_x(document);
  const double pos_y = zathura_document_get_position_y(document);

  const unsigned int npag = zathura_document_get_number_of_pages(document);
  const unsigned int ncol = zathura_document_get_pages_per_row(document);

  if (npag == 0 || ncol == 0) {
    return;
  }

  const unsigned int c0   = zathura_document_get_first_page_column(document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol; /* number of rows */

  unsigned int current_page    = position_to_page_number(document, pos_x, pos_y);
  zathura_page_t* page         = zathura_document_get_page(document, current_page);
  double page_height           = zathura_page_get_height(page);

  unsigned int current_row = (current_page + c0 - 1 + ncol - 1) / ncol;
  *row_count = MIN(nrow, cairo_max_size / page_height);

  if (nrow < current_row + *row_count / 2) {
    // current_page is near the end of the document
    *start_row = nrow - *row_count;
  } else if (current_row < *row_count / 2) {
    // current_page is near the start of the document
    *start_row = 0;
  } else {
    *start_row = current_row - *row_count / 2;
  }
}

static unsigned int zathura_document_widget_row_first_index(zathura_t* zathura, unsigned int row) {
  g_return_val_if_fail(zathura != NULL && zathura->document != NULL, 0);

  ZathuraDocumentWidget* widget      = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);
  zathura_document_t* document       = zathura_get_document(zathura);

  const unsigned int ncol = zathura_document_get_pages_per_row(document);
  const unsigned int c0   = zathura_document_get_first_page_column(document);

  unsigned int page_index = row == 0 ? 0 : priv->start_row * ncol - (c0 - 1);
  return page_index;
}

static void zathura_document_update_grid(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL && zathura->document != NULL);

  ZathuraDocumentWidget* widget      = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);
  zathura_document_t* document       = zathura_get_document(zathura);

  gtk_grid_set_row_spacing(GTK_GRID(widget), priv->v_spacing);
  gtk_grid_set_column_spacing(GTK_GRID(widget), priv->h_spacing);

  zathura_document_widget_clear_pages(GTK_WIDGET(widget));

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(document);
  const unsigned int current_page = zathura_document_get_current_page_number(document);
  const unsigned int c0   = zathura_document_get_first_page_column(document);
  const unsigned int ncol = zathura_document_get_pages_per_row(document);

  zathura_document_render_range(zathura, &priv->start_row, &priv->row_count);
  girara_debug("updating grid: start %u current %d row count %u", priv->start_row, current_page, priv->row_count);

  unsigned int page_index = zathura_document_widget_row_first_index(zathura, priv->start_row);
  girara_debug("start page index: %u", page_index);

  for (unsigned int i = 0; i < priv->row_count; i++) {
    unsigned int row = priv->start_row + i;
    unsigned int start_col = row == 0 ? c0 - 1 : 0;

    for (unsigned int col = start_col; col < ncol && page_index < number_of_pages; col++) {
      unsigned int x = col;
      unsigned int y = row;

      GtkWidget* page_widget = zathura->pages[page_index];
      if (priv->page_right_to_left) {
        x = ncol - 1 - x;
      }

      gtk_grid_attach(GTK_GRID(widget), page_widget, x, y, 1, 1);
      page_index++;
    }
  }

  priv->do_render = false;
}

void zathura_document_widget_render(zathura_t* zathura) {
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  ZathuraDocumentWidget* widget      = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);

  if (priv->do_render) {
    zathura_document_update_grid(zathura);
    return;
  }

  unsigned int start_row, end_row;
  zathura_document_widget_view_range(zathura, &start_row, &end_row);
  if (priv->start_row <= start_row && end_row < priv->start_row + priv->row_count) {
    return;
  }

  zathura_document_update_grid(zathura);
}

void zathura_document_widget_set_mode(zathura_t* zathura, unsigned int page_v_padding, unsigned int page_h_padding, 
                                      bool page_right_to_left) {
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  ZathuraDocumentWidget* widget      = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);

  priv->v_spacing          = page_v_padding;
  priv->h_spacing          = page_h_padding;
  priv->page_right_to_left = page_right_to_left;
  priv->do_render          = true;

  zathura_document_widget_render(zathura);
  gtk_widget_show_all(zathura->ui.document_widget);
}

static void zathura_document_widget_get_size(zathura_t* zathura, unsigned int* height, unsigned int* width) {
  g_return_if_fail(zathura != NULL && zathura->document != NULL);

  ZathuraDocumentWidget* widget      = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);
  zathura_document_t* document       = zathura_get_document(zathura);

  g_return_if_fail(document != NULL && height != NULL && width != NULL);

  const unsigned int nrow = priv->row_count;
  const unsigned int ncol = zathura_document_get_pages_per_row(document);
  const unsigned int pad  = zathura_document_get_page_padding(document);

  if (nrow == 0 || ncol == 0) {
    return;
  }

  unsigned int cell_height = 0;
  unsigned int cell_width  = 0;
  zathura_document_get_cell_size(document, &cell_height, &cell_width);

  *width  = ncol * cell_width + (ncol - 1) * pad;
  *height = nrow * cell_height + (nrow - 1) * pad;
}

static void zathura_document_widget_get_offset(zathura_t* zathura, double* pos_x, double* pos_y) {
  g_return_if_fail(zathura != NULL && zathura->document != NULL);

  ZathuraDocumentWidget* widget      = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);
  zathura_document_t* document       = zathura_get_document(zathura);

  double zero_x, zero_y;
  page_number_to_position(document, 0, 0.0, 0.0, &zero_x, &zero_y);

  unsigned int start_index = zathura_document_widget_row_first_index(zathura, priv->start_row);

  double start_x, start_y;
  page_number_to_position(document, start_index, 0.0, 0.0, &start_x, &start_y);

  *pos_x = start_x - zero_x;
  *pos_y = start_y - zero_y;
}

static gdouble zathura_adjustment_get_ratio(GtkAdjustment* adjustment) {
  gdouble lower     = gtk_adjustment_get_lower(adjustment);
  gdouble upper     = gtk_adjustment_get_upper(adjustment);
  gdouble page_size = gtk_adjustment_get_page_size(adjustment);
  gdouble value     = gtk_adjustment_get_value(adjustment);

  return (value - lower + page_size / 2.0) / (upper - lower);
}

gdouble zathura_document_widget_get_ratio(zathura_t* zathura, GtkAdjustment* adjustment, bool width) {
  if (zathura == NULL || zathura->document == NULL) {
    return 0.0;
  }

  zathura_document_t* document = zathura_get_document(zathura);

  unsigned int doc_height, doc_width;
  zathura_document_get_document_size(document, &doc_height, &doc_width);

  unsigned int widget_height, widget_width;
  zathura_document_widget_get_size(zathura, &widget_height, &widget_width);

  gdouble ratio = zathura_adjustment_get_ratio(adjustment);

  gdouble start_x, start_y;
  zathura_document_widget_get_offset(zathura, &start_x, &start_y);

  /* convert the ratio in the document widget coordinates to the document coordinates */
  gdouble ratio_x = (ratio * widget_width) / doc_width + start_x;
  gdouble ratio_y = (ratio * widget_height) / doc_height + start_y;

  return width ? ratio_x : ratio_y;
}

static void zathura_adjustment_set_value_from_ratio(GtkAdjustment* adjustment, gdouble ratio) {
  if (ratio == 0.0) {
    return;
  }

  gdouble lower     = gtk_adjustment_get_lower(adjustment);
  gdouble upper     = gtk_adjustment_get_upper(adjustment);
  gdouble page_size = gtk_adjustment_get_page_size(adjustment);

  gdouble value = (upper - lower) * ratio + lower - page_size / 2.0;

  zathura_adjustment_set_value(adjustment, value);
}

void zathura_document_widget_set_value_from_ratio(zathura_t* zathura, GtkAdjustment* adjustment, double ratio,
                                                  bool width) {
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  zathura_document_t* document = zathura_get_document(zathura);
  zathura_document_widget_render(zathura);

  unsigned int doc_height, doc_width;
  zathura_document_get_document_size(document, &doc_height, &doc_width);

  unsigned int widget_height, widget_width;
  zathura_document_widget_get_size(zathura, &widget_height, &widget_width);

  gdouble start_x, start_y;
  zathura_document_widget_get_offset(zathura, &start_x, &start_y);

  /* convert the ratio in the document coordinates to the document widget coordinates */
  double ratio_x = ((ratio - start_x) * doc_width) / widget_width;
  double ratio_y = ((ratio - start_y) * doc_height) / widget_height;

  zathura_adjustment_set_value_from_ratio(adjustment, width ? ratio_x : ratio_y);
}
