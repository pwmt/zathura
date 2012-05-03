/* See LICENSE file for license and copyright information */

#include <girara/utils.h>
#include <girara/settings.h>
#include <girara/datastructures.h>
#include <girara/session.h>
#include <string.h>
#include <glib/gi18n.h>

#include "page-widget.h"
#include "page.h"
#include "render.h"
#include "utils.h"
#include "shortcuts.h"

G_DEFINE_TYPE(ZathuraPage, zathura_page_widget, GTK_TYPE_DRAWING_AREA)

typedef struct zathura_page_widget_private_s {
  zathura_page_t* page;
  zathura_t* zathura;
  cairo_surface_t* surface; /**< Cairo surface */
  GStaticMutex lock; /**< Lock */
  girara_list_t* links; /**< List of links on the page */
  bool links_got; /**< True if we already tried to retrieve the list of links */
  bool draw_links; /**< True if links should be drawn */
  unsigned int link_offset; /**< Offset to the links */
  unsigned int number_of_links; /**< Offset to the links */
  girara_list_t* search_results; /**< A list if there are search results that should be drawn */
  int search_current; /**< The index of the current search result */
  bool draw_search_results; /**< Draw search results */
  zathura_rectangle_t selection; /**< Region selected with the mouse */
  struct {
    int x;
    int y;
  } selection_basepoint;
  girara_list_t* images; /**< List of images on the page */
  bool images_got; /**< True if we already tried to retrieve the list of images */
  zathura_image_t* current_image; /**< Image data of selected image */
  gint64 last_view;

  struct {
    bool retrieved;
    girara_list_t* list;
    zathura_annotation_t* current;
  } annotations;

  struct {
    struct {
      int x;
      int y;
    } mouse_click;
  } events;
} zathura_page_widget_private_t;

typedef void (*menu_callback_t)(GtkMenuItem*, ZathuraPage*);

#define ZATHURA_PAGE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ZATHURA_TYPE_PAGE, zathura_page_widget_private_t))

static gboolean zathura_page_widget_draw(GtkWidget* widget, cairo_t* cairo);
#if GTK_MAJOR_VERSION == 2
static gboolean zathura_page_widget_expose(GtkWidget* widget, GdkEventExpose* event);
#endif
static void zathura_page_widget_finalize(GObject* object);
static void zathura_page_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void zathura_page_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void zathura_page_widget_size_allocate(GtkWidget* widget, GdkRectangle* allocation);
static void redraw_rect(ZathuraPage* widget, zathura_rectangle_t* rectangle);
static void redraw_all_rects(ZathuraPage* widget, girara_list_t* rectangles);
static void zathura_page_widget_popup_menu(GtkWidget* widget, GdkEventButton* event);
static gboolean cb_zathura_page_widget_button_press_event(GtkWidget* widget, GdkEventButton* button);
static gboolean cb_zathura_page_widget_button_release_event(GtkWidget* widget, GdkEventButton* button);
static gboolean cb_zathura_page_widget_motion_notify(GtkWidget* widget, GdkEventMotion* event);
static gboolean cb_zathura_page_widget_popup_menu(GtkWidget* widget);
static void menu_add_item(GtkWidget* menu, GtkWidget* page, char* text, menu_callback_t callback);
static void cb_menu_annotation_properties(GtkMenuItem* item, ZathuraPage* page);
static void cb_menu_annotation_add(GtkMenuItem* item, ZathuraPage* page);
static void cb_menu_image_copy(GtkMenuItem* item, ZathuraPage* page);
static void cb_menu_image_save(GtkMenuItem* item, ZathuraPage* page);

enum properties_e
{
  PROP_0,
  PROP_PAGE,
  PROP_ZATHURA,
  PROP_DRAW_LINKS,
  PROP_LINKS_OFFSET,
  PROP_LINKS_NUMBER,
  PROP_SEARCH_RESULT,
  PROP_SEARCH_RESULTS,
  PROP_SEARCH_RESULTS_LENGTH,
  PROP_SEARCH_RESULTS_CURRENT,
  PROP_DRAW_SEARCH_RESULTS
};

