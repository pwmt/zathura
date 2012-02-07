/* See LICENSE file for license and copyright information */

#include "page_widget.h"
#include "render.h"
#include "utils.h"
#include <girara/utils.h>
#include <girara/settings.h>
#include <girara/datastructures.h>

G_DEFINE_TYPE(ZathuraPage, zathura_page_widget, GTK_TYPE_DRAWING_AREA)

typedef struct zathura_page_widget_private_s {
  zathura_page_t* page;
  zathura_t* zathura;
  cairo_surface_t* surface; /** Cairo surface */
  GStaticMutex lock; /**< Lock */
  girara_list_t* links; /**< List of links on the page */
  bool links_got; /**< True if we already tried to retrieve the list of links */
  bool draw_links; /**< True if links should be drawn */
  unsigned int link_offset; /**< Offset to the links */
  unsigned int number_of_links; /**< Offset to the links */
  girara_list_t* search_results; /** A list if there are search results that should be drawn */
  unsigned int search_current; /** The index of the current search result */
} zathura_page_widget_private_t;

#define ZATHURA_PAGE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ZATHURA_TYPE_PAGE, zathura_page_widget_private_t))

static gboolean zathura_page_widget_expose(GtkWidget* widget, GdkEventExpose* event);
static void zathura_page_widget_finalize(GObject* object);
static void zathura_page_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void zathura_page_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void zathura_page_widget_size_allocate(GtkWidget* widget, GdkRectangle* allocation);
static void redraw_rect(ZathuraPage* widget, zathura_rectangle_t* rectangle);
static void redraw_all_rects(ZathuraPage* widget, girara_list_t* rectangles);

enum properties_e
{
  PROP_0,
  PROP_PAGE,
  PROP_DRAW_LINKS,
  PROP_LINKS_OFFSET,
  PROP_LINKS_NUMBER,
  PROP_SEARCH_RESULTS,
  PROP_SEARCH_RESULTS_LENGTH,
  PROP_SEARCH_RESULTS_CURRENT
};

