/* SPDX-License-Identifier: Zlib */

#ifndef DOCUMENT_WIDGET_H
#define DOCUMENT_WIDGET_H

#include <gtk/gtk.h>
#include "types.h"

/**
 * The document view widget. 
 *
 * Draw pages of the document to a canvas.
 * Place the canvas inside a scrolled window to allow scrolling.
 * Zoom by resizing the canvas'.
 *
 * */
struct zathura_document_widget_s {
  GtkGrid parent;
};

struct zathura_document_widget_class_s {
  GtkGridClass parent_class;
};

#define ZATHURA_TYPE_DOCUMENT (zathura_document_widget_get_type())
#define ZATHURA_DOCUMENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_DOCUMENT, ZathuraDocument))
#define ZATHURA_DOCUMENT_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_DOCUMENT, ZathuraDocumentClass))
#define ZATHURA_IS_DOCUMENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_DOCUMENT))
#define ZATHURA_IS_DOCUMENT_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_DOCUMENT))
#define ZATHURA_DOCUMENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_DOCUMENT, ZathuraDocumentClass))

/**
 * Returns the type of the document view widget.
 * @return the type
 */
GType zathura_document_widget_get_type(void) G_GNUC_CONST;
/**
 * Create a document view widget.
 * @param zathura the zathura instance
 * @return a document view widget
 */
GtkWidget* zathura_document_widget_new(zathura_t *zathura);
/**
 * Update the pages in the document view
 * @param widget the document view widget
 */
void zathura_document_widget_update_pages(GtkWidget *widget);
/**
 * Clear pages from the document view
 * @param widget the document view widget
 */
void zathura_document_widget_clear_pages(GtkWidget *widget);
#endif // DOCUMENT_WIDGET_H
