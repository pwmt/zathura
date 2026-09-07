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
  GtkWidget** pages;
  guint page_widget_preload_source;
  unsigned int page_widget_preload_next;
  bool page_widgets_loaded;
  bool draw_signatures;

  /* Layout */
  document_widget_mode_t layout_mode;
  gboolean pages_right_to_left;
  unsigned int nrow;
  unsigned int ncol;
  document_widget_line_s* row_heights;
  document_widget_line_s* col_widths;
  unsigned int pages_per_row;     /**< number of pages in a row */
  unsigned int first_page_column; /**< column of the first page */
  unsigned int page_v_padding;    /**< padding between pages */
  unsigned int page_h_padding;    /**< padding between pages */
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

static void zathura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value,
                                                 GParamSpec* pspec);
static void zathura_document_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void zathura_document_widget_size_allocate(GtkWidget* widget, int width, int height, int baseline);
static void zathura_document_widget_measure(GtkWidget* widget, GtkOrientation orientation, int for_size, int* minimum,
                                            int* natural, int* minimum_baseline, int* natural_baseline);
static void zathura_document_widget_dispose(GObject* object);
static void zathura_document_widget_finalize(GObject* object);
static gboolean zathura_document_widget_preload_pages(gpointer data);
static bool zathura_document_widget_page_is_visible(ZathuraDocumentWidget* document, unsigned int page_number);

enum signals_e {
  PAGE_WIDGETS_LOADED,
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

  priv->grid = gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(priv->grid), FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(priv->grid), FALSE);
  gtk_widget_set_halign(priv->grid, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(priv->grid, GTK_ALIGN_CENTER);
  gtk_widget_set_parent(priv->grid, GTK_WIDGET(widget));
}

GtkWidget* zathura_document_widget_new(zathura_t* zathura, zathura_document_t* zathura_document) {
  g_return_val_if_fail(zathura_document != NULL, NULL);

  GObject* ret = g_object_new(ZATHURA_TYPE_DOCUMENT_WIDGET, "zathura", zathura, NULL);
  if (ret == NULL) {
    return NULL;
  }

  ZathuraDocumentWidget* widget = ZATHURA_DOCUMENT_WIDGET(ret);
  GtkWidget* gtk_widget         = GTK_WIDGET(widget);

  if (zathura_document_widget_set_document(widget, zathura_document) == false) {
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
  GtkWidget** pages                  = g_try_malloc0_n(number_of_pages, sizeof(GtkWidget*));
  if (pages == NULL) {
    return false;
  }

  priv->document            = document;
  priv->pages               = pages;
  priv->page_widgets_loaded = false;

  return true;
}

zathura_document_t* zathura_document_widget_get_document(ZathuraDocumentWidget* document) {
  g_return_val_if_fail(document != NULL, NULL);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->document;
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

  g_signal_connect_swapped(adjustment, "value-changed", G_CALLBACK(gtk_widget_queue_allocate), widget);

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
  case PROP_LAYOUT_MODE:
    priv->layout_mode = g_value_get_int(value);
    zathura_document_widget_update_mode(document);
    gtk_widget_queue_allocate(GTK_WIDGET(document));
    break;
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
    zathura_document_t* z_document = priv->document;
    if (z_document != NULL) {
      zathura_document_set_position_x(z_document, 0.0);
      zathura_document_set_position_y(z_document, 0.0);
    }
    gtk_adjustment_set_value(priv->hadjustment, 0);
    gtk_adjustment_set_value(priv->vadjustment, 0);
  } else {
    zathura_document_widget_compute_layout(document);
  }

  gtk_widget_queue_resize(GTK_WIDGET(document));
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

  const int value_h = gtk_adjustment_get_value(priv->hadjustment);
  const int value_v = gtk_adjustment_get_value(priv->vadjustment);
  const int clamp_h = MAX(MIN(-value_h, 0), -((int)page_width - width));
  const int clamp_v = MAX(MIN(-value_v, 0), -((int)page_height - height));

  const int x = ((int)page_width < width) ? (width - (int)page_width) / 2 : clamp_h;
  const int y = ((int)page_height < height) ? (height - (int)page_height) / 2 : clamp_v;

  const GtkAllocation allocation = {.x = x, .y = y, .width = (int)page_width, .height = (int)page_height};
  gtk_widget_size_allocate(priv->grid, &allocation, baseline);
}

