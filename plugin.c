/* See LICENSE file for license and copyright information */

#include "plugin.h"

#include <stdlib.h>
#include <glib/gi18n.h>

#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/statusbar.h>
#include <girara/session.h>
#include <girara/settings.h>

/**
 * Document plugin structure
 */
struct zathura_plugin_s {
  girara_list_t* content_types; /**< List of supported content types */
  zathura_plugin_register_function_t register_function; /**< Document open function */
  zathura_plugin_functions_t functions; /**< Document functions */
  GModule* handle; /**< DLL handle */
  char* name; /**< Name of the plugin */
  char* path; /**< Path to the plugin */
  zathura_plugin_version_t version; /**< Version information */
};

/**
 * Plugin mapping
 */
typedef struct zathura_type_plugin_mapping_s {
  const gchar* type; /**< Plugin type */
  zathura_plugin_t* plugin; /**< Mapped plugin */
} zathura_type_plugin_mapping_t;

/**
 * Plugin manager
 */
struct zathura_plugin_manager_s {
  girara_list_t* plugins; /**< List of plugins */
  girara_list_t* path; /**< List of plugin paths */
  girara_list_t* type_plugin_mapping; /**< List of type -> plugin mappings */
};

typedef void (*zathura_plugin_register_service_t)(zathura_plugin_t*);
typedef unsigned int (*zathura_plugin_api_version_t)(void);
typedef unsigned int (*zathura_plugin_abi_version_t)(void);
typedef unsigned int (*zathura_plugin_version_function_t)(void);

static bool register_plugin(zathura_plugin_manager_t* plugin_manager, zathura_plugin_t* plugin);
static bool plugin_mapping_new(zathura_plugin_manager_t* plugin_manager, const gchar* type, zathura_plugin_t* plugin);
static void zathura_plugin_free(zathura_plugin_t* plugin);
static void zathura_type_plugin_mapping_free(zathura_type_plugin_mapping_t* mapping);

zathura_plugin_manager_t*
zathura_plugin_manager_new()
{
  zathura_plugin_manager_t* plugin_manager = g_try_malloc0(sizeof(zathura_plugin_manager_t));
  if (plugin_manager == NULL) {
    return NULL;
  }

  plugin_manager->plugins = girara_list_new2((girara_free_function_t) zathura_plugin_free);
  plugin_manager->path    = girara_list_new2(g_free);
  plugin_manager->type_plugin_mapping = girara_list_new2((girara_free_function_t)zathura_type_plugin_mapping_free);

  if (plugin_manager->plugins == NULL
      || plugin_manager->path == NULL
      || plugin_manager->type_plugin_mapping == NULL) {
    zathura_plugin_manager_free(plugin_manager);
    return NULL;
  }

  return plugin_manager;
}

void
zathura_plugin_manager_add_dir(zathura_plugin_manager_t* plugin_manager, const char* dir)
{
  if (plugin_manager == NULL || plugin_manager->path == NULL) {
    return;
  }

  girara_list_append(plugin_manager->path, g_strdup(dir));
}

