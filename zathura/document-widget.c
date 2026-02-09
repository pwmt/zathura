/* SPDX-License-Identifier: Zlib */

#include "document-widget.h"
#include "girara-gtk/settings.h"
#include "girara/log.h"

#include "page.h"
#include "utils.h"
#include "zathura.h"
#include "adjustment.h"

typedef struct {
  unsigned int pos;
  unsigned int size;
} document_widget_line_s;

typedef struct zathura_document_widget_private_s {
  zathura_t* zathura;

  /* Layout */
  document_widget_mode_t layout_mode;
  gboolean pages_right_to_left;
  unsigned int nrow;
  unsigned int ncol;
  document_widget_line_s* row_heights;
  document_widget_line_s* col_widths;

  /* Scrolling */
  GtkAdjustment* hadjustment;
  GtkAdjustment* vadjustment;
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;
} ZathuraDocumentWidgetPrivate;

G_DEFINE_TYPE_WITH_CODE(ZathuraDocumentWidget, zathura_document_widget, GTK_TYPE_CONTAINER,
                        G_ADD_PRIVATE(ZathuraDocumentWidget) G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))

static void zathura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value,
                                                 GParamSpec* pspec);
static void zathura_document_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void zathura_document_widget_size_allocate(GtkWidget* widget, GtkAllocation* allocation);
static void zathura_document_widget_container_add(GtkContainer* container, GtkWidget* widget);
static void zathura_document_widget_container_remove(GtkContainer* container, GtkWidget* widget);
static void zathura_document_widget_container_forall(GtkContainer* container, gboolean include_internals,
                                                     GtkCallback callback, gpointer user_data);
static void zathura_document_widget_dispose(GObject* object);

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

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->set_property = zathura_document_widget_set_property;
  object_class->get_property = zathura_document_widget_get_property;
  object_class->dispose      = zathura_document_widget_dispose;

  GtkContainerClass* container_class = GTK_CONTAINER_CLASS(class);
  container_class->add               = zathura_document_widget_container_add;
  container_class->remove            = zathura_document_widget_container_remove;
  container_class->forall            = zathura_document_widget_container_forall;

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
}

static void zathura_document_widget_init(ZathuraDocumentWidget* widget) {
  gtk_widget_set_has_window(GTK_WIDGET(widget), false);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(widget);

  priv->zathura = NULL;

  priv->layout_mode = DOCUMENT_WIDGET_GRID;
  priv->nrow        = 0;
  priv->ncol        = 0;
  priv->row_heights = NULL;
  priv->col_widths  = NULL;
}

GtkWidget* zathura_document_widget_new(zathura_t* zathura) {
  GObject* ret = g_object_new(ZATHURA_TYPE_DOCUMENT, "zathura", zathura, NULL);
  if (ret == NULL) {
    return NULL;
  }

  ZathuraDocumentWidget* widget = ZATHURA_DOCUMENT_WIDGET(ret);
  GtkWidget* gtk_widget         = GTK_WIDGET(widget);

  return gtk_widget;
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

  if (adjustment && adjustment == *to_set)
    return;

  if (*to_set) {
    g_signal_handlers_disconnect_by_data(*to_set, widget);
    g_object_unref(*to_set);
  }

  if (!adjustment)
    adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

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
  zathura_document_t* z_document     = zathura_get_document(priv->zathura);

  const unsigned int c0   = zathura_document_get_first_page_column(z_document);
  const unsigned int ncol = zathura_document_get_pages_per_row(z_document);

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
  zathura_document_t* z_document     = zathura_get_document(priv->zathura);

  const unsigned int c0   = zathura_document_get_first_page_column(z_document);
  const unsigned int npag = zathura_document_get_number_of_pages(z_document);
  const unsigned int ncol = zathura_document_get_pages_per_row(z_document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol;

  const unsigned int page_v_padding = zathura_document_get_page_v_padding(z_document);
  const unsigned int page_h_padding = zathura_document_get_page_h_padding(z_document);

  memset(priv->row_heights, 0, nrow * sizeof(document_widget_line_s));
  memset(priv->col_widths, 0, ncol * sizeof(document_widget_line_s));

  unsigned int row, col;

  // calculate the max width and height required for each column and row
  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page = zathura_document_get_page(z_document, i);
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

static void document_adjustment(ZathuraDocumentWidget* document, int height, int width, int* adj_v, int* adj_h) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  const unsigned int value_v = gtk_adjustment_get_value(priv->vadjustment);
  const unsigned int value_h = gtk_adjustment_get_value(priv->hadjustment);

  unsigned int doc_height, doc_width;
  zathura_document_widget_get_document_size(document, &doc_height, &doc_width);

  const int center_v = (height - doc_height) / 2;
  const int center_h = (width - doc_width) / 2;

  // if document is smaller than allocation, center the document
  *adj_v = ((int)doc_height < height) ? -center_v : (int)value_v;
  *adj_h = ((int)doc_width < width) ? -center_h : (int)value_h;
}

/*
 * Calculate allocation for a single page
 *
 * If height or width is smaller than the allocation,
 * set the position to 0 and height/width to allocation
 * so the page centers that dimension. Otherwise,
 * use the document position, clamped to the page edges,
 * to offset the page position.
 *
 * @param document ZathuraDocumentWidget
 * @param page_id  index of the page in the document
 * @param height   allocation height
 * @param width    allocation width
 * @return page_alloc the final allocation
 */
static void page_allocation(ZathuraDocumentWidget* document, int page_id, int height, int width,
                            GtkAllocation* page_alloc) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  unsigned int row, col;
  zathura_document_widget_get_page_position(document, page_id, &row, &col);

  int x = priv->pages_right_to_left ? priv->ncol - 1 - col : col;
  int y = row;

  const int page_width  = priv->col_widths[x].size;
  const int page_height = priv->row_heights[y].size;
  const int value_h     = gtk_adjustment_get_value(priv->hadjustment) - priv->col_widths[x].pos;
  const int value_v     = gtk_adjustment_get_value(priv->vadjustment) - priv->row_heights[y].pos;

  /* clamp x and y offsets so we don't leave the page */
  const int clamp_h = MAX(MIN(-value_h, 0), -(page_width - width));
  const int clamp_v = MAX(MIN(-value_v, 0), -(page_height - height));

  page_alloc->x      = ((int)page_width < width) ? 0 : clamp_h;
  page_alloc->y      = ((int)page_height < height) ? 0 : clamp_v;
  page_alloc->width  = MAX(page_width, width);
  page_alloc->height = MAX(page_height, height);
}

