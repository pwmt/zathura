/* See LICENSE file for license and copyright information */

#ifndef PAGE_VIEW_WIDGET_H
#define PAGE_VIEW_WIDGET_H

#include <gtk/gtk.h>
#include "document.h"

/** The page view widget. The widget handles all the rendering on its own. It
 * only has to be resized. */
typedef struct zathura_page_view_s ZathuraPageView;
typedef struct zathura_page_view_class_s ZathuraPageViewClass;

struct zathura_page_view_s {
  GtkDrawingArea parent;
};

struct zathura_page_view_class_s {
  GtkDrawingAreaClass parent_class;
};

#define ZATHURA_TYPE_PAGE_VIEW \
  (zathura_page_view_get_type ())
#define ZATHURA_PAGE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), ZATHURA_TYPE_PAGE_VIEW, ZathuraPageView))
#define ZATHURA_PAGE_VIEW_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST ((obj), ZATHURA_PAGE_VIEW, ZathuraPageViewClass))
#define ZATHURA_IS_PAGE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ZATHURA_PAGE_VIEW))
#define ZATHURA_IS_PAGE_VIEW_WDIGET_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE ((obj), ZATHURA_TYPE_PAGE_VIEW))
#define ZATHURA_PAGE_VIEW_GET_CLASS \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), ZATHURA_TYPE_PAGE_VIEW, ZathuraPageViewclass))

/** Returns the type of the page view widget.
 * @return the type
 */
GType zathura_page_view_get_type(void);
/** Create a page view widget.
 * @param page the page to be displayed
 * @return a page view widget
 */
GtkWidget* zathura_page_view_new(zathura_page_t* page);
/** Update the widget's surface. This should only be called from the render
 * thread.
 * @param widget the widget
 * @param surface the new surface
 */
void zathura_page_view_update_surface(ZathuraPageView* widget, cairo_surface_t* surface);

#endif
