/* SPDX-License-Identifier: Zlib */

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

typedef struct zathura_page_widget_private_s {
  zathura_page_t* page; /**< Page object */
  zathura_t* zathura; /**< Zathura object */
  cairo_surface_t* surface; /**< Cairo surface */
  cairo_surface_t* thumbnail; /**< Cairo surface */
  ZathuraRenderRequest* render_request; /* Request object */
  bool cached; /**< Cached state */

  struct {
    girara_list_t* list; /**< List of links on the page */
    gboolean retrieved; /**< True if we already tried to retrieve the list of links */
    gboolean draw; /**< True if links should be drawn */
    unsigned int offset; /**< Offset to the links */
    unsigned int n; /**< Number */
  } links;

  struct {
    girara_list_t* list; /**< A list if there are search results that should be drawn */
    int current; /**< The index of the current search result */
    gboolean draw; /**< Draw search results */
  } search;

  struct {
      girara_list_t *list; /**< List of selection rectangles that should be drawn */
      gboolean draw; /** Draw selection */
  } selection;

  struct {
    girara_list_t* list; /**< List of images on the page */
    gboolean retrieved; /**< True if we already tried to retrieve the list of images */
    zathura_image_t* current; /**< Image data of selected image */
  } images;

  struct {
    zathura_rectangle_t selection; /**< x1 y1: click point, x2 y2: current position */
    gboolean over_link;
  } mouse;

  struct {
    zathura_rectangle_t bounds; /**< Highlight bounds */
    gboolean draw; /**< Draw highlighted region */
  } highlighter;

  struct {
    girara_list_t* list; /**< List of signatures on the page */
    gboolean retrieved;  /**< True if we already tried to retrieve the list of signatures */
    gboolean draw;       /**< True if links should be drawn */
  } signatures;
} ZathuraPagePrivate;

G_DEFINE_TYPE_WITH_CODE(ZathuraPage, zathura_page_widget, GTK_TYPE_DRAWING_AREA, G_ADD_PRIVATE(ZathuraPage))

static gboolean zathura_page_widget_draw(GtkWidget* widget, cairo_t* cairo);
static void zathura_page_widget_finalize(GObject* object);
static void zathura_page_widget_dispose(GObject* object);
static void zathura_page_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void zathura_page_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void zathura_page_widget_size_allocate(GtkWidget* widget, GdkRectangle* allocation);
static void redraw_rect(ZathuraPage* widget, zathura_rectangle_t* rectangle);
static void redraw_all_rects(ZathuraPage* widget, girara_list_t* rectangles);
static void evaluate_link_at_mouse_position(ZathuraPage* widget, int oldx, int oldy);
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
  PROP_DRAW_SIGNATURES,
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
  /* overwrite methods */
  GtkWidgetClass* widget_class       = GTK_WIDGET_CLASS(class);
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
  g_object_class_install_property(object_class, PROP_DRAW_SIGNATURES,
                                  g_param_spec_boolean("draw-signatures", "draw-signatures",
                                                       "Set to true if signatures should be drawn", FALSE,
                                                       G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

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
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);
  priv->page               = NULL;
  priv->zathura            = NULL;
  priv->surface            = NULL;
  priv->thumbnail          = NULL;
  priv->render_request     = NULL;
  priv->cached             = false;

  priv->links.list      = NULL;
  priv->links.retrieved = false;
  priv->links.draw      = false;
  priv->links.offset    = 0;
  priv->links.n         = 0;

  priv->search.list    = NULL;
  priv->search.current = INT_MAX;
  priv->search.draw    = false;

  priv->selection.list = NULL;
  priv->selection.draw = false;

  priv->images.list      = NULL;
  priv->images.retrieved = false;
  priv->images.current   = NULL;

  priv->mouse.selection.x1 = -1;
  priv->mouse.selection.y1 = -1;
  priv->mouse.selection.x2 = -1;
  priv->mouse.selection.y2 = -1;
  priv->mouse.over_link    = false;

  priv->highlighter.bounds.x1 = -1;
  priv->highlighter.bounds.y1 = -1;
  priv->highlighter.bounds.x2 = -1;
  priv->highlighter.bounds.y2 = -1;
  priv->highlighter.draw      = false;

  priv->signatures.list      = NULL;
  priv->signatures.retrieved = false;
  priv->signatures.draw      = false;

  const unsigned int event_mask =
      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK;
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
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);
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
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);

  g_clear_object(&priv->render_request);

  G_OBJECT_CLASS(zathura_page_widget_parent_class)->dispose(object);
}

