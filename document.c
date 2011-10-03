/* See LICENSE file for license and copyright information */

#define _BSD_SOURCE
#define _XOPEN_SOURCE 700
// TODO: Implement realpath

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "document.h"
#include "utils.h"
#include "zathura.h"
#include "render.h"
#include "utils.h"

void
zathura_document_plugins_load(zathura_t* zathura)
{
  girara_list_iterator_t* iter = girara_list_iterator(zathura->plugins.path);
  if (iter == NULL) {
    return;
  }

  do
  {
    char* plugindir = girara_list_iterator_data(iter);

    /* read all files in the plugin directory */
    DIR* dir = opendir(plugindir);
    if (dir == NULL) {
      girara_error("could not open plugin directory: %s", plugindir);
      continue;
    }

    int fddir = dirfd(dir);
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
      struct stat statbuf;
      if (fstatat(fddir, entry->d_name, &statbuf, 0) != 0) {
        girara_error("failed to fstatat %s/%s; errno is %d.", plugindir, entry->d_name, errno);
        continue;
      }

      /* check if entry is a file */
      if (S_ISREG(statbuf.st_mode) == 0) {
        girara_info("%s/%s is not a regular file. Skipping.", plugindir, entry->d_name);
        continue;
      }

      void* handle                      = NULL;
      zathura_document_plugin_t* plugin = NULL;
      char* path                        = NULL;

      /* get full path */
      path = g_build_filename(plugindir, entry->d_name, NULL);
      if (path == NULL) {
        g_error("failed to allocate memory!");
        break;
      }

      /* load plugin */
      handle = dlopen(path, RTLD_NOW);
      if (handle == NULL) {
        girara_error("could not load plugin %s (%s)", path, dlerror());
        g_free(path);
        continue;
      }

      /* resolve symbol */
      zathura_plugin_register_service_t register_plugin;
      *(void**)(&register_plugin) = dlsym(handle, PLUGIN_REGISTER_FUNCTION);

      if (register_plugin == NULL) {
        girara_error("could not find '%s' function in plugin %s", PLUGIN_REGISTER_FUNCTION, path);
        g_free(path);
        dlclose(handle);
        continue;
      }

      plugin = malloc(sizeof(zathura_document_plugin_t));

      if (plugin == NULL) {
        g_error("failed to allocate memory!");
        break;
      }

      plugin->open_function  = NULL;
      plugin->content_types = girara_list_new();
      girara_list_set_free_function(plugin->content_types, g_free);

      register_plugin(plugin);

      bool r = zathura_document_plugin_register(zathura, plugin, handle);

      if (r == false) {
        girara_error("could not register plugin %s", path);
        free(plugin);
        dlclose(handle);
      }
      else  {
        girara_info("successfully loaded plugin %s", path);
      }

      g_free(path);
    }

    if (closedir(dir) == -1) {
      girara_error("could not close plugin directory %s", plugindir);
    }
  } while (girara_list_iterator_next(iter));
  girara_list_iterator_free(iter);
}

void
zathura_document_plugin_free(zathura_document_plugin_t* plugin)
{
  if (plugin == NULL) {
    return;
  }

  girara_list_free(plugin->content_types);
  free(plugin);
}

bool
zathura_document_plugin_register(zathura_t* zathura, zathura_document_plugin_t* new_plugin, void* handle)
{
  if( (new_plugin == NULL) || (new_plugin->content_types == NULL) || (new_plugin->open_function == NULL)
      || (handle == NULL) ) {
    girara_error("plugin: could not register\n");
    return false;
  }

  bool atleastone = false;
  GIRARA_LIST_FOREACH(new_plugin->content_types, gchar*, iter, type)
    if (!zathura_type_plugin_mapping_new(zathura, type, new_plugin)) {
      girara_error("plugin: already registered for filetype %s\n", type);
    }
    atleastone = true;
  GIRARA_LIST_FOREACH_END(new_plugin->content_types, gchar*, iter, type)

  if (atleastone) {
    girara_list_append(zathura->plugins.plugins, new_plugin);
  }
  return atleastone;
}

