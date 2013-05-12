/* See LICENSE file for license and copyright information */

#include "adjustment.h"
#include "utils.h"

GtkAdjustment*
zathura_adjustment_clone(GtkAdjustment* adjustment)
{
  gdouble value          = gtk_adjustment_get_value(adjustment);
  gdouble lower          = gtk_adjustment_get_lower(adjustment);
  gdouble upper          = gtk_adjustment_get_upper(adjustment);
  gdouble step_increment = gtk_adjustment_get_step_increment(adjustment);
  gdouble page_increment = gtk_adjustment_get_page_increment(adjustment);
  gdouble page_size      = gtk_adjustment_get_page_size(adjustment);

  return GTK_ADJUSTMENT(gtk_adjustment_new(value, lower, upper, step_increment,
        page_increment, page_size));
}

void
zathura_adjustment_set_value(GtkAdjustment* adjustment, gdouble value)
{
  gtk_adjustment_set_value(adjustment,
                           MAX(gtk_adjustment_get_lower(adjustment),
                               MIN(gtk_adjustment_get_upper(adjustment) -
                                   gtk_adjustment_get_page_size(adjustment),
                                   value)));
}

gdouble
zathura_adjustment_get_ratio(GtkAdjustment* adjustment)
{
  gdouble lower     = gtk_adjustment_get_lower(adjustment);
  gdouble upper     = gtk_adjustment_get_upper(adjustment);
  gdouble page_size = gtk_adjustment_get_page_size(adjustment);
  gdouble value     = gtk_adjustment_get_value(adjustment);

  return (value - lower + page_size / 2.0) / (upper - lower);
}

void
zathura_adjustment_set_value_from_ratio(GtkAdjustment* adjustment,
                                        gdouble ratio)
{
  if (ratio == 0.0) {
    return;
  }

  gdouble lower = gtk_adjustment_get_lower(adjustment);
  gdouble upper = gtk_adjustment_get_upper(adjustment);
  gdouble page_size = gtk_adjustment_get_page_size(adjustment);

  gdouble value = (upper - lower) * ratio + lower - page_size / 2.0;

  zathura_adjustment_set_value(adjustment, value);
}
