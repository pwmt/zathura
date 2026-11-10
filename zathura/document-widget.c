/* SPDX-License-Identifier: Zlib */

#include "document-widget.h"

#include <girara-gtk/settings.h>
#include <girara/log.h>
#include <math.h>

#include "adjustment.h"
#include "callbacks.h"
#include "page-widget.h"
#include "page.h"
#include "render.h"
#include "utils.h"
#include "zathura.h"

typedef struct {
  unsigned int pos;
  unsigned int size;
} document_widget_line_s;

typedef struct zathura_document_widget_private_s {
  zathura_t* zathura;
  zathura_document_t* document;
  ZathuraRenderer* renderer;
  GtkWidget** pages;
  guint page_widget_preload_source;
  unsigned int page_widget_preload_next;
  bool page_widgets_loaded;
  bool draw_signatures;

  /* Layout */
  document_widget_mode_t layout_mode;
  bool mode_change_pending; /**< Track pending layout mode changes; FIXME: remove this workaround */
  bool pages_right_to_left;
  unsigned int nrow;
  unsigned int ncol;
  document_widget_line_s* row_heights;
  document_widget_line_s* col_widths;
  unsigned int pages_per_row;        /**< number of pages in a row */
  unsigned int first_page_column;    /**< column of the first page */
  unsigned int page_v_padding;       /**< padding between pages */
  unsigned int page_h_padding;       /**< padding between pages */
  zathura_adjust_mode_t adjust_mode; /**< Adjust mode (best-fit, width) */
  unsigned int view_width;           /**< width of current viewport */
  unsigned int view_height;          /**< height of current viewport */
  double position_x;                 /**< X adjustment */
  double position_y;                 /**< Y adjustment */
  int alloc_width;
  int alloc_height;

  GtkWidget* grid;

  /* Scrolling */
  GtkAdjustment* hadjustment;
  GtkAdjustment* vadjustment;
  GtkScrollablePolicy hscroll_policy;
  GtkScrollablePolicy vscroll_policy;
} ZathuraDocumentWidgetPrivate;

G_DEFINE_TYPE_WITH_CODE(ZathuraDocumentWidget, zathura_document_widget, GTK_TYPE_WIDGET,
                        G_ADD_PRIVATE(ZathuraDocumentWidget) G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))

/* places the page widgets at the positions of the computed layout like GtkGrid but without needing every page */
G_DECLARE_FINAL_TYPE(ZathuraPageGrid, zathura_page_grid, ZATHURA, PAGE_GRID, GtkWidget)

struct _ZathuraPageGrid {
  GtkWidget parent_instance;
};

G_DEFINE_FINAL_TYPE(ZathuraPageGrid, zathura_page_grid, GTK_TYPE_WIDGET)

static void zathura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value,
                                                 GParamSpec* pspec);
static void zathura_document_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void zathura_document_widget_size_allocate(GtkWidget* widget, int width, int height, int baseline);
static void zathura_document_widget_measure(GtkWidget* widget, GtkOrientation orientation, int for_size, int* minimum,
                                            int* natural, int* minimum_baseline, int* natural_baseline);
static void zathura_document_widget_dispose(GObject* object);
static void zathura_document_widget_finalize(GObject* object);
static gboolean zathura_document_widget_preload_pages(gpointer data);

