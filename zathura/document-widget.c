/* SPDX-License-Identifier: Zlib */

#include "cairo.h"
#include "gdk/gdk.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "glib-object.h"
#include "zathura/document.h"
#include "zathura/page-widget.h"
#include "zathura/page.h"
#include "zathura/types.h"
#include "zathura/zathura.h"
#include <girara/log.h>
#include "document-widget.h"

typedef struct zathura_document_widget_private_s {
  zathura_t *zathura;
  unsigned int spacing;
  unsigned int pages_per_row;
  unsigned int first_page_column;
  bool page_right_to_left;
} ZathuraDocumentPrivate;

G_DEFINE_TYPE_WITH_CODE(ZathuraDocument, zathura_document_widget, GTK_TYPE_GRID, G_ADD_PRIVATE(ZathuraDocument))

static void zathura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);

enum properties_e {
  PROP_0,
  PROP_ZATHURA,
  PROP_SPACING,
  PROP_PAGES_PER_ROW,
  PROP_FIRST_PAGE_COLUMN,
  PROP_PAGE_RIGHT_TO_LEFT,
};

static void zathura_document_widget_class_init(ZathuraDocumentClass* class) {
  /* overwrite methods */
  // GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->set_property = zathura_document_widget_set_property;

  /* add properties */
  g_object_class_install_property(
      object_class, PROP_ZATHURA,
      g_param_spec_pointer("zathura", "zathura", "zathura context",
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      object_class, PROP_SPACING,
      g_param_spec_uint("spacing", "spacing", "page spacing",
                           0, G_MAXUINT, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      object_class, PROP_PAGES_PER_ROW,
      g_param_spec_uint("pages-per-row", "pages-per-row", "number of pages per row",
                           1, 3, 1, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      object_class, PROP_FIRST_PAGE_COLUMN,
      g_param_spec_uint("first-page-column", "first-page-column", "column to display the first page",
                           0, 2, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      object_class, PROP_PAGE_RIGHT_TO_LEFT,
      g_param_spec_boolean("page-right-to-left", "page-right-to-left", "render pages right to left", 
                           false, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}

static void zathura_document_widget_init(ZathuraDocument *widget) {
  ZathuraDocumentPrivate *priv = zathura_document_widget_get_instance_private(widget);

  priv->zathura = NULL;
}

GtkWidget* zathura_document_widget_new(zathura_t *zathura) {
  GObject* ret = g_object_new(ZATHURA_TYPE_DOCUMENT, "zathura", zathura, NULL);
  if (ret == NULL) {
    return NULL;
  }

  ZathuraDocument* widget      = ZATHURA_DOCUMENT(ret);

  gtk_grid_set_row_homogeneous(GTK_GRID(widget), TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(widget), TRUE);

  gtk_widget_set_halign(GTK_WIDGET(widget), GTK_ALIGN_CENTER);
  gtk_widget_set_valign(GTK_WIDGET(widget), GTK_ALIGN_CENTER);

  gtk_widget_set_hexpand_set(GTK_WIDGET(widget), TRUE);
  gtk_widget_set_hexpand(GTK_WIDGET(widget), FALSE);
  gtk_widget_set_vexpand_set(GTK_WIDGET(widget), TRUE);
  gtk_widget_set_vexpand(GTK_WIDGET(widget), FALSE);

  return GTK_WIDGET(ret);
}

static void zathura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
  ZathuraDocument *widget = ZATHURA_DOCUMENT(object);
  ZathuraDocumentPrivate *priv = zathura_document_widget_get_instance_private(widget);

  switch (prop_id) {
    case PROP_ZATHURA:
      priv->zathura = g_value_get_pointer(value);
      break;
    case PROP_SPACING:
      priv->spacing = g_value_get_uint(value);
      break;
    case PROP_FIRST_PAGE_COLUMN:
      priv->first_page_column = g_value_get_uint(value);
      break;
    case PROP_PAGES_PER_ROW:
      priv->pages_per_row = g_value_get_uint(value);
      break;
    case PROP_PAGE_RIGHT_TO_LEFT:
      priv->page_right_to_left = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void remove_page_from_table(GtkWidget* page, gpointer UNUSED(permanent)) {
  gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(page)), page);
}

void zathura_document_widget_clear_pages(GtkWidget *widget) {
  gtk_container_foreach(GTK_CONTAINER(widget), remove_page_from_table, NULL);
}

void zathura_document_widget_update_pages(GtkWidget *widget) {
  ZathuraDocument *document = ZATHURA_DOCUMENT(widget);
  ZathuraDocumentPrivate *priv = zathura_document_widget_get_instance_private(document);
  zathura_t *zathura = priv->zathura;

  if (zathura == NULL) {
    return;
  }

  if (zathura->document == NULL) {
    return;
  }

  int page_count = zathura_document_get_number_of_pages(zathura->document);

  gtk_grid_set_row_spacing(GTK_GRID(widget), priv->spacing);
  gtk_grid_set_column_spacing(GTK_GRID(widget), priv->spacing);

  zathura_document_widget_clear_pages(widget);

  for (int i = 0; i < page_count; i++) {
    int x = (i + priv->first_page_column - 1) % priv->pages_per_row;
    int y = (i + priv->first_page_column - 1) / priv->pages_per_row;

    GtkWidget* page_widget = zathura->pages[i];
    if (priv->page_right_to_left) {
      x = priv->pages_per_row - 1 - x;
    }

    gtk_grid_attach(GTK_GRID(widget), page_widget, x, y, 1, 1);
  }
}