void
zathura_plugin_manager_load(zathura_plugin_manager_t* plugin_manager)
{
  if (plugin_manager == NULL || plugin_manager->path == NULL) {
    return;
  }

  GIRARA_LIST_FOREACH(plugin_manager->path, char*, iter, plugindir)
  /* read all files in the plugin directory */
  GDir* dir = g_dir_open(plugindir, 0, NULL);
  if (dir == NULL) {
    girara_error("could not open plugin directory: %s", plugindir);
    girara_list_iterator_next(iter);
    continue;
  }

  char* name = NULL;
  while ((name = (char*) g_dir_read_name(dir)) != NULL) {
    char* path = g_build_filename(plugindir, name, NULL);
    if (g_file_test(path, G_FILE_TEST_IS_REGULAR) == 0) {
      girara_debug("%s is not a regular file. Skipping.", path);
      g_free(path);
      continue;
    }

    if (g_str_has_suffix(path, ".so") == FALSE) {
      girara_debug("%s is not a plugin file. Skipping.", path);
      g_free(path);
      continue;
    }

    zathura_plugin_t* plugin = NULL;

    /* load plugin */
    GModule* handle = g_module_open(path, G_MODULE_BIND_LOCAL);
    if (handle == NULL) {
      girara_error("could not load plugin %s (%s)", path, g_module_error());
      g_free(path);
      continue;
    }

    /* resolve symbols and check API and ABI version*/
    zathura_plugin_api_version_t api_version = NULL;
    if (g_module_symbol(handle, PLUGIN_API_VERSION_FUNCTION, (gpointer*) &api_version) == FALSE ||
        api_version == NULL) {
      girara_error("could not find '%s' function in plugin %s", PLUGIN_API_VERSION_FUNCTION, path);
      g_free(path);
      g_module_close(handle);
      continue;
    }

    if (api_version() != ZATHURA_API_VERSION) {
      girara_error("plugin %s has been built againt zathura with a different API version (plugin: %d, zathura: %d)",
                   path, api_version(), ZATHURA_API_VERSION);
      g_free(path);
      g_module_close(handle);
      continue;
    }

    zathura_plugin_abi_version_t abi_version = NULL;
    if (g_module_symbol(handle, PLUGIN_ABI_VERSION_FUNCTION, (gpointer*) &abi_version) == FALSE ||
        abi_version == NULL) {
      girara_error("could not find '%s' function in plugin %s", PLUGIN_ABI_VERSION_FUNCTION, path);
      g_free(path);
      g_module_close(handle);
      continue;
    }

    if (abi_version() != ZATHURA_ABI_VERSION) {
      girara_error("plugin %s has been built againt zathura with a different ABI version (plugin: %d, zathura: %d)",
                   path, abi_version(), ZATHURA_ABI_VERSION);
      g_free(path);
      g_module_close(handle);
      continue;
    }

    zathura_plugin_register_service_t register_service = NULL;
    if (g_module_symbol(handle, PLUGIN_REGISTER_FUNCTION, (gpointer*) &register_service) == FALSE ||
        register_service == NULL) {
      girara_error("could not find '%s' function in plugin %s", PLUGIN_REGISTER_FUNCTION, path);
      g_free(path);
      g_module_close(handle);
      continue;
    }

    plugin = g_try_malloc0(sizeof(zathura_plugin_t));
    if (plugin == NULL) {
      girara_error("Failed to allocate memory for plugin.");
      g_free(path);
      g_module_close(handle);
      continue;
    }

    plugin->content_types = girara_list_new2(g_free);
    plugin->handle = handle;

    register_service(plugin);

    /* register functions */
    if (plugin->register_function == NULL) {
      girara_error("plugin has no document functions register function");
      g_free(path);
      g_free(plugin);
      g_module_close(handle);
      continue;
    }

    plugin->register_function(&(plugin->functions));
    plugin->path = path;

    bool ret = register_plugin(plugin_manager, plugin);
    if (ret == false) {
      girara_error("could not register plugin %s", path);
      zathura_plugin_free(plugin);
    } else {
      girara_debug("successfully loaded plugin %s", path);

      zathura_plugin_version_function_t plugin_major = NULL, plugin_minor = NULL, plugin_rev = NULL;
      g_module_symbol(handle, PLUGIN_VERSION_MAJOR_FUNCTION,    (gpointer*) &plugin_major);
      g_module_symbol(handle, PLUGIN_VERSION_MINOR_FUNCTION,    (gpointer*) &plugin_minor);
      g_module_symbol(handle, PLUGIN_VERSION_REVISION_FUNCTION, (gpointer*) &plugin_rev);
      if (plugin_major != NULL && plugin_minor != NULL && plugin_rev != NULL) {
        plugin->version.major = plugin_major();
        plugin->version.minor = plugin_minor();
        plugin->version.rev   = plugin_rev();
        girara_debug("plugin '%s': version %u.%u.%u", path,
                     plugin->version.major, plugin->version.minor,
                     plugin->version.rev);
      }
    }
  }
  g_dir_close(dir);
  GIRARA_LIST_FOREACH_END(zathura->plugins.path, char*, iter, plugindir);
}

zathura_plugin_t*
zathura_plugin_manager_get_plugin(zathura_plugin_manager_t* plugin_manager, const char* type)
{
  if (plugin_manager == NULL || plugin_manager->type_plugin_mapping == NULL || type == NULL) {
    return NULL;
  }

  zathura_plugin_t* plugin = NULL;
  GIRARA_LIST_FOREACH(plugin_manager->type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping)
  if (g_content_type_equals(type, mapping->type)) {
    plugin = mapping->plugin;
    break;
  }
  GIRARA_LIST_FOREACH_END(plugin_manager->type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping);

  return plugin;
}