enum signals_e {
  PAGE_WIDGETS_LOADED,
  READY,
  RENDER_FAILED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

enum properties_e {
  PROP_0,
  PROP_ZATHURA,
  PROP_LAYOUT_MODE,
  PROP_PAGES_RIGHT_TO_LEFT,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
};

static void zathura_document_widget_class_init(ZathuraDocumentWidgetClass* class) {
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
  widget_class->size_allocate  = zathura_document_widget_size_allocate;
  widget_class->measure        = zathura_document_widget_measure;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->set_property = zathura_document_widget_set_property;
  object_class->get_property = zathura_document_widget_get_property;
  object_class->dispose      = zathura_document_widget_dispose;
  object_class->finalize     = zathura_document_widget_finalize;

  g_object_class_install_property(
      object_class, PROP_ZATHURA,
      g_param_spec_pointer("zathura", "zathura", "the zathura instance",
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(object_class, PROP_LAYOUT_MODE,
                                  g_param_spec_int("layout-mode", "layout-mode", "set the page layout mode", 0,
                                                   DOCUMENT_WIDGET_MODE_COUNT, DOCUMENT_WIDGET_GRID,
                                                   G_PARAM_WRITABLE | G_PARAM_READABLE));

  g_object_class_install_property(object_class, PROP_PAGES_RIGHT_TO_LEFT,
                                  g_param_spec_boolean("pages-right-to-left", "pages-right-to-left",
                                                       "layout pages left to right", false,
                                                       G_PARAM_WRITABLE | G_PARAM_READABLE));

  g_object_class_override_property(object_class, PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property(object_class, PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property(object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property(object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  signals[PAGE_WIDGETS_LOADED] = g_signal_new("page-widgets-loaded", ZATHURA_TYPE_DOCUMENT_WIDGET, G_SIGNAL_RUN_LAST, 0,
                                              NULL, NULL, g_cclosure_marshal_generic, G_TYPE_NONE, 0);

  signals[READY] = g_signal_new("ready", ZATHURA_TYPE_DOCUMENT_WIDGET, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                g_cclosure_marshal_generic, G_TYPE_NONE, 0);

  signals[RENDER_FAILED] = g_signal_new("render-failed", ZATHURA_TYPE_DOCUMENT_WIDGET, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                        g_cclosure_marshal_generic, G_TYPE_NONE, 0);
}

static void zathura_document_widget_init(ZathuraDocumentWidget* widget) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);

  priv->zathura                    = NULL;
  priv->document                   = NULL;
  priv->pages                      = NULL;
  priv->page_widget_preload_source = 0;
  priv->page_widget_preload_next   = 0;
  priv->page_widgets_loaded        = false;
  priv->draw_signatures            = false;
  priv->layout_mode                = DOCUMENT_WIDGET_GRID;
  priv->pages_per_row              = 1;
  priv->first_page_column          = 1;
  priv->nrow                       = 0;
  priv->ncol                       = 0;
  priv->row_heights                = NULL;
  priv->col_widths                 = NULL;

  /* clip the offset grid like GtkViewport does; gtk4 widgets do not clip children by default */
  gtk_widget_set_overflow(GTK_WIDGET(widget), GTK_OVERFLOW_HIDDEN);

  priv->grid = g_object_new(zathura_page_grid_get_type(), NULL);
  gtk_widget_set_halign(priv->grid, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(priv->grid, GTK_ALIGN_CENTER);
  gtk_widget_set_parent(priv->grid, GTK_WIDGET(widget));
}

static ZathuraRenderer* create_renderer(zathura_t* zathura) {
  /* page cache size */
  unsigned int cache_size = 0;
  girara_setting_get(zathura->ui.session, "page-cache-size", &cache_size);
  if (cache_size == 0) {
    girara_warning("page-cache-size is not positive, using %u instead", ZATHURA_PAGE_CACHE_DEFAULT_SIZE);
    cache_size = ZATHURA_PAGE_CACHE_DEFAULT_SIZE;
  }

  girara_debug("starting renderer with cache size %u", cache_size);
  ZathuraRenderer* renderer = zathura_renderer_new(cache_size);
  if (!renderer) {
    return NULL;
  }

  return renderer;
}

GtkWidget* zathura_document_widget_new(zathura_t* zathura, zathura_document_t* zathura_document) {
  g_return_val_if_fail(zathura_document != NULL, NULL);
  g_return_val_if_fail(zathura != NULL, NULL);

  GObject* ret = g_object_new(ZATHURA_TYPE_DOCUMENT_WIDGET, "zathura", zathura, NULL);
  if (ret == NULL) {
    return NULL;
  }

  ZathuraDocumentWidget* widget = ZATHURA_DOCUMENT_WIDGET(ret);
  GtkWidget* gtk_widget         = GTK_WIDGET(widget);

  if (!zathura_document_widget_set_document(widget, zathura_document)) {
    g_object_unref(ret);
    return NULL;
  }

  return gtk_widget;
}

bool zathura_document_widget_set_document(ZathuraDocumentWidget* document_widget, zathura_document_t* document) {
  g_return_val_if_fail(document_widget != NULL, false);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document_widget);
  zathura_document_widget_clear_pages(document_widget);

  if (!document) {
    return true;
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(document);
  g_autofree GtkWidget** pages       = g_try_malloc0_n(number_of_pages, sizeof(GtkWidget*));
  if (!pages) {
    return false;
  }

  ZathuraRenderer* renderer = create_renderer(priv->zathura);
  if (!renderer) {
    return false;
  }

  priv->renderer            = renderer;
  priv->document            = document;
  priv->pages               = g_steal_pointer(&pages);
  priv->page_widgets_loaded = false;

  zathura_document_widget_apply_recolor_settings(document_widget);

  return true;
}

zathura_document_t* zathura_document_widget_get_document(ZathuraDocumentWidget* document) {
  g_return_val_if_fail(document != NULL, NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->document;
}

static void invalidate_recolor(ZathuraDocumentWidget* document) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  /* Initial configuration precedes layout and cannot have cached surfaces yet. */
  if (priv->row_heights != NULL && priv->col_widths != NULL) {
    zathura_document_widget_render_all(document);
    zathura_document_widget_clear_thumbnails(document);
  }
}

static bool invalidate_recolor_if_changed(ZathuraDocumentWidget* document, bool changed) {
  if (changed) {
    invalidate_recolor(document);
  }

  return changed;
}

typedef enum {
  RECOLOR_OPTION_ENABLED,
  RECOLOR_OPTION_HUE,
  RECOLOR_OPTION_REVERSE_VIDEO,
  RECOLOR_OPTION_ADJUST_LIGHTNESS,
} recolor_option_t;

static bool set_recolor_option(ZathuraDocumentWidget* document, recolor_option_t option, bool enable) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (!priv->renderer) {
    return false;
  }

  switch (option) {
  case RECOLOR_OPTION_ENABLED:
    if (zathura_renderer_recolor_enabled(priv->renderer) == enable) {
      return false;
    }
    zathura_renderer_enable_recolor(priv->renderer, enable);
    break;
  case RECOLOR_OPTION_HUE:
    if (zathura_renderer_recolor_hue_enabled(priv->renderer) == enable) {
      return false;
    }
    zathura_renderer_enable_recolor_hue(priv->renderer, enable);
    break;
  case RECOLOR_OPTION_REVERSE_VIDEO:
    if (zathura_renderer_recolor_reverse_video_enabled(priv->renderer) == enable) {
      return false;
    }
    zathura_renderer_enable_recolor_reverse_video(priv->renderer, enable);
    break;
  case RECOLOR_OPTION_ADJUST_LIGHTNESS:
    if (zathura_renderer_recolor_adjust_lightness_enabled(priv->renderer) == enable) {
      return false;
    }
    zathura_renderer_enable_recolor_adjust_lightness(priv->renderer, enable);
    break;
  default:
    g_return_val_if_reached(false);
  }

  return true;
}

bool zathura_document_widget_set_recolor(ZathuraDocumentWidget* document, bool enable) {
  g_return_val_if_fail(document, false);

  return invalidate_recolor_if_changed(document, set_recolor_option(document, RECOLOR_OPTION_ENABLED, enable));
}

bool zathura_document_widget_set_recolor_hue(ZathuraDocumentWidget* document, bool enable) {
  g_return_val_if_fail(document, false);

  return invalidate_recolor_if_changed(document, set_recolor_option(document, RECOLOR_OPTION_HUE, enable));
}

bool zathura_document_widget_set_recolor_reverse_video(ZathuraDocumentWidget* document, bool enable) {
  g_return_val_if_fail(document, false);

  return invalidate_recolor_if_changed(document, set_recolor_option(document, RECOLOR_OPTION_REVERSE_VIDEO, enable));
}

bool zathura_document_widget_set_recolor_adjust_lightness(ZathuraDocumentWidget* document, bool enable) {
  g_return_val_if_fail(document, false);

  return invalidate_recolor_if_changed(document, set_recolor_option(document, RECOLOR_OPTION_ADJUST_LIGHTNESS, enable));
}

static bool set_recolor_colors(ZathuraDocumentWidget* document, const char* light, const char* dark) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (!priv->renderer) {
    return false;
  }

  GdkRGBA old_light;
  GdkRGBA old_dark;
  zathura_renderer_get_recolor_colors(priv->renderer, &old_light, &old_dark);

  GdkRGBA new_light = old_light;
  GdkRGBA new_dark  = old_dark;
  if (light != NULL && !parse_color(&new_light, light)) {
    new_light = old_light;
  }
  if (dark != NULL && !parse_color(&new_dark, dark)) {
    new_dark = old_dark;
  }

  if (gdk_rgba_equal(&old_light, &new_light) && gdk_rgba_equal(&old_dark, &new_dark)) {
    // both colors are equal, so no need to update
    return false;
  }

  zathura_renderer_set_recolor_colors(priv->renderer, &new_light, &new_dark);
  return true;
}

bool zathura_document_widget_set_recolor_colors(ZathuraDocumentWidget* document, const char* light, const char* dark) {
  g_return_val_if_fail(document, false);

  return invalidate_recolor_if_changed(document, set_recolor_colors(document, light, dark));
}

static const struct {
  const char* setting;
  recolor_option_t option;
} options[] = {
    {"recolor", RECOLOR_OPTION_ENABLED},
    {"recolor-keephue", RECOLOR_OPTION_HUE},
    {"recolor-reverse-video", RECOLOR_OPTION_REVERSE_VIDEO},
    {"recolor-adjust-lightness", RECOLOR_OPTION_ADJUST_LIGHTNESS},
};

void zathura_document_widget_apply_recolor_settings(ZathuraDocumentWidget* document) {
  g_return_if_fail(document);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  bool changed = false;
  for (size_t i = 0; i < G_N_ELEMENTS(options); ++i) {
    bool enable = false;

    girara_setting_get(priv->zathura->ui.session, options[i].setting, &enable);
    changed |= set_recolor_option(document, options[i].option, enable);
  }

  g_autofree char* light = NULL;
  g_autofree char* dark  = NULL;
  girara_setting_get(priv->zathura->ui.session, "recolor-lightcolor", &light);
  girara_setting_get(priv->zathura->ui.session, "recolor-darkcolor", &dark);

  changed |= set_recolor_colors(document, light, dark);
  invalidate_recolor_if_changed(document, changed);
}

void zathura_document_widget_stop(ZathuraDocumentWidget* document) {
  g_return_if_fail(document);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  zathura_document_widget_stop_page_widget_preload(document);
  if (priv->renderer) {
    zathura_renderer_stop(priv->renderer);
  }
}

GtkWidget* zathura_document_widget_get_page(ZathuraDocumentWidget* document, unsigned int page_number) {
  g_return_val_if_fail(document != NULL, NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->document == NULL) {
    return NULL;
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);

  if (priv->pages == NULL || page_number >= number_of_pages) {
    return NULL;
  }

  return priv->pages[page_number];
}

static void zathura_document_widget_set_scroll_adjustment(ZathuraDocumentWidget* widget, GtkOrientation orientation,
                                                          GtkAdjustment* adjustment) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);
  GtkAdjustment** to_set;
  const gchar* prop_name;

  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    to_set    = &priv->hadjustment;
    prop_name = "hadjustment";
  } else {
    to_set    = &priv->vadjustment;
    prop_name = "vadjustment";
  }

  if (adjustment && adjustment == *to_set) {
    return;
  }

  if (*to_set) {
    g_signal_handlers_disconnect_by_data(*to_set, widget);
    g_object_unref(*to_set);
  }

  if (!adjustment) {
    adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  }

  g_signal_connect_object(adjustment, "value-changed", G_CALLBACK(gtk_widget_queue_allocate), widget,
                          G_CONNECT_SWAPPED);

  *to_set = g_object_ref_sink(adjustment);

  g_object_notify(G_OBJECT(widget), prop_name);
}

static void zathura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value,
                                                 GParamSpec* pspec) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(object);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  switch (prop_id) {
  case PROP_ZATHURA:
    priv->zathura = g_value_get_pointer(value);
    break;
  case PROP_LAYOUT_MODE: {
    const document_widget_mode_t new_mode = g_value_get_int(value);
    if (priv->layout_mode == new_mode) {
      break;
    }
    /* Keep the selected page authoritative until the new geometry is allocated. */
    priv->mode_change_pending = priv->document != NULL;
    priv->layout_mode         = g_value_get_int(value);
    zathura_document_widget_update_mode(document);
    gtk_widget_queue_allocate(GTK_WIDGET(document));
    break;
  }
  case PROP_PAGES_RIGHT_TO_LEFT:
    priv->pages_right_to_left = g_value_get_boolean(value);
    break;
  case PROP_HADJUSTMENT:
    zathura_document_widget_set_scroll_adjustment(document, GTK_ORIENTATION_HORIZONTAL, g_value_get_object(value));
    break;
  case PROP_VADJUSTMENT:
    zathura_document_widget_set_scroll_adjustment(document, GTK_ORIENTATION_VERTICAL, g_value_get_object(value));
    break;
  case PROP_HSCROLL_POLICY:
    priv->hscroll_policy = g_value_get_enum(value);
    gtk_widget_queue_resize(GTK_WIDGET(document));
    break;
  case PROP_VSCROLL_POLICY:
    priv->vscroll_policy = g_value_get_enum(value);
    gtk_widget_queue_resize(GTK_WIDGET(document));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void zathura_document_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(object);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  switch (prop_id) {
  case PROP_HADJUSTMENT:
    g_value_set_object(value, priv->hadjustment);
    break;
  case PROP_LAYOUT_MODE:
    g_value_set_int(value, priv->layout_mode);
    break;
  case PROP_PAGES_RIGHT_TO_LEFT:
    g_value_set_boolean(value, priv->pages_right_to_left);
    break;
  case PROP_VADJUSTMENT:
    g_value_set_object(value, priv->vadjustment);
    break;
  case PROP_HSCROLL_POLICY:
    g_value_set_enum(value, priv->hscroll_policy);
    break;
  case PROP_VSCROLL_POLICY:
    g_value_set_enum(value, priv->vscroll_policy);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

/* drawing */
static void zathura_document_widget_get_page_position(ZathuraDocumentWidget* document, unsigned int page_index,
                                                      unsigned int* row, unsigned int* col) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  const unsigned int c0   = priv->first_page_column;
  const unsigned int ncol = priv->pages_per_row;

  *row = (page_index + c0 - 1) / ncol;
  *col = (page_index + c0 - 1) % ncol;
}

static void zathura_page_grid_measure(GtkWidget* widget, GtkOrientation orientation, int UNUSED(for_size), int* minimum,
                                      int* natural, int* minimum_baseline, int* natural_baseline) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(gtk_widget_get_parent(widget));
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  /* measure the pages like any container does so their later size changes reach the grid */
  for (GtkWidget* child = gtk_widget_get_first_child(widget); child != NULL;
       child            = gtk_widget_get_next_sibling(child)) {
    if (gtk_widget_should_layout(child) == true) {
      gtk_widget_measure(child, orientation, -1, NULL, NULL, NULL, NULL);
    }
  }

  unsigned int height = 0, width = 0;
  if (priv->layout_mode == DOCUMENT_WIDGET_SINGLE && priv->document != NULL) {
    zathura_page_t* page =
        zathura_document_get_page(priv->document, zathura_document_get_current_page_number(priv->document));
    page_calc_height_width(priv->document, page, &height, &width, true);
  } else {
    zathura_document_widget_get_document_size(document, &height, &width);
  }
  *minimum = *natural = orientation == GTK_ORIENTATION_HORIZONTAL ? (int)width : (int)height;
  *minimum_baseline = *natural_baseline = -1;
}

