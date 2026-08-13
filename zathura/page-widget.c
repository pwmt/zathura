/* SPDX-License-Identifier: Zlib */

#include "page-widget.h"

#include <girara/utils.h>
#include <girara-gtk/callbacks.h>
#include <girara-gtk/settings.h>
#include <girara/datastructures.h>
#include <girara-gtk/session.h>
#include <string.h>
#include <glib/gi18n.h>
#include <math.h>

#include "links-internal.h"
#include "page.h"
#include "adjustment.h"
#include "render.h"
#include "utils.h"
#include "shortcuts.h"
#include "zathura.h"
#include "document-widget.h"

typedef struct zathura_page_widget_private_s {
  zathura_page_t* page;                 /**< Page object */
  zathura_t* zathura;                   /**< Zathura object */
  cairo_surface_t* surface;             /**< Cairo surface */
  cairo_surface_t* thumbnail;           /**< Cairo surface */
  ZathuraRenderRequest* render_request; /* Request object */
  bool cached;                          /**< Cached state */

  struct {
    girara_list_t* list; /**< List of links on the page */
    gboolean retrieved;  /**< True if we already tried to retrieve the list of links */
    gboolean draw;       /**< True if links should be drawn */
    unsigned int offset; /**< Offset to the links */
    unsigned int n;      /**< Number */
  } links;

  struct {
    girara_list_t* list; /**< A list if there are search results that should be drawn */
    int current;         /**< The index of the current search result */
    gboolean draw;       /**< Draw search results */
  } search;

  struct {
    girara_list_t* list; /**< List of selection rectangles that should be drawn */
    gboolean draw;       /** Draw selection */
  } selection;

  struct {
    girara_list_t* list;      /**< List of images on the page */
    gboolean retrieved;       /**< True if we already tried to retrieve the list of images */
    zathura_image_t* current; /**< Image data of selected image */
  } images;

  struct {
    zathura_rectangle_t selection; /**< x1 y1: click point, x2 y2: current position */
    gboolean over_link;
  } mouse;

  struct {
    zathura_rectangle_t bounds; /**< Highlight bounds */
    gboolean draw;              /**< Draw highlighted region */
  } highlighter;

  struct {
    girara_list_t* list; /**< List of signatures on the page */
    gboolean retrieved;  /**< True if we already tried to retrieve the list of signatures */
    gboolean draw;       /**< True if links should be drawn */
  } signatures;

  GtkWidget* drawing_area;           /**< child layer that draws the page */
  GtkWidget* image_popover;          /**< lazily created image context menu */
  GSimpleActionGroup* image_actions; /**< action group for the image popup */
} ZathuraPageWidgetPrivate;

G_DEFINE_TYPE_WITH_CODE(ZathuraPageWidget, zathura_page_widget, GTK_TYPE_WIDGET, G_ADD_PRIVATE(ZathuraPageWidget))

static void cb_page_draw(GtkDrawingArea* area, cairo_t* cairo, int width, int height, gpointer data);
static void zathura_page_widget_finalize(GObject* object);
static void zathura_page_widget_dispose(GObject* object);
static void zathura_page_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void zathura_page_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void evaluate_link_at_mouse_position(ZathuraPageWidget* widget, int oldx, int oldy);
static void zathura_page_widget_popup_menu(GtkWidget* widget, double x, double y);
static void cb_menu_image_copy(GSimpleAction* action, GVariant* parameter, gpointer data);
static void cb_menu_image_save(GSimpleAction* action, GVariant* parameter, gpointer data);
static void cb_zathura_page_widget_button_press_event(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y,
                                                      gpointer data);
static void cb_zathura_page_widget_button_release_event(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y,
                                                        gpointer data);
static void cb_zathura_page_widget_motion_notify(GtkEventControllerMotion* controller, gdouble x, gdouble y,
                                                 gpointer data);
static void cb_zathura_page_widget_leave_notify(GtkEventControllerMotion* controller, gpointer data);
static void cb_update_surface(ZathuraRenderRequest* request, cairo_surface_t* surface, void* data);
static void cb_cache_added(ZathuraRenderRequest* request, void* data);
static void cb_cache_invalidated(ZathuraRenderRequest* request, void* data);
static bool surface_small_enough(cairo_surface_t* surface, size_t max_size, cairo_surface_t* old);
static cairo_surface_t* draw_thumbnail_image(cairo_surface_t* surface, size_t max_size);

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
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

