/* SPDX-License-Identifier: Zlib */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <gtk/gtk.h>
#include <girara/datastructures.h>
#include <girara/session.h>
#include <girara/settings.h>
#include <girara/utils.h>

#include "links.h"
#include "utils.h"
#include "zathura.h"
#include "internal.h"
#include "document.h"
#include "page.h"
#include "plugin.h"
#include "content-type.h"

double
zathura_correct_zoom_value(girara_session_t* session, const double zoom)
{
  if (session == NULL) {
    return zoom;
  }

  /* zoom limitations */
  int zoom_min_int = 10;
  int zoom_max_int = 1000;
  girara_setting_get(session, "zoom-min", &zoom_min_int);
  girara_setting_get(session, "zoom-max", &zoom_max_int);

  const double zoom_min = zoom_min_int * 0.01;
  const double zoom_max = zoom_max_int * 0.01;

  if (zoom < zoom_min) {
    return zoom_min;
  } else if (zoom > zoom_max) {
    return zoom_max;
  } else {
    return zoom;
  }
}

bool
file_valid_extension(zathura_t* zathura, const char* path)
{
  if (zathura == NULL || path == NULL || zathura->plugins.manager == NULL) {
    return false;
  }

  char* content_type = zathura_content_type_guess(zathura->content_type_context, path, NULL);
  if (content_type == NULL) {
    return false;
  }

  zathura_plugin_t* plugin = zathura_plugin_manager_get_plugin(zathura->plugins.manager, content_type);
  g_free(content_type);

  return (plugin == NULL) ? false : true;
}

static void
index_element_free(void* data, GObject* UNUSED(object))
{
  zathura_index_element_t* element = data;
  zathura_index_element_free(element);
}

void
document_index_build(GtkTreeModel* model, GtkTreeIter* parent,
                     girara_tree_node_t* tree)
{
  girara_list_t* list = girara_node_get_children(tree);

  GIRARA_LIST_FOREACH_BODY(list, girara_tree_node_t*, node,
    zathura_index_element_t* index_element = girara_node_get_data(node);

    zathura_link_type_t type     = zathura_link_get_type(index_element->link);
    zathura_link_target_t target = zathura_link_get_target(index_element->link);

    gchar* description = NULL;
    if (type == ZATHURA_LINK_GOTO_DEST) {
      description = g_strdup_printf("Page %d", target.page_number + 1);
    } else {
      description = g_strdup(target.value);
    }

    GtkTreeIter tree_iter;
    gtk_tree_store_append(GTK_TREE_STORE(model), &tree_iter, parent);
    gchar* markup = g_markup_escape_text(index_element->title, -1);
    gtk_tree_store_set(GTK_TREE_STORE(model), &tree_iter, 0, markup, 1, description, 2, index_element, -1);
    g_free(markup);
    g_object_weak_ref(G_OBJECT(model), index_element_free, index_element);
    g_free(description);

    if (girara_node_get_num_children(node) > 0) {
      document_index_build(model, &tree_iter, node);
    }
  );
}

zathura_rectangle_t
rotate_rectangle(zathura_rectangle_t rectangle, unsigned int degree, double height, double width)
{
  zathura_rectangle_t tmp;
  switch (degree) {
    case 90:
      tmp.x1 = height - rectangle.y2;
      tmp.x2 = height - rectangle.y1;
      tmp.y1 = rectangle.x1;
      tmp.y2 = rectangle.x2;
      break;
    case 180:
      tmp.x1 = width - rectangle.x2;
      tmp.x2 = width - rectangle.x1;
      tmp.y1 = height - rectangle.y2;
      tmp.y2 = height - rectangle.y1;
      break;
    case 270:
      tmp.x1 = rectangle.y1;
      tmp.x2 = rectangle.y2;
      tmp.y1 = width - rectangle.x2;
      tmp.y2 = width - rectangle.x1;
      break;
    default:
      tmp.x1 = rectangle.x1;
      tmp.x2 = rectangle.x2;
      tmp.y1 = rectangle.y1;
      tmp.y2 = rectangle.y2;
  }

  return tmp;
}

zathura_rectangle_t
recalc_rectangle(zathura_page_t* page, zathura_rectangle_t rectangle)
{
  if (page == NULL) {
    goto error_ret;
  }

  zathura_document_t* document = zathura_page_get_document(page);

  if (document == NULL) {
    goto error_ret;
  }

  double page_height = zathura_page_get_height(page);
  double page_width  = zathura_page_get_width(page);
  double scale       = zathura_document_get_scale(document);

  zathura_rectangle_t tmp = rotate_rectangle(rectangle, zathura_document_get_rotation(document), page_height, page_width);
  tmp.x1 *= scale;
  tmp.x2 *= scale;
  tmp.y1 *= scale;
  tmp.y2 *= scale;

  return tmp;

error_ret:

  return rectangle;
}