/* Calculate position of top left corner of the page relative the document */
static void calculate_page_origin(ZathuraDocumentWidget* document, unsigned int page_index, unsigned int page_width,
                                  unsigned int page_height, unsigned int grid_width, bool rtl, unsigned int* x,
                                  unsigned int* y) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->layout_mode == DOCUMENT_WIDGET_SINGLE) {
    *x = 0;
    *y = 0;
    return;
  }

  unsigned int row = 0;
  unsigned int col = 0;
  zathura_document_widget_get_page_position(document, page_index, &row, &col);

  const unsigned int column = priv->pages_right_to_left ? priv->ncol - 1 - col : col;

  *x = priv->col_widths[column].pos;
  if (priv->ncol == 1) {
    *x += ((int)priv->col_widths[column].size - page_width) / 2;
  } else if (column == 0) {
    *x += (int)priv->col_widths[column].size - page_width;
  }
  if (rtl) {
    *x = grid_width - *x - page_width;
  }

  *y = priv->row_heights[row].pos + ((int)priv->row_heights[row].size - page_height) / 2;
}

static void zathura_page_grid_size_allocate(GtkWidget* widget, int width, int UNUSED(height), int UNUSED(baseline)) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(gtk_widget_get_parent(widget));
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->row_heights == NULL || priv->col_widths == NULL) {
    return;
  }

  const bool rtl                     = gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL;
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);
  for (unsigned int i = 0; i < number_of_pages; i++) {
    GtkWidget* page_widget = priv->pages[i];
    if (page_widget == NULL || gtk_widget_get_parent(page_widget) != widget ||
        gtk_widget_should_layout(page_widget) == false) {
      continue;
    }

    GtkRequisition size;
    gtk_widget_get_preferred_size(page_widget, &size, NULL);

    /* the single visible page fills the grid, other pages are aligned in their cell towards the middle */
    unsigned int x = 0;
    unsigned int y = 0;
    calculate_page_origin(document, i, size.width, size.height, width, rtl, &x, &y);

    GtkAllocation allocation = {.x = x, .y = y, .width = size.width, .height = size.height};
    gtk_widget_size_allocate(page_widget, &allocation, -1);
  }
}

static void zathura_page_grid_class_init(ZathuraPageGridClass* class) {
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
  widget_class->measure        = zathura_page_grid_measure;
  widget_class->size_allocate  = zathura_page_grid_size_allocate;
}

static void zathura_page_grid_init(ZathuraPageGrid* UNUSED(grid)) {}

static void zathura_document_widget_line_prefix_sum(document_widget_line_s* array, unsigned int n, unsigned int pad) {
  array[0].pos = 0;

  for (unsigned int i = 1; i < n; i++) {
    array[i].pos = array[i - 1].pos + array[i - 1].size + pad;
  }
}

static void zathura_document_widget_arrange_grid(ZathuraDocumentWidget* widget) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);
  zathura_document_t* z_document     = priv->document;

  const unsigned int c0   = priv->first_page_column;
  const unsigned int ncol = priv->pages_per_row;
  const unsigned int npag = zathura_document_get_number_of_pages(z_document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol;

  const unsigned int page_v_padding = priv->page_v_padding;
  const unsigned int page_h_padding = priv->page_h_padding;

  memset(priv->row_heights, 0, nrow * sizeof(document_widget_line_s));
  memset(priv->col_widths, 0, ncol * sizeof(document_widget_line_s));

  // calculate the max width and height required for each column and row
  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page = zathura_document_get_page(z_document, i);

    unsigned int row = 0;
    unsigned int col = 0;
    zathura_document_widget_get_page_position(widget, i, &row, &col);

    unsigned int x = priv->pages_right_to_left ? priv->ncol - 1 - col : col;
    unsigned int y = row;

    unsigned int page_width, page_height;
    page_calc_height_width(z_document, page, &page_height, &page_width, true);

    priv->row_heights[y].size = MAX(page_height, priv->row_heights[y].size);
    priv->col_widths[x].size  = MAX(page_width, priv->col_widths[x].size);
  }

  zathura_document_widget_line_prefix_sum(priv->col_widths, ncol, page_h_padding);
  zathura_document_widget_line_prefix_sum(priv->row_heights, nrow, page_v_padding);
}

void zathura_document_widget_update_mode(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = priv->document;
  if (z_document == NULL || priv->grid == NULL) {
    return;
  }

  const bool single          = priv->layout_mode == DOCUMENT_WIDGET_SINGLE;
  const unsigned int npag    = zathura_document_get_number_of_pages(z_document);
  const unsigned int page_id = zathura_document_get_current_page_number(z_document);

  for (unsigned int i = 0; i < npag; i++) {
    GtkWidget* page_widget = zathura_document_widget_get_page(document, i);
    if (page_widget != NULL) {
      gtk_widget_set_visible(page_widget, single == false || i == page_id);
    }
  }

  if (single == true) {
    /* store the position to match the reset */
    zathura_document_widget_set_position_x(document, 0.0);
    zathura_document_widget_set_position_y(document, 0.0);
    gtk_adjustment_set_value(priv->hadjustment, 0);
    gtk_adjustment_set_value(priv->vadjustment, 0);
  } else {
    zathura_document_widget_compute_layout(document);
  }

  gtk_widget_queue_resize(GTK_WIDGET(document));
}

bool zathura_document_widget_mode_change_pending(ZathuraDocumentWidget* document) {
  g_return_val_if_fail(document != NULL, false);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->mode_change_pending;
}

static void get_default_position(ZathuraDocumentWidget* document, bool mode_transition, double* x, double* y) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  if (mode_transition && priv->layout_mode == DOCUMENT_WIDGET_SINGLE) {
    *x = 0.5;
    *y = 0.0;
    return;
  }

  bool vertical_center = false;
  bool zoom_center     = false;
  girara_setting_get(priv->zathura->ui.session, "vertical-center", &vertical_center);
  girara_setting_get(priv->zathura->ui.session, "zoom-center", &zoom_center);

  page_number_to_position(document, zathura_document_get_current_page_number(priv->document), 0.5,
                          vertical_center ? 0.5 : 0.0, x, y);
  if (zoom_center || priv->adjust_mode == ZATHURA_ADJUST_BESTFIT || priv->adjust_mode == ZATHURA_ADJUST_WIDTH) {
    *x = 0.5;
  }
}

