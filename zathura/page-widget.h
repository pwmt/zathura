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
  GtkWidget parent;
};

struct zathura_page_widget_class_s {
  GtkWidgetClass parent_class;
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
GType zathura_page_widget_get_type(void);
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
 * Update the mouse selection of the page to end at a point.
 * @param widget the widget
 * @param x,y the end of the selection, relative to the widget
 */
void zathura_page_widget_update_mouse_selection(ZathuraPageWidget* widget, double x, double y);

/**
 * Convert a point relative to the widget to page coordinates.
 * @param widget the widget
 * @param x,y the point, relative to the widget
 * @param page_x,page_y set to the point in unscaled and unrotated page coordinates
 */
void zathura_page_widget_point_to_page(ZathuraPageWidget* widget, double x, double y, double* page_x, double* page_y);

/**
 * Set the rectangles of text that was just copied, drawn briefly on top of
 * the page.
 * @param widget the widget
 * @param flash list of rectangles in page coordinates, or NULL. The widget
 *   takes ownership of the list.
 */
void zathura_page_widget_set_flash(ZathuraPageWidget* widget, girara_list_t* flash);

/**
 * Get the labels the widget draws.
 * @param widget the widget
 * @return list of zathura_page_widget_label_t owned by the widget, or NULL
 */
girara_list_t* zathura_page_widget_get_labels(ZathuraPageWidget* widget);

/**
 * Whether the widget draws the flash of copied text or a copied image.
 * @param widget the widget
 * @return true if a flash is drawn
 */
bool zathura_page_widget_has_flash(ZathuraPageWidget* widget);

/**
 * Label of a position that can be jumped to
 */
typedef struct zathura_page_widget_label_s {
  zathura_rectangle_t position; /**< Where to draw the label, in page coordinates */
  char text[4];                 /**< The label */
  bool outline;                 /**< Outline bounds, e.g. for the label of an image */
  zathura_rectangle_t bounds;   /**< What the label is for, outlined if outline is set */
} zathura_page_widget_label_t;

/**
 * Set the labels to draw.
 * @param widget the widget
 * @param labels list of zathura_page_widget_label_t, or NULL. The widget takes
 *   ownership of the list.
 */
void zathura_page_widget_set_labels(ZathuraPageWidget* widget, girara_list_t* labels);

/**
 * Set the keyboard selection and caret to draw.
 * @param widget the widget
 * @param selection list of rectangles in page coordinates, or NULL. The widget
 *   takes ownership of the list.
 * @param caret bounding box of the character under the caret, or NULL to not
 *   draw a caret
 */
void zathura_page_widget_set_visual(ZathuraPageWidget* widget, girara_list_t* selection,
                                    const zathura_rectangle_t* caret);
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

/**
 * Set size request for the page widget
 *
 * @param widget the widget
 * @param width  page width
 * @param height page height
 */
void zathura_page_widget_set_size_request(ZathuraPageWidget* widget, int width, int height);

/**
 * Clear stored thumbnails
 *
 * @param widget the widget
 */
void zathura_page_widget_clear_thumbnail(ZathuraPageWidget* widget);

/* scaled-button-release signal payload */
typedef struct scaled_button_release_event_s {
  double x;              /**< x in page coordinates */
  double y;              /**< y in page coordinates */
  guint button;          /**< button number */
  GdkModifierType state; /**< modifier state */
} scaled_button_release_event_t;

#endif
