/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_ADJUSTMENT_H
#define ZATHURA_ADJUSTMENT_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include "document.h"

/**
 * Calculate the page size according to the current scaling and rotation if
 * desired.
 *
 * @param document the document
 * @param height the original height
 * @param width the original width
 * @param page_height the scaled and rotated height
 * @param page_width the scaled and rotated width
 * @param rotate honor page's rotation
 * @return real scale after rounding
 */
double page_calc_height_width(zathura_document_t* document, double height, double width,
                       unsigned int* page_height, unsigned int* page_width, bool rotate);

/**
 * Calculate a page relative position after a rotation. The positions x y are
 * relative to a page, i.e. 0=top of page, 1=bottom of page. They are NOT
 * relative to the entire document.
 *
 * @param document the document
 * @param x the x coordinates on the unrotated page
 * @param y the y coordinates on the unrotated page
 * @param xn the x coordinates after rotation
 * @param yn the y coordinates after rotation
 */
void page_calc_position(zathura_document_t* document, double x, double y,
                        double *xn, double *yn);

/**
 * Converts a relative position within the document to a page number.
 *
 * @param document The document
 * @param pos_x the x position relative to the document
 * @param pos_y the y position relative to the document
 * @return page sitting in that position
 */
unsigned int position_to_page_number(zathura_document_t* document,
                                         double pos_x, double pos_y);

/**
 * Converts a page number to a position in units relative to the document
 *
 * We can specify where to aliwn the viewport and the page. For instance, xalign
 * = 0 means align them on the left margin, xalign = 0.5 means centered, and
 * xalign = 1.0 align them on the right margin.
 *
 * The return value is the position in in units relative to the document (0=top
 * 1=bottom) of the point thet will lie at the center of the viewport.
 *
 * @param document The document
 * @param page_number the given page number
 * @param xalign where to align the viewport and the page
 * @param yalign where to align the viewport and the page
 * @param pos_x position that will lie at the center of the viewport.
 * @param pos_y position that will lie at the center of the viewport.
 */
void page_number_to_position(zathura_document_t* document, unsigned int page_number,
                             double xalign, double yalign, double *pos_x, double *pos_y);

/**
 * Checks whether a given page falls within the viewport
 *
 * @param document The document
 * @param page_number the page number
 * @return true if the page intersects the viewport
 */
bool page_is_visible(zathura_document_t *document, unsigned int page_number);

/**
 * Set the adjustment value while enforcing its limits
 *
 * @param adjustment Adjustment instance
 * @param value Adjustment value
 */
void zathura_adjustment_set_value(GtkAdjustment* adjustment, gdouble value);

/**
 * Compute the adjustment ratio
 *
 * That is, the ratio between the length from the lower bound to the middle of
 * the slider, and the total length of the scrollbar.
 *
 * @param adjustment Scrollbar adjustment
 * @return Adjustment ratio
 */
gdouble zathura_adjustment_get_ratio(GtkAdjustment* adjustment);

/**
 * Set the adjustment value from ratio
 *
 * The ratio is usually obtained from a previous call to
 * zathura_adjustment_get_ratio().
 *
 * @param adjustment Adjustment instance
 * @param ratio Ratio from which the adjustment value will be set
 */
void zathura_adjustment_set_value_from_ratio(GtkAdjustment* adjustment,
                                             gdouble ratio);

#endif /* ZATHURA_ADJUSTMENT_H */