/* Called after the bounds and viewport size have been updated, before placing the grid. */
static void apply_pending_page_anchor(ZathuraDocumentWidget* document) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->mode_change_pending == false || priv->document == NULL) {
    return;
  }

  /* Navigation while allocation was pending may have selected a newer page. */
  double x = 0.5, y = 0.5;
  get_default_position(document, true, &x, &y);

  zathura_document_widget_set_position_x(document, x);
  zathura_document_widget_set_position_y(document, y);
  zathura_adjustment_set_value_from_ratio(priv->hadjustment, x);
  zathura_adjustment_set_value_from_ratio(priv->vadjustment, y);
  zathura_document_widget_set_position_x(document, zathura_adjustment_get_ratio(priv->hadjustment));
  zathura_document_widget_set_position_y(document, zathura_adjustment_get_ratio(priv->vadjustment));
  priv->mode_change_pending = false;
  if (priv->zathura->ui.document_widget == document) {
    statusbar_page_number_update(priv->zathura);
  }
}

static void size_allocate_single(ZathuraDocumentWidget* document, int width, int height, int baseline) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = priv->document;

  zathura_page_t* page = zathura_document_get_page(z_document, zathura_document_get_current_page_number(z_document));
  if (page == NULL) {
    return;
  }

  unsigned int page_width = 0, page_height = 0;
  page_calc_height_width(z_document, page, &page_height, &page_width, true);

  if ((int)gtk_adjustment_get_upper(priv->hadjustment) != (int)page_width) {
    gtk_adjustment_set_upper(priv->hadjustment, page_width);
  }
  if ((int)gtk_adjustment_get_upper(priv->vadjustment) != (int)page_height) {
    gtk_adjustment_set_upper(priv->vadjustment, page_height);
  }

  apply_pending_page_anchor(document);

  const int value_h = gtk_adjustment_get_value(priv->hadjustment);
  const int value_v = gtk_adjustment_get_value(priv->vadjustment);
  const int clamp_h = MAX(MIN(-value_h, 0), -((int)page_width - width));
  const int clamp_v = MAX(MIN(-value_v, 0), -((int)page_height - height));

  const int x = ((int)page_width < width) ? (width - (int)page_width) / 2 : clamp_h;
  const int y = ((int)page_height < height) ? (height - (int)page_height) / 2 : clamp_v;

  const GtkAllocation allocation = {.x = x, .y = y, .width = (int)page_width, .height = (int)page_height};
  gtk_widget_size_allocate(priv->grid, &allocation, baseline);
}

void zathura_document_widget_configure_viewport(ZathuraDocumentWidget* document, unsigned int width,
                                                unsigned int height) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  const double x = priv->position_x;
  const double y = priv->position_y;

  priv->view_width  = width;
  priv->view_height = height;

  gtk_adjustment_set_page_size(priv->vadjustment, height);
  gtk_adjustment_set_page_size(priv->hadjustment, width);
  gtk_adjustment_set_page_increment(priv->vadjustment, height * 0.9);
  gtk_adjustment_set_page_increment(priv->hadjustment, width * 0.9);

  if (priv->document) {
    zathura_adjustment_set_value_from_ratio(priv->vadjustment, y);
    zathura_adjustment_set_value_from_ratio(priv->hadjustment, x);
  }
}

static void zathura_document_widget_size_allocate(GtkWidget* widget, int width, int height, int baseline) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = priv->document;

  bool size_changed = false;
  if (z_document) {
    size_changed       = width != priv->alloc_width || height != priv->alloc_height;
    priv->alloc_width  = width;
    priv->alloc_height = height;

    if (size_changed) {
      /* Fitting needs the new dimensions before adjustments are configured below. */
      priv->view_height = height;
      priv->view_width  = width;
      zathura_document_widget_adjust_view(document);
      /* the scale settled and the zoom is now fit so release the held render in this same frame */
      if (priv->zathura->sync.initial_render_held == true && priv->zathura->sync.scale_settled == true) {
        priv->zathura->sync.initial_render_held = false;
        render_focused_page_now(priv->zathura);
      }
    }
    zathura_document_widget_update_visible_pages(document);
  }

  /* set the page size after adjust_view so the changed handler stores the position against the
   * final document height and the view stays at the top on first open */
  if (priv->grid && size_changed) {
    zathura_document_widget_configure_viewport(document, width, height);
  }

  if (priv->grid) {
    /* position the grid by the adjustment values so navigation moves the view */
    /* read natural size from arrange_grid totals to avoid measuring during size_allocate */
    unsigned int doc_w = 0, doc_h = 0;
    if (priv->col_widths != NULL && priv->row_heights != NULL && priv->nrow > 0 && priv->ncol > 0) {
      zathura_document_widget_get_document_size(document, &doc_h, &doc_w);
    }

    const int alloc_w = MAX(width, (int)doc_w);
    const int alloc_h = MAX(height, (int)doc_h);

    if (priv->layout_mode == DOCUMENT_WIDGET_SINGLE && z_document) {
      size_allocate_single(document, width, height, baseline);
      return;
    }

    apply_pending_page_anchor(document);

    const int x                    = -(int)gtk_adjustment_get_value(priv->hadjustment);
    const int y                    = -(int)gtk_adjustment_get_value(priv->vadjustment);
    const GtkAllocation allocation = {.x = x, .y = y, .width = alloc_w, .height = alloc_h};
    gtk_widget_size_allocate(priv->grid, &allocation, baseline);
  }
}

static void zathura_document_widget_measure(GtkWidget* widget, GtkOrientation orientation, int for_size, int* minimum,
                                            int* natural, int* minimum_baseline, int* natural_baseline) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  if (priv->grid != NULL) {
    gtk_widget_measure(priv->grid, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
    return;
  }
  *minimum = *natural = 0;
  if (minimum_baseline != NULL) {
    *minimum_baseline = -1;
  }
  if (natural_baseline != NULL) {
    *natural_baseline = -1;
  }
}

static void zathura_document_widget_dispose(GObject* object) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(object);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  zathura_document_widget_clear_pages(document);
  g_clear_pointer(&priv->grid, gtk_widget_unparent);

  g_clear_object(&priv->hadjustment);
  g_clear_object(&priv->vadjustment);

  G_OBJECT_CLASS(zathura_document_widget_parent_class)->dispose(object);
}

static void zathura_document_widget_finalize(GObject* object) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(object);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  g_free(priv->col_widths);
  g_free(priv->row_heights);

  priv->col_widths  = NULL;
  priv->row_heights = NULL;

  G_OBJECT_CLASS(zathura_document_widget_parent_class)->finalize(object);
}

void zathura_document_widget_refresh_layout(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = priv->document;

  priv->alloc_width  = -1;
  priv->alloc_height = -1;

  if (!z_document) {
    return;
  }

  const unsigned int c0   = priv->first_page_column;
  const unsigned int ncol = priv->pages_per_row;
  const unsigned int npag = zathura_document_get_number_of_pages(z_document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol;

  document_widget_line_s* tmp = g_try_realloc_n(priv->col_widths, ncol, sizeof(document_widget_line_s));
  if (tmp == NULL) {
    girara_error("Failed to allocate document grid (%u columns, %u rows)", ncol, nrow);
    return;
  }
  priv->col_widths = tmp;
  tmp              = g_try_realloc_n(priv->row_heights, nrow, sizeof(document_widget_line_s));
  if (tmp == NULL) {
    girara_error("Failed to allocate document grid (%u columns, %u rows)", ncol, nrow);
    return;
  }
  priv->row_heights = tmp;

  priv->ncol = ncol;
  priv->nrow = nrow;

  /* page sizes may stay the same while their cells change */
  gtk_widget_queue_resize(priv->grid);

  zathura_document_widget_compute_layout(document);

  gtk_widget_set_visible(GTK_WIDGET(document), true);
  zathura_document_widget_update_mode(document);
  gtk_widget_queue_resize(GTK_WIDGET(document));

  /* the cached visibility flags are stale after pages move */
  zathura_document_widget_update_visible_pages(document);
}

static void zathura_document_widget_attach_page(ZathuraDocumentWidget* document, unsigned int page_index) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->grid == NULL || priv->ncol == 0) {
    return;
  }

  GtkWidget* page_widget = zathura_document_widget_get_page(document, page_index);
  if (page_widget == NULL || gtk_widget_get_parent(page_widget) == priv->grid) {
    return;
  }

  gtk_widget_set_parent(page_widget, priv->grid);
}