static void
zathura_page_widget_finalize(GObject* object)
{
  ZathuraPage* widget = ZATHURA_PAGE(object);
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);

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
set_font_from_property(cairo_t* cairo, zathura_t* zathura, cairo_font_weight_t weight)
{
  if (zathura == NULL) {
    return;
  }

  /* get user font description */
  char* font = NULL;
  girara_setting_get(zathura->ui.session, "font", &font);
  if (font == NULL) {
    return;
  }

  /* use pango to extract font family and size */
  PangoFontDescription* descr = pango_font_description_from_string(font);

  const char* family = pango_font_description_get_family(descr);

  /* get font size: can be points or absolute.
   * absolute units: example: value 10*PANGO_SCALE = 10 (unscaled) device units (logical pixels)
   * point units:    example: value 10*PANGO_SCALE = 10 points = 10*(font dpi config / 72) device units */
  double size = pango_font_description_get_size(descr) / (double)PANGO_SCALE;

  /* convert point size to device units */
  if (!pango_font_description_get_size_is_absolute(descr)) {
    double font_dpi = 96.0;
    if (zathura->ui.session != NULL) {
      if (gtk_widget_has_screen(zathura->ui.session->gtk.view)) {
        GdkScreen* screen = gtk_widget_get_screen(zathura->ui.session->gtk.view);
        font_dpi = gdk_screen_get_resolution(screen);
      }
    }
    size = size * font_dpi / 72;
  }

  cairo_select_font_face(cairo, family, CAIRO_FONT_SLANT_NORMAL, weight);
  cairo_set_font_size(cairo, size);

  pango_font_description_free(descr);
  g_free(font);
}

static cairo_text_extents_t
get_text_extents(const char* string, zathura_t* zathura, cairo_font_weight_t weight) {
  cairo_text_extents_t text = {0,};

  if (zathura == NULL) {
    return text;
  }

  /* make dummy surface to satisfy API requirements */
  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 0, 0);
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    return text;
  }

  cairo_t* cairo = cairo_create(surface);
  if (cairo_status(cairo) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(surface);
    return text;
  }

  set_font_from_property(cairo, zathura, weight);
  cairo_text_extents(cairo, string, &text);

  /* add some margin (for some reason the reported extents can be a bit short) */
  text.width += 6;
  text.height += 2;

  cairo_destroy(cairo);
  cairo_surface_destroy(surface);

  return text;
}