static void size_allocate_grid(ZathuraDocumentWidget* document, GtkAllocation* allocation) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = zathura_get_document(priv->zathura);

  const unsigned int npag = zathura_document_get_number_of_pages(z_document);

  gtk_widget_show_all(GTK_WIDGET(document));

  int adj_v, adj_h;
  document_adjustment(document, allocation->height, allocation->width, &adj_v, &adj_h);

  unsigned int x, y, row, col;

  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page   = zathura_document_get_page(z_document, i);
    GtkWidget* page_widget = zathura_page_get_widget(priv->zathura, page);
    zathura_document_widget_get_page_position(document, i, &row, &col);

    x = priv->pages_right_to_left ? priv->ncol - 1 - col : col;
    y = row;

    document_widget_line_s col_line = priv->col_widths[x];
    document_widget_line_s row_line = priv->row_heights[y];

    GtkAllocation page_alloc = {
        .x      = col_line.pos - adj_h,
        .y      = row_line.pos - adj_v,
        .width  = col_line.size,
        .height = row_line.size,
    };

    gtk_widget_size_allocate(page_widget, &page_alloc);
  }
}

static void size_allocate_single_page(ZathuraDocumentWidget* document, GtkAllocation* allocation) {
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = zathura_get_document(priv->zathura);

  const unsigned int npag    = zathura_document_get_number_of_pages(z_document);
  const unsigned int page_id = zathura_document_get_current_page_number(z_document);

  GtkAllocation page_alloc;
  page_allocation(document, page_id, allocation->height, allocation->width, &page_alloc);

  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page   = zathura_document_get_page(z_document, i);
    GtkWidget* page_widget = zathura_page_get_widget(priv->zathura, page);

    gtk_widget_set_visible(page_widget, page_id == i);

    if (i == page_id) {
      gtk_widget_size_allocate(page_widget, &page_alloc);
    }
  }
}

static void zathura_document_widget_size_allocate(GtkWidget* widget, GtkAllocation* allocation) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(widget);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = zathura_get_document(priv->zathura);

  if (z_document == NULL || priv->zathura == NULL) {
    return;
  }

  gtk_adjustment_set_page_size(priv->hadjustment, allocation->width);
  gtk_adjustment_set_page_size(priv->vadjustment, allocation->height);

  zathura_document_set_viewport_height(z_document, allocation->height);
  zathura_document_set_viewport_width(z_document, allocation->width);

  adjust_view(priv->zathura);

  /* allocate pages */
  switch (priv->layout_mode) {
  case DOCUMENT_WIDGET_GRID:
    size_allocate_grid(document, allocation);
    break;
  case DOCUMENT_WIDGET_SINGLE:
    size_allocate_single_page(document, allocation);
    break;
  default:
    girara_error("unknown layout mode");
  }

  GTK_WIDGET_CLASS(zathura_document_widget_parent_class)->size_allocate(widget, allocation);
}

/* container class funcs */

static void zathura_document_widget_container_add(GtkContainer* container, GtkWidget* widget) {
  GtkWidget* parent = gtk_widget_get_parent(widget);
  if (parent != NULL) {
    girara_warning("page widget is already added to a document widget");
    return;
  }

  gtk_widget_set_parent(widget, GTK_WIDGET(container));
}