static void disconnect_page_render_callbacks(ZathuraDocumentWidget* document) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  const unsigned int count           = zathura_document_get_number_of_pages(priv->document);
  const guint page_signals[]         = {g_signal_lookup("rendered", ZATHURA_TYPE_PAGE_WIDGET),
                                        g_signal_lookup("render-failed", ZATHURA_TYPE_PAGE_WIDGET)};
  for (unsigned int i = 0; i < count; ++i) {
    if (priv->pages[i] == NULL) {
      continue;
    }
    for (size_t j = 0; j < G_N_ELEMENTS(page_signals); ++j) {
      g_signal_handlers_disconnect_matched(priv->pages[i], G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA, page_signals[j], 0,
                                           NULL, NULL, document);
    }
  }
}

static void cb_page_render_completed(ZathuraPageWidget* page_widget, ZathuraDocumentWidget* document) {
  const unsigned int page_number = zathura_page_get_index(zathura_page_widget_get_page(page_widget));
  if (zathura_document_widget_page_is_visible(document, page_number)) {
    disconnect_page_render_callbacks(document);
    g_signal_emit(document, signals[READY], 0);
  }
}

static void cb_page_render_failed(ZathuraPageWidget* page_widget, ZathuraDocumentWidget* document) {
  const unsigned int page_number = zathura_page_get_index(zathura_page_widget_get_page(page_widget));
  if (zathura_document_widget_page_is_visible(document, page_number)) {
    g_signal_emit(document, signals[RENDER_FAILED], 0);
  }
}

GtkWidget* zathura_document_widget_ensure_page(ZathuraDocumentWidget* document, unsigned int page_index) {
  g_return_val_if_fail(document != NULL, NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->pages == NULL || priv->document == NULL ||
      page_index >= zathura_document_get_number_of_pages(priv->document)) {
    return NULL;
  }

  GtkWidget* page_widget = priv->pages[page_index];
  if (page_widget == NULL) {
    zathura_page_t* page = zathura_document_get_page(priv->document, page_index);
    if (page == NULL) {
      return NULL;
    }

    page_widget = zathura_page_widget_new(priv->zathura, page, priv->renderer);
    if (page_widget == NULL) {
      return NULL;
    }

    priv->pages[page_index] = g_object_ref_sink(page_widget);

    const bool show = priv->layout_mode != DOCUMENT_WIDGET_SINGLE ||
                      page_index == zathura_document_get_current_page_number(priv->document);
    gtk_widget_set_visible(page_widget, show);

    zathura_page_widget_sync_size(ZATHURA_PAGE_WIDGET(page_widget), false);

    g_signal_connect(G_OBJECT(page_widget), "text-selected", G_CALLBACK(cb_page_widget_text_selected), priv->zathura);
    g_signal_connect(G_OBJECT(page_widget), "image-selected", G_CALLBACK(cb_page_widget_image_selected), priv->zathura);
    g_signal_connect(G_OBJECT(page_widget), "enter-link", G_CALLBACK(cb_page_widget_link), (gpointer) true);
    g_signal_connect(G_OBJECT(page_widget), "leave-link", G_CALLBACK(cb_page_widget_link), (gpointer) false);
    g_signal_connect(G_OBJECT(page_widget), "scaled-button-release", G_CALLBACK(cb_page_widget_scaled_button_release),
                     priv->zathura);
    g_object_set(G_OBJECT(page_widget), "draw-signatures", priv->draw_signatures, NULL);
  }

  zathura_document_widget_attach_page(document, page_index);
  return page_widget;
}

static gboolean zathura_document_widget_preload_pages(gpointer data) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(data);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  if (priv->document == NULL) {
    priv->page_widget_preload_source = 0;
    return G_SOURCE_REMOVE;
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);
  const gint64 deadline              = g_get_monotonic_time() + 4000;
  while (priv->page_widget_preload_next < number_of_pages && g_get_monotonic_time() < deadline) {
    zathura_document_widget_ensure_page(document, priv->page_widget_preload_next++);
  }

  if (priv->page_widget_preload_next < number_of_pages) {
    return G_SOURCE_CONTINUE;
  }

  /* ensure_page already applied visibility; changing mode would reset single-page scrolling. */
  zathura_document_widget_compute_layout(document);
  gtk_widget_queue_resize(GTK_WIDGET(document));
  priv->page_widget_preload_source = 0;
  priv->page_widgets_loaded        = true;
  g_signal_emit(document, signals[PAGE_WIDGETS_LOADED], 0);
  return G_SOURCE_REMOVE;
}

void zathura_document_widget_start_page_widget_preload(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_widget_stop_page_widget_preload(document);
  if (priv->document == NULL) {
    return;
  }

  priv->page_widget_preload_next = 0;
  priv->page_widgets_loaded      = false;
  priv->page_widget_preload_source =
      g_idle_add_full(G_PRIORITY_LOW, zathura_document_widget_preload_pages, document, NULL);
}

void zathura_document_widget_stop_page_widget_preload(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  g_clear_handle_id(&priv->page_widget_preload_source, g_source_remove);
  priv->page_widget_preload_next = 0;
  priv->page_widgets_loaded      = false;
}

bool zathura_document_widget_page_widgets_loaded(ZathuraDocumentWidget* document) {
  g_return_val_if_fail(document != NULL, false);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->page_widgets_loaded;
}

bool zathura_document_widget_page_is_visible(ZathuraDocumentWidget* document, unsigned int page_number) {
  g_return_val_if_fail(document != NULL, false);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->row_heights == NULL || priv->col_widths == NULL || priv->nrow == 0 ||
      priv->ncol == 0 || page_number >= zathura_document_get_number_of_pages(priv->document)) {
    return false;
  }

  if (priv->layout_mode == DOCUMENT_WIDGET_SINGLE) {
    return page_number == zathura_document_get_current_page_number(priv->document);
  }

  unsigned int row = 0;
  unsigned int col = 0;
  zathura_document_widget_get_page_position(document, page_number, &row, &col);
  if (row >= priv->nrow || col >= priv->ncol) {
    return false;
  }

  const unsigned int document_height = priv->row_heights[priv->nrow - 1].pos + priv->row_heights[priv->nrow - 1].size;
  const unsigned int document_width  = priv->col_widths[priv->ncol - 1].pos + priv->col_widths[priv->ncol - 1].size;
  if (document_height == 0 || document_width == 0) {
    return false;
  }

  const double page_x = priv->col_widths[col].pos + 0.5 * priv->col_widths[col].size;
  const double page_y = priv->row_heights[row].pos + 0.5 * priv->row_heights[row].size;
  return fabs(page_x - priv->position_x * document_width) <
             0.5 * (priv->view_width + (double)priv->col_widths[col].size) &&
         fabs(page_y - priv->position_y * document_height) <
             0.5 * (priv->view_height + (double)priv->row_heights[row].size);
}

static void activate_page(ZathuraDocumentWidget* document, unsigned int page_id) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_page_t* page               = zathura_document_get_page(priv->document, page_id);

  if (!zathura_page_get_visibility(page)) {
    zathura_page_set_visibility(page, true);
    zathura_renderer_page_cache_add(priv->renderer, page_id);
  }
}

void zathura_document_widget_update_visible_pages(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->zathura == NULL || priv->renderer == NULL) {
    return;
  }

  /* Detached replacements queue their visible pages explicitly after layout has settled. */
  if (gtk_widget_get_parent(GTK_WIDGET(document)) == NULL) {
    return;
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    zathura_page_t* page = zathura_document_get_page(priv->document, page_id);
    if (page == NULL) {
      continue;
    }

    const bool visible = zathura_document_widget_page_is_visible(document, page_id);
    if (visible) {
      zathura_document_widget_ensure_page(document, page_id);
    }

    GtkWidget* page_widget = zathura_document_widget_get_page(document, page_id);
    if (page_widget == NULL) {
      continue;
    }
    ZathuraPageWidget* zathura_page_widget = ZATHURA_PAGE_WIDGET(page_widget);

    if (visible) {
      activate_page(document, page_id);

      for (unsigned int i = priv->pages_per_row; i; --i) {
        if (page_id >= i) {
          GtkWidget* previous = zathura_document_widget_get_page(document, page_id - i);
          if (previous) {
            zathura_page_widget_update_view_time(ZATHURA_PAGE_WIDGET(previous));
          }
        }
        if (page_id + i < number_of_pages) {
          GtkWidget* next = zathura_document_widget_get_page(document, page_id + i);
          if (next) {
            zathura_page_widget_update_view_time(ZATHURA_PAGE_WIDGET(next));
          }
        }
      }
      zathura_page_widget_update_view_time(zathura_page_widget);
    } else {
      if (zathura_page_get_visibility(page)) {
        zathura_page_set_visibility(page, false);
        zathura_page_widget_abort_render_request(zathura_page_widget);
      }

      girara_list_t* results = NULL;
      g_object_get(G_OBJECT(page_widget), "search-results", &results, NULL);
      if (results != NULL) {
        g_object_set(G_OBJECT(page_widget), "search-current", 0, NULL);
      }
    }
  }
}