static void zathura_page_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
  ZathuraPage* pageview    = ZATHURA_PAGE(object);
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(pageview);

  cairo_text_extents_t text;

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
    if (priv->links.draw == TRUE && priv->links.retrieved == FALSE) {
      priv->links.list      = zathura_page_links_get(priv->page, NULL);
      priv->links.retrieved = TRUE;
      priv->links.n         = (priv->links.list == NULL) ? 0 : girara_list_size(priv->links.list);
    }

    if (priv->links.retrieved == TRUE && priv->links.list != NULL) {
      /* get size of text that should be large enough for every link hint */
      text = get_text_extents("888", priv->zathura, CAIRO_FONT_WEIGHT_BOLD);

      for (size_t idx = 0; idx != girara_list_size(priv->links.list); ++idx) {
        zathura_link_t* link = girara_list_nth(priv->links.list, idx);
        if (link != NULL) {
          /* redraw link area */
          zathura_rectangle_t rectangle = recalc_rectangle(priv->page, zathura_link_get_position(link));
          redraw_rect(pageview, &rectangle);

          /* also redraw area for link hint */
          rectangle.x2 = rectangle.x1 + text.width;
          rectangle.y1 = rectangle.y2 - text.height;
          redraw_rect(pageview, &rectangle);
        }
      }
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
      priv->links.draw = FALSE;
      redraw_all_rects(pageview, priv->search.list);
    }
    priv->search.current = -1;
    break;
  case PROP_SEARCH_RESULTS_CURRENT: {
    g_return_if_fail(priv->search.list != NULL);
    if (priv->search.current >= 0 && priv->search.current < (signed)girara_list_size(priv->search.list)) {
      zathura_rectangle_t* rect     = girara_list_nth(priv->search.list, priv->search.current);
      zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
      redraw_rect(pageview, &rectangle);
    }
    int val = g_value_get_int(value);
    if (val < 0) {
      priv->search.current = girara_list_size(priv->search.list);
    } else {
      priv->search.current = val;
      if (priv->search.draw == TRUE && val >= 0 && val < (signed)girara_list_size(priv->search.list)) {
        zathura_rectangle_t* rect     = girara_list_nth(priv->search.list, priv->search.current);
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
  case PROP_DRAW_SIGNATURES:
    priv->signatures.draw = g_value_get_boolean(value);
    /* get links */
    if (priv->signatures.draw == TRUE && priv->signatures.retrieved == FALSE) {
      priv->signatures.list      = zathura_page_get_signatures(priv->page, NULL);
      priv->signatures.retrieved = TRUE;
    }

    if (priv->signatures.retrieved == TRUE && priv->signatures.list != NULL) {
      for (size_t idx = 0; idx != girara_list_size(priv->signatures.list); ++idx) {
        zathura_signature_info_t* signature = girara_list_nth(priv->signatures.list, idx);
        if (signature == NULL) {
          continue;
        }
        /* redraw signature area */
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, signature->position);
        redraw_rect(pageview, &rectangle);
      }
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
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(pageview);

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

static zathura_device_factors_t
get_safe_device_factors(cairo_surface_t* surface)
{
  zathura_device_factors_t factors;
  cairo_surface_get_device_scale(surface, &factors.x, &factors.y);

  if (fabs(factors.x) < DBL_EPSILON) {
    factors.x = 1.0;
  }
  if (fabs(factors.y) < DBL_EPSILON) {
    factors.y = 1.0;
  }

  return factors;
}

static gboolean zathura_page_widget_draw(GtkWidget* widget, cairo_t* cairo) {
  ZathuraPage* page        = ZATHURA_PAGE(widget);
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(page);
  zathura_t* zathura       = priv->zathura;

  zathura_document_t* document   = zathura_page_get_document(priv->page);
  const unsigned int page_height = gtk_widget_get_allocated_height(widget);
  const unsigned int page_width  = gtk_widget_get_allocated_width(widget);

  bool smooth_reload = true;
  girara_setting_get(zathura->ui.session, "smooth-reload", &smooth_reload);

  bool surface_exists = priv->surface != NULL || priv->thumbnail != NULL;

  if (zathura->predecessor_document != NULL && zathura->predecessor_pages != NULL && smooth_reload && !surface_exists) {
    unsigned int page_index = zathura_page_get_index(priv->page);

    if (page_index < zathura_document_get_number_of_pages(priv->zathura->predecessor_document)) {
      /* render real page */
      zathura_render_request(priv->render_request, g_get_real_time());

      girara_debug("using predecessor page for idx %d", page_index);
      document = priv->zathura->predecessor_document;
      page     = ZATHURA_PAGE(priv->zathura->predecessor_pages[page_index]);
      priv     = zathura_page_widget_get_instance_private(page);
    }
    surface_exists = priv->surface != NULL || priv->thumbnail != NULL;
  }

  if (surface_exists) {
    cairo_save(cairo);

    const unsigned int rotation = zathura_document_get_rotation(document);
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
      unsigned int pheight = (rotation % 180 ? page_width : page_height);
      unsigned int pwidth = (rotation % 180 ? page_height : page_width);

      /* note: this always returns 1 and 1 if Cairo too old for device scale API */
      zathura_device_factors_t device = get_safe_device_factors(priv->thumbnail);
      pwidth *= device.x;
      pheight *= device.y;

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
      const gint64 penalty = (gint64)pwidth * (gint64)pheight;
      zathura_render_request(priv->render_request, g_get_real_time() + penalty);
      return FALSE;
    }

    /* draw links */
    set_font_from_property(cairo, zathura, CAIRO_FONT_WEIGHT_BOLD);

    float transparency = 0.5;
    girara_setting_get(zathura->ui.session, "highlight-transparency", &transparency);

    if (priv->links.draw == true && priv->links.n != 0) {
      unsigned int link_counter = 0;
      for (size_t idx = 0; idx != girara_list_size(priv->links.list); ++idx) {
        zathura_link_t* link = girara_list_nth(priv->links.list, idx);
        if (link != NULL) {
          zathura_rectangle_t rectangle = recalc_rectangle(priv->page, zathura_link_get_position(link));

          /* draw position */
          const GdkRGBA color = zathura->ui.colors.highlight_color;
          cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
          cairo_rectangle(cairo, rectangle.x1, rectangle.y1, (rectangle.x2 - rectangle.x1),
                          (rectangle.y2 - rectangle.y1));
          cairo_fill(cairo);

          /* draw text */
          const GdkRGBA color_fg = zathura->ui.colors.highlight_color_fg;
          cairo_set_source_rgba(cairo, color_fg.red, color_fg.green, color_fg.blue, transparency);
          cairo_move_to(cairo, rectangle.x1 + 1, rectangle.y2 - 1);
          char* link_number = g_strdup_printf("%i", priv->links.offset + ++link_counter);
          cairo_show_text(cairo, link_number);
          g_free(link_number);
        }
      }
    }

    /* draw signatures */
    if (priv->signatures.draw == true && priv->signatures.list != NULL) {
      for (size_t idx = 0; idx != girara_list_size(priv->signatures.list); ++idx) {
        zathura_signature_info_t* signature = girara_list_nth(priv->signatures.list, idx);
        if (signature == NULL) {
          continue;
        }

        GdkRGBA color;
        char* text     = NULL;
        bool free_text = false;
        switch (signature->state) {
        case ZATHURA_SIGNATURE_VALID: {
          color = zathura->ui.colors.signature_success;

          char* sig_time = g_date_time_format(signature->time, "%F %T");
          text = g_strdup_printf(_("Signature is valid.\nThis document is signed by\n  %s\non %s."), signature->signer,
                                 sig_time);
          free_text = true;
          g_free(sig_time);
          break;
        }
        case ZATHURA_SIGNATURE_CERTIFICATE_EXPIRED:
          color = zathura->ui.colors.signature_warning;
          text  = _("Signature certificate is expired.");
          break;
        case ZATHURA_SIGNATURE_CERTIFICATE_REVOKED:
          color = zathura->ui.colors.signature_error;
          text  = _("Signature certificate is revoked.");
          break;
        case ZATHURA_SIGNATURE_CERTIFICATE_UNTRUSTED:
          color = zathura->ui.colors.signature_error;
          text  = _("Signature certificate is not trusted.");
          break;
        case ZATHURA_SIGNATURE_CERTIFICATE_INVALID:
          color = zathura->ui.colors.signature_error;
          text  = _("Signature certificate is invalid.");
          break;
        default:
          color = zathura->ui.colors.signature_error;
          text  = _("Signature is invalid.");
        }

        /* draw position */
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, signature->position);
        cairo_set_source_rgba(cairo, color.red, color.green, color.blue, color.alpha);
        cairo_rectangle(cairo, rectangle.x1, rectangle.y1, (rectangle.x2 - rectangle.x1),
                        (rectangle.y2 - rectangle.y1));
        cairo_fill(cairo);

        /* draw text */
        const GdkRGBA color_fg = zathura->ui.colors.highlight_color_fg;
        cairo_set_source_rgba(cairo, color_fg.red, color_fg.green, color_fg.blue, transparency);
        cairo_text_extents_t extents;
        cairo_text_extents(cairo, text, &extents);

        cairo_move_to(cairo, rectangle.x1 + 1, rectangle.y1 + extents.height + 1);
        cairo_show_text(cairo, text);
        if (free_text == true) {
          g_free(text);
        }
      }
    }

    /* draw search results */
    if (priv->search.list != NULL && priv->search.draw == true) {
      int idx = 0;
      for (size_t inner_idx = 0; inner_idx != girara_list_size(priv->search.list); ++inner_idx) {
        zathura_rectangle_t* rect     = girara_list_nth(priv->search.list, inner_idx);
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);

        /* draw position */
        if (idx == priv->search.current) {
          const GdkRGBA color = zathura->ui.colors.highlight_color_active;
          cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
        } else {
          const GdkRGBA color = zathura->ui.colors.highlight_color;
          cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
        }
        cairo_rectangle(cairo, rectangle.x1, rectangle.y1, (rectangle.x2 - rectangle.x1),
                        (rectangle.y2 - rectangle.y1));
        cairo_fill(cairo);
        ++idx;
      }
    }
    if (priv->selection.list != NULL && priv->selection.draw == true) {
      const GdkRGBA color = priv->zathura->ui.colors.highlight_color;
      cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
      GIRARA_LIST_FOREACH_BODY(priv->selection.list, zathura_rectangle_t*, rect,
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
        cairo_rectangle(cairo, rectangle.x1, rectangle.y1,
                        rectangle.x2 - rectangle.x1, rectangle.y2 - rectangle.y1);
        cairo_fill(cairo);
      );
    }
    if (priv->highlighter.bounds.x1 != -1
        && priv->highlighter.bounds.y1 != -1
        && priv->highlighter.draw == true) {
      const GdkRGBA color = priv->zathura->ui.colors.highlight_color;
      cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
      zathura_rectangle_t rectangle = recalc_rectangle(priv->page, priv->highlighter.bounds);
      cairo_rectangle(cairo, rectangle.x1, rectangle.y1,
                      rectangle.x2 - rectangle.x1, rectangle.y2 - rectangle.y1);
      cairo_fill(cairo);
    }
  } else {
	if (smooth_reload) {
		girara_debug("rendering loading screen, flicker might be happening");
	}

    /* set background color */
    if (zathura_renderer_recolor_enabled(priv->zathura->sync.render_thread) == true) {
      GdkRGBA color;
      zathura_renderer_get_recolor_colors(priv->zathura->sync.render_thread, &color, NULL);
      cairo_set_source_rgba(cairo, color.red, color.green, color.blue, color.alpha);
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
#define THUMBNAIL_INITIAL_ZOOM 0.5
/* small enough to make bilinear downscaling fast */
#define THUMBNAIL_MAX_ZOOM 0.5

static bool
surface_small_enough(cairo_surface_t* surface, size_t max_size, cairo_surface_t* old)
{
  if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE) {
    return true;
  }

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
    if (new_size < old_size && new_size >= old_size * THUMBNAIL_MAX_ZOOM * THUMBNAIL_MAX_ZOOM) {
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
  double scale = sqrt((double)max_size / (width * height)) * THUMBNAIL_INITIAL_ZOOM;
  if (scale > THUMBNAIL_MAX_ZOOM) {
    scale = THUMBNAIL_MAX_ZOOM;
  }
  width = width * scale;
  height = height * scale;

  /* note: this always returns 1 and 1 if Cairo too old for device scale API */
  zathura_device_factors_t device = get_safe_device_factors(surface);
  const unsigned int unscaled_width = width / device.x;
  const unsigned int unscaled_height = height / device.y;

  /* create thumbnail surface, taking width and height as _unscaled_ device units */
  cairo_surface_t* thumbnail;
  thumbnail = cairo_surface_create_similar(surface, CAIRO_CONTENT_COLOR, unscaled_width, unscaled_height);
  if (cairo_surface_status(thumbnail) != CAIRO_STATUS_SUCCESS) {
    return NULL;
  }

  cairo_t* cairo = cairo_create(thumbnail);
  if (cairo_status(cairo) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(thumbnail);
    return NULL;
  }

  cairo_scale(cairo, scale, scale);
  cairo_set_source_surface(cairo, surface, 0, 0);
  cairo_pattern_set_filter(cairo_get_source(cairo), CAIRO_FILTER_BILINEAR);
  cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cairo);
  cairo_destroy(cairo);

  return thumbnail;
}

void
zathura_page_widget_update_surface(ZathuraPage* widget, cairo_surface_t* surface, bool keep_thumbnail)
{
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);
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

  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);
  priv->cached = true;
}

