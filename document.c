/* See LICENSE file for license and copyright information */

#define _BSD_SOURCE
#define _XOPEN_SOURCE 700
// TODO: Implement realpath

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <dlfcn.h>
#include <errno.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "document.h"
#include "utils.h"
#include "zathura.h"
#include "render.h"
#include "database.h"
#include "page.h"
#include "page-widget.h"

#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/statusbar.h>
#include <girara/session.h>
#include <girara/settings.h>

/** Read a most GT_MAX_READ bytes before falling back to file. */
static const size_t GT_MAX_READ = 1 << 16;

/**
 * Register document plugin
 */
static bool zathura_document_plugin_register(zathura_t* zathura, zathura_document_plugin_t* new_plugin);

void
zathura_document_plugins_load(zathura_t* zathura)
{
  GIRARA_LIST_FOREACH(zathura->plugins.path, char*, iter, plugindir)
    /* read all files in the plugin directory */
    GDir* dir = g_dir_open(plugindir, 0, NULL);
    if (dir == NULL) {
      girara_error("could not open plugin directory: %s", plugindir);
      girara_list_iterator_next(iter);
      continue;
    }

    char* name = NULL;
    while ((name = (char*) g_dir_read_name(dir)) != NULL) {
      char* path           = g_build_filename(plugindir, name, NULL);
      if (g_file_test(path, G_FILE_TEST_IS_REGULAR) == 0) {
        girara_info("%s is not a regular file. Skipping.", path);
        g_free(path);
        continue;
      }

      void* handle                      = NULL;
      zathura_document_plugin_t* plugin = NULL;

      /* load plugin */
      handle = dlopen(path, RTLD_NOW);
      if (handle == NULL) {
        girara_error("could not load plugin %s (%s)", path, dlerror());
        g_free(path);
        continue;
      }

      /* resolve symbols and check API and ABI version*/
      zathura_plugin_api_version_t api_version;
      *(void**)(&api_version) = dlsym(handle, PLUGIN_API_VERSION_FUNCTION);
      if (api_version != NULL) {
        if (api_version() != ZATHURA_API_VERSION) {
          girara_error("plugin %s has been built againt zathura with a different API version (plugin: %d, zathura: %d)",
              path, api_version(), ZATHURA_API_VERSION);
          g_free(path);
          dlclose(handle);
          continue;
        }
      } else {
#if ZATHURA_API_VERSION == 1
        girara_warning("could not find '%s' function in plugin %s ... loading anyway", PLUGIN_API_VERSION_FUNCTION, path);
#else
        girara_error("could not find '%s' function in plugin %s", PLUGIN_API_VERSION_FUNCTION, path);
        g_free(path);
        dlclose(handle);
        continue;
#endif
      }

      zathura_plugin_abi_version_t abi_version;
      *(void**)(&abi_version) = dlsym(handle, PLUGIN_ABI_VERSION_FUNCTION);
      if (abi_version != NULL) {
        if (abi_version() != ZATHURA_ABI_VERSION) {
          girara_error("plugin %s has been built againt zathura with a different ABI version (plugin: %d, zathura: %d)",
              path, abi_version(), ZATHURA_ABI_VERSION);
          g_free(path);
          dlclose(handle);
          continue;
        }
      } else {
#if ZATHURA_API_VERSION == 1
        girara_warning("could not find '%s' function in plugin %s ... loading anyway", PLUGIN_ABI_VERSION_FUNCTION, path);
#else
        girara_error("could not find '%s' function in plugin %s", PLUGIN_ABI_VERSION_FUNCTION, path);
        g_free(path);
        dlclose(handle);
        continue;
#endif
      }

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
    g_dir_close(dir);
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

  if (uncertain == FALSE) {
    return content_type;
  }

  girara_debug("g_content_type is uncertain, guess: %s\n", content_type);

  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    return NULL;
  }

  const int fd = fileno(f);
  guchar* content = NULL;
  size_t length = 0u;
  while (uncertain == TRUE && length < GT_MAX_READ) {
    g_free((void*)content_type);
    content_type = NULL;

    content = g_realloc(content, length + BUFSIZ);
    const ssize_t r = read(fd, content + length, BUFSIZ);
    if (r == -1) {
      break;
    }

    length += r;
    content_type = g_content_type_guess(NULL, content, length, &uncertain);
    girara_debug("new guess: %s uncertain: %d, read: %zu\n", content_type, uncertain, length);
  }

  fclose(f);
  g_free(content);
  if (uncertain == FALSE) {
    return content_type;
  }

  g_free((void*)content_type);
  content_type = NULL;

  girara_debug("falling back to file");

  GString* command = g_string_new("file -b --mime-type ");
  char* tmp        = g_shell_quote(path);

  g_string_append(command, tmp);
  g_free(tmp);

  GError* error = NULL;
  char* out = NULL;
  int ret = 0;
  g_spawn_command_line_sync(command->str, &out, NULL, &ret, &error);
  g_string_free(command, TRUE);
  if (error != NULL) {
    girara_warning("failed to execute command: %s", error->message);
    g_error_free(error);
    g_free(out);
    return NULL;
  }
  if (WEXITSTATUS(ret) != 0) {
    girara_warning("file failed with error code: %d", WEXITSTATUS(ret));
    g_free(out);
    return NULL;
  }

  g_strdelimit(out, "\n\r", '\0');
  return out;
}

