/* See LICENSE file for license and copyright information */

#include <girara/utils.h>
#include <girara/settings.h>
#include <girara/datastructures.h>
#include <girara/session.h>
#include <string.h>
#include <glib/gi18n.h>
#include <math.h>

#include "links.h"
#include "document-widget.h"
#include "render.h"
#include "utils.h"
#include "shortcuts.h"
#include "synctex.h"

G_DEFINE_TYPE(ZathuraDocument, zathura_document_widget, GTK_TYPE_BIN)

typedef struct zathura_document_widget_private_s {
  zathura_document_t* document; /**< Document object */
  zathura_t* zathura; /**< Zathura object */
  bool continuous_mode; /**< Continuous mode */
  int page_padding; /**< Page padding */
  int pages_per_row; /** Pages per row */
  int first_page_column; /** First page column */
  GtkWidget* view;
  GStaticMutex lock; /**< Lock */
} zathura_document_widget_private_t;

#define ZATHURA_DOCUMENT_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ZATHURA_TYPE_DOCUMENT, zathura_document_widget_private_t))

static void zathura_document_widget_finalize(GObject* object);
static void zathura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void zathura_document_widget_size_allocate(GtkWidget* widget, GdkRectangle* allocation);
static void zathura_document_widget_update_page_padding(zathura_document_widget_private_t* priv);
static void zathura_document_widget_update_mode(zathura_document_widget_private_t* priv);

static void
remove_page_from_table(GtkWidget* page, gpointer permanent)
{
  if (permanent == false) {
    g_object_ref(G_OBJECT(page));
  }

  gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(page)), page);
}

enum properties_e
{
  PROP_0,
  PROP_DOCUMENT,
  PROP_ZATHURA,
  PROP_CONTINUOUS_MODE,
  PROP_PAGES_PER_ROW,
  PROP_FIRST_PAGE_COLUMN,
  PROP_PAGE_PADDING
};

