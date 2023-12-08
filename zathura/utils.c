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

  return plugin == NULL;
}

static void
index_element_free(void* data, GObject* UNUSED(object))
{
  zathura_index_element_t* element = data;
  zathura_index_element_free(element);
}

void document_index_build(GtkTreeModel* model, GtkTreeIter* parent, girara_tree_node_t* tree) {
  girara_list_t* list = girara_node_get_children(tree);

  for (size_t idx = 0; idx != girara_list_size(list); ++idx) {
    girara_tree_node_t* node               = girara_list_nth(list, idx);
    zathura_index_element_t* index_element = girara_node_get_data(node);
    zathura_link_type_t type               = zathura_link_get_type(index_element->link);
    zathura_link_target_t target           = zathura_link_get_target(index_element->link);

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
  }
}

zathura_rectangle_t rotate_rectangle(zathura_rectangle_t rectangle, unsigned int degree, double height, double width) {
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
    return rectangle;
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

char* zathura_get_version_string(zathura_t* zathura, bool markup) {
  if (zathura == NULL) {
    return NULL;
  }

  GString* string = g_string_new(NULL);

  /* zathura version */
  g_string_append(string, "zathura " ZATHURA_VERSION);
  g_string_append_printf(string, "\ngirara " GIRARA_VERSION " (runtime: %s)", girara_version());

  const char* format =
      (markup == true) ? "\n<i>(plugin)</i> %s (%d.%d.%d) <i>(%s)</i>" : "\n(plugin) %s (%d.%d.%d) (%s)";

  /* plugin information */
  girara_list_t* plugins = zathura_plugin_manager_get_plugins(zathura->plugins.manager);
  if (plugins != NULL) {
    for (size_t idx = 0; idx != girara_list_size(plugins); ++idx) {
      zathura_plugin_t* plugin         = girara_list_nth(plugins, idx);
      const char* name                 = zathura_plugin_get_name(plugin);
      zathura_plugin_version_t version = zathura_plugin_get_version(plugin);
      g_string_append_printf(string, format, (name == NULL) ? "-" : name, version.major, version.minor, version.rev,
                             zathura_plugin_get_path(plugin));
    }
  }

  return g_string_free(string, FALSE);
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

typedef struct zathura_point_s {
  uintptr_t x;
  uintptr_t y;
} zathura_point_t;

static int cmp_point(const void* va, const void* vb) {
  const zathura_point_t* a = va;
  const zathura_point_t* b = vb;

  if (a->x == b->x) {
    if (a->y == b->y) {
      return 0;
    }

    return a->y < b->y ? -1 : 1;
  }

  return a->x < b->x ? -1 : 1;
}

static uintptr_t ufloor(double f) {
  return floor(f);
}

static uintptr_t uceil(double f) {
  return ceil(f);
}

static int cmp_uint(const void* vx, const void* vy) {
  const uintptr_t x = (uintptr_t)vx;
  const uintptr_t y = (uintptr_t)vy;

  return x == y ? 0 : (x > y ? 1 : -1);
}

static int cmp_rectangle(const void* vr1, const void* vr2) {
  const zathura_rectangle_t* r1 = vr1;
  const zathura_rectangle_t* r2 = vr2;

  return (ufloor(r1->x1) == ufloor(r2->x1) && uceil(r1->x2) == uceil(r2->x2) && ufloor(r1->y1) == ufloor(r2->y1) &&
          uceil(r1->y2) == uceil(r2->y2))
             ? 0
             : -1;
}

static bool girara_list_append_unique(girara_list_t* l, girara_compare_function_t cmp, void* item) {
  if (girara_list_find(l, cmp, item) != NULL) {
    return false;
  }

  girara_list_append(l, item);
  return true;
}

static void append_unique_point(girara_list_t* list, const uintptr_t x, const uintptr_t y) {
  zathura_point_t* p = g_try_malloc(sizeof(zathura_point_t));
  if (p == NULL) {
    return;
  }

  p->x = x;
  p->y = y;

  if (girara_list_append_unique(list, cmp_point, p) == false) {
    g_free(p);
  }
}

static void rectangle_to_points(void* vrect, void* vlist) {
  const zathura_rectangle_t* rect = vrect;
  girara_list_t* list             = vlist;

  append_unique_point(list, ufloor(rect->x1), ufloor(rect->y1));
  append_unique_point(list, ufloor(rect->x1), uceil(rect->y2));
  append_unique_point(list, uceil(rect->x2), ufloor(rect->y1));
  append_unique_point(list, uceil(rect->x2), uceil(rect->y2));
}

static void append_unique_uint(girara_list_t* list, const uintptr_t v) {
  girara_list_append_unique(list, cmp_uint, (void*)v);
}

// transform a rectangle into multiple new ones according a grid of points
static void cut_rectangle(const zathura_rectangle_t* rect, girara_list_t* points, girara_list_t* rectangles) {
  // Lists of ordred relevant points
  girara_list_t* xs = girara_sorted_list_new(cmp_uint);
  girara_list_t* ys = girara_sorted_list_new(cmp_uint);

  append_unique_uint(xs, uceil(rect->x2));
  append_unique_uint(ys, uceil(rect->y2));

  for (size_t idx = 0; idx != girara_list_size(points); ++idx) {
    const zathura_point_t* pt = girara_list_nth(points, idx);
    if (pt->x > ufloor(rect->x1) && pt->x < uceil(rect->x2)) {
      append_unique_uint(xs, pt->x);
    }
    if (pt->y > ufloor(rect->y1) && pt->y < uceil(rect->y2)) {
      append_unique_uint(ys, pt->y);
    }
  }

  double x = ufloor(rect->x1);
  for (size_t idx = 0; idx != girara_list_size(xs); ++idx) {
    const uintptr_t cx = (uintptr_t)girara_list_nth(xs, idx);
    double y           = ufloor(rect->y1);
    for (size_t inner_idx = 0; inner_idx != girara_list_size(ys); ++inner_idx) {
      const uintptr_t cy     = (uintptr_t)girara_list_nth(ys, inner_idx);
      zathura_rectangle_t* r = g_try_malloc(sizeof(zathura_rectangle_t));

      *r = (zathura_rectangle_t){x, y, cx, cy};
      y  = cy;
      girara_list_append_unique(rectangles, cmp_rectangle, r);
    }
    x = cx;
  }

  girara_list_free(xs);
  girara_list_free(ys);
}

girara_list_t* flatten_rectangles(girara_list_t* rectangles) {
  girara_list_t* new_rectangles = girara_list_new2(g_free);
  girara_list_t* points         = girara_list_new2(g_free);
  girara_list_foreach(rectangles, rectangle_to_points, points);

  for (size_t idx = 0; idx != girara_list_size(rectangles); ++idx) {
    const zathura_rectangle_t* r = girara_list_nth(rectangles, idx);
    cut_rectangle(r, points, new_rectangles);
  }
  girara_list_free(points);
  return new_rectangles;
}
