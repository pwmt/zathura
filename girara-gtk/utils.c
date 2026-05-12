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

  GtkStyleContext* context = gtk_widget_get_style_context(widget);
  if (gtk_style_context_has_class(context, styleclass) == FALSE) {
    gtk_style_context_add_class(context, styleclass);
  }
}

void widget_remove_class(GtkWidget* widget, const char* styleclass) {
  if (widget == NULL || styleclass == NULL) {
    return;
  }

  GtkStyleContext* context = gtk_widget_get_style_context(widget);
  if (gtk_style_context_has_class(context, styleclass) == TRUE) {
    gtk_style_context_remove_class(context, styleclass);
  }
}