static void
cb_cache_invalidated(ZathuraRenderRequest* UNUSED(request), void* data)
{
  ZathuraPage* widget = data;
  g_return_if_fail(ZATHURA_IS_PAGE(widget));

  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);
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

static void redraw_all_rects(ZathuraPage* widget, girara_list_t* rectangles) {
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);

  for (size_t idx = 0; idx != girara_list_size(rectangles); ++idx) {
    zathura_rectangle_t* rect     = girara_list_nth(rectangles, idx);
    zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
    redraw_rect(widget, &rectangle);
  }
}

static void
evaluate_link_at_mouse_position(ZathuraPage* page, int oldx, int oldy)
{
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(page);
  /* simple single click */
  /* get links */
  if (priv->links.retrieved == false) {
    priv->links.list      = zathura_page_links_get(priv->page, NULL);
    priv->links.retrieved = true;
    priv->links.n         = (priv->links.list == NULL) ? 0 : girara_list_size(priv->links.list);
  }

  if (priv->links.list != NULL && priv->links.n > 0) {
    for (size_t idx = 0; idx != girara_list_size(priv->links.list); ++idx) {
      zathura_link_t* link           = girara_list_nth(priv->links.list, idx);
      const zathura_rectangle_t rect = recalc_rectangle(priv->page, zathura_link_get_position(link));
      if (rect.x1 <= oldx && rect.x2 >= oldx && rect.y1 <= oldy && rect.y2 >= oldy) {
        zathura_link_evaluate(priv->zathura, link);
        break;
      }
    }
  }
}

