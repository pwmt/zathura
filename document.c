/* See LICENSE file for license and copyright information */

#define _BSD_SOURCE
#define _XOPEN_SOURCE 700
// TODO: Implement realpath

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <glib.h>

#include "document.h"
#include "utils.h"
#include "zathura.h"
#include "render.h"
#include "database.h"
#include "page_widget.h"

#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/statusbar.h>

/**
 * Register document plugin
 */
static bool zathura_document_plugin_register(zathura_t* zathura, zathura_document_plugin_t* new_plugin);

void
zathura_document_plugins_load(zathura_t* zathura)
{
  GIRARA_LIST_FOREACH(zathura->plugins.path, char*, iter, plugindir)
    /* TODO: rewrite with GDir */
    /* read all files in the plugin directory */
    DIR* dir = opendir(plugindir);
    if (dir == NULL) {
      girara_error("could not open plugin directory: %s", plugindir);
      girara_list_iterator_next(iter);
      continue;
    }

    int fddir = dirfd(dir);
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
      }

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

      plugin = g_malloc0(sizeof(zathura_document_plugin_t));
      plugin->content_types = girara_list_new2(g_free);
      plugin->handle = handle;

      register_plugin(plugin);

      bool r = zathura_document_plugin_register(zathura, plugin);

      if (r == false) {
        girara_error("could not register plugin %s", path);
        zathura_document_plugin_free(plugin);
      } else {
        girara_info("successfully loaded plugin %s", path);
      }

      g_free(path);
    }

    if (closedir(dir) == -1) {
      girara_error("could not close plugin directory %s", plugindir);
    }
  GIRARA_LIST_FOREACH_END(zathura->plugins.path, char*, iter, plugindir);
}

void
zathura_document_plugin_free(zathura_document_plugin_t* plugin)
{
  if (plugin == NULL) {
    return;
  }

  dlclose(plugin->handle);
  girara_list_free(plugin->content_types);
  g_free(plugin);
}

static bool
zathura_document_plugin_register(zathura_t* zathura, zathura_document_plugin_t* new_plugin)
{
  if (new_plugin == NULL || new_plugin->content_types == NULL || new_plugin->open_function == NULL) {
    girara_error("plugin: could not register\n");
    return false;
  }

  bool atleastone = false;
  GIRARA_LIST_FOREACH(new_plugin->content_types, gchar*, iter, type)
    if (!zathura_type_plugin_mapping_new(zathura, type, new_plugin)) {
      girara_error("plugin: already registered for filetype %s\n", type);
    } else {
      atleastone = true;
    }
  GIRARA_LIST_FOREACH_END(new_plugin->content_types, gchar*, iter, type);

  if (atleastone) {
    girara_list_append(zathura->plugins.plugins, new_plugin);
  }
  return atleastone;
}

static const gchar*
guess_type(const char* path)
{
  gboolean uncertain;
  const gchar* content_type = g_content_type_guess(path, NULL, 0, &uncertain);
  if (content_type == NULL) {
    return NULL;
  }

  FILE* f = fopen(path, "r");
  if (f == NULL) {
    return NULL;
  }

  const int fd = fileno(f);
  guchar* content = NULL;
  size_t length = 0u;
  while (uncertain == TRUE) {
    g_free((void*)content_type);
    content_type = NULL;

    content = g_realloc(content, length + BUFSIZ);
    const ssize_t r = read(fd, content + length, BUFSIZ);
    if (r == -1) {
      break;
    }

    length += r;
    content_type = g_content_type_guess(NULL, content, length, &uncertain);
  }

  fclose(f);
  if (uncertain == TRUE) {
    g_free((void*)content_type);
    content_type = NULL;
  }

  g_free(content);
  return content_type;
}

zathura_document_t*
zathura_document_open(zathura_t* zathura, const char* path, const char* password)
{
  if (path == NULL) {
    return NULL;
  }

  if (file_exists(path) == false) {
    girara_error("File '%s' does not exist", path);
    return NULL;
  }

  const gchar* content_type = guess_type(path);
  if (content_type == NULL) {
    girara_error("Could not determine file type.");
    return NULL;
  }

  char* file_uri = NULL;

  /* determine real path */
  long path_max;
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
  GIRARA_LIST_FOREACH_END(zathura->plugins.type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping);
  g_free((void*)content_type);

  if (plugin == NULL) {
    girara_error("unknown file type\n");
    goto error_free;
  }

  document = g_malloc0(sizeof(zathura_document_t));

  document->file_path           = real_path;
  document->password            = password;
  document->scale               = 1.0;
  document->zathura             = zathura;

  int offset = 0;
  zathura_db_get_fileinfo(zathura->database, document->file_path,
      &document->current_page_number, &offset, &document->scale, &document->rotate);
  if (document->scale <= FLT_EPSILON) {
    girara_warning("document info: '%s' has non positive scale", document->file_path);
    document->scale = 1;
  }

  if (plugin->open_function == NULL || plugin->open_function(document) != ZATHURA_PLUGIN_ERROR_OK) {
    girara_error("could not open file\n");
    goto error_free;
  }

  /* check current page number */
  if (document->current_page_number < 1 || document->current_page_number > document->number_of_pages) {
    girara_warning("document info: '%s' has an invalid page number", document->file_path);
    document->current_page_number = 1;
  }

  /* update statusbar */
  girara_statusbar_item_set_text(zathura->ui.session, zathura->ui.statusbar.file, real_path);

  /* read all pages */
  document->pages = calloc(document->number_of_pages, sizeof(zathura_page_t*));
  if (document->pages == NULL) {
    goto error_free;
  }

  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zathura_page_t* page = zathura_page_get(document, page_id, NULL);
    if (page == NULL) {
      goto error_free;
    }

    document->pages[page_id] = page;
  }

  /* install file monitor */
  file_uri = g_filename_to_uri(real_path, NULL, NULL);
  if (file_uri == NULL) {
    goto error_free;
  }

  document->file_monitor.file = g_file_new_for_uri(file_uri);
  if (document->file_monitor.file == NULL) {
    goto error_free;
  }

  document->file_monitor.monitor = g_file_monitor_file(document->file_monitor.file, G_FILE_MONITOR_NONE, NULL, NULL);
  if (document->file_monitor.monitor == NULL) {
    goto error_free;
  }

  g_signal_connect(G_OBJECT(document->file_monitor.monitor), "changed", G_CALLBACK(cb_file_monitor), zathura->ui.session);

  g_free(file_uri);

  return document;

