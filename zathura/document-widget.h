/* SPDX-License-Identifier: Zlib */

#ifndef DOCUMENT_WIDGET_H
#define DOCUMENT_WIDGET_H

#include <gtk/gtk.h>
#include "types.h"

/**
 * The document view widget.
 */
struct zathura_document_widget_s {
  GtkContainer parent;
};

struct zathura_document_widget_class_s {
  GtkContainerClass parent_class;
};

#define ZATHURA_TYPE_DOCUMENT_WIDGET (zathura_document_widget_get_type())
#define ZATHURA_DOCUMENT_WIDGET(obj)                                                                                   \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_DOCUMENT_WIDGET, ZathuraDocumentWidget))
#define ZATHURA_DOCUMENT_WIDGET_CLASS(obj)                                                                             \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_DOCUMENT_WIDGET, ZathuraDocumentWidgetClass))
#define ZATHURA_IS_DOCUMENT_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_DOCUMENT_WIDGET))
#define ZATHURA_IS_DOCUMENT_WIDGET_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_DOCUMENT_WIDGET))
#define ZATHURA_DOCUMENT_WIDGET_GET_CLASS(obj)                                                                         \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_DOCUMENT_WIDGET, ZathuraDocumentWidgetClass))

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

/**
 * Clear all thumbnails.
 *
 * @param document ZathuraDocumentWidget
 */
void zathura_document_widget_clear_thumbnails(ZathuraDocumentWidget* document);

/**
 * This function is used to unmark all pages as not rendered. This should
 * be used if all pages should be rendered again (e.g.: the zoom level or the
 * colors have changed)
 *
 * @param zathura Zathura object
 */
void zathura_document_widget_render_all(ZathuraDocumentWidget* document);

/**
 * Sets the layout of the pages in the document
 *
 * @param[in]  document          The document instance
 * @param[in]  page_v_padding      pixels of vertical padding between pages
 * @param[in]  page_h_padding      pixels of horizontal padding between pages
 * @param[in]  pages_per_row     number of pages per row
 * @param[in]  first_page_column column of the first page (first column is 1)
 */
void zathura_document_widget_set_page_layout(ZathuraDocumentWidget* document, unsigned int page_v_padding,
                                             unsigned int page_h_padding, unsigned int pages_per_row,
                                             unsigned int first_page_column);

/**
 * Returns the vertical padding in pixels between pages
 *
 * @param document The document
 * @return The padding in pixels between pages
 */
unsigned int zathura_document_widget_get_page_v_padding(ZathuraDocumentWidget* document);

/**
 * Returns the horizontal padding in pixels between pages
 *
 * @param document The document
 * @return The padding in pixels between pages
 */
unsigned int zathura_document_widget_get_page_h_padding(ZathuraDocumentWidget* document);

/**
 * Returns the number of pages per row
 *
 * @param document The document
 * @return The number of pages per row
 */
unsigned int zathura_document_widget_get_pages_per_row(ZathuraDocumentWidget* document);

/**
 * Returns the column for the first page (first column = 1)
 *
 * @param document The document
 * @return The column for the first page
 */
unsigned int zathura_document_widget_get_first_page_column(ZathuraDocumentWidget* document);

#endif // DOCUMENT_WIDGET_H
