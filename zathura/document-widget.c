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
  unsigned int spacing;
  unsigned int pages_per_row;
  unsigned int first_page_column;
  unsigned int page_count;
  unsigned int start_index;
  bool page_right_to_left;
  bool do_render;
} ZathuraDocumentWidgetPrivate;

G_DEFINE_TYPE_WITH_CODE(ZathuraDocumentWidget, zathura_document_widget, GTK_TYPE_GRID,
                        G_ADD_PRIVATE(ZathuraDocumentWidget))

static void zathura_document_widget_class_init(ZathuraDocumentWidgetClass* GIRARA_UNUSED(class)) {}

static void zathura_document_widget_init(ZathuraDocumentWidget* widget) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);

  priv->spacing            = 0;
  priv->pages_per_row      = 1;
  priv->first_page_column  = 0;
  priv->page_count         = 0;
  priv->start_index        = 0;
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

static void zathura_document_widget_view_range(zathura_t* zathura, unsigned int* start, unsigned int* end) {
  zathura_document_t* document  = zathura_get_document(zathura);
  const unsigned int page_count = zathura_document_get_number_of_pages(document);

  unsigned int internal_start = page_count;
  unsigned int internal_end   = 0;

  for (unsigned int i = 0; i < page_count; i++) {
    if (!page_is_visible(document, i)) {
      continue;
    }

    internal_start = MIN(internal_start, i);
    internal_end   = MAX(internal_end, i);
  }

  *start = internal_start;
  *end   = internal_end;
}

static void zathura_document_widget_get_render_range(zathura_t* zathura, unsigned int* start_index,
                                                     unsigned int* page_count) {
  g_return_if_fail(zathura != NULL && zathura->document != NULL);

  ZathuraDocumentWidget* widget      = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);
  zathura_document_t* document       = zathura_get_document(zathura);

  unsigned int current_page    = zathura_document_get_current_page_number(document);
  unsigned int number_of_pages = zathura_document_get_number_of_pages(document);
  zathura_page_t* page         = zathura_document_get_page(document, current_page);
  double page_height           = zathura_page_get_height(page);

  unsigned int max_pages = MIN(number_of_pages, cairo_max_size * priv->pages_per_row / page_height);
  if (max_pages < current_page) {
    *start_index = current_page - max_pages;
  } else {
    *start_index = 0;
  }
  *page_count = MIN(number_of_pages - *start_index, current_page + max_pages);
}

static void zathura_document_update_grid(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL && zathura->document != NULL);

  ZathuraDocumentWidget* widget      = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);
  zathura_document_t* document       = zathura_get_document(zathura);

  gtk_grid_set_row_spacing(GTK_GRID(widget), priv->spacing);
  gtk_grid_set_column_spacing(GTK_GRID(widget), priv->spacing);

  zathura_document_widget_clear_pages(GTK_WIDGET(widget));

  unsigned int current_page = zathura_document_get_current_page_number(document);

  zathura_document_widget_get_render_range(zathura, &priv->start_index, &priv->page_count);
  girara_debug("updating grid: start %u current %d page count %u", priv->start_index, current_page, priv->page_count);

  for (unsigned int i = 0; i < priv->page_count; i++) {
    unsigned int x = (i + priv->first_page_column - 1) % priv->pages_per_row;
    unsigned int y = (i + priv->first_page_column - 1) / priv->pages_per_row;

    GtkWidget* page_widget = zathura->pages[priv->start_index + i];
    if (priv->page_right_to_left) {
      x = priv->pages_per_row - 1 - x;
    }

    gtk_grid_attach(GTK_GRID(widget), page_widget, x, y, 1, 1);
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

  unsigned int start, end;
  zathura_document_widget_view_range(zathura, &start, &end);
  if (priv->start_index <= start && end < priv->start_index + priv->page_count) {
    return;
  }

  zathura_document_update_grid(zathura);
}

void zathura_document_widget_set_mode(zathura_t* zathura, unsigned int page_padding, unsigned int pages_per_row,
                                      unsigned int first_page_column, bool page_right_to_left) {
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  ZathuraDocumentWidget* widget      = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);

  priv->spacing            = page_padding;
  priv->page_right_to_left = page_right_to_left;
  priv->do_render          = true;

  /* show at least one page */
  if (pages_per_row == 0) {
    pages_per_row = 1;
  }

  priv->pages_per_row = pages_per_row;

  /* ensure: 0 < first_page_column <= pages_per_row */
  if (first_page_column < 1) {
    first_page_column = 1;
  }
  if (first_page_column > pages_per_row) {
    first_page_column = ((first_page_column - 1) % pages_per_row) + 1;
  }

  priv->first_page_column = first_page_column;

  zathura_document_widget_render(zathura);
  gtk_widget_show_all(zathura->ui.document_widget);
}

static void zathura_document_widget_get_size(zathura_t* zathura, unsigned int* height, unsigned int* width) {
  g_return_if_fail(zathura != NULL && zathura->document != NULL);

  ZathuraDocumentWidget* widget      = ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);
  zathura_document_t* document       = zathura_get_document(zathura);

  g_return_if_fail(document != NULL && height != NULL && width != NULL);

  const unsigned int npag = priv->page_count;
  const unsigned int ncol = zathura_document_get_pages_per_row(document);

  if (npag == 0 || ncol == 0) {
    return;
  }

  const unsigned int c0   = zathura_document_get_first_page_column(document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol; /* number of rows */
  const unsigned int pad  = zathura_document_get_page_padding(document);

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

  double start_x, start_y;
  page_number_to_position(document, priv->start_index, 0.0, 0.0, &start_x, &start_y);

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