zathura_link_t*
zathura_page_widget_link_get(ZathuraPage* widget, unsigned int index)
{
  g_return_val_if_fail(widget != NULL, NULL);
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);
  g_return_val_if_fail(priv != NULL, NULL);

  if (priv->links.list != NULL && index >= priv->links.offset &&
      girara_list_size(priv->links.list) > index - priv->links.offset) {
    return girara_list_nth(priv->links.list, index - priv->links.offset);
  } else {
    return NULL;
  }
}

static void
rotate_point(zathura_document_t* document, double orig_x, double orig_y,
             double* x, double* y)
{
  const unsigned int rotation = zathura_document_get_rotation(document);
  if (rotation == 0) {
    *x = orig_x;
    *y = orig_y;
    return;
  }

  unsigned int height, width;
  zathura_document_get_cell_size(document, &height, &width);
  switch (rotation) {
    case 90:
      *x = orig_y;
      *y = width - orig_x;
      break;
    case 180:
      *x = width - orig_x;
      *y = height - orig_y;
      break;
    case 270:
      *x = height - orig_y;
      *y = orig_x;
      break;
    default:
      *x = orig_x;
      *y = orig_y;
  }
}

static gboolean
cb_zathura_page_widget_button_press_event(GtkWidget* widget, GdkEventButton* button)
{
  g_return_val_if_fail(widget != NULL, false);
  g_return_val_if_fail(button != NULL, false);

  ZathuraPage* page            = ZATHURA_PAGE(widget);
  ZathuraPagePrivate* priv     = zathura_page_widget_get_instance_private(page);
  zathura_document_t* document = zathura_page_get_document(priv->page);

  if (girara_callback_view_button_press_event(widget, button, priv->zathura->ui.session) == true) {
    return true;
  }

  if (priv->selection.list != NULL || priv->highlighter.draw == true) {
    priv->selection.draw = false;
    priv->highlighter.draw = false;
    zathura_page_widget_redraw_canvas(page);
  }

  if (button->button == GDK_BUTTON_PRIMARY) { /* left click */
    if (button->type == GDK_BUTTON_PRESS) {
      /* start the selection */
      double x, y;
      rotate_point(document, button->x, button->y, &x, &y);

      priv->mouse.selection.x1 = x;
      priv->mouse.selection.y1 = y;
      priv->mouse.selection.x2 = x;
      priv->mouse.selection.y2 = y;

    } else if (button->type == GDK_2BUTTON_PRESS || button->type == GDK_3BUTTON_PRESS) {
      /* abort the selection */
      priv->mouse.selection.x1 = -1;
      priv->mouse.selection.y1 = -1;
      priv->mouse.selection.x2 = -1;
      priv->mouse.selection.y2 = -1;
    }

    return true;
  } else if (gdk_event_triggers_context_menu((GdkEvent*) button) == TRUE && button->type == GDK_BUTTON_PRESS) { /* right click */
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

  ZathuraPage* page        = ZATHURA_PAGE(widget);
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(page);

  zathura_document_t* document = zathura_page_get_document(priv->page);
  const double scale           = zathura_document_get_scale(document);

  const int oldx = button->x;
  const int oldy = button->y;

  button->x /= scale;
  button->y /= scale;

  g_signal_emit(page, signals[BUTTON_RELEASE], 0, button);

  button->x = oldx;
  button->y = oldy;

  if (button->button != GDK_BUTTON_PRIMARY) {
    return false;
  }

  if (priv->mouse.selection.x2 == -1 && priv->mouse.selection.y2 == -1) {
    /* simple single click */
    /* get links */
    if (priv->zathura->global.double_click_follow) {
      evaluate_link_at_mouse_position(page, oldx, oldy);
    }
  } else {
    zathura_rectangle_t tmp = priv->mouse.selection;

    tmp.x1 /= scale;
    tmp.x2 /= scale;
    tmp.y1 /= scale;
    tmp.y2 /= scale;

    char* text = zathura_page_get_text(priv->page, tmp, NULL);
    if (text != NULL && *text != '\0') {
      /* emit text-selected signal */
      g_signal_emit(page, signals[TEXT_SELECTED], 0, text);
    } else if (priv->zathura->global.double_click_follow == false) {
      evaluate_link_at_mouse_position(page, oldx, oldy);
    }
    g_free(text);
  }

  priv->mouse.selection.x1 = -1;
  priv->mouse.selection.y1 = -1;
  priv->mouse.selection.x2 = -1;
  priv->mouse.selection.y2 = -1;

  return false;
}

static zathura_rectangle_t
next_selection_rectangle(double basepoint_x, double basepoint_y,
                         double next_x, double next_y)
{
  zathura_rectangle_t rect;

  /* make sure that x2 > x1 && y2 > y1 holds */
  if (next_x > basepoint_x) {
    rect.x1 = basepoint_x;
    rect.x2 = next_x;
  } else {
    rect.x1 = next_x;
    rect.x2 = basepoint_x;
  }
  if (next_y > basepoint_y) {
    rect.y1 = basepoint_y;
    rect.y2 = next_y;
  } else {
    rect.y1 = next_y;
    rect.y2 = basepoint_y;
  }

  return rect;
}

static gboolean
cb_zathura_page_widget_motion_notify(GtkWidget* widget, GdkEventMotion* event)
{
  g_return_val_if_fail(widget != NULL, false);
  g_return_val_if_fail(event != NULL, false);

  ZathuraPage* page        = ZATHURA_PAGE(widget);
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(page);

  zathura_document_t* document = zathura_page_get_document(priv->page);
  const double scale           = zathura_document_get_scale(document);

  if (event->state & GDK_BUTTON1_MASK) { /* holding left mouse button */
    if (event->state & priv->zathura->global.highlighter_modmask) {
      double x, y;
      rotate_point(document, event->x, event->y, &x, &y);
      priv->highlighter.bounds = next_selection_rectangle(priv->mouse.selection.x1,
                                                          priv->mouse.selection.y1,
                                                          x, y);
      priv->highlighter.bounds.x1 /= scale;
      priv->highlighter.bounds.y1 /= scale;
      priv->highlighter.bounds.x2 /= scale;
      priv->highlighter.bounds.y2 /= scale;

      priv->highlighter.draw = true;
      zathura_page_widget_redraw_canvas(page);
    } else {
      /* calculate next selection */
      rotate_point(document, event->x, event->y,
                   &priv->mouse.selection.x2, &priv->mouse.selection.y2);

      zathura_rectangle_t selection = priv->mouse.selection;
      selection.x1 /= scale;
      selection.y1 /= scale;
      selection.x2 /= scale;
      selection.y2 /= scale;

      if (priv->selection.list != NULL) {
        girara_list_free(priv->selection.list);
      }

      priv->selection.list = zathura_page_get_selection(priv->page, selection, NULL);
      if (priv->selection.list != NULL && girara_list_size(priv->selection.list) != 0) {
        priv->selection.draw = true;
        zathura_page_widget_redraw_canvas(page);
      }
    }
  } else {
    if (priv->links.retrieved == false) {
      priv->links.list      = zathura_page_links_get(priv->page, NULL);
      priv->links.retrieved = true;
      priv->links.n         = (priv->links.list == NULL) ? 0 : girara_list_size(priv->links.list);
    }

    if (priv->links.list != NULL && priv->links.n > 0) {
      bool over_link = false;
      for (size_t idx = 0; idx != girara_list_size(priv->links.list); ++idx) {
        zathura_link_t* link     = girara_list_nth(priv->links.list, idx);
        zathura_rectangle_t rect = recalc_rectangle(priv->page, zathura_link_get_position(link));
        if (rect.x1 <= event->x && rect.x2 >= event->x && rect.y1 <= event->y && rect.y2 >= event->y) {
          over_link = true;
          break;
        }
      }

      if (priv->mouse.over_link != over_link) {
        if (over_link == true) {
          g_signal_emit(page, signals[ENTER_LINK], 0);
        } else {
          g_signal_emit(page, signals[LEAVE_LINK], 0);
        }
        priv->mouse.over_link = over_link;
      }
    }
  }

  return false;
}

static gboolean
cb_zathura_page_widget_leave_notify(GtkWidget* widget, GdkEventCrossing* UNUSED(event))
{
  g_return_val_if_fail(widget != NULL, false);

  ZathuraPage* page        = ZATHURA_PAGE(widget);
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(page);
  if (priv->selection.list != NULL) {
    priv->selection.draw = false;
    zathura_page_widget_redraw_canvas(page);
  }
  if (priv->mouse.over_link == true) {
    g_signal_emit(page, signals[LEAVE_LINK], 0);
    priv->mouse.over_link = false;
  }
  return false;
}

static void
zathura_page_widget_popup_menu(GtkWidget* widget, GdkEventButton* event)
{
  g_return_if_fail(widget != NULL);
  if (event == NULL) {
    /* do something here in the future in case we have general popups */
    return;
  }

  ZathuraPage* page        = ZATHURA_PAGE(widget);
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(page);

  if (priv->images.retrieved == false) {
    priv->images.list      = zathura_page_images_get(priv->page, NULL);
    priv->images.retrieved = true;
  }

  if (priv->images.list == NULL) {
    return;
  }

  /* search for underlaying image */
  zathura_image_t* image = NULL;
  for (size_t idx = 0; idx != girara_list_size(priv->images.list); ++idx) {
    zathura_image_t* image_it = girara_list_nth(priv->images.list, idx);
    zathura_rectangle_t rect  = recalc_rectangle(priv->page, image_it->position);
    if (rect.x1 <= event->x && rect.x2 >= event->x && rect.y1 <= event->y && rect.y2 >= event->y) {
      image = image_it;
    }
  }

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
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(menu_items[i].callback), page);
  }

  /* attach and popup */
  gtk_menu_attach_to_widget(GTK_MENU(menu), widget, NULL);
  gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*) event);
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
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(page);
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
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(page);
  g_return_if_fail(priv->images.current != NULL);
  g_return_if_fail(priv->images.list != NULL);

  /* generate image identifier */
  unsigned int page_id  = zathura_page_get_index(priv->page) + 1;
  unsigned int image_id = 1;

  for (size_t idx = 0; idx != girara_list_size(priv->images.list); ++idx, ++image_id) {
    zathura_image_t* image_it = girara_list_nth(priv->images.list, idx);
    if (image_it == priv->images.current) {
      break;
    }
  }

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
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);

  if (zathura_page_get_visibility(priv->page) == true) {
    zathura_render_request_update_view_time(priv->render_request);
  }
}

bool
zathura_page_widget_have_surface(ZathuraPage* widget)
{
  g_return_val_if_fail(ZATHURA_IS_PAGE(widget), false);
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);
  return priv->surface != NULL;
}

void
zathura_page_widget_abort_render_request(ZathuraPage* widget)
{
  g_return_if_fail(ZATHURA_IS_PAGE(widget));
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);
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
  ZathuraPagePrivate* priv = zathura_page_widget_get_instance_private(widget);

  return priv->page;
}