static void
zathura_page_widget_class_init(ZathuraPageClass* class)
{
  /* add private members */
  g_type_class_add_private(class, sizeof(zathura_page_widget_private_t));

  /* overwrite methods */
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
  widget_class->expose_event = zathura_page_widget_expose;
  widget_class->size_allocate = zathura_page_widget_size_allocate;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize = zathura_page_widget_finalize;
  object_class->set_property = zathura_page_widget_set_property;
  object_class->get_property = zathura_page_widget_get_property;

  /* add properties */
  g_object_class_install_property(object_class, PROP_PAGE,
      g_param_spec_pointer("page", "page", "the page to draw", G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(object_class, PROP_DRAW_LINKS,
      g_param_spec_boolean("draw-links", "draw-links", "Set to true if links should be drawn", FALSE, G_PARAM_WRITABLE));
  g_object_class_install_property(object_class, PROP_LINKS_OFFSET,
      g_param_spec_int("offset-links", "offset-links", "Offset for the link numbers", 0, INT_MAX, 0, G_PARAM_WRITABLE));
  g_object_class_install_property(object_class, PROP_LINKS_NUMBER,
      g_param_spec_int("number-of-links", "number-of-links", "Number of links", 0, INT_MAX, 0, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS,
      g_param_spec_pointer("search-results", "search-results", "Set to the list of search results", G_PARAM_WRITABLE));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS_CURRENT,
      g_param_spec_int("search-current", "search-current", "The current search result", -1, INT_MAX, 0, G_PARAM_WRITABLE | G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS_LENGTH,
      g_param_spec_int("search-length", "search-length", "The number of search results", -1, INT_MAX, 0, G_PARAM_READABLE));
}

static void
zathura_page_widget_init(ZathuraPage* widget)
{
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  priv->page        = NULL;
  priv->surface     = NULL;
  priv->links       = NULL;
  priv->links_got   = false;
  priv->link_offset = 0;
  g_static_mutex_init(&(priv->lock));

  /* we want mouse events */
  gtk_widget_add_events(GTK_WIDGET(widget),
    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_FOCUS_CHANGE_MASK);
  /* widgets can focus */
  gtk_widget_set_can_focus(GTK_WIDGET(widget), TRUE);
}

GtkWidget*
zathura_page_widget_new(zathura_page_t* page)
{
  g_return_val_if_fail(page != NULL, NULL);

  return g_object_new(ZATHURA_TYPE_PAGE, "page", page, NULL);
}

static void
zathura_page_widget_finalize(GObject* object)
{
  ZathuraPage* widget = ZATHURA_PAGE(object);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  if (priv->surface) {
    cairo_surface_destroy(priv->surface);
  }
  g_static_mutex_free(&(priv->lock));
  girara_list_free(priv->links);

  G_OBJECT_CLASS(zathura_page_widget_parent_class)->finalize(object);
}

static void
zathura_page_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
  ZathuraPage* pageview = ZATHURA_PAGE(object);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(pageview);

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
        priv->number_of_links = (priv->links == NULL) ? 0 : girara_list_size(priv->links);
      }

      if (priv->links_got == true && priv->links != NULL) {
        GIRARA_LIST_FOREACH(priv->links, zathura_link_t*, iter, link)
          zathura_rectangle_t rectangle = recalc_rectangle(priv->page, link->position);
          redraw_rect(pageview, &rectangle);
        GIRARA_LIST_FOREACH_END(priv->links, zathura_link_t*, iter, link);
      }
      break;
    case PROP_LINKS_OFFSET:
      priv->link_offset = g_value_get_int(value);
      break;
    case PROP_SEARCH_RESULTS:
      if (priv->search_results != NULL) {
        redraw_all_rects(pageview, priv->search_results);
        girara_list_free(priv->search_results);
      }
      priv->search_results = g_value_get_pointer(value);
      if (priv->search_results != NULL) {
        priv->draw_links = false;
        redraw_all_rects(pageview, priv->search_results);
      }
      priv->search_current = -1;
      break;
    case PROP_SEARCH_RESULTS_CURRENT: {
      g_return_if_fail(priv->search_results != NULL);
      if (priv->search_current < girara_list_size(priv->search_results)) {
        zathura_rectangle_t* rect = girara_list_nth(priv->search_results, priv->search_current);
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
        redraw_rect(pageview, &rectangle);
      }
      int val = g_value_get_int(value);
      if (val == -1) {
        priv->search_current = girara_list_size(priv->search_results);
      } else {
        priv->search_current = val;
        zathura_rectangle_t* rect = girara_list_nth(priv->search_results, priv->search_current);
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
        redraw_rect(pageview, &rectangle);
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
zathura_page_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
  ZathuraPage* pageview = ZATHURA_PAGE(object);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(pageview);

  switch (prop_id) {
    case PROP_LINKS_NUMBER:
      g_value_set_int(value, priv->number_of_links);
      break;
    case PROP_SEARCH_RESULTS_LENGTH:
      g_value_set_int(value, priv->search_results == NULL ? 0 : girara_list_size(priv->search_results));
      break;
    case PROP_SEARCH_RESULTS_CURRENT:
      g_value_set_int(value, priv->search_results == NULL ? -1 : priv->search_current);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static gboolean
zathura_page_widget_expose(GtkWidget* widget, GdkEventExpose* event)
{
  cairo_t* cairo = gdk_cairo_create(widget->window);
  if (cairo == NULL) {
    girara_error("Could not retreive cairo object");
    return FALSE;
  }

  /* set clip region */
  cairo_rectangle(cairo, event->area.x, event->area.y, event->area.width, event->area.height);
  cairo_clip(cairo);

  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
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
    g_free(font);

    /* draw links */
    if (priv->draw_links == true && priv->number_of_links != 0) {
      unsigned int link_counter = 0;
      GIRARA_LIST_FOREACH(priv->links, zathura_link_t*, iter, link)
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, link->position);

        /* draw position */
        GdkColor color = priv->zathura->ui.colors.highlight_color;
        cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
        cairo_rectangle(cairo, rectangle.x1, rectangle.y1,
            (rectangle.x2 - rectangle.x1), (rectangle.y2 - rectangle.y1));
        cairo_fill(cairo);

        /* draw text */
        cairo_set_source_rgba(cairo, 0, 0, 0, 1);
        cairo_set_font_size(cairo, 10);
        cairo_move_to(cairo, rectangle.x1 + 1, rectangle.y1 - 1);
        char* link_number = g_strdup_printf("%i", priv->link_offset + ++link_counter);
        cairo_show_text(cairo, link_number);
        g_free(link_number);
      GIRARA_LIST_FOREACH_END(priv->links, zathura_link_t*, iter, link);
    }

    /* draw search results */
    if (priv->search_results != NULL) {
      unsigned int idx = 0;
      GIRARA_LIST_FOREACH(priv->search_results, zathura_rectangle_t*, iter, rect)
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);

        /* draw position */
        GdkColor color = priv->zathura->ui.colors.highlight_color;
        if (idx == priv->search_current) {
          cairo_set_source_rgba(cairo, 0, color.green, color.blue, transparency);
        } else {
          cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
        }
        cairo_rectangle(cairo, rectangle.x1, rectangle.y1,
            (rectangle.x2 - rectangle.x1), (rectangle.y2 - rectangle.y1));
        cairo_fill(cairo);
        ++idx;
      GIRARA_LIST_FOREACH_END(priv->search_results, zathura_rectangle_t*, iter, rect);
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
zathura_page_widget_redraw_canvas(ZathuraPage* pageview)
{
  GtkWidget* widget = GTK_WIDGET(pageview);
  gtk_widget_queue_draw(widget);
}

void
zathura_page_widget_update_surface(ZathuraPage* widget, cairo_surface_t* surface)
{
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  g_static_mutex_lock(&(priv->lock));
  if (priv->surface != NULL) {
    cairo_surface_destroy(priv->surface);
  }
  priv->surface = surface;
  g_static_mutex_unlock(&(priv->lock));
  /* force a redraw here */
  zathura_page_widget_redraw_canvas(widget);
}

static void
zathura_page_widget_size_allocate(GtkWidget* widget, GdkRectangle* allocation)
{
  GTK_WIDGET_CLASS(zathura_page_widget_parent_class)->size_allocate(widget, allocation);
  zathura_page_widget_update_surface(ZATHURA_PAGE(widget), NULL);
}

static void
redraw_rect(ZathuraPage* widget, zathura_rectangle_t* rectangle)
{
   /* cause the rect to be drawn */
  GdkRectangle grect;
  grect.x = rectangle->x1;
  grect.y = rectangle->y2;
  grect.width = rectangle->x2 - rectangle->x1;
  grect.height = rectangle->y1 - rectangle->y2;
  gdk_window_invalidate_rect(GTK_WIDGET(widget)->window, &grect, TRUE);
}

static void
redraw_all_rects(ZathuraPage* widget, girara_list_t* rectangles)
{
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

  GIRARA_LIST_FOREACH(rectangles, zathura_rectangle_t*, iter, rect)
    zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
    redraw_rect(widget, &rectangle);
  GIRARA_LIST_FOREACH_END(rectangles, zathura_recantgle_t*, iter, rect);
}

zathura_link_t*
zathura_page_widget_link_get(ZathuraPage* widget, unsigned int index)
{
  g_return_val_if_fail(widget != NULL, NULL);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  g_return_val_if_fail(priv != NULL, NULL);

  if (priv->links != NULL && index >= priv->link_offset &&
      girara_list_size(priv->links) >= index - priv->link_offset) {
    return girara_list_nth(priv->links, index - priv->link_offset);
  } else {
    return NULL;
  }
}
