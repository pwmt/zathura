/* SPDX-License-Identifier: Zlib */

#ifndef PAGE_WIDGET_H
#define PAGE_WIDGET_H

#include <gtk/gtk.h>
#include "types.h"
#include "document.h"

/**
 * The page view widget. The widget handles all the rendering on its own. It
 * only has to be resized. The widget also manages and handles all the
 * rectangles for highlighting.
 *
 * Before the properties contain the correct values, 'draw-links' has to be set
 * to TRUE at least one time.
 * */
struct zathura_page_widget_s {
  GtkDrawingArea parent;
};

struct zathura_page_widget_class_s {
  GtkDrawingAreaClass parent_class;
};

#define ZATHURA_TYPE_PAGE_WIDGET (zathura_page_widget_get_type())
#define ZATHURA_PAGE_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_PAGE_WIDGET, ZathuraPageWidget))
#define ZATHURA_PAGE_WIDGET_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_PAGE_WIDGET, ZathuraPageClass))
#define ZATHURA_IS_PAGE_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_PAGE_WIDGET))
#define ZATHURA_IS_PAGE_WIDGET_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_PAGE_WIDGET))
#define ZATHURA_PAGE_WIDGET_GET_CLASS(obj)                                                                             \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_PAGE_WIDGET, ZathuraPageClass))

/**
 * Returns the type of the page view widget.
 * @return the type
 */
GType zathura_page_widget_get_type(void) G_GNUC_CONST;
/**
 * Create a page view widget.
 * @param zathura the zathura instance
 * @param page the page to be displayed
 * @return a page view widget
 */
GtkWidget* zathura_page_widget_new(zathura_t* zathura, zathura_page_t* page);
/**
 * Update the widget's surface. This should only be called from the render
 * thread.
 * @param widget the widget
 * @param surface the new surface
 * @param keep_thumbnail don't destroy when surface is NULL
 */
void zathura_page_widget_update_surface(ZathuraPageWidget* widget, cairo_surface_t* surface, bool keep_thumbnail);
/**
 * Clear highlight of the selection/highlighter.
 * @param widget the widget
 */
void zathura_page_widget_clear_selection(ZathuraPageWidget* widget);
/**
 * Draw a rectangle to mark links or search results
 * @param widget the widget
 * @param rectangle the rectangle
 * @param linkid the link id if it's a link, -1 otherwise
 */
zathura_link_t* zathura_page_widget_link_get(ZathuraPageWidget* widget, unsigned int index);
/**
 * Update the last view time of the page.
 *
 * @param widget the widget
 */
void zathura_page_widget_update_view_time(ZathuraPageWidget* widget);
/**
 * Check if we have a surface.
 *
 * @param widget the widget
 * @returns true if the widget has a surface, false otherwise
 */
bool zathura_page_widget_have_surface(ZathuraPageWidget* widget);
/**
 * Abort outstanding render requests
 *
 * @param widget the widget
 */
void zathura_page_widget_abort_render_request(ZathuraPageWidget* widget);
/**
 * Get underlying page
 *
 * @param widget the widget
 * @return underlying zathura_page_t instance
 */
zathura_page_t* zathura_page_widget_get_page(ZathuraPageWidget* widget);

#endif