static void
zathura_document_widget_class_init(ZathuraDocumentClass* class)
{
  /* add private members */
  g_type_class_add_private(class, sizeof(zathura_document_widget_private_t));

  /* overwrite methods */
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
  widget_class->size_allocate  = zathura_document_widget_size_allocate;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = zathura_document_widget_finalize;
  object_class->set_property = zathura_document_widget_set_property;

  /* add properties */
  g_object_class_install_property(object_class, PROP_ZATHURA,
      g_param_spec_pointer("zathura", "zathura", "the zathura instance", G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(object_class, PROP_DOCUMENT,
      g_param_spec_pointer("document", "document", "the document_to_display", G_PARAM_WRITABLE));
  g_object_class_install_property(object_class, PROP_CONTINUOUS_MODE,
      g_param_spec_boolean("continuous-mode", "continuous-mode", "Set wether to enable or disable continuous mode", FALSE, G_PARAM_WRITABLE));
  g_object_class_install_property(object_class, PROP_PAGE_PADDING,
      g_param_spec_int("page-padding", "page-padding", "Padding between the pages", 0, INT_MAX, 0, G_PARAM_WRITABLE));
  g_object_class_install_property(object_class, PROP_PAGES_PER_ROW,
      g_param_spec_int("pages-per-row", "pages-per-row", "Pages per row", 0, INT_MAX, 1, G_PARAM_WRITABLE));
  g_object_class_install_property(object_class, PROP_FIRST_PAGE_COLUMN,
      g_param_spec_int("first-page-column", "first-page-column", "First page column", 0, INT_MAX, 1, G_PARAM_WRITABLE));
}

static void
zathura_document_widget_init(ZathuraDocument* widget)
{
  zathura_document_widget_private_t* priv = ZATHURA_DOCUMENT_GET_PRIVATE(widget);
  priv->zathura  = NULL;
  priv->document = NULL;
  priv->pages_per_row = 1;
  priv->first_page_column = 1;

#if (GTK_MAJOR_VERSION == 3)
  priv->view = gtk_grid_new();
#else
  priv->view = gtk_table_new(0, 0, TRUE);
#endif

  g_static_mutex_init(&(priv->lock));
}

GtkWidget*
zathura_document_widget_new(zathura_t* zathura)
{
  g_return_val_if_fail(zathura != NULL, NULL);

  return g_object_new(ZATHURA_TYPE_DOCUMENT, "zathura", zathura, NULL);
}

static void
zathura_document_widget_size_allocate(GtkWidget* widget, GdkRectangle* allocation)
{
  GTK_WIDGET_CLASS(zathura_document_widget_parent_class)->size_allocate(widget, allocation);
}

static void
zathura_document_widget_finalize(GObject* object)
{
  ZathuraDocument* widget = ZATHURA_DOCUMENT(object);
  zathura_document_widget_private_t* priv = ZATHURA_DOCUMENT_GET_PRIVATE(widget);

  g_static_mutex_free(&(priv->lock));

  G_OBJECT_CLASS(zathura_document_widget_parent_class)->finalize(object);
}

static void
zathura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
  ZathuraDocument* document = ZATHURA_DOCUMENT(object);
  zathura_document_widget_private_t* priv = ZATHURA_DOCUMENT_GET_PRIVATE(document);

  switch (prop_id) {
    case PROP_DOCUMENT:
      priv->document = g_value_get_pointer(value);
      break;
    case PROP_ZATHURA:
      priv->zathura = g_value_get_pointer(value);
      break;
    case PROP_CONTINUOUS_MODE:
      priv->continuous_mode = g_value_get_boolean(value);
      break;
    case PROP_PAGES_PER_ROW:
      priv->pages_per_row = g_value_get_int(value);
      zathura_document_widget_update_mode(priv);
      break;
    case PROP_PAGE_PADDING:
      priv->page_padding = g_value_get_int(value);
      zathura_document_widget_update_page_padding(priv);
      break;
    case PROP_FIRST_PAGE_COLUMN:
      priv->first_page_column = g_value_get_int(value);
      zathura_document_widget_update_mode(priv);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
zathura_document_widget_update_page_padding(zathura_document_widget_private_t* priv)
{
#if (GTK_MAJOR_VERSION == 3)
    gtk_grid_set_row_spacing(GTK_GRID(priv->view),    priv->page_padding);
    gtk_grid_set_column_spacing(GTK_GRID(priv->view), priv->page_padding);
#else
    gtk_table_set_row_spacings(GTK_TABLE(priv->view), priv->page_padding);
    gtk_table_set_col_spacings(GTK_TABLE(priv->view), priv->page_padding);
#endif
}

static void
zathura_document_widget_update_mode(zathura_document_widget_private_t* priv)
{
  /* show at least one page */
  if (priv->pages_per_row < 1) {
    priv->pages_per_row = 1;
  }

	/* ensure: 0 < first_page_column <= pages_per_row */
	if (priv->first_page_column < 1) {
		priv->first_page_column = 1;
	}

	if (priv->first_page_column > priv->pages_per_row) {
		priv->first_page_column = ((priv->first_page_column - 1) % priv->pages_per_row) + 1;
	}

  gtk_container_foreach(GTK_CONTAINER(priv->view), remove_page_from_table, (gpointer)0);

  unsigned int number_of_pages = zathura_document_get_number_of_pages(priv->document);
#if (GTK_MAJOR_VERSION == 3)
#else
  gtk_table_resize(GTK_TABLE(priv->view), ceil((number_of_pages + priv->first_page_column - 1) / priv->pages_per_row), priv->pages_per_row);
#endif

  for (unsigned int i = 0; i < number_of_pages; i++) {
    int x = (i + priv->first_page_column - 1) % priv->pages_per_row;
    int y = (i + priv->first_page_column - 1) / priv->pages_per_row;

    zathura_page_t* page   = zathura_document_get_page(priv->document, i);
    GtkWidget* page_widget = zathura_page_get_widget(priv->zathura, page);
#if (GTK_MAJOR_VERSION == 3)
    gtk_grid_attach(GTK_GRID(priv->view), page_widget, x, y, 1, 1);
#else
    gtk_table_attach(GTK_TABLE(priv->view), page_widget, x, x + 1, y, y + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
#endif
  }
}