void zathura_document_widget_render_current_page(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->zathura == NULL || priv->renderer == NULL) {
    return;
  }

  const unsigned int current = zathura_document_get_current_page_number(priv->document);
  GtkWidget* page_widget     = zathura_document_widget_get_page(document, current);
  if (page_widget != NULL) {
    zathura_page_widget_render(ZATHURA_PAGE_WIDGET(page_widget));
  }
}

bool zathura_document_widget_prepare_visible_pages(ZathuraDocumentWidget* document, double position_x,
                                                   double position_y) {
  g_return_val_if_fail(ZATHURA_IS_DOCUMENT_WIDGET(document), false);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->renderer == NULL || priv->view_width == 0 || priv->view_height == 0) {
    return false;
  }

  zathura_document_widget_configure_viewport(document, priv->view_width, priv->view_height);
  apply_pending_page_anchor(document);
  if (position_x >= 0) {
    priv->position_x = position_x;
  }
  if (position_y >= 0) {
    priv->position_y = position_y;
  }

  const unsigned int count = zathura_document_get_number_of_pages(priv->document);
  // Lazy parsing can change row sizes and bring further pages into the viewport. Finish parsing those pages before
  // rendering any surfaces at the final scale.
  for (unsigned int pass = 0; pass <= count; ++pass) {
    unsigned int width = 0, height = 0;
    if (priv->layout_mode == DOCUMENT_WIDGET_SINGLE) {
      zathura_page_t* page =
          zathura_document_get_page(priv->document, zathura_document_get_current_page_number(priv->document));
      page_calc_height_width(priv->document, page, &height, &width, true);
    } else {
      zathura_document_widget_get_document_size(document, &height, &width);
    }
    if (!width || !height) {
      return false;
    }

    // Clamp viewport before deciding which pages need surfaces. Also handles reloads that shortened the document, and
    // pages smaller than the viewport.
    const double half_width  = MIN(0.5, (double)priv->view_width / (2.0 * width));
    const double half_height = MIN(0.5, (double)priv->view_height / (2.0 * height));
    priv->position_x         = CLAMP(priv->position_x, half_width, 1.0 - half_width);
    priv->position_y         = CLAMP(priv->position_y, half_height, 1.0 - half_height);

    bool changed = false;
    for (unsigned int i = 0; i < count; ++i) {
      if (!zathura_document_widget_page_is_visible(document, i)) {
        continue;
      }

      zathura_page_t* page = zathura_document_get_page(priv->document, i);
      const double width   = zathura_page_get_width(page);
      const double height  = zathura_page_get_height(page);
      if (!zathura_page_load(page, NULL)) {
        return false;
      }

      changed |= width != zathura_page_get_width(page) || height != zathura_page_get_height(page);
    }

    if (!changed) {
      break;
    }

    zathura_document_widget_compute_layout(document);
    zathura_document_widget_adjust_view(document);
    if (pass == count) {
      return false;
    }
  }

  /* Keep the detached adjustments ready for adoption when the focused page finishes rendering. */
  zathura_adjustment_set_value_from_ratio(priv->hadjustment, priv->position_x);
  zathura_adjustment_set_value_from_ratio(priv->vadjustment, priv->position_y);

  const unsigned int current = zathura_document_get_current_page_number(priv->document);
  for (unsigned int offset = 0; offset < count; ++offset) {
    const unsigned int i = (current + offset) % count;
    if (i != current && !zathura_document_widget_page_is_visible(document, i)) {
      continue;
    }

    GtkWidget* widget = zathura_document_widget_ensure_page(document, i);
    if (!widget) {
      return false;
    }

    zathura_page_widget_sync_size(ZATHURA_PAGE_WIDGET(widget), false);
    activate_page(document, i);

    g_signal_connect_object(widget, "rendered", G_CALLBACK(cb_page_render_completed), document, 0);
    g_signal_connect_object(widget, "render-failed", G_CALLBACK(cb_page_render_failed), document, 0);
    zathura_page_widget_update_view_time(ZATHURA_PAGE_WIDGET(widget));
  }

  return true;
}

bool zathura_document_widget_page_has_surface(ZathuraDocumentWidget* document, unsigned int page_number) {
  GtkWidget* page_widget = zathura_document_widget_get_page(document, page_number);
  return page_widget != NULL && zathura_page_widget_have_surface(ZATHURA_PAGE_WIDGET(page_widget));
}

void zathura_document_widget_set_draw_signatures(ZathuraDocumentWidget* document, bool draw) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  priv->draw_signatures              = draw;
  if (priv->document == NULL) {
    return;
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);
  for (unsigned int page = 0; page < number_of_pages; ++page) {
    GtkWidget* page_widget = zathura_document_widget_get_page(document, page);
    if (page_widget != NULL) {
      g_object_set(G_OBJECT(page_widget), "draw-signatures", draw, NULL);
    }
  }
}

void zathura_document_widget_set_draw_search_results(ZathuraDocumentWidget* document, bool draw) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (!priv->document) {
    return;
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);
  for (unsigned int page = 0; page < number_of_pages; ++page) {
    GtkWidget* page_widget = zathura_document_widget_get_page(document, page);
    if (page_widget) {
      g_object_set(G_OBJECT(page_widget), "draw-search-results", draw, NULL);
    }
  }
}

bool zathura_document_widget_prepare_links(ZathuraDocumentWidget* document) {
  g_return_val_if_fail(document != NULL, false);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (!priv->document) {
    return false;
  }

  bool show_links                    = false;
  unsigned int page_offset           = 0;
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    zathura_page_t* page = zathura_document_get_page(priv->document, page_id);
    if (!page) {
      continue;
    }

    GtkWidget* page_widget = zathura_document_widget_get_page(document, page_id);
    if (!page_widget) {
      continue;
    }

    GObject* object = G_OBJECT(page_widget);
    g_object_set(object, "draw-search-results", FALSE, NULL);
    const bool visible = zathura_page_get_visibility(page);
    g_object_set(object, "draw-links", visible, NULL);
    if (visible) {
      int number_of_links = 0;
      g_object_get(object, "number-of-links", &number_of_links, NULL);
      show_links |= number_of_links != 0;
      g_object_set(object, "offset-links", page_offset, NULL);
      page_offset += number_of_links;
    }
  }
  return show_links;
}

void zathura_document_widget_hide_links(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (!priv->document) {
    return;
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);
  for (unsigned int page = 0; page < number_of_pages; ++page) {
    GtkWidget* page_widget = zathura_document_widget_get_page(document, page);
    if (page_widget != NULL) {
      g_object_set(G_OBJECT(page_widget), "draw-links", FALSE, NULL);
    }
  }
}

