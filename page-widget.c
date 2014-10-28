/* See LICENSE file for license and copyright information */

#include <girara/utils.h>
#include <girara/settings.h>
#include <girara/datastructures.h>
#include <girara/session.h>
#include <string.h>
#include <glib/gi18n.h>
#include <math.h>

#include "links.h"
#include "page-widget.h"
#include "page.h"
#include "render.h"
#include "utils.h"
#include "shortcuts.h"
#include "zathura.h"

G_DEFINE_TYPE(ZathuraPage, zathura_page_widget, GTK_TYPE_DRAWING_AREA)

typedef struct zathura_page_widget_private_s {
  zathura_page_t* page; /**< Page object */
  zathura_t* zathura; /**< Zathura object */
  cairo_surface_t* surface; /**< Cairo surface */
  cairo_surface_t* thumbnail; /**< Cairo surface */
  ZathuraRenderRequest* render_request; /* Request object */
  bool cached; /**< Cached state */

  struct {
    girara_list_t* list; /**< List of links on the page */
    bool retrieved; /**< True if we already tried to retrieve the list of links */
    bool draw; /**< True if links should be drawn */
    unsigned int offset; /**< Offset to the links */
    unsigned int n; /**< Number */
  } links;

  struct {
    girara_list_t* list; /**< A list if there are search results that should be drawn */
    int current; /**< The index of the current search result */
    bool draw; /**< Draw search results */
  } search;

  struct {
    girara_list_t* list; /**< List of images on the page */
    bool retrieved; /**< True if we already tried to retrieve the list of images */
    zathura_image_t* current; /**< Image data of selected image */
  } images;

  struct {
    zathura_rectangle_t selection; /**< Region selected with the mouse */
    struct {
      int x; /**< X coordinate */
      int y; /**< Y coordinate */
    } selection_basepoint;
    bool over_link;
  } mouse;
} zathura_page_widget_private_t;

#define ZATHURA_PAGE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZATHURA_TYPE_PAGE, \
                               zathura_page_widget_private_t))

static gboolean zathura_page_widget_draw(GtkWidget* widget, cairo_t* cairo);
static void zathura_page_widget_finalize(GObject* object);
static void zathura_page_widget_dispose(GObject* object);
static void zathura_page_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void zathura_page_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void zathura_page_widget_size_allocate(GtkWidget* widget, GdkRectangle* allocation);
static void redraw_rect(ZathuraPage* widget, zathura_rectangle_t* rectangle);
static void redraw_all_rects(ZathuraPage* widget, girara_list_t* rectangles);
static void zathura_page_widget_popup_menu(GtkWidget* widget, GdkEventButton* event);
static gboolean cb_zathura_page_widget_button_press_event(GtkWidget* widget, GdkEventButton* button);
static gboolean cb_zathura_page_widget_button_release_event(GtkWidget* widget, GdkEventButton* button);
static gboolean cb_zathura_page_widget_motion_notify(GtkWidget* widget, GdkEventMotion* event);
static gboolean cb_zathura_page_widget_leave_notify(GtkWidget* widget, GdkEventCrossing* event);
static gboolean cb_zathura_page_widget_popup_menu(GtkWidget* widget);
static void cb_menu_image_copy(GtkMenuItem* item, ZathuraPage* page);
static void cb_menu_image_save(GtkMenuItem* item, ZathuraPage* page);
static void cb_update_surface(ZathuraRenderRequest* request, cairo_surface_t* surface, void* data);
static void cb_cache_added(ZathuraRenderRequest* request, void* data);
static void cb_cache_invalidated(ZathuraRenderRequest* request, void* data);
static bool surface_small_enough(cairo_surface_t* surface, size_t max_size, cairo_surface_t* old);
static cairo_surface_t *draw_thumbnail_image(cairo_surface_t* surface, size_t max_size);

enum properties_e {
  PROP_0,
  PROP_PAGE,
  PROP_ZATHURA,
  PROP_DRAW_LINKS,
  PROP_LINKS_OFFSET,
  PROP_LINKS_NUMBER,
  PROP_SEARCH_RESULTS,
  PROP_SEARCH_RESULTS_LENGTH,
  PROP_SEARCH_RESULTS_CURRENT,
  PROP_DRAW_SEARCH_RESULTS,
  PROP_LAST_VIEW,
};

