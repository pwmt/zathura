/* See LICENSE file for license and copyright information */

#include "plugin.h"

#include <stdlib.h>
#include <dlfcn.h>
#include <glib/gi18n.h>

#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/statusbar.h>
#include <girara/session.h>
#include <girara/settings.h>

/**
 * Register document plugin
 */
static bool zathura_document_plugin_register(zathura_t* zathura, zathura_plugin_t* new_plugin);

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

      void* handle             = NULL;
      zathura_plugin_t* plugin = NULL;

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

      plugin = g_malloc0(sizeof(zathura_plugin_t));
      plugin->content_types = girara_list_new2(g_free);
      plugin->handle = handle;

      register_plugin(plugin);

      /* register functions */
      if (plugin->register_function == NULL) {
        girara_error("plugin has no document functions register function");
        g_free(path);
        dlclose(handle);
        continue;
      }

      plugin->register_function(&(plugin->functions));

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
zathura_document_plugin_free(zathura_plugin_t* plugin)
{
  if (plugin == NULL) {
    return;
  }

  dlclose(plugin->handle);
  girara_list_free(plugin->content_types);
  g_free(plugin);
}

static bool
zathura_document_plugin_register(zathura_t* zathura, zathura_plugin_t* new_plugin)
{
  if (new_plugin == NULL || new_plugin->content_types == NULL || new_plugin->register_function == NULL) {
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

bool
zathura_type_plugin_mapping_new(zathura_t* zathura, const gchar* type, zathura_plugin_t* plugin)
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
zathura_plugin_set_register_functions_function(zathura_plugin_t* plugin, zathura_plugin_register_function_t register_function)
{
  if (plugin == NULL || register_function == NULL) {
    return;
  }

  plugin->register_function = register_function;
}

void
zathura_plugin_add_mimetype(zathura_plugin_t* plugin, const char* mime_type)
{
  if (plugin == NULL || mime_type == NULL) {
    return;
  }

  girara_list_append(plugin->content_types, g_content_type_from_mime_type(mime_type)); 
}