zathura_link_t* zathura_document_widget_get_visible_link(ZathuraDocumentWidget* document, unsigned int index) {
  g_return_val_if_fail(document != NULL, NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (!priv->document) {
    return NULL;
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);
  for (unsigned int page = 0; page < number_of_pages; ++page) {
    zathura_page_t* zathura_page = zathura_document_get_page(priv->document, page);
    GtkWidget* page_widget       = zathura_document_widget_get_page(document, page);
    if (zathura_page != NULL && zathura_page_get_visibility(zathura_page) && page_widget != NULL) {
      zathura_link_t* link = zathura_page_widget_link_get(ZATHURA_PAGE_WIDGET(page_widget), index);
      if (link != NULL) {
        return link;
      }
    }
  }
  return NULL;
}

unsigned int zathura_document_widget_get_search_result_count(ZathuraDocumentWidget* document, unsigned int end_page) {
  g_return_val_if_fail(document, 0);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (!priv->document) {
    return 0;
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);
  end_page                           = MIN(end_page, number_of_pages);

  unsigned int count = 0;
  for (unsigned int page = 0; page < end_page; ++page) {
    GtkWidget* page_widget = zathura_document_widget_get_page(document, page);
    if (page_widget != NULL) {
      int page_count = 0;
      g_object_get(G_OBJECT(page_widget), "search-length", &page_count, NULL);
      if (page_count > 0) {
        count += (unsigned int)page_count;
      }
    }
  }

  return count;
}

void zathura_document_widget_compute_layout(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_widget_arrange_grid(document);

  /* update allocation values */
  unsigned int doc_height = 0, doc_width = 0;
  if (priv->layout_mode == DOCUMENT_WIDGET_SINGLE) {
    zathura_page_t* page =
        zathura_document_get_page(priv->document, zathura_document_get_current_page_number(priv->document));
    page_calc_height_width(priv->document, page, &doc_height, &doc_width, true);
  } else {
    zathura_document_widget_get_document_size(document, &doc_height, &doc_width);
  }

  const double stored_x = zathura_document_widget_get_position_x(document);
  const double stored_y = zathura_document_widget_get_position_y(document);
  gtk_adjustment_set_upper(priv->vadjustment, doc_height);
  gtk_adjustment_set_upper(priv->hadjustment, doc_width);
  // Better set vadjustment first because it's likely that hadjustment_value_changed
  // callback to call excess zathura_document_set_current_page_number
  zathura_adjustment_set_value_from_ratio(priv->vadjustment, stored_y);
  zathura_adjustment_set_value_from_ratio(priv->hadjustment, stored_x);

  float scroll_step = 40;
  girara_setting_get(priv->zathura->ui.session, "scroll-step", &scroll_step);

  gtk_adjustment_set_step_increment(priv->vadjustment, scroll_step);
}

void zathura_document_widget_get_page_origin(ZathuraDocumentWidget* document, unsigned int page_index, double* x,
                                             double* y) {
  g_return_if_fail(ZATHURA_IS_DOCUMENT_WIDGET(document) && x != NULL && y != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->col_widths == NULL || priv->row_heights == NULL || priv->ncol == 0 ||
      priv->nrow == 0 || page_index >= zathura_document_get_number_of_pages(priv->document)) {
    return;
  }

  zathura_page_t* page     = zathura_document_get_page(priv->document, page_index);
  unsigned int page_width  = 0;
  unsigned int page_height = 0;
  page_calc_height_width(priv->document, page, &page_height, &page_width, true);

  unsigned int doc_width  = 0;
  unsigned int doc_height = 0;
  zathura_document_widget_get_document_size(document, &doc_height, &doc_width);

  const bool rtl = gtk_widget_get_direction(priv->grid != NULL ? priv->grid : GTK_WIDGET(document)) == GTK_TEXT_DIR_RTL;
  unsigned int origin_x = 0;
  unsigned int origin_y = 0;
  calculate_page_origin(document, page_index, page_width, page_height, MAX(doc_width, priv->view_width), rtl, &origin_x,
                        &origin_y);

  *x = origin_x;
  *y = origin_y;
}

void zathura_document_widget_get_cell_pos(ZathuraDocumentWidget* document, unsigned int page_index, unsigned int* pos_x,
                                          unsigned int* pos_y) {
  g_return_if_fail(document != NULL && pos_x != NULL && pos_y != NULL);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = priv->document;

  if (!priv->col_widths || !priv->row_heights) {
    return;
  }

  const unsigned int npag = zathura_document_get_number_of_pages(z_document);
  if (page_index >= npag) {
    girara_warning("tried to get cell size for page %u, document has %u pages", page_index, npag);
    return;
  }

  unsigned int row, col;
  zathura_document_widget_get_page_position(document, page_index, &row, &col);

  *pos_x = priv->col_widths[col].pos;
  *pos_y = priv->row_heights[row].pos;
}

void zathura_document_widget_get_cell_size(ZathuraDocumentWidget* document, unsigned int page_index,
                                           unsigned int* height, unsigned int* width) {
  g_return_if_fail(document != NULL && height != NULL && width != NULL);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = priv->document;

  if (!priv->col_widths || !priv->row_heights) {
    return;
  }

  const unsigned int npag = zathura_document_get_number_of_pages(z_document);
  if (page_index >= npag) {
    girara_warning("tried to get cell size for page %u, document has %u pages", page_index, npag);
    return;
  }

  unsigned int row, col;
  zathura_document_widget_get_page_position(document, page_index, &row, &col);

  *height = priv->row_heights[row].size;
  *width  = priv->col_widths[col].size;
}

void zathura_document_widget_get_row(ZathuraDocumentWidget* document, unsigned int row, unsigned int* pos,
                                     unsigned int* size) {
  g_return_if_fail(document != NULL && pos != NULL && size != NULL);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  if (!priv->col_widths || !priv->row_heights) {
    return;
  }

  if (row >= priv->nrow) {
    girara_warning("tried to get row %u size, document has %u rows", row, priv->nrow);
    return;
  }

  *pos  = priv->row_heights[row].pos;
  *size = priv->row_heights[row].size;
}

void zathura_document_widget_get_col(ZathuraDocumentWidget* document, unsigned int col, unsigned int* pos,
                                     unsigned int* size) {
  g_return_if_fail(document != NULL && pos != NULL && size != NULL);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  if (!priv->col_widths || !priv->row_heights) {
    return;
  }

  if (col >= priv->ncol) {
    girara_warning("tried to get col %u size, document has %u columns", col, priv->ncol);
    return;
  }

  *pos  = priv->col_widths[col].pos;
  *size = priv->col_widths[col].size;
}

void zathura_document_widget_get_document_size(ZathuraDocumentWidget* document, unsigned int* height,
                                               unsigned int* width) {
  g_return_if_fail(document != NULL && height != NULL && width != NULL);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  if (!priv->col_widths || !priv->row_heights) {
    return;
  }

  document_widget_line_s last_row = priv->row_heights[priv->nrow - 1];
  document_widget_line_s last_col = priv->col_widths[priv->ncol - 1];

  *height = last_row.pos + last_row.size;
  *width  = last_col.pos + last_col.size;
}

void zathura_document_widget_clear_pages(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  priv->mode_change_pending          = false;
  zathura_document_widget_stop(document);
  const unsigned int number_of_pages =
      priv->document != NULL && priv->pages != NULL ? zathura_document_get_number_of_pages(priv->document) : 0;

  for (unsigned int i = 0; i < number_of_pages; ++i) {
    GtkWidget* page_widget = priv->pages[i];
    if (page_widget != NULL) {
      if (gtk_widget_get_parent(page_widget) != NULL) {
        gtk_widget_unparent(page_widget);
      }
      g_clear_object(&priv->pages[i]);
    }
  }

  g_clear_pointer(&priv->pages, g_free);
  g_clear_object(&priv->renderer);
  g_clear_pointer(&priv->col_widths, g_free);
  g_clear_pointer(&priv->row_heights, g_free);

  priv->view_width   = 0;
  priv->view_height  = 0;
  priv->position_x   = 0.0;
  priv->position_y   = 0.0;
  priv->adjust_mode  = ZATHURA_ADJUST_NONE;
  priv->document     = NULL;
  priv->nrow         = 0;
  priv->ncol         = 0;
  priv->alloc_width  = -1;
  priv->alloc_height = -1;
}

void zathura_document_widget_clear_thumbnails(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);

  for (unsigned int i = 0; i < number_of_pages; ++i) {
    GtkWidget* page_widget = priv->pages[i];

    /* the widget exists only if the background fill already created it */
    if (page_widget != NULL) {
      zathura_page_widget_clear_thumbnail(ZATHURA_PAGE_WIDGET(page_widget));
    }
  }
}

void zathura_document_widget_render_all(ZathuraDocumentWidget* document) {
  if (!document) {
    return;
  }

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = priv->document;
  if (!z_document) {
    return;
  }

  priv->alloc_width  = -1;
  priv->alloc_height = -1;

  zathura_document_widget_compute_layout(document);

  /* unmark all pages */
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(z_document);
  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    GtkWidget* page_widget = zathura_document_widget_get_page(document, page_id);
    if (page_widget != NULL) {
      zathura_page_widget_sync_size(ZATHURA_PAGE_WIDGET(page_widget), false);
      gtk_widget_queue_resize(page_widget);
    }
  }
}

void zathura_document_widget_set_page_layout(ZathuraDocumentWidget* document, unsigned int page_v_padding,
                                             unsigned int page_h_padding, unsigned int pages_per_row,
                                             unsigned int first_page_column) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  priv->page_v_padding = page_v_padding;
  priv->page_h_padding = page_h_padding;
  priv->pages_per_row  = pages_per_row;

  if (first_page_column < 1) {
    first_page_column = 1;
  } else if (first_page_column > pages_per_row) {
    first_page_column = ((first_page_column - 1) % pages_per_row) + 1;
  }

  priv->first_page_column = first_page_column;
}

unsigned int zathura_document_widget_get_page_v_padding(ZathuraDocumentWidget* document) {
  if (!document) {
    return 0;
  }

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->page_v_padding;
}

unsigned int zathura_document_widget_get_page_h_padding(ZathuraDocumentWidget* document) {
  if (!document) {
    return 0;
  }

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->page_h_padding;
}

unsigned int zathura_document_widget_get_pages_per_row(ZathuraDocumentWidget* document) {
  if (!document) {
    return 0;
  }

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->pages_per_row;
}

