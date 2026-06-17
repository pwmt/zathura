/* SPDX-License-Identifier: Zlib */

#include <girara/datastructures.h>
#include <girara/log.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "settings.h"
#include "session.h"

void widget_add_class(GtkWidget* widget, const char* styleclass) {
  if (widget == NULL || styleclass == NULL) {
    return;
  }

  if (gtk_widget_has_css_class(widget, styleclass) == FALSE) {
    gtk_widget_add_css_class(widget, styleclass);
  }
}

void widget_remove_class(GtkWidget* widget, const char* styleclass) {
  if (widget == NULL || styleclass == NULL) {
    return;
  }

  if (gtk_widget_has_css_class(widget, styleclass) == TRUE) {
    gtk_widget_remove_css_class(widget, styleclass);
  }
}