girara_list_t*
zathura_plugin_manager_get_plugins(zathura_plugin_manager_t* plugin_manager)
{
  if (plugin_manager == NULL || plugin_manager->plugins == NULL) {
    return NULL;
  }

  return plugin_manager->plugins;
}

void
zathura_plugin_manager_free(zathura_plugin_manager_t* plugin_manager)
{
  if (plugin_manager == NULL) {
    return;
  }

  if (plugin_manager->plugins != NULL) {
    girara_list_free(plugin_manager->plugins);
  }

  if (plugin_manager->path != NULL) {
    girara_list_free(plugin_manager->path);
  }

  if (plugin_manager->type_plugin_mapping != NULL) {
    girara_list_free(plugin_manager->type_plugin_mapping);
  }

  g_free(plugin_manager);
}

static bool
register_plugin(zathura_plugin_manager_t* plugin_manager, zathura_plugin_t* plugin)
{
  if (plugin == NULL
      || plugin->content_types == NULL
      || plugin->register_function == NULL
      || plugin_manager == NULL
      || plugin_manager->plugins == NULL) {
    girara_error("plugin: could not register\n");
    return false;
  }

  bool at_least_one = false;
  GIRARA_LIST_FOREACH(plugin->content_types, gchar*, iter, type)
  if (plugin_mapping_new(plugin_manager, type, plugin) == false) {
    girara_error("plugin: already registered for filetype %s\n", type);
  } else {
    at_least_one = true;
  }
  GIRARA_LIST_FOREACH_END(plugin->content_types, gchar*, iter, type);

  if (at_least_one == true) {
    girara_list_append(plugin_manager->plugins, plugin);
  }

  return at_least_one;
}

static bool
plugin_mapping_new(zathura_plugin_manager_t* plugin_manager, const gchar* type, zathura_plugin_t* plugin)
{
  g_return_val_if_fail(plugin_manager != NULL, false);
  g_return_val_if_fail(type           != NULL, false);
  g_return_val_if_fail(plugin         != NULL, false);

  GIRARA_LIST_FOREACH(plugin_manager->type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping)
  if (g_content_type_equals(type, mapping->type)) {
    girara_list_iterator_free(iter);
    return false;
  }
  GIRARA_LIST_FOREACH_END(plugin_manager->type_plugin_mapping, zathura_type_plugin_mapping_t*, iter, mapping);

  zathura_type_plugin_mapping_t* mapping = g_try_malloc0(sizeof(zathura_type_plugin_mapping_t));
  if (mapping == NULL) {
    return false;
  }

  mapping->type   = g_strdup(type);
  mapping->plugin = plugin;
  girara_list_append(plugin_manager->type_plugin_mapping, mapping);

  return true;
}

static void
zathura_type_plugin_mapping_free(zathura_type_plugin_mapping_t* mapping)
{
  if (mapping == NULL) {
    return;
  }

  g_free((void*)mapping->type);
  g_free(mapping);
}

static void
zathura_plugin_free(zathura_plugin_t* plugin)
{
  if (plugin == NULL) {
    return;
  }

  if (plugin->name != NULL) {
    g_free(plugin->name);
  }

  if (plugin->path != NULL) {
    g_free(plugin->path);
  }

  g_module_close(plugin->handle);
  girara_list_free(plugin->content_types);

  g_free(plugin);
}

/* plugin-api.h */
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

zathura_plugin_functions_t*
zathura_plugin_get_functions(zathura_plugin_t* plugin)
{
  if (plugin != NULL) {
    return &(plugin->functions);
  } else {
    return NULL;
  }
}

void
zathura_plugin_set_name(zathura_plugin_t* plugin, const char* name)
{
  if (plugin != NULL && name != NULL) {
    plugin->name = g_strdup(name);
  }
}

char*
zathura_plugin_get_name(zathura_plugin_t* plugin)
{
  if (plugin != NULL) {
    return plugin->name;
  } else {
    return NULL;
  }
}

char*
zathura_plugin_get_path(zathura_plugin_t* plugin)
{
  if (plugin != NULL) {
    return plugin->path;
  } else {
    return NULL;
  }
}

zathura_plugin_version_t
zathura_plugin_get_version(zathura_plugin_t* plugin)
{
  if (plugin != NULL) {
    return plugin->version;
  }

  zathura_plugin_version_t version = { 0, 0, 0 };
  return version;
}
