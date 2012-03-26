/* See LICENSE file for license and copyright information */

#include <girara/session.h>
#include <girara/utils.h>
#include <glib/gi18n.h>

#include "document.h"
#include "page.h"
#include "page-widget.h"
#include "utils.h"

struct zathura_page_s {
  double height; /**< Page height */
  double width; /**< Page width */
  unsigned int id; /**< Page number */
  void* data; /**< Custom data */
  bool visible; /**< Page is visible */
  GtkWidget* widget; /**< Drawing area */
  zathura_document_t* document; /**< Document */
};

zathura_page_t*
zathura_page_get(zathura_document_t* document, unsigned int page_id, zathura_plugin_error_t* error)
{
  if (document == NULL || document->zathura == NULL || document->zathura->ui.session == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
    }
    goto error_ret;
  }

  zathura_page_t* page = NULL;

  if (document->functions.page_get == NULL) {
    if (error != NULL) {
      girara_notify(page->document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
      girara_error("%s not implemented", __FUNCTION__);
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    goto error_ret;
  }

  page = document->functions.page_get(document, page_id, error);

  if (page != NULL) {
    page->id       = page_id;
    page->visible  = false;
    page->document = document;

    page->widget = zathura_page_widget_new(page);
    if (page->widget == NULL) {
      if (error != NULL) {
        *error = ZATHURA_PLUGIN_ERROR_UNKNOWN;
      }
      goto error_free;
    }

    unsigned int page_height = 0;
    unsigned int page_width  = 0;
    page_calc_height_width(page, &page_height, &page_width, true);

    gtk_widget_set_size_request(page->widget, page_width, page_height);
  }

  return page;

error_free:

  if (page != NULL) {
    zathura_page_free(page);
  }

error_ret:

  return NULL;
}

zathura_plugin_error_t
zathura_page_free(zathura_page_t* page)
{
  if (page == NULL || page->document == NULL || page->document->zathura == NULL || page->document->zathura->ui.session == NULL) {
    return ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
  }

  if (page->document->functions.page_free == NULL) {
    girara_notify(page->document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    return ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
  }

  return page->document->functions.page_free(page);
}

zathura_document_t*
zathura_page_get_document(zathura_page_t* page)
{
  if (page == NULL) {
    return NULL;
  }

  return page->document;
}

unsigned int
zathura_page_get_id(zathura_page_t* page)
{
  if (page == NULL) {
    return 0;
  }

  return page->id;
}

GtkWidget*
zathura_page_get_widget(zathura_page_t* page)
{
  if (page == NULL) {
    return NULL;
  }

  return page->widget;
}

double
zathura_page_get_width(zathura_page_t* page)
{
  if (page == NULL) {
    return -1;
  }

  return page->width;
}

void
zathura_page_set_width(zathura_page_t* page, double width)
{
  if (page == NULL) {
    return;
  }

  page->width = width;
}

double
zathura_page_get_height(zathura_page_t* page)
{
  if (page == NULL) {
    return -1;
  }

  return page->height;
}

void
zathura_page_set_height(zathura_page_t* page, double height)
{
  if (page == NULL) {
    return;
  }

  page->height = height;
}

bool
zathura_page_get_visibility(zathura_page_t* page)
{
  if (page == NULL) {
    return false;
  }

  return page->visible;
}

void
zathura_page_set_visibility(zathura_page_t* page, bool visibility)
{
  if (page == NULL) {
    return;
  }

  page->visible = visibility;
}

void*
zathura_page_get_data(zathura_page_t* page)
{
  if (page == NULL) {
    return NULL;
  }

  return page->data;
}

void
zathura_page_set_data(zathura_page_t* page, void* data)
{
  if (page == NULL) {
    return;
  }

  page->data = data;
}

girara_list_t*
zathura_page_search_text(zathura_page_t* page, const char* text, zathura_plugin_error_t* error)
{
  if (page == NULL || page->document == NULL || text == NULL ||
      page->document->zathura == NULL || page->document->zathura->ui.session == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  if (page->document->functions.page_search_text == NULL) {
    girara_notify(page->document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return page->document->functions.page_search_text(page, text, error);
}

girara_list_t*
zathura_page_links_get(zathura_page_t* page, zathura_plugin_error_t* error)
{
  if (page == NULL || page->document == NULL || page->document->zathura == NULL
      || page->document->zathura->ui.session == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  if (page->document->functions.page_links_get == NULL) {
    girara_notify(page->document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return page->document->functions.page_links_get(page, error);
}

zathura_plugin_error_t
zathura_page_links_free(girara_list_t* UNUSED(list))
{
  return false;
}

girara_list_t*
zathura_page_form_fields_get(zathura_page_t* page, zathura_plugin_error_t* error)
{
  if (page == NULL || page->document == NULL || page->document->zathura == NULL
      || page->document->zathura->ui.session == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  if (page->document->functions.page_form_fields_get == NULL) {
    girara_notify(page->document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return page->document->functions.page_form_fields_get(page, error);
}

zathura_plugin_error_t
zathura_page_form_fields_free(girara_list_t* UNUSED(list))
{
  return ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
}

girara_list_t*
zathura_page_images_get(zathura_page_t* page, zathura_plugin_error_t* error)
{
  if (page == NULL || page->document == NULL || page->document->zathura == NULL
      || page->document->zathura->ui.session == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  if (page->document->functions.page_images_get == NULL) {
    girara_notify(page->document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return page->document->functions.page_images_get(page, error);
}

cairo_surface_t*
zathura_page_image_get_cairo(zathura_page_t* page, zathura_image_t* image, zathura_plugin_error_t* error)
{
  if (page == NULL || page->document == NULL || image == NULL ||
      page->document->zathura == NULL || page->document->zathura->ui.session ==
      NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  if (page->document->functions.page_image_get_cairo == NULL) {
    girara_notify(page->document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return page->document->functions.page_image_get_cairo(page, image, error);
}

char* zathura_page_get_text(zathura_page_t* page, zathura_rectangle_t rectangle, zathura_plugin_error_t* error)
{
  if (page == NULL || page->document == NULL || page->document->zathura == NULL || page->document->zathura->ui.session == NULL) {
    if (error) {
      *error = ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  if (page->document->functions.page_get_text == NULL) {
    girara_notify(page->document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    if (error) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return page->document->functions.page_get_text(page, rectangle, error);
}

zathura_plugin_error_t
zathura_page_render(zathura_page_t* page, cairo_t* cairo, bool printing)
{
  if (page == NULL || page->document == NULL || cairo == NULL ||
      page->document->zathura == NULL || page->document->zathura->ui.session ==
      NULL) {
    return ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
  }

  if (page->document->functions.page_render_cairo == NULL) {
    girara_notify(page->document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    return ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
  }

  return page->document->functions.page_render_cairo(page, cairo, printing);
}