zathura_document_t*
zathura_document_open(zathura_t* zathura, const char* path, const char* password)
{
  if (path == NULL) {
    return NULL;
  }

  if (file_exists(path) == false) {
    girara_error("File does not exist");
    return NULL;
  }

  const gchar* content_type = g_content_type_guess(path, NULL, 0, NULL);
  if (content_type == NULL) {
    girara_error("Could not determine file type");
    return NULL;
  }

  /* determine real path */
  size_t path_max;
#ifdef PATH_MAX
  path_max = PATH_MAX;
#else
  path_max = pathconf(path,_PC_PATH_MAX);
  if (path_max <= 0)
    path_max = 4096;
#endif

  char* real_path              = NULL;
  zathura_document_t* document = NULL;

  real_path = malloc(sizeof(char) * path_max);
  if (real_path == NULL) {
    g_free((void*)content_type);
    return NULL;
  }

  if (realpath(path, real_path) == NULL) {
    g_free((void*)content_type);
    free(real_path);
    return NULL;
  }

  zathura_document_plugin_t* plugin = NULL;
  GIRARA_LIST_FOREACH(zathura->plugins.type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping)
    if (g_content_type_equals(content_type, mapping->type)) {
      plugin = mapping->plugin;
      break;
    }
  GIRARA_LIST_FOREACH_END(zathura->plugins.type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping)
  g_free((void*)content_type);

  if (plugin == NULL) {
    girara_error("unknown file type\n");
    goto error_free;
  }

  document = g_malloc0(sizeof(zathura_document_t));
  if (document == NULL) {
    goto error_free;
  }

  document->file_path           = real_path;
  document->password            = password;
  document->scale               = 1.0;
  document->zathura             = zathura;

  if (plugin->open_function != NULL) {
    if (plugin->open_function(document) == true) {
      /* update statusbar */
      girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, real_path);

      /* read all pages */
      document->pages = calloc(document->number_of_pages, sizeof(zathura_page_t*));
      if (document->pages == NULL) {
        goto error_free;
      }

      for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
        zathura_page_t* page = zathura_page_get(document, page_id);
        if (page == NULL) {
          goto error_free;
        }

        document->pages[page_id] = page;
      }

      return document;
    }
  }

  girara_error("could not open file\n");

error_free:

  free(real_path);

  if (document != NULL && document->pages != NULL) {
    for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
      zathura_page_free(document->pages[page_id]);
    }

    free(document->pages);
  }

  g_free(document);
  return NULL;
}

bool
zathura_document_free(zathura_document_t* document)
{
  if (document == NULL) {
    return false;
  }

  /* free pages */
  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zathura_page_free(document->pages[page_id]);
    document->pages[page_id] = NULL;
  }

  free(document->pages);

  /* free document */
  if (document->functions.document_free == NULL) {
    girara_error("%s not implemented", __FUNCTION__);

    if (document->file_path != NULL) {
      free(document->file_path);
    }

    g_free(document);
    return true;
  }

  bool r = document->functions.document_free(document);

  if (document->file_path != NULL) {
    free(document->file_path);
  }

  g_free(document);

  return r;
}

bool
zathura_document_save_as(zathura_document_t* document, const char* path)
{
  if (document == NULL || path == NULL) {
    return false;
  }

  if (document->functions.document_save_as == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return false;
  }

  return document->functions.document_save_as(document, path);
}

