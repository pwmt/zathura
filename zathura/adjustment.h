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
 * @param widget the document widget
 * @param page the page
 * @param page_height the scaled and rotated height
 * @param page_width the scaled and rotated width
 * @param rotate honor page's rotation
 * @return real scale after rounding
 */
double page_calc_height_width(ZathuraDocumentWidget* widget, zathura_page_t* page, unsigned int* page_height,
                              unsigned int* page_width, bool rotate);

/**
 * Calculates rounded page dimensions from a scale and rotation without
 * accessing widgets.
 *
 * @param page The page object
 * @param scale The scale in logical pixels per point
 * @param rotation The rotation in degrees (0, 90, 180 or 270)
 * @param page_height The scaled and rotated height
 * @param page_width The scaled and rotated width
 * @return The effective scale
 */
double page_calc_size(zathura_page_t* page, double scale, unsigned int rotation, unsigned int* page_height,
                      unsigned int* page_width);

/**
 * Converts a relative position within the document to a page number.
 *
 * @param zathura The zathura instance
 * @param pos_x the x position relative to the document
 * @param pos_y the y position relative to the document
 * @return page sitting in that position
 */
unsigned int position_to_page_number(zathura_t* zathura, double pos_x, double pos_y);

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
 * @param widget The target document widget
 * @param page_number the given page number
 * @param xalign where to align the viewport and the page
 * @param yalign where to align the viewport and the page
 * @param pos_x position that will lie at the center of the viewport.
 * @param pos_y position that will lie at the center of the viewport.
 */
void page_number_to_position(ZathuraDocumentWidget* widget, unsigned int page_number, double xalign, double yalign,
                             double* pos_x, double* pos_y);

gdouble zathura_adjustment_get_ratio(GtkAdjustment* adjustment);
void zathura_adjustment_set_value(GtkAdjustment* adjustment, gdouble value);
void zathura_adjustment_set_value_from_ratio(GtkAdjustment* adjustment, gdouble ratio);

/**
 * Check whether the value belongs to the given ratio. A value that does not was
 * set by someone else.
 *
 * @param adjustment the adjustment
 * @param ratio the ratio
 * @return true if the value belongs to the ratio
 */
bool zathura_adjustment_value_matches_ratio(GtkAdjustment* adjustment, gdouble ratio);

#endif /* ZATHURA_ADJUSTMENT_H */