error_free:

  if (file_uri != NULL) {
    g_free(file_uri);
  }

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

zathura_plugin_error_t
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
  bool r = true;
  if (document->functions.document_free == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
  } else {
    r = document->functions.document_free(document);
  }

  if (document->file_path != NULL) {
    free(document->file_path);
  }

  g_free(document);

  return r;
}

zathura_plugin_error_t
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
zathura_document_index_generate(zathura_document_t* document, zathura_plugin_error_t* error)
{
  if (document == NULL) {
    return NULL;
  }

  if (document->functions.document_index_generate == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_index_generate(document, error);
}

girara_list_t*
zathura_document_attachments_get(zathura_document_t* document, zathura_plugin_error_t* error)
{
  if (document == NULL) {
    return NULL;
  }

  if (document->functions.document_attachments_get == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_attachments_get(document, error);
}

zathura_plugin_error_t
zathura_document_attachment_save(zathura_document_t* document, const char* attachment, const char* file)
{
  if (document == NULL) {
    return ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
  }

  if (document->functions.document_attachment_save == NULL) {
    return ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
  }

  return document->functions.document_attachment_save(document, attachment, file);
}

char*
zathura_document_meta_get(zathura_document_t* document, zathura_document_meta_t meta, zathura_plugin_error_t* error)
{
  if (document == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  if (document->functions.document_meta_get == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return document->functions.document_meta_get(document, meta, error);
}

zathura_page_t*
zathura_page_get(zathura_document_t* document, unsigned int page_id, zathura_plugin_error_t* error)
{
  if (document == NULL) {
    return NULL;
  }

  if (document->functions.page_get == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  zathura_page_t* page = document->functions.page_get(document, page_id, error);

  if (page != NULL) {
    page->number       = page_id;
    page->visible      = false;
    page->drawing_area = zathura_page_widget_new(page);
    page->document     = document;

    unsigned int page_height = 0;
    unsigned int page_width = 0;
    page_calc_height_width(page, &page_height, &page_width, true);

    gtk_widget_set_size_request(page->drawing_area, page_width, page_height);
  }

  return page;
}

zathura_plugin_error_t
zathura_page_free(zathura_page_t* page)
{
  if (page == NULL || page->document == NULL) {
    return false;
  }

  if (page->document->functions.page_free == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return false;
  }

  return page->document->functions.page_free(page);
}

girara_list_t*
zathura_page_search_text(zathura_page_t* page, const char* text, zathura_plugin_error_t* error)
{
  if (page == NULL || page->document == NULL || text == NULL) {
    return NULL;
  }

  if (page->document->functions.page_search_text == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_search_text(page, text, error);
}

girara_list_t*
zathura_page_links_get(zathura_page_t* page, zathura_plugin_error_t* error)
{
  if (page == NULL || page->document == NULL) {
    return NULL;
  }

  if (page->document->functions.page_links_get == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
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
  if (page == NULL || page->document == NULL) {
    return NULL;
  }

  if (page->document->functions.page_form_fields_get == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return page->document->functions.page_form_fields_get(page, error);
}

zathura_plugin_error_t
zathura_page_form_fields_free(girara_list_t* UNUSED(list))
{
  return false;
}

girara_list_t*
zathura_page_images_get(zathura_page_t* page, zathura_plugin_error_t* error)
{
  if (page == NULL || page->document == NULL) {
    return NULL;
  }

  if (page->document->functions.page_images_get == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return false;
  }

  return page->document->functions.page_images_get(page, error);
}

zathura_plugin_error_t
zathura_page_image_save(zathura_page_t* page, zathura_image_t* image, const char* file)
{
  if (page == NULL || page->document == NULL || image == NULL || file == NULL) {
    return false;
  }

  if (page->document->functions.page_image_save == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return false;
  }

  return page->document->functions.page_image_save(page, image, file);
}

zathura_plugin_error_t
zathura_page_render(zathura_page_t* page, cairo_t* cairo, bool printing)
{
  if (page == NULL || page->document == NULL || cairo == NULL) {
    return ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
  }

  if (page->document->functions.page_render_cairo == NULL) {
    girara_error("%s not implemented", __FUNCTION__);
    return ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
  }

  return page->document->functions.page_render_cairo(page, cairo, printing);
}

zathura_index_element_t*
zathura_index_element_new(const char* title)
{
  if (title == NULL) {
    return NULL;
  }

  zathura_index_element_t* res = g_malloc0(sizeof(zathura_index_element_t));

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
  GIRARA_LIST_FOREACH_END(zathura->plugins.type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping);

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

void
zathura_link_free(zathura_link_t* link)
{
  if (link == NULL) {
    return;
  }

  if (link->type == ZATHURA_LINK_EXTERNAL) {
    g_free(link->target.value);
  }
  g_free(link);
}
