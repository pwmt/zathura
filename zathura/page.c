/* SPDX-License-Identifier: Zlib */

#include "page.h"

#include <math.h>
#include <girara-gtk/session.h>
#include <girara/utils.h>
#include <glib/gi18n.h>

#include "document.h"
#include "plugin.h"
#include "utils.h"
#include "internal.h"
#include "types.h"

struct zathura_page_s {
  zathura_document_t* document; /**< Parent document */
  void* data;                   /**< Custom data */
  char* label;                  /**< Page label */
  double height;                /**< Page height */
  double width;                 /**< Page width */
  double zoom;                  /**< Page zoom */
  unsigned int index;           /**< Page number */
  bool visible;                 /**< Page is visible */
  bool label_is_number;         /**< Page label is the same as the page number */
  gint loaded;                  /**< Page has been parsed by the plugin, atomic */
};

zathura_page_t* zathura_page_new(zathura_document_t* document, unsigned int index, zathura_error_t* error) {
  if (document == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  /* init page */
  g_autoptr(zathura_page_t) page = g_try_malloc0(sizeof(zathura_page_t));
  if (page == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  page->index           = index;
  page->visible         = false;
  page->document        = document;
  page->label_is_number = false;
  page->zoom            = 1.0;
  page->loaded          = 0;

  /* the page is parsed later when it is used, not here */
  return g_steal_pointer(&page);
}

bool zathura_page_load(zathura_page_t* page, zathura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return false;
  }

  if (g_atomic_int_get(&page->loaded) == 1) {
    return true;
  }

  zathura_document_lock(page->document);

  /* another thread may have parsed the page while waiting for the lock */
  if (g_atomic_int_get(&page->loaded) == 1) {
    zathura_document_unlock(page->document);
    return true;
  }

  /* init plugin */
  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);

  zathura_error_t ret = functions->page_init(page);
  if (ret != ZATHURA_ERROR_OK) {
    girara_error("Failed to initialize page %u: %d", page->index + 1, ret);
    zathura_check_set_error(error, ret);
    zathura_document_unlock(page->document);
    return false;
  }

  /* get the label from the plugin */
  if (functions->page_get_label != NULL) {
    ret = functions->page_get_label(page, page->data, &page->label);
    if (ret != ZATHURA_ERROR_OK) {
      girara_info("Failed to get label of page %u: %d", page->index + 1, ret);
      zathura_check_set_error(error, ret);
      zathura_document_unlock(page->document);
      return false;
    }

    if (page->label != NULL) {
      char page_number_string[G_ASCII_DTOSTR_BUF_SIZE];
      g_ascii_dtostr(page_number_string, G_ASCII_DTOSTR_BUF_SIZE, page->index + 1);
      page->label_is_number = g_strcmp0(page->label, page_number_string) == 0;
    }
  }

  g_atomic_int_set(&page->loaded, 1);
  zathura_document_unlock(page->document);
  return true;
}

bool zathura_page_is_loaded(zathura_page_t* page) {
  return page != NULL && g_atomic_int_get(&page->loaded) == 1;
}

zathura_error_t zathura_page_free(zathura_page_t* page) {
  if (page == NULL) {
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }

  if (page->document == NULL) {
    g_free(page);
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }

  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);

  /* the plugin only has data to clear when the page has been parsed */
  zathura_error_t error = ZATHURA_ERROR_OK;
  if (g_atomic_int_get(&page->loaded) == 1) {
    error = functions->page_clear(page, page->data);
  }

  g_free(page->label);
  g_free(page);

  return error;
}

zathura_document_t* zathura_page_get_document(zathura_page_t* page) {
  if (page == NULL) {
    return NULL;
  }

  return page->document;
}

unsigned int zathura_page_get_index(zathura_page_t* page) {
  if (page == NULL) {
    return 0;
  }

  return page->index;
}

double zathura_page_get_width(zathura_page_t* page) {
  if (page == NULL) {
    return -1;
  }

  zathura_page_load(page, NULL);

  return page->width;
}

void zathura_page_set_width(zathura_page_t* page, double width) {
  if (page == NULL) {
    return;
  }

  if (!isfinite(width) || width < DBL_EPSILON) {
    girara_warning("Invalid page width: %f, falling back to default", width);
    return;
  }

  page->width = width;
}

double zathura_page_get_height(zathura_page_t* page) {
  if (page == NULL) {
    return -1;
  }

  zathura_page_load(page, NULL);

  return page->height;
}

void zathura_page_set_height(zathura_page_t* page, double height) {
  if (page == NULL) {
    return;
  }

  if (!isfinite(height) || height < DBL_EPSILON) {
    girara_warning("Invalid page height: %f, falling back to default", height);
    return;
  }

  page->height = height;
}