static void zathura_document_widget_size_allocate(GtkWidget* widget, int width, int height, int baseline) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = priv->document;

  bool size_changed = false;

  if (z_document != NULL) {
    size_changed       = width != priv->alloc_width || height != priv->alloc_height;
    priv->alloc_width  = width;
    priv->alloc_height = height;
  }

  if (z_document != NULL) {
    if (size_changed == true) {
      zathura_document_set_viewport_height(z_document, height);
      zathura_document_set_viewport_width(z_document, width);
      adjust_view(priv->zathura);
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
  if (priv->grid != NULL && size_changed == true) {
    gtk_adjustment_set_page_size(priv->hadjustment, width);
    gtk_adjustment_set_page_size(priv->vadjustment, height);
    gtk_adjustment_set_page_increment(priv->hadjustment, width * 0.9);
    gtk_adjustment_set_page_increment(priv->vadjustment, height * 0.9);
  }

  if (priv->grid != NULL) {
    /* position the grid by the adjustment values so navigation moves the view */
    /* read natural size from arrange_grid totals to avoid measuring during size_allocate */
    unsigned int doc_w = 0, doc_h = 0;
    if (priv->col_widths != NULL && priv->row_heights != NULL && priv->nrow > 0 && priv->ncol > 0) {
      zathura_document_widget_get_document_size(document, &doc_h, &doc_w);
    }

    const int alloc_w = MAX(width, (int)doc_w);
    const int alloc_h = MAX(height, (int)doc_h);

    if (priv->layout_mode == DOCUMENT_WIDGET_SINGLE && z_document != NULL) {
      size_allocate_single(document, width, height, baseline);
      return;
    }

    /* align tall documents to the top so the first page stays visible while the grid fills */
    gtk_widget_set_valign(priv->grid, (int)doc_h > height ? GTK_ALIGN_START : GTK_ALIGN_CENTER);

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
  priv->alloc_width                  = -1;
  priv->alloc_height                 = -1;
  zathura_document_t* z_document     = priv->document;

  if (z_document == NULL) {
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

  for (unsigned int i = 0; i < npag; i++) {
    GtkWidget* page_widget = zathura_document_widget_get_page(document, i);
    if (page_widget == NULL) {
      continue;
    }

    GtkWidget* parent = gtk_widget_get_parent(page_widget);
    if (parent == priv->grid) {
      gtk_grid_remove(GTK_GRID(priv->grid), page_widget);
    } else if (parent != NULL) {
      gtk_widget_unparent(page_widget);
    }

    unsigned int row = 0;
    unsigned int col = 0;
    zathura_document_widget_get_page_position(document, i, &row, &col);
    unsigned int x  = priv->pages_right_to_left ? priv->ncol - 1 - col : col;
    GtkAlign halign = priv->ncol == 1 ? GTK_ALIGN_CENTER : (x == 0 ? GTK_ALIGN_END : GTK_ALIGN_START);
    gtk_widget_set_halign(page_widget, halign);
    gtk_grid_attach(GTK_GRID(priv->grid), page_widget, (int)x, (int)row, 1, 1);
  }

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

  unsigned int row = 0;
  unsigned int col = 0;
  zathura_document_widget_get_page_position(document, page_index, &row, &col);
  const unsigned int x  = priv->pages_right_to_left ? priv->ncol - 1 - col : col;
  const GtkAlign halign = priv->ncol == 1 ? GTK_ALIGN_CENTER : (x == 0 ? GTK_ALIGN_END : GTK_ALIGN_START);
  gtk_widget_set_halign(page_widget, halign);
  gtk_grid_attach(GTK_GRID(priv->grid), page_widget, (int)x, (int)row, 1, 1);
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

    page_widget = zathura_page_widget_new(priv->zathura, page);
    if (page_widget == NULL) {
      return NULL;
    }

    priv->pages[page_index] = g_object_ref_sink(page_widget);

    gtk_widget_set_halign(page_widget, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(page_widget, GTK_ALIGN_CENTER);

    const bool show = priv->layout_mode != DOCUMENT_WIDGET_SINGLE ||
                      page_index == zathura_document_get_current_page_number(priv->document);
    gtk_widget_set_visible(page_widget, show);

    unsigned int page_height = 0;
    unsigned int page_width  = 0;
    page_calc_height_width(priv->document, page, &page_height, &page_width, true);
    zathura_page_widget_set_size_request(ZATHURA_PAGE_WIDGET(page_widget), page_width, page_height);

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

  zathura_document_widget_update_mode(document);
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
  if (priv->page_widget_preload_source != 0) {
    g_source_remove(priv->page_widget_preload_source);
    priv->page_widget_preload_source = 0;
  }
  priv->page_widget_preload_next = 0;
  priv->page_widgets_loaded      = false;
}

bool zathura_document_widget_page_widgets_loaded(ZathuraDocumentWidget* document) {
  g_return_val_if_fail(document != NULL, false);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  return priv->page_widgets_loaded;
}

static bool zathura_document_widget_page_is_visible(ZathuraDocumentWidget* document, unsigned int page_number) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->row_heights == NULL || priv->col_widths == NULL || priv->nrow == 0 ||
      priv->ncol == 0) {
    return false;
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

  const double page_x = ((double)priv->col_widths[col].pos + 0.5 * priv->col_widths[col].size) / (double)document_width;
  const double page_y =
      ((double)priv->row_heights[row].pos + 0.5 * priv->row_heights[row].size) / (double)document_height;
  const double pos_x = zathura_document_get_position_x(priv->document);
  const double pos_y = zathura_document_get_position_y(priv->document);

  unsigned int view_height = 0;
  unsigned int view_width  = 0;
  zathura_document_get_viewport_size(priv->document, &view_height, &view_width);

  return fabs(pos_x - page_x) < 0.5 * (double)(view_width + priv->col_widths[col].size) / (double)document_width &&
         fabs(pos_y - page_y) < 0.5 * (double)(view_height + priv->row_heights[row].size) / (double)document_height;
}

void zathura_document_widget_update_visible_pages(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->zathura == NULL || priv->zathura->sync.render_thread == NULL) {
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
      if (!zathura_page_get_visibility(page)) {
        zathura_page_set_visibility(page, true);
        zathura_renderer_page_cache_add(priv->zathura->sync.render_thread, page_id);
      }

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
  if (priv->document == NULL || priv->zathura == NULL || priv->zathura->sync.render_thread == NULL) {
    return;
  }

  const unsigned int current = zathura_document_get_current_page_number(priv->document);
  zathura_page_t* page       = zathura_document_get_page(priv->document, current);
  GtkWidget* page_widget     = zathura_document_widget_get_page(document, current);
  if (page == NULL || page_widget == NULL) {
    return;
  }

  cairo_surface_t* rendered = zathura_renderer_render_page(priv->zathura->sync.render_thread, page);
  if (rendered != NULL) {
    zathura_page_widget_update_surface(ZATHURA_PAGE_WIDGET(page_widget), rendered, false);
    cairo_surface_destroy(rendered);
  }
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
  zathura_document_widget_get_document_size(document, &doc_height, &doc_width);

  gtk_adjustment_set_upper(priv->hadjustment, doc_width);
  gtk_adjustment_set_upper(priv->vadjustment, doc_height);

  float scroll_step = 40;
  girara_setting_get(priv->zathura->ui.session, "scroll-step", &scroll_step);

  gtk_adjustment_set_step_increment(priv->vadjustment, scroll_step);
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
  zathura_document_widget_stop_page_widget_preload(document);
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
  g_clear_pointer(&priv->col_widths, g_free);
  g_clear_pointer(&priv->row_heights, g_free);

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
    zathura_page_t* page = zathura_document_get_page(z_document, page_id);

    unsigned int page_height = 0, page_width = 0;
    page_calc_height_width(z_document, page, &page_height, &page_width, true);

    girara_debug("Queuing resize for page %u to %u x %u.", page_id, page_width, page_height);
    GtkWidget* page_widget = zathura_document_widget_get_page(document, page_id);
    if (page_widget != NULL) {
      zathura_page_widget_set_size_request(ZATHURA_PAGE_WIDGET(page_widget), page_width, page_height);
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

  /* keep grid spacing in sync with the padding arrange_grid uses */
  /* otherwise position_to_page_number reads stale positions and navigation jumps */
  if (priv->grid != NULL) {
    gtk_grid_set_row_spacing(GTK_GRID(priv->grid), page_v_padding);
    gtk_grid_set_column_spacing(GTK_GRID(priv->grid), page_h_padding);
  }

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
