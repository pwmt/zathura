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
 * @param document the associated document
 * @return a document view widget
 */
GtkWidget* zathura_document_widget_new(zathura_t* zathura, zathura_document_t* document);

/**
 * Associate a document with the widget and initialize its page storage.
 * Existing rendering is stopped and page widgets are released first.
 * A renderer is created and configured for the new document.
 *
 * @param document_widget A non-NULL document widget
 * @param document The document to associate, or NULL to clear the widget
 * @return true on success, false if page storage or renderer creation fails
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
 * Enable or disable document recoloring.
 *
 * A changed value invalidates rendered page surfaces and cached thumbnails.
 *
 * @param document A non-NULL document widget
 * @param enable true to enable recoloring, false to disable it
 * @return true if the value changed; false if unchanged or the widget has no document
 */
bool zathura_document_widget_set_recolor(ZathuraDocumentWidget* document, bool enable);

/**
 * Enable or disable hue preservation during recoloring.
 *
 * A changed value invalidates rendered page surfaces and cached thumbnails.
 *
 * @param document A non-NULL document widget
 * @param enable true to preserve hues, false to disable hue preservation
 * @return true if the value changed; false if unchanged or the widget has no document
 */
bool zathura_document_widget_set_recolor_hue(ZathuraDocumentWidget* document, bool enable);

/**
 * Enable or disable image preservation in reverse-video recoloring.
 *
 * A changed value invalidates rendered page surfaces and cached thumbnails.
 *
 * @param document A non-NULL document widget
 * @param enable true to preserve images, false to recolor them
 * @return true if the value changed; false if unchanged or the widget has no document
 */
bool zathura_document_widget_set_recolor_reverse_video(ZathuraDocumentWidget* document, bool enable);

/**
 * Enable or disable lightness adjustment during recoloring.
 *
 * A changed value invalidates rendered page surfaces and cached thumbnails.
 *
 * @param document A non-NULL document widget
 * @param enable true to adjust lightness, false to disable the adjustment
 * @return true if the value changed; false if unchanged or the widget has no document
 */
bool zathura_document_widget_set_recolor_adjust_lightness(ZathuraDocumentWidget* document, bool enable);

/**
 * Set recolor colors using color strings.
 *
 * Parse and compare the supplied colors before updating the renderer. Changed colors
 * invalidate rendered page surfaces and cached thumbnails. Invalid colors are ignored.
 *
 * @param document A non-NULL document widget
 * @param light Light color string, or NULL to leave it unchanged
 * @param dark Dark color string, or NULL to leave it unchanged
 * @return true if colors changed; false if unchanged or the widget has no document
 */
bool zathura_document_widget_set_recolor_colors(ZathuraDocumentWidget* document, const char* light, const char* dark);

/** Apply current recolor settings
 *
 * Invalidates cached surfaces once if any recolor setting changed.
 *
 * @param document the document widget
 */
void zathura_document_widget_apply_recolor_settings(ZathuraDocumentWidget* document);

/**
 * Check whether a page's layout cell intersects the viewport.
 *
 * In single-page mode only the selected page is visible.
 *
 * @param document the document widget
 * @param page_number the page number
 * @return true the page is visible, false otherwise
 */
bool zathura_document_widget_page_is_visible(ZathuraDocumentWidget* document, unsigned int page_number);

/**
 * Stop page-widget preloading and rendering for the current document.
 *
 * Wait for rendering workers to finish. Existing page widgets and cached surfaces
 * remain available, but the stopped renderer cannot accept new jobs. Attaching a
 * document with zathura_document_widget_set_document() creates a new renderer.
 *
 * @param document A non-NULL document widget
 */
void zathura_document_widget_stop(ZathuraDocumentWidget* document);

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

/** Whether a layout transition is waiting to restore the selected page during allocation. */
bool zathura_document_widget_mode_change_pending(ZathuraDocumentWidget* document);

/**
 * Calculate the position of each grid cell.
 * Required when any page size is changed.
 *
 * @param document ZathuraDocumentWidget
 */
void zathura_document_widget_compute_layout(ZathuraDocumentWidget* document);

/**
 * Return the page's top-left corner in document pixel coordinates at the current
 * scale. In single-page mode both coordinates are zero.
 *
 * @param document   ZathuraDocumentWidget
 * @param page_index index of the page
 * @param x          pixel offset in the x direction
 * @param y          pixel offset in the y direction
 */
void zathura_document_widget_get_page_origin(ZathuraDocumentWidget* document, unsigned int page_index, double* x,
                                             double* y);

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
 * Stop and release the renderer and all page widgets, then clear the associated document and layout.
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
 * @param document The document widget
 * @return The column for the first page
 */
unsigned int zathura_document_widget_get_first_page_column(ZathuraDocumentWidget* document);

/**
 * Set viewport dimensions, configure scroll increments, and restore the stored position.
 *
 * Used to prepare detached views; allocation configures attached views after fitting zoom.
 *
 * @param document The document widget
 * @param height   The height of the viewport
 * @param width    The width of the viewport
 */
