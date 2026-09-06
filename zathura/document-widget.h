/* SPDX-License-Identifier: Zlib */

#ifndef DOCUMENT_WIDGET_H
#define DOCUMENT_WIDGET_H

#include <stdbool.h>
#include <gtk/gtk.h>
#include "types.h"

/**
 * The document view widget.
 */
struct zathura_document_widget_s {
  GtkWidget parent;
};

struct zathura_document_widget_class_s {
  GtkWidgetClass parent_class;
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
GType zathura_document_widget_get_type(void);

/**
 * Create a document view widget.
 *
 * @param zathura the zathura instance
 * @param zathura_document the associated document, or NULL for an empty widget
 * @return a document view widget
 */
GtkWidget* zathura_document_widget_new(zathura_t* zathura, zathura_document_t* document);

/**
 * Associate a document with the widget and initialize its page storage.
 * Existing page widgets are released first.
 *
 * @param document the document widget
 * @param zathura_document the document, or NULL to clear the widget
 * @return true on success
 */
bool zathura_document_widget_set_document(ZathuraDocumentWidget* document_widget, zathura_document_t* document);

/**
 * Return the document associated with the widget.
 *
 * @param document the document widget
 * @return the associated document
 */
zathura_document_t* zathura_document_widget_get_document(ZathuraDocumentWidget* document);

/**
 * Return a page widget by page number.
 *
 * @param document the document widget
 * @param page_number the page number
 * @return the page widget, or NULL if it has not been created
 */
GtkWidget* zathura_document_widget_get_page(ZathuraDocumentWidget* document, unsigned int page_number);

/**
 * Create a page widget if necessary and attach it to the document grid.
 *
 * @param document the document widget
 * @param page_number the page number
 * @return the page widget, or NULL on error
 */
GtkWidget* zathura_document_widget_ensure_page(ZathuraDocumentWidget* document, unsigned int page_number);

/**
 * Schedule creation of all missing page widgets at low idle priority.
 * Any active preload is restarted. The "page-widgets-loaded" signal is emitted
 * after every page widget has been created.
 *
 * @param document the document widget
 */
void zathura_document_widget_start_page_widget_preload(ZathuraDocumentWidget* document);

/**
 * Cancel page-widget preloading and reset its completion state.
 *
 * @param document the document widget
 */
void zathura_document_widget_stop_page_widget_preload(ZathuraDocumentWidget* document);

/**
 * Return whether the most recent page-widget preload completed.
 *
 * @param document the document widget
 * @return true if all page widgets were created by the preload
 */
bool zathura_document_widget_page_widgets_loaded(ZathuraDocumentWidget* document);

/**
 * Recalculate page visibility from the viewport. This creates newly visible
 * page widgets, updates render priority and caching, and aborts render requests
 * for pages that left the viewport.
 *
 * @param document the document widget
 */
void zathura_document_widget_update_visible_pages(ZathuraDocumentWidget* document);

/**
 * Render the document's current page synchronously and install the resulting
 * surface in its page widget. Does nothing if the renderer or page widget is
 * unavailable.
 *
 * @param document the document widget
 */
void zathura_document_widget_render_current_page(ZathuraDocumentWidget* document);

/**
 * Check whether a page widget exists and has a rendered surface.
 *
 * @param document the document widget
 * @param page_number the page number
 * @return true if the page widget has a rendered surface
 */
bool zathura_document_widget_page_has_surface(ZathuraDocumentWidget* document, unsigned int page_number);

/**
 * Enable or disable signature information on all existing page widgets and on
 * page widgets created later.
 *
 * @param document the document widget
 * @param draw whether signature information should be drawn
 */
void zathura_document_widget_set_draw_signatures(ZathuraDocumentWidget* document, bool draw);

/**
 * Enable or disable search-result highlighting on all existing page widgets.
 *
 * @param document the document widget
 * @param draw whether search results should be drawn
 */
void zathura_document_widget_set_draw_search_results(ZathuraDocumentWidget* document, bool draw);

/**
 * Prepare link hints for the visible pages. Search-result highlighting is
 * disabled and link indices are made continuous across those pages.
 *
 * @param document the document widget
 * @return true if at least one visible page contains a link
 */
bool zathura_document_widget_prepare_links(ZathuraDocumentWidget* document);

/**
 * Disable link hints on all existing page widgets.
 *
 * @param document the document widget
 */
void zathura_document_widget_hide_links(ZathuraDocumentWidget* document);

/**
 * Find a link by its displayed index among the visible page widgets.
 * The returned link remains owned by its page widget.
 *
 * @param document the document widget
 * @param index the displayed link index
 * @return the matching link, or NULL if no visible page contains it
 */
zathura_link_t* zathura_document_widget_get_visible_link(ZathuraDocumentWidget* document, unsigned int index);

/**
 * Count search results on page widgets before a given page. The upper bound is
 * clamped to the document's number of pages, and pages without widgets count as
 * zero.
 *
 * @param document the document widget
 * @param end_page exclusive upper page bound
 * @return the number of search results in pages [0, end_page)
 */
unsigned int zathura_document_widget_get_search_result_count(ZathuraDocumentWidget* document, unsigned int end_page);

/**
 * Update internal layout structures when pages-per-row,
 * first page column or document changes.
 *
 * @param document ZathuraDocumentWidget
 */
void zathura_document_widget_refresh_layout(ZathuraDocumentWidget* document);

void zathura_document_widget_update_mode(ZathuraDocumentWidget* document);

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
 * Release all page widgets and clear the associated document and layout.
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