static void zathura_page_widget_class_init(ZathuraPageWidgetClass* class) {
  /* overwrite methods */
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
  gtk_widget_class_set_layout_manager_type(widget_class, GTK_TYPE_BIN_LAYOUT);

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->dispose      = zathura_page_widget_dispose;
  object_class->finalize     = zathura_page_widget_finalize;
  object_class->set_property = zathura_page_widget_set_property;
  object_class->get_property = zathura_page_widget_get_property;

  /* add properties */
  g_object_class_install_property(
      object_class, PROP_PAGE,
      g_param_spec_pointer("page", "page", "the page to draw",
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(
      object_class, PROP_ZATHURA,
      g_param_spec_pointer("zathura", "zathura", "the zathura instance",
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_DRAW_LINKS,
                                  g_param_spec_boolean("draw-links", "draw-links",
                                                       "Set to true if links should be drawn", FALSE,
                                                       G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_LINKS_OFFSET,
                                  g_param_spec_int("offset-links", "offset-links", "Offset for the link numbers", 0,
                                                   INT_MAX, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_LINKS_NUMBER,
                                  g_param_spec_int("number-of-links", "number-of-links", "Number of links", 0, INT_MAX,
                                                   0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS,
                                  g_param_spec_pointer("search-results", "search-results",
                                                       "Set to the list of search results",
                                                       G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS_CURRENT,
                                  g_param_spec_int("search-current", "search-current", "The current search result", -1,
                                                   INT_MAX, 0,
                                                   G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS_LENGTH,
                                  g_param_spec_int("search-length", "search-length", "The number of search results", -1,
                                                   INT_MAX, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_DRAW_SEARCH_RESULTS,
                                  g_param_spec_boolean("draw-search-results", "draw-search-results",
                                                       "Set to true if search results should be drawn", FALSE,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(object_class, PROP_DRAW_SIGNATURES,
                                  g_param_spec_boolean("draw-signatures", "draw-signatures",
                                                       "Set to true if signatures should be drawn", FALSE,
                                                       G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /* add signals */
  signals[TEXT_SELECTED] = g_signal_new("text-selected", ZATHURA_TYPE_PAGE_WIDGET, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                        g_cclosure_marshal_generic, G_TYPE_NONE, 1, G_TYPE_STRING);

  signals[IMAGE_SELECTED] = g_signal_new("image-selected", ZATHURA_TYPE_PAGE_WIDGET, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                         g_cclosure_marshal_generic, G_TYPE_NONE, 1, G_TYPE_OBJECT);

  signals[ENTER_LINK] = g_signal_new("enter-link", ZATHURA_TYPE_PAGE_WIDGET, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                     g_cclosure_marshal_generic, G_TYPE_NONE, 0);

  signals[LEAVE_LINK] = g_signal_new("leave-link", ZATHURA_TYPE_PAGE_WIDGET, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                     g_cclosure_marshal_generic, G_TYPE_NONE, 0);

  signals[BUTTON_RELEASE] = g_signal_new("scaled-button-release", ZATHURA_TYPE_PAGE_WIDGET, G_SIGNAL_RUN_LAST, 0, NULL,
                                         NULL, g_cclosure_marshal_generic, G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void zathura_page_widget_init(ZathuraPageWidget* widget) {
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  priv->page                     = NULL;
  priv->zathura                  = NULL;
  priv->surface                  = NULL;
  priv->thumbnail                = NULL;
  priv->render_request           = NULL;
  priv->cached                   = false;

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

  /* page is drawn into a child drawing area */
  priv->drawing_area = gtk_drawing_area_new();
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(priv->drawing_area), cb_page_draw, widget, NULL);
  gtk_widget_set_parent(priv->drawing_area, GTK_WIDGET(widget));

  GtkGesture* click = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
  g_signal_connect(click, "pressed", G_CALLBACK(cb_zathura_page_widget_button_press_event), widget);
  g_signal_connect(click, "released", G_CALLBACK(cb_zathura_page_widget_button_release_event), widget);
  gtk_widget_add_controller(GTK_WIDGET(widget), GTK_EVENT_CONTROLLER(click));

  GtkEventController* motion = gtk_event_controller_motion_new();
  g_signal_connect(motion, "motion", G_CALLBACK(cb_zathura_page_widget_motion_notify), widget);
  g_signal_connect(motion, "leave", G_CALLBACK(cb_zathura_page_widget_leave_notify), widget);
  gtk_widget_add_controller(GTK_WIDGET(widget), motion);

  /* image popup actions are looked up by the popover via the image prefix */
  static const GActionEntry image_action_entries[] = {
      {"copy", cb_menu_image_copy, NULL, NULL, NULL, {0, 0, 0}},
      {"save", cb_menu_image_save, NULL, NULL, NULL, {0, 0, 0}},
  };
  priv->image_actions = g_simple_action_group_new();
  g_action_map_add_action_entries(G_ACTION_MAP(priv->image_actions), image_action_entries,
                                  G_N_ELEMENTS(image_action_entries), widget);
  gtk_widget_insert_action_group(GTK_WIDGET(widget), "image", G_ACTION_GROUP(priv->image_actions));
}

GtkWidget* zathura_page_widget_new(zathura_t* zathura, zathura_page_t* page) {
  g_return_val_if_fail(page != NULL, NULL);

  GObject* ret = g_object_new(ZATHURA_TYPE_PAGE_WIDGET, "page", page, "zathura", zathura, NULL);
  if (ret == NULL) {
    return NULL;
  }

  ZathuraPageWidget* widget      = ZATHURA_PAGE_WIDGET(ret);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  priv->render_request           = zathura_render_request_new(zathura->sync.render_thread, page);
  g_signal_connect_object(priv->render_request, "completed", G_CALLBACK(cb_update_surface), widget, 0);
  g_signal_connect_object(priv->render_request, "cache-added", G_CALLBACK(cb_cache_added), widget, 0);
  g_signal_connect_object(priv->render_request, "cache-invalidated", G_CALLBACK(cb_cache_invalidated), widget, 0);

  return GTK_WIDGET(ret);
}

static void zathura_page_widget_dispose(GObject* object) {
  ZathuraPageWidget* widget      = ZATHURA_PAGE_WIDGET(object);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);

  g_clear_object(&priv->render_request);
  g_clear_pointer(&priv->image_popover, gtk_widget_unparent);
  g_clear_object(&priv->image_actions);
  g_clear_pointer(&priv->drawing_area, gtk_widget_unparent);

  G_OBJECT_CLASS(zathura_page_widget_parent_class)->dispose(object);
}

static void zathura_page_widget_finalize(GObject* object) {
  ZathuraPageWidget* widget      = ZATHURA_PAGE_WIDGET(object);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);

  cairo_surface_destroy(priv->surface);
  cairo_surface_destroy(priv->thumbnail);
  girara_list_free(priv->search.list);
  girara_list_free(priv->links.list);
  girara_list_free(priv->signatures.list);

  G_OBJECT_CLASS(zathura_page_widget_parent_class)->finalize(object);
}

static void set_font_from_property(cairo_t* cairo, zathura_t* zathura, cairo_font_weight_t weight) {
  if (zathura == NULL) {
    return;
  }

  /* get user font description */
  g_autofree char* font = NULL;
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
    /* gtk-xft-dpi units are 1024ths of a point */
    double font_dpi = 96.0;
    if (zathura->ui.session != NULL) {
      int xft_dpi = -1;
      g_object_get(gtk_widget_get_settings(zathura->ui.session->gtk.view), "gtk-xft-dpi", &xft_dpi, NULL);
      if (xft_dpi > 0) {
        font_dpi = xft_dpi / 1024.0;
      }
    }
    size = size * font_dpi / 72;
  }

  cairo_select_font_face(cairo, family, CAIRO_FONT_SLANT_NORMAL, weight);
  cairo_set_font_size(cairo, size);

  pango_font_description_free(descr);
}

static void zathura_page_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
  ZathuraPageWidget* pageview    = ZATHURA_PAGE_WIDGET(object);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(pageview);

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
    if (priv->links.draw && !priv->links.retrieved) {
      priv->links.list      = zathura_page_links_get(priv->page, NULL);
      priv->links.retrieved = TRUE;
      priv->links.n         = (priv->links.list == NULL) ? 0 : girara_list_size(priv->links.list);
    }

    if (priv->links.retrieved && priv->links.list != NULL) {
      if (priv->drawing_area != NULL) {
        gtk_widget_queue_draw(priv->drawing_area);
      }
    }
    break;
  case PROP_LINKS_OFFSET:
    priv->links.offset = g_value_get_int(value);
    break;
  case PROP_SEARCH_RESULTS:
    if (priv->search.list != NULL && priv->search.draw) {
      if (priv->drawing_area != NULL) {
        gtk_widget_queue_draw(priv->drawing_area);
      }
    }
    girara_list_free(priv->search.list);
    priv->search.list = g_value_get_pointer(value);
    if (priv->search.list != NULL && priv->search.draw) {
      priv->links.draw = FALSE;
      if (priv->drawing_area != NULL) {
        gtk_widget_queue_draw(priv->drawing_area);
      }
    }
    priv->search.current = -1;
    break;
  case PROP_SEARCH_RESULTS_CURRENT: {
    g_return_if_fail(priv->search.list != NULL);
    if (priv->search.current >= 0 && priv->search.current < (signed)girara_list_size(priv->search.list)) {
      if (priv->drawing_area != NULL) {
        gtk_widget_queue_draw(priv->drawing_area);
      }
    }
    int val = g_value_get_int(value);
    if (val < 0) {
      priv->search.current = girara_list_size(priv->search.list);
    } else {
      priv->search.current = val;
      if (priv->search.draw && val >= 0 && val < (signed)girara_list_size(priv->search.list)) {
        if (priv->drawing_area != NULL) {
          gtk_widget_queue_draw(priv->drawing_area);
        }
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
      if (priv->drawing_area != NULL) {
        gtk_widget_queue_draw(priv->drawing_area);
      }
    }
    break;
  case PROP_DRAW_SIGNATURES:
    priv->signatures.draw = g_value_get_boolean(value);
    /* get links */
    if (priv->signatures.draw && !priv->signatures.retrieved) {
      priv->signatures.list      = zathura_page_get_signatures(priv->page, NULL);
      priv->signatures.retrieved = TRUE;
    }

    if (priv->signatures.retrieved && priv->signatures.list != NULL) {
      if (priv->drawing_area != NULL) {
        gtk_widget_queue_draw(priv->drawing_area);
      }
    }
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void zathura_page_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
  ZathuraPageWidget* pageview    = ZATHURA_PAGE_WIDGET(object);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(pageview);

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

/* test against current geometry because the cached visibility flag can lag behind layout */
static bool page_widget_on_screen(GtkWidget* widget) {
  GtkWidget* document_widget = gtk_widget_get_ancestor(widget, ZATHURA_TYPE_DOCUMENT_WIDGET);
  if (document_widget == NULL) {
    return gtk_widget_get_mapped(widget);
  }

  graphene_rect_t bounds = {0};
  if (gtk_widget_compute_bounds(widget, document_widget, &bounds) == false) {
    return false;
  }

  const graphene_rect_t view =
      GRAPHENE_RECT_INIT(0, 0, gtk_widget_get_width(document_widget), gtk_widget_get_height(document_widget));
  return graphene_rect_intersection(&view, &bounds, NULL);
}

static zathura_device_factors_t get_safe_device_factors(cairo_surface_t* surface) {
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

static void cb_page_draw(GtkDrawingArea* GIRARA_UNUSED(area), cairo_t* cairo, int width, int height, gpointer data) {
  GtkWidget* widget              = GTK_WIDGET(data);
  ZathuraPageWidget* page        = ZATHURA_PAGE_WIDGET(widget);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(page);
  zathura_t* zathura             = priv->zathura;

  zathura_document_t* document   = zathura_page_get_document(priv->page);
  const unsigned int page_height = (unsigned int)height;
  const unsigned int page_width  = (unsigned int)width;

  bool surface_exists = priv->surface != NULL || priv->thumbnail != NULL;

  if (zathura->predecessor_document != NULL && zathura->predecessor_pages != NULL && !surface_exists) {
    unsigned int page_index = zathura_page_get_index(priv->page);

    if (page_index < zathura_document_get_number_of_pages(priv->zathura->predecessor_document)) {
      /* render real page */
      if (page_widget_on_screen(widget) == true) {
        zathura_render_request(priv->render_request, g_get_real_time());
      }

      girara_debug("using predecessor page for idx %d", page_index);
      document = priv->zathura->predecessor_document;
      page     = ZATHURA_PAGE_WIDGET(priv->zathura->predecessor_pages[page_index]);
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
      girara_debug("drawing thumbnail for page %d", zathura_page_get_index(priv->page));

      const unsigned int height = cairo_image_surface_get_height(priv->thumbnail);
      const unsigned int width  = cairo_image_surface_get_width(priv->thumbnail);
      unsigned int pheight      = (rotation % 180 ? page_width : page_height);
      unsigned int pwidth       = (rotation % 180 ? page_height : page_width);

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
      if (page_widget_on_screen(widget) == true) {
        zathura_render_request(priv->render_request, g_get_real_time() + penalty);
      }
      return;
    }

    /* draw links */
    set_font_from_property(cairo, zathura, CAIRO_FONT_WEIGHT_BOLD);

    if (priv->links.draw == true && priv->links.n != 0) {
      unsigned int link_counter = 0;
      for (size_t idx = 0; idx != girara_list_size(priv->links.list); ++idx) {
        zathura_link_t* link = girara_list_nth(priv->links.list, idx);
        if (link != NULL) {
          zathura_rectangle_t rectangle = recalc_rectangle(priv->page, zathura_link_get_position(link));

          /* draw position */
          const GdkRGBA color = zathura->ui.colors.highlight_color;
          cairo_set_source_rgba(cairo, color.red, color.green, color.blue, color.alpha);
          cairo_rectangle(cairo, rectangle.x1, rectangle.y1, (rectangle.x2 - rectangle.x1),
                          (rectangle.y2 - rectangle.y1));
          cairo_fill(cairo);

          /* draw text */
          const GdkRGBA color_fg = zathura->ui.colors.highlight_color_fg;
          cairo_set_source_rgba(cairo, color_fg.red, color_fg.green, color_fg.blue, color_fg.alpha);
          cairo_move_to(cairo, rectangle.x1 + 1, rectangle.y2 - 1);
          g_autofree char* link_number = g_strdup_printf("%i", priv->links.offset + ++link_counter);
          cairo_show_text(cairo, link_number);
        }
      }
    }

    /* draw signatures */
    if (priv->signatures.draw == true && priv->signatures.list != NULL) {
      PangoLayout* layout = pango_cairo_create_layout(cairo);

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

          g_autofree char* sig_time = g_date_time_format(signature->time, "%F %T");
          text = g_strdup_printf(_("Signature is valid.\nThis document is signed by\n  %s\non %s."), signature->signer,
                                 sig_time);
          free_text = true;
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
        cairo_set_source_rgba(cairo, color_fg.red, color_fg.green, color_fg.blue, color_fg.alpha);
        pango_layout_set_text(layout, text, strlen(text));
        cairo_move_to(cairo, rectangle.x1 + 1, rectangle.y1 + 1);
        pango_cairo_show_layout(cairo, layout);
        if (free_text == true) {
          g_free(text);
        }
      }

      g_object_unref(layout);
    }

    /* draw search results */
    if (priv->search.list != NULL && priv->search.draw == true) {
      for (size_t idx = 0; idx != girara_list_size(priv->search.list); ++idx) {
        zathura_rectangle_t* rect     = girara_list_nth(priv->search.list, idx);
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);

        /* draw position */
        if ((int)idx == priv->search.current) {
          const GdkRGBA color = zathura->ui.colors.highlight_color_active;
          cairo_set_source_rgba(cairo, color.red, color.green, color.blue, color.alpha);
        } else {
          const GdkRGBA color = zathura->ui.colors.highlight_color;
          cairo_set_source_rgba(cairo, color.red, color.green, color.blue, color.alpha);
        }
        cairo_rectangle(cairo, rectangle.x1, rectangle.y1, (rectangle.x2 - rectangle.x1),
                        (rectangle.y2 - rectangle.y1));
        cairo_fill(cairo);
      }
    }
    if (priv->selection.list != NULL && priv->selection.draw == true) {
      const GdkRGBA color = priv->zathura->ui.colors.highlight_color;
      cairo_set_source_rgba(cairo, color.red, color.green, color.blue, color.alpha);
      for (size_t idx = 0; idx != girara_list_size(priv->selection.list); ++idx) {
        zathura_rectangle_t* rect     = girara_list_nth(priv->selection.list, idx);
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
        cairo_rectangle(cairo, rectangle.x1, rectangle.y1, rectangle.x2 - rectangle.x1, rectangle.y2 - rectangle.y1);
        cairo_fill(cairo);
      }
    }
    if (priv->highlighter.bounds.x1 != -1 && priv->highlighter.bounds.y1 != -1 && priv->highlighter.draw == true) {
      const GdkRGBA color = priv->zathura->ui.colors.highlight_color;
      cairo_set_source_rgba(cairo, color.red, color.green, color.blue, color.alpha);
      zathura_rectangle_t rectangle = recalc_rectangle(priv->page, priv->highlighter.bounds);
      cairo_rectangle(cairo, rectangle.x1, rectangle.y1, rectangle.x2 - rectangle.x1, rectangle.y2 - rectangle.y1);
      cairo_fill(cairo);
    }
  } else {
    /* No cached surface yet, render the on-screen page synchronously. All other rendering is asynchronous
     * This makes the rendering of the first page much faster and avoids the "Loading..." message as well */
    if (page_widget_on_screen(widget) == true) {
      cairo_surface_t* rendered = zathura_renderer_render_page(zathura->sync.render_thread, priv->page);
      if (rendered != NULL) {
        zathura_page_widget_update_surface(page, rendered, false);
        cairo_set_source_surface(cairo, rendered, 0, 0);
        cairo_paint(cairo);
        cairo_surface_destroy(rendered);
        return;
      }
    }

    girara_debug("rendering loading screen, flicker might be happening");

    GdkRGBA color_fg = priv->zathura->ui.colors.render_loading_fg;
    GdkRGBA color_bg = priv->zathura->ui.colors.render_loading_bg;
    if (zathura_renderer_recolor_enabled(priv->zathura->sync.render_thread) == true) {
      zathura_renderer_get_recolor_colors(priv->zathura->sync.render_thread, &color_bg, &color_fg);
    }

    /* set background color and draw */
    cairo_set_source_rgba(cairo, color_bg.red, color_bg.green, color_bg.blue, color_bg.alpha);
    cairo_rectangle(cairo, 0, 0, page_width, page_height);
    cairo_fill(cairo);

    bool render_loading = true;
    girara_setting_get(priv->zathura->ui.session, "render-loading", &render_loading);

    /* write text */
    if (render_loading == true) {
      cairo_set_source_rgb(cairo, color_fg.red, color_fg.green, color_fg.blue);

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

    /* request a render for every page that intersects the view */
    if (page_widget_on_screen(widget) == true) {
      zathura_render_request(priv->render_request, g_get_real_time());
    }
  }
}

static void zathura_page_widget_redraw_canvas(ZathuraPageWidget* pageview) {
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(pageview);
  if (priv->drawing_area != NULL) {
    gtk_widget_queue_draw(priv->drawing_area);
  }
}

/* smaller than max to be replaced by actual renders */
#define THUMBNAIL_INITIAL_ZOOM 0.5
/* small enough to make bilinear downscaling fast */
#define THUMBNAIL_MAX_ZOOM 0.5

static bool surface_small_enough(cairo_surface_t* surface, size_t max_size, cairo_surface_t* old) {
  if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE) {
    return true;
  }

  const unsigned int width  = cairo_image_surface_get_width(surface);
  const unsigned int height = cairo_image_surface_get_height(surface);
  const size_t new_size     = width * height;
  if (new_size > max_size) {
    return false;
  }

  if (old != NULL) {
    const unsigned int width_old  = cairo_image_surface_get_width(old);
    const unsigned int height_old = cairo_image_surface_get_height(old);
    const size_t old_size         = width_old * height_old;
    if (new_size < old_size && new_size >= old_size * THUMBNAIL_MAX_ZOOM * THUMBNAIL_MAX_ZOOM) {
      return false;
    }
  }

  return true;
}

static cairo_surface_t* draw_thumbnail_image(cairo_surface_t* surface, size_t max_size) {
  unsigned int width  = cairo_image_surface_get_width(surface);
  unsigned int height = cairo_image_surface_get_height(surface);
  double scale        = sqrt((double)max_size / (width * height)) * THUMBNAIL_INITIAL_ZOOM;
  if (scale > THUMBNAIL_MAX_ZOOM) {
    scale = THUMBNAIL_MAX_ZOOM;
  }
  width *= scale;
  height *= scale;

  /* note: this always returns 1 and 1 if Cairo too old for device scale API */
  zathura_device_factors_t device    = get_safe_device_factors(surface);
  const unsigned int unscaled_width  = width / device.x;
  const unsigned int unscaled_height = height / device.y;

  /* create thumbnail surface, taking width and height as _unscaled_ device units */
  cairo_surface_t* thumbnail =
      cairo_surface_create_similar(surface, CAIRO_CONTENT_COLOR, unscaled_width, unscaled_height);
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

void zathura_page_widget_update_surface(ZathuraPageWidget* widget, cairo_surface_t* surface, bool keep_thumbnail) {
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  unsigned int thumbnail_size    = 0;
  girara_setting_get(priv->zathura->ui.session, "page-thumbnail-size", &thumbnail_size);
  if (thumbnail_size == 0) {
    thumbnail_size = ZATHURA_PAGE_THUMBNAIL_DEFAULT_SIZE;
  }
  bool new_render = (priv->surface == NULL && priv->thumbnail == NULL);

  if (priv->surface != NULL) {
    cairo_surface_destroy(priv->surface);
    priv->surface = NULL;
  }
  if (surface != NULL) {
    priv->surface = cairo_surface_reference(surface);

    if (surface_small_enough(surface, thumbnail_size, priv->thumbnail)) {
      if (priv->thumbnail != NULL) {
        cairo_surface_destroy(priv->thumbnail);
      }
      priv->thumbnail = cairo_surface_reference(surface);
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

static void cb_update_surface(ZathuraRenderRequest* UNUSED(request), cairo_surface_t* surface, void* data) {
  ZathuraPageWidget* widget = data;
  g_return_if_fail(ZATHURA_IS_PAGE_WIDGET(widget));
  zathura_page_widget_update_surface(widget, surface, false);

  if (surface == NULL) {
    return;
  }

  /* the page is now parsed, correct its size if it differs from the placeholder */
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  zathura_document_t* document   = zathura_page_get_document(priv->page);
  unsigned int page_width = 0, page_height = 0;
  page_calc_height_width(document, priv->page, &page_height, &page_width, true);

  int cur_width = 0, cur_height = 0;
  gtk_widget_get_size_request(GTK_WIDGET(widget), &cur_width, &cur_height);
  if ((int)page_width != cur_width || (int)page_height != cur_height) {
    /* resize directly to keep the fresh surface, it was already rendered at the corrected size */
    gtk_widget_set_size_request(GTK_WIDGET(widget), (int)page_width, (int)page_height);
    if (priv->drawing_area != NULL) {
      gtk_widget_set_size_request(priv->drawing_area, (int)page_width, (int)page_height);
      gtk_widget_queue_draw(priv->drawing_area);
    }
  }

  /* refresh the statusbar so the label of a freshly parsed current page shows up */
  if (zathura_page_label_is_number(priv->page) == false && zathura_page_get_label(priv->page, NULL) != NULL &&
      zathura_page_get_index(priv->page) == zathura_document_get_current_page_number(document)) {
    statusbar_page_number_update(priv->zathura);
  }
}

static void cb_cache_added(ZathuraRenderRequest* UNUSED(request), void* data) {
  ZathuraPageWidget* widget = data;
  g_return_if_fail(ZATHURA_IS_PAGE_WIDGET(widget));

  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  priv->cached                   = true;
}

static void cb_cache_invalidated(ZathuraRenderRequest* UNUSED(request), void* data) {
  ZathuraPageWidget* widget = data;
  g_return_if_fail(ZATHURA_IS_PAGE_WIDGET(widget));

  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  if (zathura_page_widget_have_surface(widget) == true && priv->cached == true &&
      zathura_page_get_visibility(priv->page) == false) {
    /* The page was in the cache but got removed and is invisible, so get rid of
     * the surface. */
    zathura_page_widget_update_surface(widget, NULL, false);
  }
  priv->cached = false;
}

static void evaluate_link_at_mouse_position(ZathuraPageWidget* page, int oldx, int oldy) {
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(page);
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

zathura_link_t* zathura_page_widget_link_get(ZathuraPageWidget* widget, unsigned int index) {
  g_return_val_if_fail(widget != NULL, NULL);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  g_return_val_if_fail(priv != NULL, NULL);

  if (priv->links.list != NULL && index >= priv->links.offset &&
      girara_list_size(priv->links.list) > index - priv->links.offset) {
    return girara_list_nth(priv->links.list, index - priv->links.offset);
  } else {
    return NULL;
  }
}

static void rotate_point(zathura_t* zathura, unsigned int page, double orig_x, double orig_y, double* x, double* y) {
  zathura_document_t* document = zathura_get_document(zathura);
  const unsigned int rotation  = zathura_document_get_rotation(document);
  if (rotation == 0) {
    *x = orig_x;
    *y = orig_y;
    return;
  }

  unsigned int height, width;
  zathura_document_widget_get_cell_size(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget), page, &height, &width);
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

void zathura_page_widget_clear_selection(ZathuraPageWidget* widget) {
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  if (priv->selection.list != NULL) {
    girara_list_free(priv->selection.list);
    priv->selection.list = NULL;
  }
  priv->selection.draw   = false;
  priv->highlighter.draw = false;
  zathura_page_widget_redraw_canvas(widget);
}

static void cb_zathura_page_widget_button_press_event(GtkGestureClick* gesture, gint n_press, gdouble bx, gdouble by,
                                                      gpointer data) {
  GtkWidget* widget              = GTK_WIDGET(data);
  ZathuraPageWidget* page        = ZATHURA_PAGE_WIDGET(widget);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(page);

  const guint gbutton   = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
  GdkModifierType state = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));

  /* yield to configured mouse bindings (dispatched by the view gestures) instead of starting a selection */
  girara_event_type_t etype = GIRARA_EVENT_BUTTON_PRESS;
  if (n_press == 2) {
    etype = GIRARA_EVENT_2BUTTON_PRESS;
  } else if (n_press == 3) {
    etype = GIRARA_EVENT_3BUTTON_PRESS;
  }
  if (priv->zathura != NULL && priv->zathura->ui.session != NULL &&
      girara_has_mouse_event(priv->zathura->ui.session, etype, gbutton, state) == true) {
    return;
  }

  if (gbutton == GDK_BUTTON_PRIMARY) {
    zathura_page_widget_clear_selection(page);

    if (n_press == 1) {
      /* clear pages with a selection already */
      if (priv->zathura != NULL && priv->zathura->pages != NULL) {
        zathura_document_t* document = zathura_page_get_document(priv->page);
        if (document != NULL) {
          unsigned int number_of_pages = zathura_document_get_number_of_pages(document);
          for (unsigned int i = 0; i < number_of_pages; i++) {
            ZathuraPageWidget* other_page        = ZATHURA_PAGE_WIDGET(priv->zathura->pages[i]);
            ZathuraPageWidgetPrivate* other_priv = zathura_page_widget_get_instance_private(other_page);

            if (other_priv->selection.draw == true || other_priv->highlighter.draw == true) {
              zathura_page_widget_clear_selection(other_page);
            }
          }
        }
      }

      /* start the selection */
      double x, y;
      rotate_point(priv->zathura, zathura_page_get_index(priv->page), bx, by, &x, &y);
      priv->mouse.selection.x1 = x;
      priv->mouse.selection.y1 = y;
      priv->mouse.selection.x2 = x;
      priv->mouse.selection.y2 = y;
    } else if (n_press == 2 || n_press == 3) {
      /* abort the selection */
      priv->mouse.selection.x1 = -1;
      priv->mouse.selection.y1 = -1;
      priv->mouse.selection.x2 = -1;
      priv->mouse.selection.y2 = -1;
    }
  } else if (gbutton == GDK_BUTTON_SECONDARY && n_press == 1) {
    zathura_page_widget_popup_menu(widget, bx, by);
  }
}

static void cb_zathura_page_widget_button_release_event(GtkGestureClick* gesture, gint UNUSED(n_press), gdouble bx,
                                                        gdouble by, gpointer data) {
  GtkWidget* widget              = GTK_WIDGET(data);
  ZathuraPageWidget* page        = ZATHURA_PAGE_WIDGET(widget);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(page);

  zathura_document_t* document = zathura_page_get_document(priv->page);
  const double scale           = zathura_document_get_scale(document);

  const int oldx        = bx;
  const int oldy        = by;
  const guint gbutton   = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
  GdkModifierType state = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));

  /* signal payload, gtk4 events cannot be constructed by hand */
  scaled_button_release_event_t srelease = {.x = bx / scale, .y = by / scale, .button = gbutton, .state = state};
  g_signal_emit(page, signals[BUTTON_RELEASE], 0, &srelease);

  if (gbutton != GDK_BUTTON_PRIMARY) {
    return;
  }

  if (priv->mouse.selection.x2 == -1 && priv->mouse.selection.y2 == -1) {
    /* simple single click */
    /* get links */
    if (priv->zathura->global.double_click_follow) {
      evaluate_link_at_mouse_position(page, oldx, oldy);
    }
  } else if (priv->selection.list != NULL) {
    zathura_rectangle_t tmp = priv->mouse.selection;

    tmp.x1 /= scale;
    tmp.x2 /= scale;
    tmp.y1 /= scale;
    tmp.y2 /= scale;

    g_autofree char* text = zathura_page_get_text(priv->page, tmp, NULL);
    if (text != NULL && *text != '\0') {
      /* emit text-selected signal */
      g_signal_emit(page, signals[TEXT_SELECTED], 0, text);
    } else if (priv->zathura->global.double_click_follow == false) {
      evaluate_link_at_mouse_position(page, oldx, oldy);
    }
  }

  priv->mouse.selection.x1 = -1;
  priv->mouse.selection.y1 = -1;
  priv->mouse.selection.x2 = -1;
  priv->mouse.selection.y2 = -1;
}

static zathura_rectangle_t next_selection_rectangle(double basepoint_x, double basepoint_y, double next_x,
                                                    double next_y) {
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

static void cb_zathura_page_widget_motion_notify(GtkEventControllerMotion* controller, gdouble ex, gdouble ey,
                                                 gpointer data) {
  GtkWidget* widget              = GTK_WIDGET(data);
  ZathuraPageWidget* page        = ZATHURA_PAGE_WIDGET(widget);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(page);

  zathura_document_t* document = zathura_page_get_document(priv->page);
  const double scale           = zathura_document_get_scale(document);
  GdkModifierType evstate      = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(controller));

  if (evstate & GDK_BUTTON1_MASK) { /* holding left mouse button */
    zathura_page_widget_clear_selection(page);
    if (evstate & priv->zathura->global.highlighter_modmask) {
      double x, y;
      rotate_point(priv->zathura, zathura_page_get_index(priv->page), ex, ey, &x, &y);
      priv->highlighter.bounds = next_selection_rectangle(priv->mouse.selection.x1, priv->mouse.selection.y1, x, y);
      priv->highlighter.bounds.x1 /= scale;
      priv->highlighter.bounds.y1 /= scale;
      priv->highlighter.bounds.x2 /= scale;
      priv->highlighter.bounds.y2 /= scale;

      priv->highlighter.draw = true;
      zathura_page_widget_redraw_canvas(page);
    } else {
      /* calculate next selection */
      rotate_point(priv->zathura, zathura_page_get_index(priv->page), ex, ey, &priv->mouse.selection.x2,
                   &priv->mouse.selection.y2);

      zathura_rectangle_t selection = priv->mouse.selection;
      selection.x1 /= scale;
      selection.y1 /= scale;
      selection.x2 /= scale;
      selection.y2 /= scale;

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
        if (rect.x1 <= ex && rect.x2 >= ex && rect.y1 <= ey && rect.y2 >= ey) {
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
}

static void cb_zathura_page_widget_leave_notify(GtkEventControllerMotion* UNUSED(controller), gpointer data) {
  ZathuraPageWidget* page        = ZATHURA_PAGE_WIDGET(data);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(page);

  bool keep_selection = false;
  girara_setting_get(priv->zathura->ui.session, "selection-keep-highlight", &keep_selection);

  if (keep_selection == false) {
    zathura_page_widget_clear_selection(page);
  }

  if (priv->mouse.over_link == true) {
    g_signal_emit(page, signals[LEAVE_LINK], 0);
    priv->mouse.over_link = false;
  }
}

/* show context menu for the image under the click position */
static void zathura_page_widget_popup_menu(GtkWidget* widget, double x, double y) {
  g_return_if_fail(widget != NULL);

  ZathuraPageWidget* page        = ZATHURA_PAGE_WIDGET(widget);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(page);

  if (priv->images.retrieved == false) {
    priv->images.list      = zathura_page_images_get(priv->page, NULL);
    priv->images.retrieved = true;
  }

  if (priv->images.list == NULL) {
    return;
  }

  /* find the image under the click position */
  zathura_image_t* image = NULL;
  for (size_t idx = 0; idx != girara_list_size(priv->images.list); ++idx) {
    zathura_image_t* image_it = girara_list_nth(priv->images.list, idx);
    zathura_rectangle_t rect  = recalc_rectangle(priv->page, image_it->position);
    if (rect.x1 <= x && rect.x2 >= x && rect.y1 <= y && rect.y2 >= y) {
      image = image_it;
    }
  }

  if (image == NULL) {
    return;
  }

  priv->images.current = image;

  /* lazily create the popover so unused page widgets do not pay for it */
  /* keeping it alive across right clicks avoids the action lookup failure */
  /* when unparenting during the closed signal */
  if (priv->image_popover == NULL) {
    GMenu* model = g_menu_new();
    g_menu_append(model, _("Copy image"), "image.copy");
    g_menu_append(model, _("Save image as"), "image.save");

    priv->image_popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(model));
    g_object_unref(model);

    gtk_widget_set_parent(priv->image_popover, widget);
    gtk_popover_set_has_arrow(GTK_POPOVER(priv->image_popover), FALSE);
  }

  gtk_popover_set_pointing_to(GTK_POPOVER(priv->image_popover),
                              &(GdkRectangle){.x = (int)x, .y = (int)y, .width = 1, .height = 1});
  gtk_popover_popup(GTK_POPOVER(priv->image_popover));
}

static void cb_menu_image_copy(GSimpleAction* UNUSED(action), GVariant* UNUSED(parameter), gpointer data) {
  ZathuraPageWidget* page = ZATHURA_PAGE_WIDGET(data);
  g_return_if_fail(page != NULL);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(page);
  if (priv->images.current == NULL) {
    return;
  }

  cairo_surface_t* surface = zathura_page_image_get_cairo(priv->page, priv->images.current, NULL);
  if (surface == NULL) {
    return;
  }

  /* build a GdkTexture from the cairo image surface */
  /* the image-selected signal carries a texture for the gtk4 clipboard */
  cairo_surface_flush(surface);
  if (cairo_image_surface_get_format(surface) != CAIRO_FORMAT_ARGB32) {
    /* plugins commonly return RGB24; convert so the copy still works */
    cairo_surface_t* converted = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, cairo_image_surface_get_width(surface),
                                                            cairo_image_surface_get_height(surface));
    if (cairo_surface_status(converted) != CAIRO_STATUS_SUCCESS) {
      cairo_surface_destroy(converted);
      cairo_surface_destroy(surface);
      priv->images.current = NULL;
      return;
    }
    cairo_t* cr = cairo_create(converted);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_flush(converted);
    cairo_surface_destroy(surface);
    surface = converted;
  }
  const int width      = cairo_image_surface_get_width(surface);
  const int height     = cairo_image_surface_get_height(surface);
  const int stride     = cairo_image_surface_get_stride(surface);
  const guchar* pixels = cairo_image_surface_get_data(surface);
  /* cairo native-endian ARGB32 maps to b8g8r8a8 on little endian and a8r8g8b8 on big endian */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  const GdkMemoryFormat format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED;
#else
  const GdkMemoryFormat format = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED;
#endif
  GBytes* bytes       = g_bytes_new(pixels, (gsize)stride * (gsize)height);
  GdkTexture* texture = gdk_memory_texture_new(width, height, format, bytes, (gsize)stride);
  g_bytes_unref(bytes);

  if (texture != NULL) {
    g_signal_emit(page, signals[IMAGE_SELECTED], 0, texture);
    g_object_unref(texture);
  }
  cairo_surface_destroy(surface);

  priv->images.current = NULL;
}

static void cb_menu_image_save(GSimpleAction* UNUSED(action), GVariant* UNUSED(parameter), gpointer data) {
  ZathuraPageWidget* page = ZATHURA_PAGE_WIDGET(data);
  g_return_if_fail(page != NULL);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(page);
  if (priv->images.current == NULL || priv->images.list == NULL) {
    return;
  }

  /* generate the image identifier used by :export */
  unsigned int page_id  = zathura_page_get_index(priv->page) + 1;
  unsigned int image_id = 1;
  for (size_t idx = 0; idx != girara_list_size(priv->images.list); ++idx, ++image_id) {
    zathura_image_t* image_it = girara_list_nth(priv->images.list, idx);
    if (image_it == priv->images.current) {
      break;
    }
  }

  g_autofree char* export_command = g_strdup_printf(":export image-p%d-%d ", page_id, image_id);
  girara_argument_t argument      = {.n = 0, .data = export_command};
  sc_focus_inputbar(priv->zathura->ui.session, &argument, NULL, 0);

  priv->images.current = NULL;
}

void zathura_page_widget_update_view_time(ZathuraPageWidget* widget) {
  g_return_if_fail(ZATHURA_IS_PAGE_WIDGET(widget));
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);

  if (zathura_page_get_visibility(priv->page) == true) {
    zathura_render_request_update_view_time(priv->render_request);
  }
  if (priv->surface == NULL) {
    zathura_render_request(priv->render_request, g_get_real_time());
  }
}

bool zathura_page_widget_have_surface(ZathuraPageWidget* widget) {
  g_return_val_if_fail(ZATHURA_IS_PAGE_WIDGET(widget), false);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  return priv->surface != NULL;
}

void zathura_page_widget_abort_render_request(ZathuraPageWidget* widget) {
  g_return_if_fail(ZATHURA_IS_PAGE_WIDGET(widget));
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  zathura_render_request_abort(priv->render_request);

  /* Make sure that if we are not cached and invisible, that there is no
   * surface.
   *
   * TODO: Maybe this should be moved somewhere else. */
  if (zathura_page_widget_have_surface(widget) == true && priv->cached == false) {
    zathura_page_widget_update_surface(widget, NULL, false);
  }
}

zathura_page_t* zathura_page_widget_get_page(ZathuraPageWidget* widget) {
  g_return_val_if_fail(ZATHURA_IS_PAGE_WIDGET(widget), NULL);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);

  return priv->page;
}

void zathura_page_widget_set_size_request(ZathuraPageWidget* widget, int width, int height) {
  g_return_if_fail(widget != NULL);
  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  gtk_widget_set_size_request(GTK_WIDGET(widget), width, height);
  if (priv->drawing_area != NULL) {
    gtk_widget_set_size_request(priv->drawing_area, width, height);
  }

  zathura_page_widget_abort_render_request(widget);
  zathura_page_widget_update_surface(widget, NULL, true);

  /* queue_resize does not imply queue_draw in gtk4, ensure the drawing area runs cb_page_draw */
  if (priv->drawing_area != NULL) {
    gtk_widget_queue_draw(priv->drawing_area);
  }
}

void zathura_page_widget_clear_thumbnail(ZathuraPageWidget* widget) {
  g_return_if_fail(widget != NULL);

  ZathuraPageWidgetPrivate* priv = zathura_page_widget_get_instance_private(widget);
  cairo_surface_destroy(priv->thumbnail);
  priv->thumbnail = NULL;
}