static void
zathura_page_widget_class_init(ZathuraPageClass* class)
{
  /* add private members */
  g_type_class_add_private(class, sizeof(zathura_page_widget_private_t));

  /* overwrite methods */
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
#if GTK_MAJOR_VERSION == 3
  widget_class->draw                 = zathura_page_widget_draw;
#else
  widget_class->expose_event         = zathura_page_widget_expose;
#endif
  widget_class->size_allocate        = zathura_page_widget_size_allocate;
  widget_class->button_press_event   = cb_zathura_page_widget_button_press_event;
  widget_class->button_release_event = cb_zathura_page_widget_button_release_event;
  widget_class->motion_notify_event  = cb_zathura_page_widget_motion_notify;
  widget_class->popup_menu           = cb_zathura_page_widget_popup_menu;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = zathura_page_widget_finalize;
  object_class->set_property = zathura_page_widget_set_property;
  object_class->get_property = zathura_page_widget_get_property;

  /* add properties */
  g_object_class_install_property(object_class, PROP_PAGE,
      g_param_spec_pointer("page", "page", "the page to draw", G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(object_class, PROP_ZATHURA,
      g_param_spec_pointer("zathura", "zathura", "the zathura instance", G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(object_class, PROP_DRAW_LINKS,
      g_param_spec_boolean("draw-links", "draw-links", "Set to true if links should be drawn", FALSE, G_PARAM_WRITABLE));
  g_object_class_install_property(object_class, PROP_LINKS_OFFSET,
      g_param_spec_int("offset-links", "offset-links", "Offset for the link numbers", 0, INT_MAX, 0, G_PARAM_WRITABLE));
  g_object_class_install_property(object_class, PROP_LINKS_NUMBER,
      g_param_spec_int("number-of-links", "number-of-links", "Number of links", 0, INT_MAX, 0, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS,
      g_param_spec_pointer("search-results", "search-results", "Set to the list of search results", G_PARAM_WRITABLE | G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS_CURRENT,
      g_param_spec_int("search-current", "search-current", "The current search result", -1, INT_MAX, 0, G_PARAM_WRITABLE | G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_SEARCH_RESULTS_LENGTH,
      g_param_spec_int("search-length", "search-length", "The number of search results", -1, INT_MAX, 0, G_PARAM_READABLE));
  g_object_class_install_property(object_class, PROP_DRAW_SEARCH_RESULTS,
      g_param_spec_boolean("draw-search-results", "draw-search-results", "Set to true if search results should be drawn", FALSE, G_PARAM_WRITABLE));
}

static void
zathura_page_widget_init(ZathuraPage* widget)
{
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  priv->page                  = NULL;
  priv->surface               = NULL;
  priv->links                 = NULL;
  priv->links_got             = false;
  priv->link_offset           = 0;
  priv->search_results        = NULL;
  priv->search_current        = INT_MAX;
  priv->selection.x1          = -1;
  priv->selection_basepoint.x = -1;
  priv->selection_basepoint.y = -1;
  priv->images                = NULL;
  priv->images_got            = false;
  priv->current_image         = NULL;
  priv->last_view             = g_get_real_time();
  priv->draw_search_results   = true;
  priv->annotations.retrieved = false;
  priv->annotations.list      = NULL;
  priv->annotations.current   = NULL;
  g_static_mutex_init(&(priv->lock));

  /* we want mouse events */
  gtk_widget_add_events(GTK_WIDGET(widget),
    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
}

GtkWidget*
zathura_page_widget_new(zathura_t* zathura, zathura_page_t* page)
{
  g_return_val_if_fail(page != NULL, NULL);

  return g_object_new(ZATHURA_TYPE_PAGE, "page", page, "zathura", zathura, NULL);
}

static void
zathura_page_widget_finalize(GObject* object)
{
  ZathuraPage* widget = ZATHURA_PAGE(object);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

  if (priv->surface != NULL) {
    cairo_surface_destroy(priv->surface);
  }

  if (priv->search_results != NULL) {
    girara_list_free(priv->search_results);
  }

  if (priv->links != NULL) {
    girara_list_free(priv->links);
  }

  if (priv->annotations.list != NULL) {
    girara_list_free(priv->annotations.list);
  }

  g_static_mutex_free(&(priv->lock));

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
      priv->draw_links = g_value_get_boolean(value);
      /* get links */
      if (priv->draw_links == true && priv->links_got == false) {
        priv->links           = zathura_page_links_get(priv->page, NULL);
        priv->links_got       = true;
        priv->number_of_links = (priv->links == NULL) ? 0 : girara_list_size(priv->links);
      }

      if (priv->links_got == true && priv->links != NULL) {
        GIRARA_LIST_FOREACH(priv->links, zathura_link_t*, iter, link)
          if (link != NULL) {
            zathura_rectangle_t rectangle = recalc_rectangle(priv->page, link->position);
            redraw_rect(pageview, &rectangle);
          }
        GIRARA_LIST_FOREACH_END(priv->links, zathura_link_t*, iter, link);
      }
      break;
    case PROP_LINKS_OFFSET:
      priv->link_offset = g_value_get_int(value);
      break;
    case PROP_SEARCH_RESULTS:
      if (priv->search_results != NULL && priv->draw_search_results) {
        redraw_all_rects(pageview, priv->search_results);
        girara_list_free(priv->search_results);
      }
      priv->search_results = g_value_get_pointer(value);
      if (priv->search_results != NULL && priv->draw_search_results) {
        priv->draw_links = false;
        redraw_all_rects(pageview, priv->search_results);
      }
      priv->search_current = -1;
      break;
    case PROP_SEARCH_RESULTS_CURRENT: {
      g_return_if_fail(priv->search_results != NULL);
      if (priv->search_current >= 0 && priv->search_current < (signed) girara_list_size(priv->search_results)) {
        zathura_rectangle_t* rect = girara_list_nth(priv->search_results, priv->search_current);
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
        redraw_rect(pageview, &rectangle);
      }
      int val = g_value_get_int(value);
      if (val < 0) {
        priv->search_current = girara_list_size(priv->search_results);
      } else {
        priv->search_current = val;
        zathura_rectangle_t* rect = girara_list_nth(priv->search_results, priv->search_current);
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);
        if (priv->draw_search_results) {
          redraw_rect(pageview, &rectangle);
        }
      }
      break;
    }
    case PROP_DRAW_SEARCH_RESULTS:
      priv->draw_search_results = g_value_get_boolean(value);
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
      g_value_set_int(value, priv->number_of_links);
      break;
    case PROP_SEARCH_RESULTS_LENGTH:
      g_value_set_int(value, priv->search_results == NULL ? 0 : girara_list_size(priv->search_results));
      break;
    case PROP_SEARCH_RESULTS_CURRENT:
      g_value_set_int(value, priv->search_results == NULL ? -1 : priv->search_current);
      break;
    case PROP_SEARCH_RESULTS:
      g_value_set_pointer(value, priv->search_results);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

#if GTK_MAJOR_VERSION == 2
static gboolean
zathura_page_widget_expose(GtkWidget* widget, GdkEventExpose* event)
{
  cairo_t* cairo = gdk_cairo_create(gtk_widget_get_window(widget));
  if (cairo == NULL) {
    girara_error("Could not retrieve cairo object");
    return FALSE;
  }

  /* set clip region */
  cairo_rectangle(cairo, event->area.x, event->area.y, event->area.width, event->area.height);
  cairo_clip(cairo);

  const gboolean ret = zathura_page_widget_draw(widget, cairo);
  cairo_destroy(cairo);
  return ret;
}
#endif

static gboolean
zathura_page_widget_draw(GtkWidget* widget, cairo_t* cairo)
{
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  g_static_mutex_lock(&(priv->lock));

  zathura_document_t* document = zathura_page_get_document(priv->page);

#if GTK_MAJOR_VERSION == 2
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  const unsigned int page_height = allocation.height;
  const unsigned int page_width  = allocation.width;
#else
  const unsigned int page_height = gtk_widget_get_allocated_height(widget);
  const unsigned int page_width  = gtk_widget_get_allocated_width(widget);
#endif

  if (priv->surface != NULL) {
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
        if (link != NULL) {
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
          cairo_move_to(cairo, rectangle.x1 + 1, rectangle.y2 - 1);
          char* link_number = g_strdup_printf("%i", priv->link_offset + ++link_counter);
          cairo_show_text(cairo, link_number);
          g_free(link_number);
        }
      GIRARA_LIST_FOREACH_END(priv->links, zathura_link_t*, iter, link);
    }

    /* draw search results */
    if (priv->search_results != NULL && priv->draw_search_results == true) {
      int idx = 0;
      GIRARA_LIST_FOREACH(priv->search_results, zathura_rectangle_t*, iter, rect)
        zathura_rectangle_t rectangle = recalc_rectangle(priv->page, *rect);

        /* draw position */
        if (idx == priv->search_current) {
          GdkColor color = priv->zathura->ui.colors.highlight_color_active;
          cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
        } else {
          GdkColor color = priv->zathura->ui.colors.highlight_color;
          cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
        }
        cairo_rectangle(cairo, rectangle.x1, rectangle.y1,
            (rectangle.x2 - rectangle.x1), (rectangle.y2 - rectangle.y1));
        cairo_fill(cairo);
        ++idx;
      GIRARA_LIST_FOREACH_END(priv->search_results, zathura_rectangle_t*, iter, rect);
    }
    /* draw selection */
    if (priv->selection.y2 != -1 && priv->selection.x2 != -1) {
      GdkColor color = priv->zathura->ui.colors.highlight_color;
      cairo_set_source_rgba(cairo, color.red, color.green, color.blue, transparency);
      cairo_rectangle(cairo, priv->selection.x1, priv->selection.y1,
        (priv->selection.x2 - priv->selection.x1), (priv->selection.y2 - priv->selection.y1));
      cairo_fill(cairo);
    }
  } else {
    /* set background color */
    cairo_set_source_rgb(cairo, 255, 255, 255);
    cairo_rectangle(cairo, 0, 0, page_width, page_height);
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
      double x = page_width * 0.5 - (extents.width * 0.5 + extents.x_bearing);
      double y = page_height * 0.5 - (extents.height * 0.5 + extents.y_bearing);
      cairo_move_to(cairo, x, y);
      cairo_show_text(cairo, text);
    }

    /* render real page */
    render_page(priv->zathura->sync.render_thread, priv->page);
  }
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
  grect.y = rectangle->y1;
  grect.width  = rectangle->x2 - rectangle->x1;
  grect.height = rectangle->y2 - rectangle->y1;
#if (GTK_MAJOR_VERSION == 3)
  gtk_widget_queue_draw_area(GTK_WIDGET(widget), grect.x, grect.y, grect.width, grect.height);
#else
  gdk_window_invalidate_rect(gtk_widget_get_window(GTK_WIDGET(widget)), &grect, TRUE);
#endif
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
      girara_list_size(priv->links) > index - priv->link_offset) {
    return girara_list_nth(priv->links, index - priv->link_offset);
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

  /* reset selected items */
  priv->current_image       = NULL;
  priv->annotations.current = NULL;

  if (button->type == GDK_BUTTON_PRESS) {
    priv->events.mouse_click.x = button->x;
    priv->events.mouse_click.y = button->y;
  } else {
    priv->events.mouse_click.x = -1;
    priv->events.mouse_click.y = -1;
  }

  if (button->button == 1) { /* left click */
    if (button->type == GDK_BUTTON_PRESS) {
      /* start the selection */
      priv->selection_basepoint.x = button->x;
      priv->selection_basepoint.y = button->y;
      priv->selection.x1 = button->x;
      priv->selection.y1 = button->y;
      priv->selection.x2 = button->x;
      priv->selection.y2 = button->y;
    } else if (button->type == GDK_2BUTTON_PRESS || button->type == GDK_3BUTTON_PRESS) {
      /* abort the selection */
      priv->selection_basepoint.x = -1;
      priv->selection_basepoint.y = -1;
      priv->selection.x1 = -1;
      priv->selection.y1 = -1;
      priv->selection.x2 = -1;
      priv->selection.y2 = -1;
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
  if (button->type != GDK_BUTTON_RELEASE || button->button != 1) {
    return false;
  }

  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  zathura_document_t* document        = zathura_page_get_document(priv->page);

  if (priv->selection.y2 == -1 && priv->selection.x2 == -1 ) {
    /* simple single click */
    /* get links */
    if (priv->links_got == false) {
      priv->links = zathura_page_links_get(priv->page, NULL);
      priv->links_got = true;
      priv->number_of_links = (priv->links == NULL) ? 0 : girara_list_size(priv->links);
    }

    if (priv->links != NULL && priv->number_of_links > 0) {
      GIRARA_LIST_FOREACH(priv->links, zathura_link_t*, iter, link)
        zathura_rectangle_t rect = recalc_rectangle(priv->page, link->position);
        if (rect.x1 <= button->x && rect.x2 >= button->x
            && rect.y1 <= button->y && rect.y2 >= button->y) {
          zathura_link_evaluate(priv->zathura, link);
        }
      GIRARA_LIST_FOREACH_END(priv->links, zathura_link_t*, iter, link);
    }
  } else {
    redraw_rect(ZATHURA_PAGE(widget), &priv->selection);
    zathura_rectangle_t tmp = priv->selection;

    double scale = zathura_document_get_scale(document);
    tmp.x1 /= scale;
    tmp.x2 /= scale;
    tmp.y1 /= scale;
    tmp.y2 /= scale;

    char* text = zathura_page_get_text(priv->page, tmp, NULL);
    if (text != NULL) {
      if (strlen(text) > 0) {
        /* copy to clipboard */
        gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), text, -1);


        if (priv->page != NULL && document != NULL && priv->zathura != NULL) {
          char* stripped_text = g_strdelimit(g_strdup(text), "\n\t\r\n", ' ');
          girara_notify(priv->zathura->ui.session, GIRARA_INFO, _("Copied selected text to clipboard: %s"), stripped_text);
          g_free(stripped_text);
        }
      }

      g_free(text);
    }
  }

  priv->selection_basepoint.x = -1;
  priv->selection_basepoint.y = -1;
  priv->selection.x1 = -1;
  priv->selection.y1 = -1;
  priv->selection.x2 = -1;
  priv->selection.y2 = -1;

  return false;
}

static gboolean
cb_zathura_page_widget_motion_notify(GtkWidget* widget, GdkEventMotion* event)
{
  g_return_val_if_fail(widget != NULL, false);
  g_return_val_if_fail(event != NULL, false);
  if ((event->state & GDK_BUTTON1_MASK) == 0) {
    return false;
  }

  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  zathura_rectangle_t tmp = priv->selection;
  if (event->x < priv->selection_basepoint.x) {
    tmp.x1 = event->x;
    tmp.x2 = priv->selection_basepoint.x;
  } else {
    tmp.x2 = event->x;
    tmp.x1 = priv->selection_basepoint.x;
  }
  if (event->y < priv->selection_basepoint.y) {
    tmp.y1 = event->y;
    tmp.y2 = priv->selection_basepoint.y;
  } else {
    tmp.y1 = priv->selection_basepoint.y;
    tmp.y2 = event->y;
  }

  redraw_rect(ZATHURA_PAGE(widget), &priv->selection);
  redraw_rect(ZATHURA_PAGE(widget), &tmp);
  priv->selection = tmp;

  return false;
}

static void
zathura_page_widget_popup_menu(GtkWidget* widget, GdkEventButton* event)
{
  g_return_if_fail(widget != NULL);
  g_return_if_fail(event != NULL);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

#if GTK_MAJOR_VERSION == 3 // FIXME
  return;
#endif

  /* setup menu */
  GtkWidget* menu = gtk_menu_new();

  /* images */
  if (priv->images_got == false) {
    priv->images     = zathura_page_images_get(priv->page, NULL);
    priv->images_got = true;
  }

  /* search for underlaying image */
  zathura_image_t* image = NULL;
  GIRARA_LIST_FOREACH(priv->images, zathura_image_t*, iter, image_it)
    zathura_rectangle_t rect = recalc_rectangle(priv->page, image_it->position);
    if (rect.x1 <= event->x && rect.x2 >= event->x && rect.y1 <= event->y && rect.y2 >= event->y) {
      image = image_it;
    }
  GIRARA_LIST_FOREACH_END(priv->images, zathura_image_t*, iter, image_it);

  if (image != NULL) {
    menu_add_item(menu, widget, _("Copy image"),    cb_menu_image_copy);
    menu_add_item(menu, widget, _("Save image as"), cb_menu_image_save);

    priv->current_image = image;
  }

  /* annotations */
  if (priv->annotations.retrieved == false) {
    priv->annotations.list = zathura_page_get_annotations(priv->page, NULL);
    priv->annotations.retrieved = true;
  }

  zathura_annotation_t* annotation = NULL;
  GIRARA_LIST_FOREACH(priv->annotations.list, zathura_annotation_t*, iter, annot)
    zathura_rectangle_t rect = recalc_rectangle(priv->page, zathura_annotation_get_position(annot));
    if (rect.x1 <= event->x && rect.x2 >= event->x && rect.y1 <= event->y && rect.y2 >= event->y) {
      annotation = annot;
    }
  GIRARA_LIST_FOREACH_END(priv->annotations.list, zathura_annotation_t*, iter, annot);

  if (annotation != NULL) {
    menu_add_item(menu, widget, _("Annotation properties"), cb_menu_annotation_properties);
    priv->annotations.current = annotation;
  } else {
    menu_add_item(menu, widget, _("Add annotation"), cb_menu_annotation_add);
  }

  /* attach and popup */
  GList* children = gtk_container_get_children(GTK_CONTAINER(menu));
  if (g_list_length(children) > 0) {
    int event_button = 0;
    int event_time   = gtk_get_current_event_time();

    if (event != NULL) {
      event_button = event->button;
      event_time   = event->time;
    }

    gtk_menu_attach_to_widget(GTK_MENU(menu), widget, NULL);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event_button, event_time);
  } else {
    gtk_widget_destroy(menu);
  }
  g_list_free(children);
}

static gboolean
cb_zathura_page_widget_popup_menu(GtkWidget* widget)
{
  zathura_page_widget_popup_menu(widget, NULL);

  return TRUE;
}

static void
cb_menu_annotation_add(GtkMenuItem* item, ZathuraPage* page)
{
  g_return_if_fail(item != NULL);
  g_return_if_fail(page != NULL);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(page);
  girara_session_t* gsession = priv->zathura->ui.session;
  g_return_if_fail(gsession != NULL);


  /* create new annotation */
  zathura_annotation_t* annotation =
    zathura_annotation_new(ZATHURA_ANNOTATION_MARKUP);
  if (annotation == NULL) {
    return;
  }

  zathura_rectangle_t position = {
    priv->events.mouse_click.x,
    priv->events.mouse_click.y,
    priv->events.mouse_click.x,
    priv->events.mouse_click.y
  };

  zathura_annotation_set_position(annotation, position);

  /* create new list if necessary */
  if (priv->annotations.list == NULL) {
    priv->annotations.list = girara_list_new2((girara_free_function_t)
        zathura_annotation_free);

    if (priv->annotations.list == NULL) {
      zathura_annotation_free(annotation);
      return;
    }
  }

  /* save to current list */
  girara_list_append(priv->annotations.list, annotation);
  zathura_page_set_annotations(priv->page, priv->annotations.list);

  /* retrieve new list */
  girara_list_free(priv->annotations.list);
  priv->annotations.list = zathura_page_get_annotations(priv->page, NULL);
  priv->annotations.retrieved = true;
}

static void
cb_menu_annotation_properties(GtkMenuItem* item, ZathuraPage* page)
{
  g_return_if_fail(item != NULL);
  g_return_if_fail(page != NULL);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(page);
  zathura_annotation_t* annotation = priv->annotations.current;
  girara_session_t* gsession = priv->zathura->ui.session;
  g_return_if_fail(annotation != NULL);
  g_return_if_fail(gsession != NULL);

  girara_notify(gsession, GIRARA_INFO, _("Annotation name: %s"), zathura_annotation_get_name(annotation));

  priv->annotations.current = NULL;
}

static void
cb_menu_image_copy(GtkMenuItem* item, ZathuraPage* page)
{
#if GTK_MAJOR_VERSION == 2 // FIXME
  g_return_if_fail(item != NULL);
  g_return_if_fail(page != NULL);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(page);
  g_return_if_fail(priv->current_image != NULL);

  cairo_surface_t* surface = zathura_page_image_get_cairo(priv->page, priv->current_image, NULL);
  if (surface == NULL) {
    return;
  }

  int width  = cairo_image_surface_get_width(surface);
  int height = cairo_image_surface_get_height(surface);

  GdkPixmap* pixmap = gdk_pixmap_new(gtk_widget_get_window(GTK_WIDGET(item)), width, height, -1);
  cairo_t* cairo    = gdk_cairo_create(pixmap);

  cairo_set_source_surface(cairo, surface, 0, 0);
  cairo_paint(cairo);
  cairo_destroy(cairo);

  GdkPixbuf* pixbuf = gdk_pixbuf_get_from_drawable(NULL, pixmap, NULL, 0, 0, 0,
      0, width, height);

  gtk_clipboard_set_image(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), pixbuf);
  gtk_clipboard_set_image(gtk_clipboard_get(GDK_SELECTION_PRIMARY), pixbuf);

  /* reset */
  priv->current_image = NULL;
#endif
}

static void
cb_menu_image_save(GtkMenuItem* item, ZathuraPage* page)
{
  g_return_if_fail(item != NULL);
  g_return_if_fail(page != NULL);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(page);
  g_return_if_fail(priv->current_image != NULL);
  g_return_if_fail(priv->images != NULL);

  /* generate image identifier */
  unsigned int page_id  = zathura_page_get_index(priv->page) + 1;
  unsigned int image_id = 1;

  GIRARA_LIST_FOREACH(priv->images, zathura_image_t*, iter, image_it)
    if (image_it == priv->current_image) {
      break;
    }

    image_id++;
  GIRARA_LIST_FOREACH_END(priv->images, zathura_image_t*, iter, image_it);

  /* set command */
  char* export_command = g_strdup_printf(":export image-p%d-%d ", page_id, image_id);
  girara_argument_t argument = { 0, export_command };
  sc_focus_inputbar(priv->zathura->ui.session, &argument, NULL, 0);
  g_free(export_command);

  /* reset */
  priv->current_image = NULL;
}

void
zathura_page_widget_update_view_time(ZathuraPage* widget)
{
  g_return_if_fail(ZATHURA_IS_PAGE(widget) == TRUE);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);

  if (zathura_page_get_visibility(priv->page) == true) {
    priv->last_view = g_get_real_time();
  }
}

void
zathura_page_widget_purge_unused(ZathuraPage* widget, gint64 threshold)
{
  g_return_if_fail(ZATHURA_IS_PAGE(widget) == TRUE);
  zathura_page_widget_private_t* priv = ZATHURA_PAGE_GET_PRIVATE(widget);
  if (zathura_page_get_visibility(priv->page) == true || priv->surface == NULL || threshold <= 0) {
    return;
  }

  const gint64 now =  g_get_real_time();
  if (now - priv->last_view >= threshold * G_USEC_PER_SEC) {
    zathura_page_widget_update_surface(widget, NULL);
  }
}

static void
menu_add_item(GtkWidget* menu, GtkWidget* page, char* text, menu_callback_t callback)
{
  if (menu == NULL || page == NULL || text == NULL || callback == NULL) {
    return;
  }

  GtkWidget* item = gtk_menu_item_new_with_label(text);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  gtk_widget_show(item);
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(callback), page);
}