void zathura_document_widget_configure_viewport(ZathuraDocumentWidget* document, unsigned int width,
                                                unsigned int height);

/**
 * Return the size of the viewport in pixels.
 *
 * @param[in]  document The document widget
 * @param[out] height   The height of the viewport
 * @param[out] width    The width of the viewport
 */
void zathura_document_widget_get_viewport_size(ZathuraDocumentWidget* document, unsigned int* height,
                                               unsigned int* width);

/**
 * Returns the X position, as a value relative to the document width (0=left,
 * 1=right).
 *
 * @param document The document
 * @return X adjustment
 */
double zathura_document_widget_get_position_x(ZathuraDocumentWidget* document);

/**
 * Returns the Y position as value relative to the document height (0=top,
 * 1=bottom)
 *
 * @param document The document
 * @return Y adjustment
 */
double zathura_document_widget_get_position_y(ZathuraDocumentWidget* document);

/**
 * Return the stored document position relative to the current page.
 *
 * @param document The document widget
 * @param position_x Optional output for the horizontal position
 * @param position_y Optional output for the vertical position
 * @return true if page and layout dimensions are available; otherwise outputs are unchanged
 */
bool zathura_document_widget_get_page_relative_position(ZathuraDocumentWidget* document, double* position_x,
                                                        double* position_y);

/**
 * Convert page-relative coordinates to document coordinates, clamped to [0, 1].
 *
 * @param document The document widget
 * @param page_id The page number
 * @param position_x X coordinate to convert
 * @param position_y Y coordinate to convert
 */
void zathura_document_widget_page_position_to_document(ZathuraDocumentWidget* document, unsigned int page_id,
                                                       double* position_x, double* position_y);

/**
 * Set the position.
 *
 * Coordinates are relative to the whole document in grid mode and to the
 * current page in single-page mode. If negative coordinates are specified, the
 * configured page alignment is used to compute the position.
 *
 * @param widget The document widget
 * @param position_x Horizontal viewport center (0=left, 1=right), or a negative value for page alignment
 * @param position_y Vertical viewport center (0=top, 1=bottom), or a negative value for page alignment
 */
void zathura_document_widget_set_position(ZathuraDocumentWidget* widget, double position_x, double position_y);

/**
 * Apply the stored viewport center to this widget's scroll adjustments.
 *
 * @param widget The document widget
 */
void zathura_document_widget_apply_position(ZathuraDocumentWidget* document);

/**
 * Returns the adjust mode of the document
 *
 * @param document The document
 * @return The adjust mode
 */
zathura_adjust_mode_t zathura_document_widget_get_adjust_mode(ZathuraDocumentWidget* document);

/**
 * Sets the new adjust mode of the document
 *
 * @param document The document
 * @param mode The new adjust mode
 */
void zathura_document_widget_set_adjust_mode(ZathuraDocumentWidget* document, zathura_adjust_mode_t mode);

/**
 * Scale individual pages to match the current page's width or height.
 *
 * Recompute layout and invalidate rendered pages after applying the page zoom factors.
 * ZATHURA_EQUAL_NONE resets individual page zoom factors to 1.0.
 *
 * @param widget A non-NULL document widget with an associated document
 * @param mode ZATHURA_EQUAL_WIDTH, ZATHURA_EQUAL_HEIGHT, or ZATHURA_EQUAL_NONE
 */
void zathura_document_widget_set_equal_mode(ZathuraDocumentWidget* widget, zathura_equal_mode_t mode);

/**
 * Adjust the document zoom to the widget's viewport and adjustment mode.
 *
 * Uses the widget's associated document and layout, including for detached widgets.
 * A zoom change that materially changes page dimensions invalidates rendered pages.
 *
 * @param widget A non-NULL document widget
 * @return true if no adjustment is needed or the adjustment was processed;
 *         false if no document is associated or required geometry is unavailable
 */
bool zathura_document_widget_adjust_view(ZathuraDocumentWidget* widget);

/**
 * Prepare layout and queue background renders for pages intersecting the viewport.
 *
 * Parse visible pages until layout dimensions settle, clamp the viewport center,
 * and submit their existing page-widget render requests. The widget may be detached;
 * its layout and nonzero viewport dimensions must be configured beforehand.
 * Rendering completes on the renderer thread; surfaces are installed on the main thread.
 *
 * @param document A non-NULL document widget with an associated document
 * @param position_x Saved horizontal viewport center, or a negative value to retain the current position or page anchor
 * @param position_y Saved vertical viewport center, or a negative value to retain the current position or page anchor
 * @return true if visible pages were queued; false if required document or viewport
 *         state is unavailable, or parsing or layout stabilization fails
 */
bool zathura_document_widget_prepare_visible_pages(ZathuraDocumentWidget* document, double position_x,
                                                   double position_y);

#endif // DOCUMENT_WIDGET_H