enum {
  TEXT_SELECTED,
  IMAGE_SELECTED,
  BUTTON_RELEASE,
  ENTER_LINK,
  LEAVE_LINK,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
zathura_page_widget_class_init(ZathuraPageClass* class)
{
  /* add private members */
  g_type_class_add_private(class, sizeof(zathura_page_widget_private_t));

  /* overwrite methods */
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
  widget_class->draw                 = zathura_page_widget_draw;
  widget_class->size_allocate        = zathura_page_widget_size_allocate;
  widget_class->button_press_event   = cb_zathura_page_widget_button_press_event;
  widget_class->button_release_event = cb_zathura_page_widget_button_release_event;
  widget_class->motion_notify_event  = cb_zathura_page_widget_motion_notify;
  widget_class->leave_notify_event   = cb_zathura_page_widget_leave_notify;
  widget_class->popup_menu           = cb_zathura_page_widget_popup_menu;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->dispose      = zathura_page_widget_dispose;
  object_class->finalize     = zathura_page_widget_finalize;
  object_class->set_property = zathura_page_widget_set_property;
  object_class->get_property = zathura_page_widget_get_property;

  /* add properties */
  g_object_class_install_property(object_class, PROP_PAGE,
                                  g_param_spec_pointer("page", "page", "the page to draw", G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_ZATHURA,
                                  g_param_spec_pointer("zathura", "zathura", "the zathura instance", G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_DRAW_LINKS,
                                  g_param_spec_boolean("draw-links", "draw-links", "Set to true if links should be drawn", FALSE, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_LINKS_OFFSET,
                                  g_param_spec_int("offset-links", "offset-links", "Offset for the link numbers", 0, INT_MAX, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_LINKS_NUMBER,
                                  g_param_spec_int("number-of-links", "number-of-links", "Number of links", 0, INT_MAX, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS,
                                  g_param_spec_pointer("search-results", "search-results", "Set to the list of search results", G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS_CURRENT,
                                  g_param_spec_int("search-current", "search-current", "The current search result", -1, INT_MAX, 0, G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS_LENGTH,
                                  g_param_spec_int("search-length", "search-length", "The number of search results", -1, INT_MAX, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_DRAW_SEARCH_RESULTS,
                                  g_param_spec_boolean("draw-search-results", "draw-search-results", "Set to true if search results should be drawn", FALSE, G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /* add signals */
  signals[TEXT_SELECTED] = g_signal_new("text-selected",
      ZATHURA_TYPE_PAGE,
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      1,
      G_TYPE_STRING);

  signals[IMAGE_SELECTED] = g_signal_new("image-selected",
      ZATHURA_TYPE_PAGE,
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      1,
      G_TYPE_OBJECT);

  signals[ENTER_LINK] = g_signal_new("enter-link",
      ZATHURA_TYPE_PAGE,
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      0);

  signals[LEAVE_LINK] = g_signal_new("leave-link",
      ZATHURA_TYPE_PAGE,
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      0);

  signals[BUTTON_RELEASE] = g_signal_new("scaled-button-release",
      ZATHURA_TYPE_PAGE,
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      1,
      G_TYPE_POINTER);
}

static void
zathura_page_widget_init(ZathuraPage* widget)
{
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  priv->page             = NULL;
  priv->surface          = NULL;
  priv->thumbnail        = NULL;
  priv->render_request   = NULL;
  priv->cached           = false;

  priv->links.list      = NULL;
  priv->links.retrieved = false;
  priv->links.draw      = false;
  priv->links.offset    = 0;
  priv->links.n         = 0;

  priv->search.list    = NULL;
  priv->search.current = INT_MAX;
  priv->search.draw    = false;

  priv->images.list      = NULL;
  priv->images.retrieved = false;
  priv->images.current   = NULL;

  priv->mouse.selection.x1          = -1;
  priv->mouse.selection.y1          = -1;
  priv->mouse.selection_basepoint.x = -1;
  priv->mouse.selection_basepoint.y = -1;

  const unsigned int event_mask = GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK;
  gtk_widget_add_events(GTK_WIDGET(widget), event_mask);
}

GtkWidget*
zathura_page_widget_new(zathura_t* zathura, zathura_page_t* page)
{
  g_return_val_if_fail(page != NULL, NULL);

  GObject* ret = g_object_new(ZATHURA_TYPE_PAGE, "page", page, "zathura", zathura, NULL);
  if (ret == NULL) {
    return NULL;
  }

  ZathuraPage* widget = ZATHURA_PAGE(ret);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  priv->render_request = zathura_render_request_new(zathura->sync.render_thread, page);
  g_signal_connect_object(priv->render_request, "completed",
      G_CALLBACK(cb_update_surface), widget, 0);
  g_signal_connect_object(priv->render_request, "cache-added",
      G_CALLBACK(cb_cache_added), widget, 0);
  g_signal_connect_object(priv->render_request, "cache-invalidated",
      G_CALLBACK(cb_cache_invalidated), widget, 0);

  return GTK_WIDGET(ret);
}

static void
zathura_page_widget_dispose(GObject* object)
{
  ZathuraPage* widget = ZATHURA_PAGE(object);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

  g_clear_object(&priv->render_request);

  G_OBJECT_CLASS(zathura_page_widget_parent_class)->dispose(object);
}

static void
zathura_page_widget_finalize(GObject* object)
{
  ZathuraPage* widget = ZATHURA_PAGE(object);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

  if (priv->surface != NULL) {
    cairo_surface_destroy(priv->surface);
  }

  if (priv->thumbnail != NULL) {
    cairo_surface_destroy(priv->thumbnail);
  }

  if (priv->search.list != NULL) {
    girara_list_free(priv->search.list);
  }

  if (priv->links.list != NULL) {
    girara_list_free(priv->links.list);
  }

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
      break;
    case PROP_ZATHURA:
      priv->zathura = g_value_get_pointer(value);
      break;
    case PROP_DRAW_LINKS:
      priv->links.draw = g_value_get_boolean(value);
      /* get links */
      if (priv->links.draw == true && priv->links.retrieved == false) {
        priv->links.list      = zathura_page_links_get(priv->page, NULL);
        priv->links.retrieved = true;
        priv->links.n         = (priv->links.list == NULL) ? 0 : girara_list_size(priv->links.list);
      }

      if (priv->links.retrieved == true && priv->links.list != NULL) {
        GIRARA_LIST_FOREACH(priv->links.list, zathura_link_t*, iter, link)
        if (link != NULL) {
          zathura_rectangle_t rectangle = recalc_rectangle(priv->page, zathura_link_get_position(link));
          redraw_rect(pageview, &rectangle);
        }
        GIRARA_LIST_FOREACH_END(priv->links.list, zathura_link_t*, iter, link);
      }
      break;
    case PROP_LINKS_OFFSET:
      priv->links.offset = g_value_get_int(value);
      break;
    case PROP_SEARCH_RESULTS:
      if (priv->search.list != NULL && priv->search.draw) {
        redraw_all_rects(pageview, priv->search.list);
        girara_list_free(priv->search.list);
      }
      priv->search.list = g_value_get_pointer(value);
      if (priv->search.list != NULL && priv->search.draw) {
        priv->links.draw = false;
        redraw_all_rects(pageview, priv->search.list);
      }
      priv->search.current = -1;
      break;
    case PROP_SEARCH_RESULTS_CURRENT: {
      g_return_if_fail(priv->search.list != NULL);
      if (priv->search.current >= 0 && priv->search.current < (signed) girara_list_size(priv->search.list)) {
        zathura_rectangle_t* rect = girara_list_nth(priv->search.list, priv->search.current);
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
        redraw_rect(pageview, &rectangle);
      }
      int val = g_value_get_int(value);
      if (val < 0) {
        priv->search.current = girara_list_size(priv->search.list);
      } else {
        priv->search.current = val;
        if (priv->search.draw == true && val >= 0 && val < (signed) girara_list_size(priv->search.list)) {
          zathura_rectangle_t* rect = girara_list_nth(priv->search.list, priv->search.current);
          zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
          redraw_rect(pageview, &rectangle);
        }
      }
      break;
    }
    case PROP_DRAW_SEARCH_RESULTS:
      priv->search.draw = g_value_get_boolean(value);

      /*
       * we do the following instead of only redrawing the rectangles of the
       * search results in order to avoid the rectangular margins that appear
       * around the search terms after their highlighted rectangular areas are
       * redrawn without highlighting.
       */

      if (priv->search.list != NULL && zathura_page_get_visibility(priv->page)) {
        gtk_widget_queue_draw(GTK_WIDGET(object));
      }
      break;
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
      g_value_set_int(value, priv->links.n);
      break;
    case PROP_SEARCH_RESULTS_LENGTH:
      g_value_set_int(value, priv->search.list == NULL ? 0 : girara_list_size(priv->search.list));
      break;
    case PROP_SEARCH_RESULTS_CURRENT:
      g_value_set_int(value, priv->search.list == NULL ? -1 : priv->search.current);
      break;
    case PROP_SEARCH_RESULTS:
      g_value_set_pointer(value, priv->search.list);
      break;
    case PROP_DRAW_SEARCH_RESULTS:
      g_value_set_boolean(value, priv->search.draw);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static gboolean
zathura_page_widget_draw(GtkWidget* widget, cairo_t* cairo)
{
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

  zathura_document_t* document   = zathura_page_get_document(priv->page);
  const unsigned int page_height = gtk_widget_get_allocated_height(widget);
  const unsigned int page_width  = gtk_widget_get_allocated_width(widget);

  if (priv->surface != NULL || priv->thumbnail != NULL) {
    cairo_save(cairo);

    unsigned int rotation = zathura_document_get_rotation(document);
    switch (rotation) {
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

    if (rotation != 0) {
      cairo_rotate(cairo, rotation * G_PI / 180.0);
    }

    if (priv->surface != NULL) {
      cairo_set_source_surface(cairo, priv->surface, 0, 0);
      cairo_paint(cairo);
      cairo_restore(cairo);
    } else {
      const unsigned int height = cairo_image_surface_get_height(priv->thumbnail);
      const unsigned int width = cairo_image_surface_get_width(priv->thumbnail);
      const unsigned int pheight = (rotation % 180 ? page_width : page_height);
      const unsigned int pwidth = (rotation % 180 ? page_height : page_width);

      cairo_scale(cairo, pwidth / (double)width, pheight / (double)height);
      cairo_set_source_surface(cairo, priv->thumbnail, 0, 0);
      cairo_pattern_set_extend(cairo_get_source(cairo), CAIRO_EXTEND_PAD);
      if (pwidth < width || pheight < height) {
        /* pixman bilinear downscaling is slow */
        cairo_pattern_set_filter(cairo_get_source(cairo), CAIRO_FILTER_FAST);
      }
      cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
      cairo_paint(cairo);
      cairo_restore(cairo);
      /* All but the last jobs requested here are aborted during zooming.
       * Processing and aborting smaller jobs first improves responsiveness. */
      const gint64 penalty = pwidth * pheight;
      zathura_render_request(priv->render_request, g_get_real_time() + penalty);
      return FALSE;
    }

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
    if (priv->links.draw == true && priv->links.n != 0) {
      unsigned int link_counter = 0;
      GIRARA_LIST_FOREACH(priv->links.list, zathura_link_t*, iter, link)
      if (link != NULL) {
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, zathura_link_get_position(link));

        /* draw position */
        const GdkRGBA color = priv->zathura->ui.colors.highlight_color;
        cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
        cairo_rectangle(cairo, rectangle.x1, rectangle.y1,
                        (rectangle.x2 - rectangle.x1), (rectangle.y2 - rectangle.y1));
        cairo_fill(cairo);

        /* draw text */
        cairo_set_source_rgba(cairo, 0, 0, 0, 1);
        cairo_set_font_size(cairo, 10);
        cairo_move_to(cairo, rectangle.x1 + 1, rectangle.y2 - 1);
        char* link_number = g_strdup_printf("%i", priv->links.offset + ++link_counter);
        cairo_show_text(cairo, link_number);
        g_free(link_number);
      }
      GIRARA_LIST_FOREACH_END(priv->links.list, zathura_link_t*, iter, link);
    }

    /* draw search results */
    if (priv->search.list != NULL && priv->search.draw == true) {
      int idx = 0;
      GIRARA_LIST_FOREACH(priv->search.list, zathura_rectangle_t*, iter, rect)
      zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);

      /* draw position */
      if (idx == priv->search.current) {
        const GdkRGBA color = priv->zathura->ui.colors.highlight_color_active;
        cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
      } else {
        const GdkRGBA color = priv->zathura->ui.colors.highlight_color;
        cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
      }
      cairo_rectangle(cairo, rectangle.x1, rectangle.y1,
                      (rectangle.x2 - rectangle.x1), (rectangle.y2 - rectangle.y1));
      cairo_fill(cairo);
      ++idx;
      GIRARA_LIST_FOREACH_END(priv->search.list, zathura_rectangle_t*, iter, rect);
    }
    /* draw selection */
    if (priv->mouse.selection.y2 != -1 && priv->mouse.selection.x2 != -1) {
      const GdkRGBA color = priv->zathura->ui.colors.highlight_color;
      cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
      cairo_rectangle(cairo, priv->mouse.selection.x1, priv->mouse.selection.y1,
                      (priv->mouse.selection.x2 - priv->mouse.selection.x1), (priv->mouse.selection.y2 - priv->mouse.selection.y1));
      cairo_fill(cairo);
    }
  } else {
    /* set background color */
    if (zathura_renderer_recolor_enabled(priv->zathura->sync.render_thread) == true) {
      GdkRGBA color;
      zathura_renderer_get_recolor_colors(priv->zathura->sync.render_thread, &color, NULL);
      cairo_set_source_rgb(cairo, color.red, color.green, color.blue);
    } else {
      const GdkRGBA color = priv->zathura->ui.colors.render_loading_bg;
      cairo_set_source_rgb(cairo, color.red, color.green, color.blue);
    }
    cairo_rectangle(cairo, 0, 0, page_width, page_height);
    cairo_fill(cairo);

    bool render_loading = true;
    girara_setting_get(priv->zathura->ui.session, "render-loading", &render_loading);

    /* write text */
    if (render_loading == true) {
      if (zathura_renderer_recolor_enabled(priv->zathura->sync.render_thread) == true) {
        GdkRGBA color;
        zathura_renderer_get_recolor_colors(priv->zathura->sync.render_thread, NULL, &color);
        cairo_set_source_rgb(cairo, color.red, color.green, color.blue);
      } else {
        const GdkRGBA color = priv->zathura->ui.colors.render_loading_fg;
        cairo_set_source_rgb(cairo, color.red, color.green, color.blue);
      }

      const char* text = _("Loading...");
      cairo_select_font_face(cairo, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
      cairo_set_font_size(cairo, 16.0);
      cairo_text_extents_t extents;
      cairo_text_extents(cairo, text, &extents);
      double x = page_width * 0.5 - (extents.width * 0.5 + extents.x_bearing);
      double y = page_height * 0.5 - (extents.height * 0.5 + extents.y_bearing);
      cairo_move_to(cairo, x, y);
      cairo_show_text(cairo, text);
    }

    /* render real page */
    zathura_render_request(priv->render_request, g_get_real_time());
  }
  return FALSE;
}

static void
zathura_page_widget_redraw_canvas(ZathuraPage* pageview)
{
  GtkWidget* widget = GTK_WIDGET(pageview);
  gtk_widget_queue_draw(widget);
}

/* smaller than max to be replaced by actual renders */
#define THUMBNAIL_INITIAL_SCALE 0.5
/* small enough to make bilinear downscaling fast */
#define THUMBNAIL_MAX_SCALE 0.5

static bool
surface_small_enough(cairo_surface_t* surface, size_t max_size, cairo_surface_t* old)
{
  if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE)
    return true;

  const unsigned int width = cairo_image_surface_get_width(surface);
  const unsigned int height = cairo_image_surface_get_height(surface);
  const size_t new_size = width * height;
  if (new_size > max_size) {
    return false;
  }

  if (old != NULL) {
    const unsigned int width_old = cairo_image_surface_get_width(old);
    const unsigned int height_old = cairo_image_surface_get_height(old);
    const size_t old_size = width_old * height_old;
    if (new_size < old_size && new_size >= old_size * THUMBNAIL_MAX_SCALE * THUMBNAIL_MAX_SCALE) {
      return false;
    }
  }

  return true;
}

static cairo_surface_t *
draw_thumbnail_image(cairo_surface_t* surface, size_t max_size)
{
  unsigned int width = cairo_image_surface_get_width(surface);
  unsigned int height = cairo_image_surface_get_height(surface);
  double scale = sqrt((double)max_size / (width * height)) * THUMBNAIL_INITIAL_SCALE;
  if (scale > THUMBNAIL_MAX_SCALE) {
    scale = THUMBNAIL_MAX_SCALE;
  }
  width = width * scale;
  height = height * scale;

  cairo_surface_t *thumbnail;
  thumbnail = cairo_surface_create_similar(surface, CAIRO_CONTENT_COLOR, width, height);
  if (thumbnail == NULL) {
    return NULL;
  }
  cairo_t *cr = cairo_create(thumbnail);
  if (cr == NULL) {
    cairo_surface_destroy(thumbnail);
    return NULL;
  }

  cairo_scale(cr, scale, scale);
  cairo_set_source_surface(cr, surface, 0, 0);
  cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_destroy(cr);

  return thumbnail;
}

void
zathura_page_widget_update_surface(ZathuraPage* widget, cairo_surface_t* surface, bool keep_thumbnail)
{
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  int thumbnail_size = 0;
  girara_setting_get(priv->zathura->ui.session, "page-thumbnail-size", &thumbnail_size);
  if (thumbnail_size <= 0) {
    thumbnail_size = ZATHURA_PAGE_THUMBNAIL_DEFAULT_SIZE;
  }
  bool new_render = (priv->surface == NULL && priv->thumbnail == NULL);

  if (priv->surface != NULL) {
    cairo_surface_destroy(priv->surface);
    priv->surface = NULL;
  }
  if (surface != NULL) {
    priv->surface = surface;
    cairo_surface_reference(surface);

    if (surface_small_enough(surface, thumbnail_size, priv->thumbnail)) {
      if (priv->thumbnail != NULL) {
        cairo_surface_destroy(priv->thumbnail);
      }
      priv->thumbnail = surface;
      cairo_surface_reference(surface);
    } else if (new_render) {
      priv->thumbnail = draw_thumbnail_image(surface, thumbnail_size);
    }
  } else if (!keep_thumbnail && priv->thumbnail != NULL) {
    cairo_surface_destroy(priv->thumbnail);
    priv->thumbnail = NULL;
  }
  /* force a redraw here */
  if (priv->surface != NULL) {
    zathura_page_widget_redraw_canvas(widget);
  }
}

static void
cb_update_surface(ZathuraRenderRequest* UNUSED(request),
    cairo_surface_t* surface, void* data)
{
  ZathuraPage* widget = data;
  g_return_if_fail(ZATHURA_IS_PAGE(widget));
  zathura_page_widget_update_surface(widget, surface, false);
}

static void
cb_cache_added(ZathuraRenderRequest* UNUSED(request), void* data)
{
  ZathuraPage* widget = data;
  g_return_if_fail(ZATHURA_IS_PAGE(widget));

  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  priv->cached = true;
}

static void
cb_cache_invalidated(ZathuraRenderRequest* UNUSED(request), void* data)
{
  ZathuraPage* widget = data;
  g_return_if_fail(ZATHURA_IS_PAGE(widget));

  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  if (zathura_page_widget_have_surface(widget) == true &&
      priv->cached == true &&
      zathura_page_get_visibility(priv->page) == false) {
    /* The page was in the cache but got removed and is invisible, so get rid of
     * the surface. */
    zathura_page_widget_update_surface(widget, NULL, false);
  }
  priv->cached = false;
}

static void
zathura_page_widget_size_allocate(GtkWidget* widget, GdkRectangle* allocation)
{
  GTK_WIDGET_CLASS(zathura_page_widget_parent_class)->size_allocate(widget, allocation);

  ZathuraPage* page = ZATHURA_PAGE(widget);
  zathura_page_widget_abort_render_request(page);
  zathura_page_widget_update_surface(page, NULL, true);
}

static void
redraw_rect(ZathuraPage* widget, zathura_rectangle_t* rectangle)
{
  /* cause the rect to be drawn */
  GdkRectangle grect;
  grect.x = rectangle->x1;
  grect.y = rectangle->y1;
  grect.width  = (rectangle->x2 + 1) - rectangle->x1;
  grect.height = (rectangle->y2 + 1) - rectangle->y1;
  gtk_widget_queue_draw_area(GTK_WIDGET(widget), grect.x, grect.y, grect.width, grect.height);
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

  if (priv->links.list != NULL && index >= priv->links.offset &&
      girara_list_size(priv->links.list) > index - priv->links.offset) {
    return girara_list_nth(priv->links.list, index - priv->links.offset);
  } else {
    return NULL;
  }
}

static gboolean
cb_zathura_page_widget_button_press_event(GtkWidget* widget, GdkEventButton* button)
{
  g_return_val_if_fail(widget != NULL, false);
  g_return_val_if_fail(button != NULL, false);

  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

  if (girara_callback_view_button_press_event(widget, button, priv->zathura->ui.session) == true) {
    return true;
  }

  if (button->button == 1) { /* left click */
    if (button->type == GDK_BUTTON_PRESS) {
      /* start the selection */
      priv->mouse.selection_basepoint.x = button->x;
      priv->mouse.selection_basepoint.y = button->y;

      priv->mouse.selection.x1 = button->x;
      priv->mouse.selection.y1 = button->y;
      priv->mouse.selection.x2 = button->x;
      priv->mouse.selection.y2 = button->y;
    } else if (button->type == GDK_2BUTTON_PRESS || button->type == GDK_3BUTTON_PRESS) {
      /* abort the selection */
      priv->mouse.selection_basepoint.x = -1;
      priv->mouse.selection_basepoint.y = -1;

      priv->mouse.selection.x1 = -1;
      priv->mouse.selection.y1 = -1;
      priv->mouse.selection.x2 = -1;
      priv->mouse.selection.y2 = -1;
    }

    return true;
  } else if (button->button == 3) { /* right click */
    zathura_page_widget_popup_menu(widget, button);
    return true;
  }

  return false;
}

static gboolean
cb_zathura_page_widget_button_release_event(GtkWidget* widget, GdkEventButton* button)
{
  g_return_val_if_fail(widget != NULL, false);
  g_return_val_if_fail(button != NULL, false);

  if (button->type != GDK_BUTTON_RELEASE) {
    return false;
  }

  const int oldx = button->x;
  const int oldy = button->y;

  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  zathura_document_t* document        = zathura_page_get_document(priv->page);

  const double scale = zathura_document_get_scale(document);

  button->x /= scale;
  button->y /= scale;

  g_signal_emit(ZATHURA_PAGE(widget), signals[BUTTON_RELEASE], 0, button);

  button->x = oldx;
  button->y = oldy;

  if (button->button != 1) {
    return false;
  }

  if (priv->mouse.selection.y2 == -1 && priv->mouse.selection.x2 == -1 ) {
    /* simple single click */
    /* get links */
    if (priv->links.retrieved == false) {
      priv->links.list      = zathura_page_links_get(priv->page, NULL);
      priv->links.retrieved = true;
      priv->links.n         = (priv->links.list == NULL) ? 0 : girara_list_size(priv->links.list);
    }

    if (priv->links.list != NULL && priv->links.n > 0) {
      GIRARA_LIST_FOREACH(priv->links.list, zathura_link_t*, iter, link)
      zathura_rectangle_t rect = recalc_rectangle(priv->page, zathura_link_get_position(link));
      if (rect.x1 <= button->x && rect.x2 >= button->x
          && rect.y1 <= button->y && rect.y2 >= button->y) {
        zathura_link_evaluate(priv->zathura, link);
      }
      GIRARA_LIST_FOREACH_END(priv->links.list, zathura_link_t*, iter, link);
    }
  } else {
    redraw_rect(ZATHURA_PAGE(widget), &priv->mouse.selection);

    zathura_rectangle_t tmp = priv->mouse.selection;

    double scale = zathura_document_get_scale(document);
    tmp.x1 /= scale;
    tmp.x2 /= scale;
    tmp.y1 /= scale;
    tmp.y2 /= scale;

    char* text = zathura_page_get_text(priv->page, tmp, NULL);
    if (text != NULL) {
      if (strlen(text) > 0) {
        /* emit text-selected signal */
        g_signal_emit(ZATHURA_PAGE(widget), signals[TEXT_SELECTED], 0, text);
      }

      g_free(text);
    }
  }

  priv->mouse.selection_basepoint.x = -1;
  priv->mouse.selection_basepoint.y = -1;

  priv->mouse.selection.x1 = -1;
  priv->mouse.selection.y1 = -1;
  priv->mouse.selection.x2 = -1;
  priv->mouse.selection.y2 = -1;

  return false;
}

static gboolean
cb_zathura_page_widget_motion_notify(GtkWidget* widget, GdkEventMotion* event)
{
  g_return_val_if_fail(widget != NULL, false);
  g_return_val_if_fail(event != NULL, false);

  if ((event->state & GDK_BUTTON1_MASK) == 0) {
    zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
    if (priv->links.retrieved == false) {
      priv->links.list      = zathura_page_links_get(priv->page, NULL);
      priv->links.retrieved = true;
      priv->links.n         = (priv->links.list == NULL) ? 0 : girara_list_size(priv->links.list);
    }

    if (priv->links.list != NULL && priv->links.n > 0) {
      bool over_link = false;
      GIRARA_LIST_FOREACH(priv->links.list, zathura_link_t*, iter, link)
        zathura_rectangle_t rect = recalc_rectangle(priv->page, zathura_link_get_position(link));
        if (rect.x1 <= event->x && rect.x2 >= event->x && rect.y1 <= event->y && rect.y2 >= event->y) {
          over_link = true;
        }
      GIRARA_LIST_FOREACH_END(priv->links.list, zathura_link_t*, iter, link);

      if (priv->mouse.over_link != over_link) {
        if (over_link == true) {
          g_signal_emit(ZATHURA_PAGE(widget), signals[ENTER_LINK], 0);
        } else {
          g_signal_emit(ZATHURA_PAGE(widget), signals[LEAVE_LINK], 0);
        }
        priv->mouse.over_link = over_link;
      }
    }

    return false;
  }

  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  zathura_rectangle_t tmp = priv->mouse.selection;
  if (event->x < priv->mouse.selection_basepoint.x) {
    tmp.x1 = event->x;
    tmp.x2 = priv->mouse.selection_basepoint.x;
  } else {
    tmp.x2 = event->x;
    tmp.x1 = priv->mouse.selection_basepoint.x;
  }
  if (event->y < priv->mouse.selection_basepoint.y) {
    tmp.y1 = event->y;
    tmp.y2 = priv->mouse.selection_basepoint.y;
  } else {
    tmp.y1 = priv->mouse.selection_basepoint.y;
    tmp.y2 = event->y;
  }

  redraw_rect(ZATHURA_PAGE(widget), &priv->mouse.selection);
  redraw_rect(ZATHURA_PAGE(widget), &tmp);
  priv->mouse.selection = tmp;

  return false;
}

static gboolean
cb_zathura_page_widget_leave_notify(GtkWidget* widget, GdkEventCrossing* UNUSED(event))
{
  g_return_val_if_fail(widget != NULL, false);

  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  if (priv->mouse.over_link == true) {
    g_signal_emit(ZATHURA_PAGE(widget), signals[LEAVE_LINK], 0);
    priv->mouse.over_link = false;
  }
  return false;
}

static void
zathura_page_widget_popup_menu(GtkWidget* widget, GdkEventButton* event)
{
  g_return_if_fail(widget != NULL);
  g_return_if_fail(event != NULL);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

  if (priv->images.retrieved == false) {
    priv->images.list      = zathura_page_images_get(priv->page, NULL);
    priv->images.retrieved = true;
  }

  if (priv->images.list == NULL) {
    return;
  }

  /* search for underlaying image */
  zathura_image_t* image = NULL;
  GIRARA_LIST_FOREACH(priv->images.list, zathura_image_t*, iter, image_it)
    zathura_rectangle_t rect = recalc_rectangle(priv->page, image_it->position);
    if (rect.x1 <= event->x && rect.x2 >= event->x && rect.y1 <= event->y && rect.y2 >= event->y) {
      image = image_it;
    }
  GIRARA_LIST_FOREACH_END(priv->images.list, zathura_image_t*, iter, image_it);

  if (image == NULL) {
    return;
  }

  priv->images.current = image;

  /* setup menu */
  GtkWidget* menu = gtk_menu_new();

  typedef struct menu_item_s {
    char* text;
    void (*callback)(GtkMenuItem*, ZathuraPage*);
  } menu_item_t;

  const menu_item_t menu_items[] = {
    { _("Copy image"),    cb_menu_image_copy },
    { _("Save image as"), cb_menu_image_save },
  };

  for (unsigned int i = 0; i < LENGTH(menu_items); i++) {
    GtkWidget* item = gtk_menu_item_new_with_label(menu_items[i].text);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(menu_items[i].callback), ZATHURA_PAGE(widget));
  }

  /* attach and popup */
  int event_button = 0;
  int event_time   = gtk_get_current_event_time();

  if (event != NULL) {
    event_button = event->button;
    event_time   = event->time;
  }

  gtk_menu_attach_to_widget(GTK_MENU(menu), widget, NULL);
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event_button, event_time);
}

static gboolean
cb_zathura_page_widget_popup_menu(GtkWidget* widget)
{
  zathura_page_widget_popup_menu(widget, NULL);

  return TRUE;
}

static void
cb_menu_image_copy(GtkMenuItem* item, ZathuraPage* page)
{
  g_return_if_fail(item != NULL);
  g_return_if_fail(page != NULL);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(page);
  g_return_if_fail(priv->images.current != NULL);

  cairo_surface_t* surface = zathura_page_image_get_cairo(priv->page, priv->images.current, NULL);
  if (surface == NULL) {
    return;
  }

  const int width  = cairo_image_surface_get_width(surface);
  const int height = cairo_image_surface_get_height(surface);

  GdkPixbuf* pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);
  g_signal_emit(page, signals[IMAGE_SELECTED], 0, pixbuf);
  g_object_unref(pixbuf);
  cairo_surface_destroy(surface);

  /* reset */
  priv->images.current = NULL;
}

static void
cb_menu_image_save(GtkMenuItem* item, ZathuraPage* page)
{
  g_return_if_fail(item != NULL);
  g_return_if_fail(page != NULL);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(page);
  g_return_if_fail(priv->images.current != NULL);
  g_return_if_fail(priv->images.list != NULL);

  /* generate image identifier */
  unsigned int page_id  = zathura_page_get_index(priv->page) + 1;
  unsigned int image_id = 1;

  GIRARA_LIST_FOREACH(priv->images.list, zathura_image_t*, iter, image_it)
    if (image_it == priv->images.current) {
      break;
    }

    image_id++;
  GIRARA_LIST_FOREACH_END(priv->images.list, zathura_image_t*, iter, image_it);

  /* set command */
  char* export_command = g_strdup_printf(":export image-p%d-%d ", page_id, image_id);
  girara_argument_t argument = { 0, export_command };
  sc_focus_inputbar(priv->zathura->ui.session, &argument, NULL, 0);
  g_free(export_command);

  /* reset */
  priv->images.current = NULL;
}

void
zathura_page_widget_update_view_time(ZathuraPage* widget)
{
  g_return_if_fail(ZATHURA_IS_PAGE(widget));
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

  if (zathura_page_get_visibility(priv->page) == true) {
    zathura_render_request_update_view_time(priv->render_request);
  }
}

bool
zathura_page_widget_have_surface(ZathuraPage* widget)
{
  g_return_val_if_fail(ZATHURA_IS_PAGE(widget), false);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  return priv->surface != NULL;
}

void
zathura_page_widget_abort_render_request(ZathuraPage* widget)
{
  g_return_if_fail(ZATHURA_IS_PAGE(widget));
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  zathura_render_request_abort(priv->render_request);

  /* Make sure that if we are not cached and invisible, that there is no
   * surface.
   *
   * TODO: Maybe this should be moved somewhere else. */
  if (zathura_page_widget_have_surface(widget) == true &&
      priv->cached == false) {
    zathura_page_widget_update_surface(widget, NULL, false);
  }
}

zathura_page_t*
zathura_page_widget_get_page(ZathuraPage* widget) {
  g_return_val_if_fail(ZATHURA_IS_PAGE(widget), NULL);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

  return priv->page;
}
