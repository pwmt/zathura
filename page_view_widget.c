/* See LICENSE file for license and copyright information */

#include "page_view_widget.h"
#include "render.h"
#include "utils.h"
#include <girara/utils.h>
#include <girara/settings.h>
#include <girara/datastructures.h>

G_DEFINE_TYPE(ZathuraPageView, zathura_page_view, GTK_TYPE_DRAWING_AREA)

typedef struct zathura_page_view_private_s {
  zathura_page_t* page;
  zathura_t* zathura;
  cairo_surface_t* surface; /** Cairo surface */
  GStaticMutex lock; /**< Lock */
  girara_list_t* links; /**< List of links on the page */
  bool links_got; /**< True if we already tried to retrieve the list of links */
  bool draw_links; /**< True if links should be drawn */
} zathura_page_view_private_t;

#define ZATHURA_PAGE_VIEW_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ZATHURA_TYPE_PAGE_VIEW, zathura_page_view_private_t))

static gboolean zathura_page_view_expose(GtkWidget* widget, GdkEventExpose* event);
static void zathura_page_view_finalize(GObject* object);
static void zathura_page_view_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void zathura_page_view_size_allocate(GtkWidget* widget, GdkRectangle* allocation);
static void redraw_rect(ZathuraPageView* widget, zathura_rectangle_t* rectangle);

enum properties_e
{
  PROP_0,
  PROP_PAGE,
  PROP_DRAW_LINKS,
};

