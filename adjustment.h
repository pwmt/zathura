/* See LICENSE file for license and copyright information */

#ifndef ZATHURA_ADJUSTMENT_H
#define ZATHURA_ADJUSTMENT_H

#include <gtk/gtk.h>

/* Clone a GtkAdjustment
 *
 * Creates a new adjustment with the same value, lower and upper bounds, step
 * and page increments and page_size as the original adjustment.
 *
 * @param adjustment Adjustment instance to be cloned
 * @return Pointer to the new adjustment
 */
GtkAdjustment* zathura_adjustment_clone(GtkAdjustment* adjustment);

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