unsigned int zathura_document_widget_get_first_page_column(ZathuraDocumentWidget* document) {
  if (!document) {
    return 0;
  }

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->first_page_column;
}

void zathura_document_widget_get_viewport_size(ZathuraDocumentWidget* document, unsigned int* height,
                                               unsigned int* width) {
  g_return_if_fail(ZATHURA_IS_DOCUMENT_WIDGET(document) && height != NULL && width != NULL);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  *height                            = priv->view_height;
  *width                             = priv->view_width;
}

double zathura_document_widget_get_position_x(ZathuraDocumentWidget* document) {
  if (!document) {
    return 0;
  }

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->position_x;
}

double zathura_document_widget_get_position_y(ZathuraDocumentWidget* document) {
  if (!document) {
    return 0;
  }

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->position_y;
}

typedef struct {
  unsigned int page_width;
  unsigned int page_height;
  unsigned int document_width;
  unsigned int document_height;
  double origin_x;
  double origin_y;
} page_geometry_t;

static bool get_page_geometry(ZathuraDocumentWidget* document, unsigned int page_id, page_geometry_t* geometry) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->document == NULL || page_id >= zathura_document_get_number_of_pages(priv->document)) {
    return false;
  }

  zathura_page_t* page = zathura_document_get_page(priv->document, page_id);
  if (page == NULL) {
    return false;
  }

  page_calc_height_width(priv->document, page, &geometry->page_height, &geometry->page_width, true);

  if (priv->layout_mode == DOCUMENT_WIDGET_GRID) {
    zathura_document_widget_get_document_size(document, &geometry->document_height, &geometry->document_width);
    zathura_document_widget_get_page_origin(document, page_id, &geometry->origin_x, &geometry->origin_y);
  } else {
    geometry->document_width  = geometry->page_width;
    geometry->document_height = geometry->page_height;
    geometry->origin_x        = 0;
    geometry->origin_y        = 0;
  }

  return geometry->page_width && geometry->page_height && geometry->document_width && geometry->document_height;
}

bool zathura_document_widget_get_page_relative_position(ZathuraDocumentWidget* document, double* position_x,
                                                        double* position_y) {
  g_return_val_if_fail(document, false);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  g_return_val_if_fail(priv->document != NULL, false);

  page_geometry_t geometry;
  if (!get_page_geometry(document, zathura_document_get_current_page_number(priv->document), &geometry)) {
    return false;
  }

  if (position_x) {
    *position_x = (priv->position_x * geometry.document_width - geometry.origin_x) / geometry.page_width;
  }
  if (position_y) {
    *position_y = (priv->position_y * geometry.document_height - geometry.origin_y) / geometry.page_height;
  }

  return true;
}

void zathura_document_widget_page_position_to_document(ZathuraDocumentWidget* document, unsigned int page_id,
                                                       double* position_x, double* position_y) {
  g_return_if_fail(document && position_x != NULL && position_y != NULL);

  page_geometry_t geometry;
  const bool valid = get_page_geometry(document, page_id, &geometry);

  if (*position_x >= 0) {
    *position_x =
        valid ? CLAMP((geometry.origin_x + *position_x * geometry.page_width) / geometry.document_width, 0.0, 1.0) : -1;
  }
  if (*position_y >= 0) {
    *position_y =
        valid ? CLAMP((geometry.origin_y + *position_y * geometry.page_height) / geometry.document_height, 0.0, 1.0)
              : -1;
  }
}

void zathura_document_widget_set_position_x(ZathuraDocumentWidget* document, double position_x) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  priv->position_x                   = position_x;
}

void zathura_document_widget_set_position_y(ZathuraDocumentWidget* document, double position_y) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  priv->position_y                   = position_y;
}

zathura_adjust_mode_t zathura_document_widget_get_adjust_mode(ZathuraDocumentWidget* document) {
  if (document == NULL) {
    return ZATHURA_ADJUST_NONE;
  }

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->adjust_mode;
}

void zathura_document_widget_set_adjust_mode(ZathuraDocumentWidget* document, zathura_adjust_mode_t mode) {
  if (document == NULL) {
    return;
  }

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  priv->adjust_mode                  = mode;
}

bool zathura_document_widget_adjust_view(ZathuraDocumentWidget* widget) {
  g_return_val_if_fail(ZATHURA_IS_DOCUMENT_WIDGET(widget), false);
  zathura_document_t* document = zathura_document_widget_get_document(widget);
  if (document == NULL) {
    return false;
  }

  zathura_adjust_mode_t adjust_mode = zathura_document_widget_get_adjust_mode(widget);
  if (adjust_mode == ZATHURA_ADJUST_NONE) {
    /* there is nothing todo */
    return true;
  }

  unsigned int cell_height = 0, cell_width = 0;
  unsigned int document_height = 0, document_width = 0;
  unsigned int view_height = 0, view_width = 0;

  const unsigned int current_page = zathura_document_get_current_page_number(document);

  zathura_document_widget_get_cell_size(widget, current_page, &cell_height, &cell_width);
  zathura_document_widget_get_document_size(widget, &document_height, &document_width);
  zathura_document_widget_get_viewport_size(widget, &view_height, &view_width);

  if (view_height == 0 || view_width == 0 || cell_height == 0 || cell_width == 0 || document_width == 0) {
    return false;
  }

  double page_ratio = (double)cell_height / (double)document_width;
  double view_ratio = (double)view_height / (double)view_width;
  double zoom       = zathura_document_get_zoom(document);
  double newzoom    = zoom;

  if (adjust_mode == ZATHURA_ADJUST_WIDTH || (adjust_mode == ZATHURA_ADJUST_BESTFIT && page_ratio < view_ratio)) {
    newzoom *= (double)view_width / (double)document_width;
  } else if (adjust_mode == ZATHURA_ADJUST_BESTFIT) {
    newzoom *= (double)view_height / (double)cell_height;
  } else {
    return true;
  }

  /* save new zoom and recompute cell size */
  zathura_document_set_zoom(document, newzoom);
  zathura_document_widget_compute_layout(widget);

  unsigned int new_cell_height = 0, new_cell_width = 0;
  zathura_document_widget_get_cell_size(widget, current_page, &new_cell_height, &new_cell_width);

  /*
   * XXX requiring a larger difference apparently circumvents #94 for some users; this is not a
   * proper fix
   */
  static const int min_change = 2;
  /* if the change in zoom changes page cell dimensions, render */
  if (abs((int)new_cell_width - (int)cell_width) > min_change ||
      abs((int)new_cell_height - (int)cell_height) > min_change) {
    zathura_document_widget_render_all(widget);
    gtk_widget_queue_resize(GTK_WIDGET(widget));
  } else {
    /* otherwise set the old zoom and leave */
    zathura_document_set_zoom(document, zoom);
    zathura_document_widget_compute_layout(widget);
  }

  return true;
}

void zathura_document_widget_set_position(ZathuraDocumentWidget* widget, double position_x, double position_y) {
  g_return_if_fail(widget != NULL);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);
  zathura_document_t* document       = priv->document;
  g_return_if_fail(document != NULL);

  if (priv->mode_change_pending) {
    apply_pending_page_anchor(widget);
  }

  double default_x = 0.5, default_y = 0.5;
  get_default_position(widget, false, &default_x, &default_y);
  if (position_x < 0) {
    position_x = default_x;
  }
  if (position_y < 0) {
    position_y = default_y;
  }

  // set the position
  zathura_document_widget_set_position_x(widget, position_x);
  zathura_document_widget_set_position_y(widget, position_y);
}

void zathura_document_widget_set_equal_mode(ZathuraDocumentWidget* widget, zathura_equal_mode_t mode) {
  g_return_if_fail(ZATHURA_IS_DOCUMENT_WIDGET(widget));

  zathura_document_t* document    = zathura_document_widget_get_document(widget);
  const unsigned int npag         = zathura_document_get_number_of_pages(document);
  const unsigned int current_page = zathura_document_get_current_page_number(document);
  zathura_page_t* c_page          = zathura_document_get_page(document, current_page);

  girara_debug("Setting page equal mode to: %d", mode);
  for (unsigned int i = 0; i != npag; i++) {
    zathura_page_t* page_i = zathura_document_get_page(document, i);

    switch (mode) {
    case ZATHURA_EQUAL_WIDTH:
      zathura_page_set_zoom(page_i, zathura_page_get_width(c_page) / zathura_page_get_width(page_i));
      break;
    case ZATHURA_EQUAL_HEIGHT:
      zathura_page_set_zoom(page_i, zathura_page_get_height(c_page) / zathura_page_get_height(page_i));
      break;
    default:
      zathura_page_set_zoom(page_i, 1.0);
    }
  }

  zathura_document_widget_render_all(widget);
  gtk_widget_queue_resize(GTK_WIDGET(widget));
}
