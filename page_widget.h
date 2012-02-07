/* See LICENSE file for license and copyright information */

#ifndef PAGE_WIDGET_H
#define PAGE_WIDGET_H

#include <gtk/gtk.h>
#include "document.h"

/**
 * The page view widget. The widget handles all the rendering on its own. It
 * only has to be resized. The widget also manages and handles all the
 * rectangles for highlighting.
 *
 * Before the properties contain the correct values, 'draw-links' has to be set
 * to TRUE at least one time.
 * */
typedef struct zathura_page_widget_s ZathuraPage;
typedef struct zathura_page_widget_class_s ZathuraPageClass;

struct zathura_page_widget_s {
  GtkDrawingArea parent;
};

struct zathura_page_widget_class_s {
  GtkDrawingAreaClass parent_class;
};

#define ZATHURA_TYPE_PAGE \
  (zathura_page_widget_get_type ())
#define ZATHURA_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), ZATHURA_TYPE_PAGE, ZathuraPage))
#define ZATHURA_PAGE_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST ((obj), ZATHURA_PAGE, ZathuraPageClass))
#define ZATHURA_IS_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ZATHURA_PAGE))
#define ZATHURA_IS_PAGE_WDIGET_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE ((obj), ZATHURA_TYPE_PAGE))
#define ZATHURA_PAGE_GET_CLASS \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), ZATHURA_TYPE_PAGE, ZathuraPageclass))

/**
 * Returns the type of the page view widget.
 * @return the type
 */
GType zathura_page_widget_get_type(void);
/**
 * Create a page view widget.
 * @param page the page to be displayed
 * @return a page view widget
 */
GtkWidget* zathura_page_widget_new(zathura_page_t* page);
/**
 * Update the widget's surface. This should only be called from the render
 * thread.
 * @param widget the widget
 * @param surface the new surface
 */
void zathura_page_widget_update_surface(ZathuraPage* widget, cairo_surface_t* surface);
/**
 * Draw a rectangle to mark links or search results
 * @param widget the widget
 * @param rectangle the rectangle
 * @param linkid the link id if it's a link, -1 otherwise
 */
void zathura_page_widget_draw_rectangle(ZathuraPage* widget, zathura_rectangle_t* rectangle, int linkid);
/**
 * Clear all rectangles.
 * @param widget the widget
 */
void zathura_page_widget_clear_rectangles(ZathuraPage* widget);

/**
 * Returns the zathura link object at the given index
 *
 * @param widget the widget
 * @param index Index of the link
 * @return Link object or NULL if an error occured
 */
zathura_link_t* zathura_page_widget_link_get(ZathuraPage* widget, unsigned int index);

#endif