double zathura_page_get_zoom(zathura_page_t* page) {
  if (page == NULL) {
    return -1;
  }

  return page->zoom;
}

void zathura_page_set_zoom(zathura_page_t* page, double zoom) {
  if (page == NULL) {
    return;
  }

  page->zoom = zoom;
}

bool zathura_page_get_visibility(zathura_page_t* page) {
  if (page == NULL) {
    return false;
  }

  return page->visible;
}

void zathura_page_set_visibility(zathura_page_t* page, bool visibility) {
  if (page == NULL) {
    return;
  }

  page->visible = visibility;
}

void* zathura_page_get_data(zathura_page_t* page) {
  if (page == NULL) {
    return NULL;
  }

  return page->data;
}

void zathura_page_set_data(zathura_page_t* page, void* data) {
  if (page == NULL) {
    return;
  }

  page->data = data;
}

girara_list_t* zathura_page_search_text(zathura_page_t* page, const char* text, zathura_error_t* error) {
  if (page == NULL || page->document == NULL || text == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions->page_search_text == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  if (zathura_page_load(page, error) == false) {
    return NULL;
  }

  return functions->page_search_text(page, page->data, text, error);
}

girara_list_t* zathura_page_links_get(zathura_page_t* page, zathura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions->page_links_get == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  if (zathura_page_load(page, error) == false) {
    return NULL;
  }

  return functions->page_links_get(page, page->data, error);
}

zathura_error_t zathura_page_links_free(girara_list_t* UNUSED(list)) {
  return false;
}

girara_list_t* zathura_page_form_fields_get(zathura_page_t* page, zathura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions->page_form_fields_get == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  return functions->page_form_fields_get(page, page->data, error);
}

zathura_error_t zathura_page_form_fields_free(girara_list_t* UNUSED(list)) {
  return ZATHURA_ERROR_NOT_IMPLEMENTED;
}

girara_list_t* zathura_page_images_get(zathura_page_t* page, zathura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions->page_images_get == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  if (zathura_page_load(page, error) == false) {
    return NULL;
  }

  return functions->page_images_get(page, page->data, error);
}

cairo_surface_t* zathura_page_image_get_cairo(zathura_page_t* page, zathura_image_t* image, zathura_error_t* error) {
  if (page == NULL || page->document == NULL || image == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions->page_image_get_cairo == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  return functions->page_image_get_cairo(page, page->data, image, error);
}

char* zathura_page_get_text(zathura_page_t* page, zathura_rectangle_t rectangle, zathura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions->page_get_text == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  if (zathura_page_load(page, error) == false) {
    return NULL;
  }

  return functions->page_get_text(page, page->data, rectangle, error);
}

girara_list_t* zathura_page_get_selection(zathura_page_t* page, zathura_rectangle_t rectangle, zathura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions->page_get_selection == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  if (zathura_page_load(page, error) == false) {
    return NULL;
  }

  return functions->page_get_selection(page, page->data, rectangle, error);
}

zathura_error_t zathura_page_render(zathura_page_t* page, cairo_t* cairo, bool printing) {
  if (page == NULL || page->document == NULL || cairo == NULL) {
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }

  zathura_error_t error = ZATHURA_ERROR_OK;
  if (zathura_page_load(page, &error) == false) {
    return error;
  }

  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);

  return functions->page_render_cairo(page, page->data, cairo, printing);
}

const char* zathura_page_get_label(zathura_page_t* page, zathura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  if (zathura_page_load(page, error) == false) {
    return NULL;
  }

  return page->label;
}

bool zathura_page_label_is_number(zathura_page_t* page) {
  if (page == NULL) {
    return false;
  }

  return page->label_is_number;
}

girara_list_t* zathura_page_get_signatures(zathura_page_t* page, zathura_error_t* error) {
  if (page == NULL || page->document == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_INVALID_ARGUMENTS);
    return NULL;
  }

  const zathura_plugin_t* plugin              = zathura_document_get_plugin(page->document);
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions->page_get_signatures == NULL) {
    zathura_check_set_error(error, ZATHURA_ERROR_NOT_IMPLEMENTED);
    return NULL;
  }

  zathura_error_t e = ZATHURA_ERROR_OK;
  if (zathura_page_load(page, error) == false) {
    return NULL;
  }

  g_autoptr(girara_list_t) ret = functions->page_get_signatures(page, page->data, &e);
  if (e != ZATHURA_ERROR_OK) {
    zathura_check_set_error(error, e);
    return NULL;
  }

  return g_steal_pointer(&ret);
}
