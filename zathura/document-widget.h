/* SPDX-License-Identifier: Zlib */

#ifndef DOCUMENT_WIDGET_H
#define DOCUMENT_WIDGET_H

#include <gtk/gtk.h>
#include "types.h"

typedef enum document_widget_mode_e {
  DOCUMENT_WIDGET_GRID,
  DOCUMENT_WIDGET_SINGLE,
  DOCUMENT_WIDGET_MODE_COUNT,
} document_widget_mode_t;

/**
 * The document view widget.
 */
struct zathura_document_widget_s {
  GtkContainer parent;
};

struct zathura_document_widget_class_s {
  GtkContainerClass parent_class;
};

#define ZATHURA_TYPE_DOCUMENT (zathura_document_widget_get_type())
#define ZATHURA_DOCUMENT_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_DOCUMENT, ZathuraDocumentWidget))
#define ZATHURA_DOCUMENT_WIDGET_CLASS(obj)                                                                             \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_DOCUMENT, ZathuraDocumentWidgetClass))
#define ZATHURA_IS_DOCUMENT_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_DOCUMENT))
#define ZATHURA_IS_DOCUMENT_WIDGET_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_DOCUMENT))
#define ZATHURA_DOCUMENT_WIDGET_GET_CLASS(obj)                                                                         \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_DOCUMENT, ZathuraDocumentWidgetClass))

/**
 * Returns the type of the document view widget.
 *
 * @return the type
 */
GType zathura_document_widget_get_type(void) G_GNUC_CONST;

/**
 * Create a document view widget.
 *
 * @param zathura the zathura instance
 * @return a document view widget
 */
GtkWidget* zathura_document_widget_new(zathura_t* zathura);

/**
 * Update internal layout structures when pages-per-row,
 * first page column or document changes.
 *
 * @param document ZathuraDocumentWidget
 */
void zathura_document_widget_refresh_layout(ZathuraDocumentWidget* document);

/**
 * Calculate the position of each grid cell.
 * Required when any page size is changed.
 *
 * @param document ZathuraDocumentWidget
 */
void zathura_document_widget_compute_layout(ZathuraDocumentWidget* document);

/**
 * Return the position of a cell from the document's layout table in pixels.
 * It takes the current scale into account.
 * Valid after a call to zathura_document_widget_compute_layout.
 *
 * @param document   ZathuraDocumentWidget
 * @param page_index index of the page
 * @return pos_x     pixel offset in the x direction
 * @return pos_y     pixel offset in the y direction
 */
void zathura_document_widget_get_cell_pos(ZathuraDocumentWidget* document, unsigned int page_index, unsigned int* pos_x,
                                          unsigned int* pos_y);

/**
 * Return the size of a cell from the document's layout table in pixels.
 * It takes the current scale into account.
 * Valid after a call to zathura_document_widget_compute_layout.
 *
 * @param document   ZathuraDocumentWidget
 * @param page_index index of the page
 * @return height    cell height
 * @return width     cell width
 */
void zathura_document_widget_get_cell_size(ZathuraDocumentWidget* document, unsigned int page_index,
                                           unsigned int* height, unsigned int* width);

/**
 * The position and size of a row in the document widget.
 * Valid after a call to zathura_document_widget_compute_layout.
 *
 * @param document   ZathuraDocumentWidget
 * @param row        row number, indexed from 0.
 * @return pos       pixel offset
 * @return size      row size
 */
void zathura_document_widget_get_row(ZathuraDocumentWidget* document, unsigned int row, unsigned int* pos,
                                     unsigned int* size);

/**
 * The position and size of a column in the document widget.
 * Valid after a call to zathura_document_widget_compute_layout.
 *
 * @param document   ZathuraDocumentWidget
 * @param col        column number, indexed from 0.
 * @return pos       pixel offset
 * @return size      col size
 */
void zathura_document_widget_get_col(ZathuraDocumentWidget* document, unsigned int col, unsigned int* pos,
                                     unsigned int* size);

/**
 * Get the size of the entire document to be displayed in pixels.
 * Takes into account the scale, layout of the pages, and padding
 * between them. Valid after a call to zathura_document_widget_compute_layout.
 *
 * @param document ZathuraDocumentWidget
 * @return height  document height in pixels
 * @return width   document width in pixels
 */
void zathura_document_widget_get_document_size(ZathuraDocumentWidget* document, unsigned int* height,
                                               unsigned int* width);

/**
 * Remove page widgets from document.
 *
 * @param document ZathuraDocumentWidget
 */
void zathura_document_widget_clear_pages(ZathuraDocumentWidget* document);

#endif // DOCUMENT_WIDGET_H
