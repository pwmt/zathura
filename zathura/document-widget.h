/* SPDX-License-Identifier: Zlib */

#ifndef DOCUMENT_WIDGET_H
#define DOCUMENT_WIDGET_H

#include <gtk/gtk.h>
#include "types.h"

/**
 * The document view widget. Places a subset of the pages of
 * the document into a grid. The widget handles updating the
 * grid to contain the pages in view, and as many pages around
 * the view as the cairo surface will allow.
 *
 * zathura_document_widget_[get_ratio|set_value|set_value_from_ratio]
 * functions replace the equivalent ones previously contained in
 * adjustment.c. They wrap these functions and perform the necessary
 * transformations to move between a ratio of [0,1] in the whole document,
 * to a ratio in the subset of the pages contained in the document widgets'
 * grid.
 *
 * */
struct zathura_document_widget_s {
  GtkGrid parent;
};

struct zathura_document_widget_class_s {
  GtkGridClass parent_class;
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
GtkWidget* zathura_document_widget_new(void);

/**
 * Builds the box structure to show the rendered pages
 *
 * @param zathura The zathura session
 * @param page_v_padding vertical padding in pixels between pages
 * @param page_h_padding horizontal padding in pixels between pages
 * @param page_right_to_left Render pages right to left
 */
void zathura_document_widget_set_mode(zathura_t* zathura, 
    unsigned int page_v_padding, unsigned int page_h_padding, bool page_right_to_left);

/**
 * Update the pages in the document view
 *
 * @param widget the document view widget
 */
void zathura_document_widget_render(zathura_t* zathura);

/**
 * Clear pages from the document view
 *
 * @param widget the document view widget
 */
void zathura_document_widget_clear_pages(GtkWidget* widget);

/**
 * Compute the adjustment ratio
 *
 * That is, the ratio between the length from the lower bound to the middle of
 * the slider, and the total length of the scrollbar.
 *
 * @param adjustment Scrollbar adjustment
 * @param width Is the adjustment for width?
 * @return Adjustment ratio
 */
gdouble zathura_document_widget_get_ratio(zathura_t* zathura, GtkAdjustment* adjustment, bool width);

/**
 * Set the adjustment value from ratio
 *
 * The ratio is usually obtained from a previous call to
 * zathura_document_widget_get_ratio().
 *
 * @param adjustment Adjustment instance
 * @param ratio Ratio from which the adjustment value will be set
 * @param width Is the adjustment for width?
 */
void zathura_document_widget_set_value_from_ratio(zathura_t* zathura, GtkAdjustment* adjustment, double ratio,
                                                  bool width);
#endif // DOCUMENT_WIDGET_H