GtkWidget*
zathura_page_get_widget(zathura_t* zathura, zathura_page_t* page)
{
  if (zathura == NULL || page == NULL || zathura->pages == NULL) {
    return NULL;
  }

  unsigned int page_number = zathura_page_get_index(page);

  return zathura->pages[page_number];
}

void
document_draw_search_results(zathura_t* zathura, bool value)
{
  if (zathura == NULL || zathura->document == NULL || zathura->pages == NULL) {
    return;
  }

  unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    g_object_set(zathura->pages[page_id], "draw-search-results", (value == true) ? TRUE : FALSE, NULL);
  }
}

char*
zathura_get_version_string(zathura_t* zathura, bool markup)
{
  if (zathura == NULL) {
    return NULL;
  }

  GString* string = g_string_new(NULL);

  /* zathura version */
  g_string_append(string, "zathura " ZATHURA_VERSION);
  g_string_append_printf(string, "\ngirara " GIRARA_VERSION " (runtime: %s)", girara_version());

  const char* format = (markup == true) ? "\n<i>(plugin)</i> %s (%d.%d.%d) <i>(%s)</i>" : "\n(plugin) %s (%d.%d.%d) (%s)";

  /* plugin information */
  girara_list_t* plugins = zathura_plugin_manager_get_plugins(zathura->plugins.manager);
  if (plugins != NULL) {
    GIRARA_LIST_FOREACH_BODY(plugins, zathura_plugin_t*, plugin,
      const char* name = zathura_plugin_get_name(plugin);
      zathura_plugin_version_t version = zathura_plugin_get_version(plugin);
      g_string_append_printf(string, format,
                             (name == NULL) ? "-" : name,
                             version.major,
                             version.minor,
                             version.rev,
                             zathura_plugin_get_path(plugin));
    );
  }

  char* version = string->str;
  g_string_free(string, FALSE);

  return version;
}

GdkAtom*
get_selection(zathura_t* zathura)
{
  g_return_val_if_fail(zathura != NULL, NULL);

  char* value = NULL;
  girara_setting_get(zathura->ui.session, "selection-clipboard", &value);
  if (value == NULL) {
    return NULL;
  }

  GdkAtom* selection = g_try_malloc(sizeof(GdkAtom));
  if (selection == NULL) {
    g_free(selection);
    return NULL;
  }

  if (strcmp(value, "primary") == 0) {
    *selection = GDK_SELECTION_PRIMARY;
  } else if (strcmp(value, "clipboard") == 0) {
    *selection = GDK_SELECTION_CLIPBOARD;
  } else {
    girara_error("Invalid value for the selection-clipboard setting");
    g_free(value);
    g_free(selection);

    return NULL;
  }

  g_free(value);

  return selection;
}

unsigned int
find_first_page_column(const char* first_page_column_list,
                       const unsigned int pages_per_row)
{
  /* sanity checks */
  unsigned int first_page_column = 1;
  g_return_val_if_fail(first_page_column_list != NULL,  first_page_column);
  g_return_val_if_fail(*first_page_column_list != '\0', first_page_column);
  g_return_val_if_fail(pages_per_row > 0,               first_page_column);

  /* split settings list */
  char** settings = g_strsplit(first_page_column_list, ":", pages_per_row + 1);
  const size_t settings_size = g_strv_length(settings);

  /* read setting value corresponding to the specified pages per row */
  unsigned int index = pages_per_row - 1;
  if (index < settings_size && *settings[index] != '\0') {
    first_page_column = atoi(settings[index]);
  } else if (*settings[settings_size - 1] != '\0') {
    first_page_column = atoi(settings[settings_size - 1]);
  }

  /* free buffers */
  g_strfreev(settings);

  return first_page_column;
}

bool
parse_color(GdkRGBA* color, const char* str)
{
  if (gdk_rgba_parse(color, str) == FALSE) {
    girara_warning("Failed to parse color string '%s'.", str);
    return false;
  }
  return true;
}

bool
running_under_wsl(void)
{
  bool result = false;
  char* content = girara_file_read("/proc/version");
  if (content != NULL && g_strstr_len(content, -1, "Microsoft")) {
    result = true;
  }
  free(content);
  return result;
}

typedef struct zathura_point_s
{
  double x;
  double y;
} zathura_point_t;

static int
cmp_point(const void* va, const void* vb) {
  const zathura_point_t* a = va;
  const zathura_point_t* b = vb;

  return a->x == b->x && a->y == b->y ? 0 : -1;
}

static int
cmp_double(const void* vx, const void* vy) {
  const double* x = vx;
  const double* y = vy;

  return *x == *y ? 0 : (*x > *y ? 1 : -1);
}