static void zathura_document_widget_container_remove(GtkContainer* UNUSED(container), GtkWidget* widget) {
  gtk_widget_unparent(widget);
}

static void zathura_document_widget_container_forall(GtkContainer* container, gboolean UNUSED(include_internals),
                                                     GtkCallback callback, gpointer user_data) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(container);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  zathura_document_t* z_document = zathura_get_document(priv->zathura);
  unsigned int npag              = zathura_document_get_number_of_pages(z_document);

  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page   = zathura_document_get_page(z_document, i);
    GtkWidget* page_widget = zathura_page_get_widget(priv->zathura, page);

    callback(page_widget, user_data);
  }
}

static void zathura_document_widget_dispose(GObject* object) {
  ZathuraDocumentWidget* document    = ZATHURA_DOCUMENT_WIDGET(object);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  g_free(priv->col_widths);
  g_free(priv->row_heights);

  priv->col_widths  = NULL;
  priv->row_heights = NULL;

  G_OBJECT_CLASS(zathura_document_widget_parent_class)->dispose(object);
}

void zathura_document_widget_refresh_layout(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document     = zathura_get_document(priv->zathura);

  const unsigned int c0   = zathura_document_get_first_page_column(z_document);
  const unsigned int npag = zathura_document_get_number_of_pages(z_document);
  const unsigned int ncol = zathura_document_get_pages_per_row(z_document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol;

  g_free(priv->col_widths);
  g_free(priv->row_heights);

  priv->col_widths  = g_try_malloc_n(ncol, sizeof(document_widget_line_s));
  priv->row_heights = g_try_malloc_n(nrow, sizeof(document_widget_line_s));

  priv->ncol = ncol;
  priv->nrow = nrow;

  // parent all page widgets to the document widget
  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page   = zathura_document_get_page(z_document, i);
    GtkWidget* page_widget = zathura_page_get_widget(priv->zathura, page);

    gtk_widget_unparent(page_widget);
    gtk_container_add(GTK_CONTAINER(document), page_widget);
  }

  zathura_document_widget_compute_layout(document);

  gtk_widget_set_visible(GTK_WIDGET(document), true);
  gtk_widget_queue_resize(GTK_WIDGET(document));
}

void zathura_document_widget_compute_layout(ZathuraDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_widget_arrange_grid(document);

  /* update allocation values */
  unsigned int doc_height, doc_width;
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
  zathura_document_t* z_document     = zathura_get_document(priv->zathura);

  if (priv->col_widths == NULL || priv->row_heights == NULL) {
    return;
  }

  const unsigned int npag = zathura_document_get_number_of_pages(z_document);
  if (page_index >= npag) {
    girara_warning("tried to get cell size for page %d, document has %d pages", page_index, npag);
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
  zathura_document_t* z_document     = zathura_get_document(priv->zathura);

  if (priv->col_widths == NULL || priv->row_heights == NULL) {
    return;
  }

  const unsigned int npag = zathura_document_get_number_of_pages(z_document);
  if (page_index >= npag) {
    girara_warning("tried to get cell size for page %d, document has %d pages", page_index, npag);
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

  if (priv->col_widths == NULL || priv->row_heights == NULL) {
    return;
  }

  if (row >= priv->nrow) {
    girara_warning("tried to get row %d size, document has %d rows", row, priv->nrow);
    return;
  }

  *pos  = priv->row_heights[row].pos;
  *size = priv->row_heights[row].size;
}

void zathura_document_widget_get_col(ZathuraDocumentWidget* document, unsigned int col, unsigned int* pos,
                                     unsigned int* size) {
  g_return_if_fail(document != NULL && pos != NULL && size != NULL);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  if (priv->col_widths == NULL || priv->row_heights == NULL) {
    return;
  }

  if (col >= priv->ncol) {
    girara_warning("tried to get col %d size, document has %d columns", col, priv->ncol);
    return;
  }

  *pos  = priv->col_widths[col].pos;
  *size = priv->col_widths[col].size;
}

void zathura_document_widget_get_document_size(ZathuraDocumentWidget* document, unsigned int* height,
                                               unsigned int* width) {
  g_return_if_fail(document != NULL && height != NULL && width != NULL);
  ZathuraDocumentWidgetPrivate* priv = zathura_document_widget_get_instance_private(document);

  if (priv->col_widths == NULL || priv->row_heights == NULL) {
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
  zathura_document_t* z_document     = zathura_get_document(priv->zathura);

  const unsigned int npag = zathura_document_get_number_of_pages(z_document);

  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page   = zathura_document_get_page(z_document, i);
    GtkWidget* page_widget = zathura_page_get_widget(priv->zathura, page);

    gtk_widget_unparent(page_widget);
  }
}