girara_tree_node_t*
zathura_document_index_generate(zathura_document_t* document)
{
  if (document == NULL) {
    return NULL;
  }

  if (document->functions.document_index_generate == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_index_generate(document);
}

zathura_list_t*
zathura_document_attachments_get(zathura_document_t* document)
{
  if (document == NULL) {
    return NULL;
  }

  if (document->functions.document_attachments_get == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_attachments_get(document);
}

bool
zathura_document_attachments_free(zathura_list_t* UNUSED(list))
{
  return false;
}

char*
zathura_document_meta_get(zathura_document_t* document, zathura_document_meta_t meta)
{
  if (document == NULL) {
    return NULL;
  }

  if (document->functions.document_meta_get == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_meta_get(document, meta);
}

zathura_page_t*
zathura_page_get(zathura_document_t* document, unsigned int page_id)
{
  if (document == NULL) {
    return NULL;
  }

  if (document->functions.page_get == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  zathura_page_t* page = document->functions.page_get(document, page_id);

  if (page != NULL) {
    page->number       = page_id;
    page->visible      = false;
    page->event_box    = gtk_event_box_new();
    page->drawing_area = gtk_drawing_area_new();
    page->surface      = NULL;
    page->document     = document;
    g_signal_connect(page->drawing_area, "expose-event", G_CALLBACK(page_expose_event), page);

    gtk_widget_set_size_request(page->drawing_area, page->width * document->scale, page->height * document->scale);
    gtk_container_add(GTK_CONTAINER(page->event_box), page->drawing_area);

    g_static_mutex_init(&(page->lock));
  }

  return page;
}

bool
zathura_page_free(zathura_page_t* page)
{
  if (page == NULL || page->document == NULL) {
    return false;
  }

  if (page->document->functions.page_free == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return false;
  }

  g_static_mutex_free(&(page->lock));

  return page->document->functions.page_free(page);
}

zathura_list_t*
zathura_page_search_text(zathura_page_t* page, const char* text)
{
  if (page == NULL || page->document == NULL || text == NULL) {
    return NULL;
  }

  if (page->document->functions.page_search_text == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_search_text(page, text);
}

zathura_list_t*
zathura_page_links_get(zathura_page_t* page)
{
  if (page == NULL || page->document == NULL) {
    return NULL;
  }

  if (page->document->functions.page_links_get == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_links_get(page);
}

bool
zathura_page_links_free(zathura_list_t* UNUSED(list))
{
  return false;
}

zathura_list_t*
zathura_page_form_fields_get(zathura_page_t* page)
{
  if (page == NULL || page->document == NULL) {
    return NULL;
  }

  if (page->document->functions.page_form_fields_get == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_form_fields_get(page);
}

bool
zathura_page_form_fields_free(zathura_list_t* UNUSED(list))
{
  return false;
}

bool
zathura_page_render(zathura_page_t* page, cairo_t* cairo)
{
  if (page == NULL || page->document == NULL || cairo == NULL) {
    return NULL;
  }

  if (page->document->functions.page_render_cairo == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_render_cairo(page, cairo);
}

zathura_index_element_t*
zathura_index_element_new(const char* title)
{
  if (title == NULL) {
    return NULL;
  }

  zathura_index_element_t* res = g_malloc0(sizeof(zathura_index_element_t));

  if (res == NULL) {
    return NULL;
  }

  res->title = g_strdup(title);

  return res;
}

void
zathura_index_element_free(zathura_index_element_t* index)
{
  if (index == NULL) {
    return;
  }

  g_free(index->title);

  if (index->type == ZATHURA_LINK_EXTERNAL) {
    g_free(index->target.uri);
  }

  g_free(index);
}

zathura_image_buffer_t*
zathura_image_buffer_create(unsigned int width, unsigned int height)
{
  zathura_image_buffer_t* image_buffer = malloc(sizeof(zathura_image_buffer_t));

  if (image_buffer == NULL) {
    return NULL;
  }

  image_buffer->data = calloc(width * height * 3, sizeof(unsigned char));

  if (image_buffer->data == NULL) {
    free(image_buffer);
    return NULL;
  }

  image_buffer->width     = width;
  image_buffer->height    = height;
  image_buffer->rowstride = width * 3;

  return image_buffer;
}

void
zathura_image_buffer_free(zathura_image_buffer_t* image_buffer)
{
  if (image_buffer == NULL) {
    return;
  }

  free(image_buffer->data);
  free(image_buffer);
}

bool
zathura_type_plugin_mapping_new(zathura_t* zathura, const gchar* type, zathura_document_plugin_t* plugin)
{
  g_return_val_if_fail(zathura && type && plugin, false);

  GIRARA_LIST_FOREACH(zathura->plugins.type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping)
    if (g_content_type_equals(type, mapping->type)) {
      girara_list_iterator_free(iter);
      return false;
    }
  GIRARA_LIST_FOREACH_END(zathura->plugins.type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping)

  zathura_type_plugin_mapping_t* mapping = g_malloc(sizeof(zathura_type_plugin_mapping_t));
  mapping->type = g_strdup(type);
  mapping->plugin = plugin;
  girara_list_append(zathura->plugins.type_plugin_mapping, mapping);
  return true;
}

void
zathura_type_plugin_mapping_free(zathura_type_plugin_mapping_t* mapping)
{
  if (mapping == NULL) {
    return;
  }

  g_free((void*)mapping->type);
  g_free(mapping);
}