static int
cmp_rectangle(const void* vr1, const void* vr2) {
  const zathura_rectangle_t* r1 = vr1;
  const zathura_rectangle_t* r2 = vr2;

  return (r1->x1 == r2->x1 && r1->x2 == r2->x2 && r1->y1 == r2->y1 &&
          r1->y2 == r2->y2)
           ? 0
           : -1;
}


static bool
girara_list_append_unique(girara_list_t* l, girara_compare_function_t cmp, void* item) {
  if (girara_list_find(l, cmp, item) != NULL) {
    return false;
  }

  girara_list_append(l, item);
  return true;
}

static void
rectangle_to_points(void* vrect, void* vlist) {
  const zathura_rectangle_t* rect = vrect;
  girara_list_t* list = vlist;

  zathura_point_t* p1 = g_try_malloc0(sizeof(zathura_point_t));
  zathura_point_t* p2 = g_try_malloc0(sizeof(zathura_point_t));
  zathura_point_t* p3 = g_try_malloc0(sizeof(zathura_point_t));
  zathura_point_t* p4 = g_try_malloc0(sizeof(zathura_point_t));
  if (p1 == NULL || p2 == NULL || p3 == NULL || p4 == NULL) {
    g_free(p4);
    g_free(p3);
    g_free(p2);
    g_free(p1);
    return;
  }

  *p1 = (zathura_point_t) { rect->x1, rect->y1 };
  *p2 = (zathura_point_t) { rect->x1, rect->y2 };
  *p3 = (zathura_point_t) { rect->x2, rect->y1 };
  *p4 = (zathura_point_t) { rect->x2, rect->y2 };

  if (!girara_list_append_unique(list, cmp_point, p1)) {
    g_free(p1);
  }
  if (!girara_list_append_unique(list, cmp_point, p2)) {
    g_free(p2);
  }
  if (!girara_list_append_unique(list, cmp_point, p3)) {
    g_free(p3);
  }
  if (!girara_list_append_unique(list, cmp_point, p4)) {
    g_free(p4);
  }
}

// transform a rectangle into multiple new ones according a grid of points 
static void
cut_rectangle(const zathura_rectangle_t* rect, girara_list_t* points, girara_list_t* rectangles) {
  // Lists of ordred relevant points
  girara_list_t* xs = girara_sorted_list_new2(cmp_double, g_free);
  girara_list_t* ys = girara_sorted_list_new2(cmp_double, g_free);

  double* rX2 = g_try_malloc(sizeof(double));
  double* rY2 = g_try_malloc(sizeof(double));
  if (rX2 == NULL || rY2 == NULL) {

    return;
  }

  *rX2 = rect->x2;
  *rY2 = rect->y2;
  girara_list_append(xs, rX2);
  girara_list_append(ys, rY2);

  GIRARA_LIST_FOREACH(points, zathura_point_t*, i_pt, pt)
    if (pt->x > rect->x1 && pt->x < rect->x2) {
      double* v = g_try_malloc(sizeof(double));
      if (v == NULL) {
        return;
      }
      *v = pt->x;
      if (!girara_list_append_unique(xs, cmp_double, v)) {
        g_free(v);
      }
    }
    if (pt->y > rect->y1 && pt->y < rect->y2) {
      double* v = g_try_malloc(sizeof(double));
      if (v == NULL) {
        return;
      }
      *v = pt->y;
      if (!girara_list_append_unique(ys, cmp_double, v)) {
        g_free(v);
      }
    }
  GIRARA_LIST_FOREACH_END(points, zathura_point_t*, i_pt, pt);

  double x = rect->x1;
  GIRARA_LIST_FOREACH(xs, double*, ix, cx)
    double y = rect->y1;
    GIRARA_LIST_FOREACH(ys, double*, iy, cy)
      zathura_rectangle_t* r = g_try_malloc(sizeof(zathura_rectangle_t));
      *r = (zathura_rectangle_t) {x, y, *cx, *cy};
      y = *cy;
      girara_list_append_unique(rectangles, cmp_rectangle, r);
    GIRARA_LIST_FOREACH_END(ys, double*, iy, cy);
    x = *cx;
  GIRARA_LIST_FOREACH_END(xs, double*, ix, cx);

  girara_list_free(xs);
  girara_list_free(ys);
}

girara_list_t*
flatten_rectangles(girara_list_t* rectangles) {
  girara_list_t* new_rectangles = girara_list_new2(g_free);
  girara_list_t* points = girara_list_new2(g_free);
  girara_list_foreach(rectangles, rectangle_to_points, points);

  GIRARA_LIST_FOREACH(rectangles, zathura_rectangle_t*, i, r)
    cut_rectangle(r, points, new_rectangles);
  GIRARA_LIST_FOREACH_END(rectangles, zathura_rectangle_t*, i, r);
  girara_list_free(points);
  return new_rectangles;
}