static void
zathura_page_view_class_init(ZathuraPageViewClass* class)
{
  /* add private members */
  g_type_class_add_private(class, sizeof(zathura_page_view_private_t));

  /* overwrite methods */
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
  widget_class->expose_event = zathura_page_view_expose;
  widget_class->size_allocate = zathura_page_view_size_allocate;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize = zathura_page_view_finalize;
  object_class->set_property = zathura_page_view_set_property;

  /* add properties */
  g_object_class_install_property(object_class, PROP_PAGE,
      g_param_spec_pointer("page", "page", "the page to draw", G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(object_class, PROP_DRAW_LINKS,
      g_param_spec_boolean("draw-links", "draw-links", "Set to true if links should be drawn", FALSE, G_PARAM_WRITABLE));
}

static void
zathura_page_view_init(ZathuraPageView* widget)
{
  zathura_page_view_private_t* priv = ZATHURA_PAGE_VIEW_GET_PRIVATE(widget);
  priv->page       = NULL;
  priv->surface    = NULL;
  priv->links      = NULL;
  priv->links_got  = false;
  g_static_mutex_init(&(priv->lock));

  /* we want mouse events */
  gtk_widget_add_events(GTK_WIDGET(widget),
    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_FOCUS_CHANGE_MASK);
  /* widgets can focus */
  gtk_widget_set_can_focus(GTK_WIDGET(widget), TRUE);
}

GtkWidget*
zathura_page_view_new(zathura_page_t* page)
{
  g_return_val_if_fail(page != NULL, NULL);

  return g_object_new(ZATHURA_TYPE_PAGE_VIEW, "page", page, NULL);
}

static void
zathura_page_view_finalize(GObject* object)
{
  ZathuraPageView* widget = ZATHURA_PAGE_VIEW(object);
  zathura_page_view_private_t* priv = ZATHURA_PAGE_VIEW_GET_PRIVATE(widget);
  if (priv->surface) {
    cairo_surface_destroy(priv->surface);
  }
  g_static_mutex_free(&(priv->lock));

  G_OBJECT_CLASS(zathura_page_view_parent_class)->finalize(object);
}

static void
zathura_page_view_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
  ZathuraPageView* pageview = ZATHURA_PAGE_VIEW(object);
  zathura_page_view_private_t* priv = ZATHURA_PAGE_VIEW_GET_PRIVATE(pageview);

  switch (prop_id) {
    case PROP_PAGE:
      priv->page = g_value_get_pointer(value);
      priv->zathura = priv->page->document->zathura;
      break;
    case PROP_DRAW_LINKS:
      priv->draw_links = g_value_get_boolean(value);
      /* get links */
      if (priv->draw_links == true && priv->links_got == false) {
        priv->links = zathura_page_links_get(priv->page);
        priv->links_got = true;
      }

      if (priv->links_got == true && priv->links != NULL) {
        GIRARA_LIST_FOREACH(priv->links, zathura_link_t*, iter, link)
          zathura_rectangle_t rectangle = recalc_rectangle(priv->page, link->position);
          redraw_rect(pageview, &rectangle);
        GIRARA_LIST_FOREACH_END(priv->links, zathura_link_t*, iter, link);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static gboolean
zathura_page_view_expose(GtkWidget* widget, GdkEventExpose* event)
{
  cairo_t* cairo = gdk_cairo_create(widget->window);
  if (cairo == NULL) {
    girara_error("Could not retreive cairo object");
    return FALSE;
  }

  /* set clip region */
  cairo_rectangle(cairo, event->area.x, event->area.y, event->area.width, event->area.height);
  cairo_clip(cairo);

  zathura_page_view_private_t* priv = ZATHURA_PAGE_VIEW_GET_PRIVATE(widget);
  g_static_mutex_lock(&(priv->lock));
  if (priv->surface != NULL) {
    cairo_save(cairo);

    unsigned int page_height = widget->allocation.height;
    unsigned int page_width = widget->allocation.width;
    if (priv->page->document->rotate % 180) {
      page_height = widget->allocation.width;
      page_width = widget->allocation.height;
    }

    switch (priv->page->document->rotate) {
      case 90:
        cairo_translate(cairo, page_width, 0);
        break;
      case 180:
        cairo_translate(cairo, page_width, page_height);
        break;
      case 270:
        cairo_translate(cairo, 0, page_height);
        break;
    }

    if (priv->page->document->rotate != 0) {
      cairo_rotate(cairo, priv->page->document->rotate * G_PI / 180.0);
    }

    cairo_set_source_surface(cairo, priv->surface, 0, 0);
    cairo_paint(cairo);
    cairo_restore(cairo);

    /* draw rectangles */
    char* font = NULL;
    girara_setting_get(priv->zathura->ui.session, "font", &font);

    float transparency = 0.5;
    girara_setting_get(priv->zathura->ui.session, "highlight-transparency", &transparency);

    if (font != NULL) {
      cairo_select_font_face(cairo, font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    }

    /* draw links */
    if (priv->draw_links == true) {
      GIRARA_LIST_FOREACH(priv->links, zathura_link_t*, iter, link)
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, link->position);

        /* draw text */
        cairo_set_font_size(cairo, 10);
        cairo_move_to(cairo, rectangle.x1 + 1, rectangle.y1 - 1);
        char* link_number = g_strdup_printf("%i", 0);
        cairo_show_text(cairo, link_number);
        g_free(link_number);

        /* draw position */
        GdkColor color = priv->zathura->ui.colors.highlight_color;
        cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
        cairo_rectangle(cairo, rectangle.x1, rectangle.y1,
            (rectangle.x2 - rectangle.x1), (rectangle.y2 - rectangle.y1));
        cairo_fill(cairo);
      GIRARA_LIST_FOREACH_END(priv->links, zathura_link_t*, iter, link);
    }
  } else {
    /* set background color */
    cairo_set_source_rgb(cairo, 255, 255, 255);
    cairo_rectangle(cairo, 0, 0, widget->allocation.width, widget->allocation.height);
    cairo_fill(cairo);

    bool render_loading = true;
    girara_setting_get(priv->zathura->ui.session, "render-loading", &render_loading);

    /* write text */
    if (render_loading == true) {
      cairo_set_source_rgb(cairo, 0, 0, 0);
      const char* text = "Loading...";
      cairo_select_font_face(cairo, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
      cairo_set_font_size(cairo, 16.0);
      cairo_text_extents_t extents;
      cairo_text_extents(cairo, text, &extents);
      double x = widget->allocation.width * 0.5 - (extents.width * 0.5 + extents.x_bearing);
      double y = widget->allocation.height * 0.5 - (extents.height * 0.5 + extents.y_bearing);
      cairo_move_to(cairo, x, y);
      cairo_show_text(cairo, text);
    }

    /* render real page */
    render_page(priv->zathura->sync.render_thread, priv->page);
  }
  cairo_destroy(cairo);

  g_static_mutex_unlock(&(priv->lock));
  return FALSE;
}

static void
zathura_page_view_redraw_canvas(ZathuraPageView* pageview)
{
  GtkWidget* widget = GTK_WIDGET(pageview);
  /* redraw the cairo canvas completely by exposing it */
  /* GdkRegion* region = gdk_drawable_get_clip_region(widget->window);
  gdk_window_invalidate_region(widget->window, region, TRUE);
  gdk_window_process_updates(widget->window, TRUE);
  gdk_region_destroy(region); */
  gtk_widget_queue_draw(widget);
}

void
zathura_page_view_update_surface(ZathuraPageView* widget, cairo_surface_t* surface)
{
  zathura_page_view_private_t* priv = ZATHURA_PAGE_VIEW_GET_PRIVATE(widget);
  g_static_mutex_lock(&(priv->lock));
  if (priv->surface != NULL) {
    cairo_surface_destroy(priv->surface);
  }
  priv->surface = surface;
  g_static_mutex_unlock(&(priv->lock));
  /* force a redraw here */
  zathura_page_view_redraw_canvas(widget);
}

static void
zathura_page_view_size_allocate(GtkWidget* widget, GdkRectangle* allocation)
{
  GTK_WIDGET_CLASS(zathura_page_view_parent_class)->size_allocate(widget, allocation);
  zathura_page_view_update_surface(ZATHURA_PAGE_VIEW(widget), NULL);
}

static void
redraw_rect(ZathuraPageView* widget, zathura_rectangle_t* rectangle)
{
   /* cause the rect to be drawn */
  GdkRectangle grect;
  grect.x = rectangle->x1;
  grect.y = rectangle->y2;
  grect.width = rectangle->x2 - rectangle->x1;
  grect.height = rectangle->y1 - rectangle->y2;
  gdk_window_invalidate_rect(GTK_WIDGET(widget)->window, &grect, TRUE);
}

zathura_link_t*
zathura_page_view_link_get(ZathuraPageView* widget, unsigned int index)
{
  g_return_val_if_fail(widget != NULL, NULL);
  zathura_page_view_private_t* priv = ZATHURA_PAGE_VIEW_GET_PRIVATE(widget);
  g_return_val_if_fail(priv != NULL, NULL);

  return girara_list_nth(priv->links, index);
}