zathura_document_t*
zathura_document_open(zathura_t* zathura, const char* path, const char* password)
{
  if (path == NULL) {
    return NULL;
  }

  if (g_file_test(path, G_FILE_TEST_EXISTS) == FALSE) {
    girara_error("File '%s' does not exist", path);
    return NULL;
  }

  const gchar* content_type = guess_type(path);
  if (content_type == NULL) {
    girara_error("Could not determine file type.");
    return NULL;
  }

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

  /* open document */
  if (plugin->open_function == NULL) {
    girara_error("plugin has no open function\n");
    goto error_free;
  }

  zathura_plugin_error_t error = plugin->open_function(document);
  if (error != ZATHURA_PLUGIN_ERROR_OK) {
    if (error == ZATHURA_PLUGIN_ERROR_INVALID_PASSWORD) {
      zathura_password_dialog_info_t* password_dialog_info = malloc(sizeof(zathura_password_dialog_info_t));
      if (password_dialog_info != NULL) {
        password_dialog_info->path    = g_strdup(path);
        password_dialog_info->zathura = zathura;

        if (path != NULL) {
          girara_dialog(zathura->ui.session, "Enter password:", true, NULL,
              (girara_callback_inputbar_activate_t) cb_password_dialog, password_dialog_info);
          goto error_free;
        } else {
          free(password_dialog_info);
        }
      }
      goto error_free;
    }

    girara_error("could not open document\n");
    goto error_free;
  }

  /* read history file */
  zathura_db_get_fileinfo(zathura->database, document->file_path,
      &document->current_page_number, &document->page_offset, &document->scale,
      &document->rotate);

  /* check for valid scale value */
  if (document->scale <= FLT_EPSILON) {
    girara_warning("document info: '%s' has non positive scale", document->file_path);
    document->scale = 1;
  }

  /* check current page number */
  if (document->current_page_number > document->number_of_pages) {
    girara_warning("document info: '%s' has an invalid page number", document->file_path);
    document->current_page_number = 0;
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

  /* jump to first page if setting enabled */
  bool always_first_page = false;
  girara_setting_get(zathura->ui.session, "open-first-page", &always_first_page);
  if (always_first_page == true) {
    document->current_page_number = 0;
  }

  /* apply open adjustment */
  char* adjust_open = "best-fit";
  document->adjust_mode = ADJUST_BESTFIT;
  if (girara_setting_get(zathura->ui.session, "adjust-open", &(adjust_open)) == true) {
    if (g_strcmp0(adjust_open, "best-fit") == 0) {
      document->adjust_mode = ADJUST_BESTFIT;
    } else if (g_strcmp0(adjust_open, "width") == 0) {
      document->adjust_mode = ADJUST_WIDTH;
    } else {
      document->adjust_mode = ADJUST_NONE;
    }
    g_free(adjust_open);
  }

  return document;

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

zathura_plugin_error_t
zathura_document_free(zathura_document_t* document)
{
  if (document == NULL || document->zathura == NULL || document->zathura->ui.session == NULL) {
    return ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
  }

  /* free pages */
  for (unsigned int page_id = 0; page_id < document->number_of_pages; page_id++) {
    zathura_page_free(document->pages[page_id]);
    document->pages[page_id] = NULL;
  }

  free(document->pages);

  /* free document */
  zathura_plugin_error_t error = ZATHURA_PLUGIN_ERROR_OK;
  if (document->functions.document_free == NULL) {
    girara_notify(document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
  } else {
    error = document->functions.document_free(document);
  }

  if (document->file_path != NULL) {
    free(document->file_path);
  }

  g_free(document);

  return error;
}

zathura_plugin_error_t
zathura_document_save_as(zathura_document_t* document, const char* path)
{
  if (document == NULL || path == NULL || document->zathura == NULL || document->zathura->ui.session == NULL) {
    return ZATHURA_PLUGIN_ERROR_UNKNOWN;
  }

  if (document->functions.document_save_as == NULL) {
    girara_notify(document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    return ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
  }

  return document->functions.document_save_as(document, path);
}

girara_tree_node_t*
zathura_document_index_generate(zathura_document_t* document, zathura_plugin_error_t* error)
{
  if (document == NULL || document->zathura == NULL || document->zathura->ui.session == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  if (document->functions.document_index_generate == NULL) {
    girara_notify(document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return document->functions.document_index_generate(document, error);
}

girara_list_t*
zathura_document_attachments_get(zathura_document_t* document, zathura_plugin_error_t* error)
{
  if (document == NULL || document->zathura == NULL || document->zathura->ui.session == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  if (document->functions.document_attachments_get == NULL) {
    girara_notify(document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
  }

  return document->functions.document_attachments_get(document, error);
}

zathura_plugin_error_t
zathura_document_attachment_save(zathura_document_t* document, const char* attachment, const char* file)
{
  if (document == NULL || document->zathura == NULL || document->zathura->ui.session == NULL) {
    return ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
  }

  if (document->functions.document_attachment_save == NULL) {
    girara_notify(document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    return ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
  }

  return document->functions.document_attachment_save(document, attachment, file);
}

char*
zathura_document_meta_get(zathura_document_t* document, zathura_document_meta_t meta, zathura_plugin_error_t* error)
{
  if (document == NULL || document->zathura == NULL || document->zathura->ui.session == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_INVALID_ARGUMENTS;
    }
    return NULL;
  }

  if (document->functions.document_meta_get == NULL) {
    if (error != NULL) {
      *error = ZATHURA_PLUGIN_ERROR_NOT_IMPLEMENTED;
    }
    girara_notify(document->zathura->ui.session, GIRARA_WARNING, _("%s not implemented"), __FUNCTION__);
    girara_error("%s not implemented", __FUNCTION__);
    return NULL;
  }

  return document->functions.document_meta_get(document, meta, error);
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
