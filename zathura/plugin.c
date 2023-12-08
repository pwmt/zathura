/* SPDX-License-Identifier: Zlib */

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
  zathura_plugin_functions_t functions; /**< Document functions */
  GModule* handle; /**< DLL handle */
  char* path; /**< Path to the plugin */
  const zathura_plugin_definition_t* definition;
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
  girara_list_t* content_types; /**< List of all registered content types */
};

static void plugin_add_mimetype(zathura_plugin_t* plugin, const char* mime_type);
static bool register_plugin(zathura_plugin_manager_t* plugin_manager, zathura_plugin_t* plugin);
static bool plugin_mapping_new(zathura_plugin_manager_t* plugin_manager, const gchar* type, zathura_plugin_t* plugin);
static void zathura_plugin_free(zathura_plugin_t* plugin);
static void zathura_type_plugin_mapping_free(zathura_type_plugin_mapping_t* mapping);

zathura_plugin_manager_t*
zathura_plugin_manager_new(void)
{
  zathura_plugin_manager_t* plugin_manager = g_try_malloc0(sizeof(zathura_plugin_manager_t));
  if (plugin_manager == NULL) {
    return NULL;
  }

  plugin_manager->plugins = girara_list_new2((girara_free_function_t) zathura_plugin_free);
  plugin_manager->path    = girara_list_new2(g_free);
  plugin_manager->type_plugin_mapping = girara_list_new2((girara_free_function_t)zathura_type_plugin_mapping_free);
  plugin_manager->content_types = girara_list_new2(g_free);

  if (plugin_manager->plugins == NULL
      || plugin_manager->path == NULL
      || plugin_manager->type_plugin_mapping == NULL
      || plugin_manager->content_types == NULL) {
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

static bool
check_suffix(const char* path)
{
#ifdef __APPLE__
  if (g_str_has_suffix(path, ".dylib") == TRUE) {
    return true;
  }
#else
  if (g_str_has_suffix(path, ".so") == TRUE) {
    return true;
  }
#endif

  return false;
}

static void
load_plugin(zathura_plugin_manager_t* plugin_manager, const char* plugindir, const char* name)
{
  char* path = g_build_filename(plugindir, name, NULL);
  if (g_file_test(path, G_FILE_TEST_IS_REGULAR) == 0) {
    girara_debug("'%s' is not a regular file. Skipping.", path);
    g_free(path);
    return;
  }

  if (check_suffix(path) == false) {
    girara_debug("'%s' is not a plugin file. Skipping.", path);
    g_free(path);
    return;
  }

  /* load plugin */
  GModule* handle = g_module_open(path, G_MODULE_BIND_LOCAL);
  if (handle == NULL) {
    girara_error("Could not load plugin '%s' (%s).", path, g_module_error());
    g_free(path);
    return;
  }

  /* resolve symbols and check API and ABI version*/
  const zathura_plugin_definition_t* plugin_definition = NULL;
  if (g_module_symbol(handle, G_STRINGIFY(ZATHURA_PLUGIN_DEFINITION_SYMBOL), (void**) &plugin_definition) == FALSE ||
      plugin_definition == NULL) {
    girara_error("Could not find '%s' in plugin %s - is not a plugin or needs to be rebuilt.", G_STRINGIFY(ZATHURA_PLUGIN_DEFINITION_SYMBOL), path);
    g_free(path);
    g_module_close(handle);
    return;
  }

  /* check name */
  if (plugin_definition->name == NULL) {
    girara_error("Plugin has no name.");
    g_free(path);
    g_module_close(handle);
    return;
  }

  /* check mime type */
  if (plugin_definition->mime_types == NULL || plugin_definition->mime_types_size == 0) {
    girara_error("Plugin does not handly any mime types.");
    g_free(path);
    g_module_close(handle);
    return;
  }

  zathura_plugin_t* plugin = g_try_malloc0(sizeof(zathura_plugin_t));
  if (plugin == NULL) {
    girara_error("Failed to allocate memory for plugin.");
    g_free(path);
    g_module_close(handle);
    return;
  }

  plugin->definition = plugin_definition;
  plugin->functions = plugin_definition->functions;
  plugin->content_types = girara_list_new2(g_free);
  plugin->handle = handle;
  plugin->path = path;

  // register mime types
  for (size_t s = 0; s != plugin_definition->mime_types_size; ++s) {
    plugin_add_mimetype(plugin, plugin_definition->mime_types[s]);
  }

  bool ret = register_plugin(plugin_manager, plugin);
  if (ret == false) {
    girara_error("Could not register plugin '%s'.", path);
    zathura_plugin_free(plugin);
  } else {
    girara_debug("Successfully loaded plugin from '%s'.", path);
    girara_debug("plugin %s: version %u.%u.%u", plugin_definition->name,
                 plugin_definition->version.major, plugin_definition->version.minor,
                 plugin_definition->version.rev);
  }
}

static void
load_dir(void* data, void* userdata)
{
  const char* plugindir                    = data;
  zathura_plugin_manager_t* plugin_manager = userdata;

  GDir* dir = g_dir_open(plugindir, 0, NULL);
  if (dir == NULL) {
    girara_error("could not open plugin directory: %s", plugindir);
  } else {
    const char* name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
      load_plugin(plugin_manager, plugindir, name);
    }
    g_dir_close(dir);
  }
}

bool
zathura_plugin_manager_load(zathura_plugin_manager_t* plugin_manager)
{
  if (plugin_manager == NULL || plugin_manager->path == NULL) {
    return false;
  }

  /* read all files in the plugin directory */
  girara_list_foreach(plugin_manager->path, load_dir, plugin_manager);
  return girara_list_size(plugin_manager->plugins) > 0;
}

zathura_plugin_t* zathura_plugin_manager_get_plugin(zathura_plugin_manager_t* plugin_manager, const char* type) {
  if (plugin_manager == NULL || plugin_manager->type_plugin_mapping == NULL || type == NULL) {
    return NULL;
  }

  for (size_t idx = 0; idx != girara_list_size(plugin_manager->type_plugin_mapping); ++idx) {
    zathura_type_plugin_mapping_t* mapping = girara_list_nth(plugin_manager->type_plugin_mapping, idx);
    if (g_content_type_equals(type, mapping->type)) {
      return mapping->plugin;
    }
  }

  return NULL;
}

girara_list_t*
zathura_plugin_manager_get_plugins(zathura_plugin_manager_t* plugin_manager)
{
  if (plugin_manager == NULL || plugin_manager->plugins == NULL) {
    return NULL;
  }

  return plugin_manager->plugins;
}

girara_list_t*
zathura_plugin_manager_get_content_types(zathura_plugin_manager_t* plugin_manager)
{
  if (plugin_manager == NULL) {
    return NULL;
  }

  return plugin_manager->content_types;
}

void
zathura_plugin_manager_free(zathura_plugin_manager_t* plugin_manager)
{
  if (plugin_manager == NULL) {
    return;
  }

  girara_list_free(plugin_manager->content_types);
  girara_list_free(plugin_manager->type_plugin_mapping);
  girara_list_free(plugin_manager->path);
  girara_list_free(plugin_manager->plugins);

  g_free(plugin_manager);
}

static bool register_plugin(zathura_plugin_manager_t* plugin_manager, zathura_plugin_t* plugin) {
  if (plugin == NULL || plugin->content_types == NULL || plugin_manager == NULL || plugin_manager->plugins == NULL) {
    girara_error("plugin: could not register");
    return false;
  }

  bool at_least_one = false;
  for (size_t idx = 0; idx != girara_list_size(plugin->content_types); ++idx) {
    gchar* type = girara_list_nth(plugin->content_types, idx);
    if (plugin_mapping_new(plugin_manager, type, plugin) == false) {
      girara_error("plugin: filetype already registered: %s", type);
    } else {
      girara_debug("plugin: filetype mapping added: %s", type);
      at_least_one = true;
    }
  }

  if (at_least_one == true) {
    girara_list_append(plugin_manager->plugins, plugin);
  }

  return at_least_one;
}

static bool plugin_mapping_new(zathura_plugin_manager_t* plugin_manager, const gchar* type, zathura_plugin_t* plugin) {
  g_return_val_if_fail(plugin_manager != NULL, false);
  g_return_val_if_fail(type != NULL, false);
  g_return_val_if_fail(plugin != NULL, false);

  for (size_t idx = 0; idx != girara_list_size(plugin_manager->type_plugin_mapping); ++idx) {
    zathura_type_plugin_mapping_t* mapping = girara_list_nth(plugin_manager->type_plugin_mapping, idx);
    if (g_content_type_equals(type, mapping->type)) {
      return false;
    }
  }

  zathura_type_plugin_mapping_t* mapping = g_try_malloc0(sizeof(zathura_type_plugin_mapping_t));
  if (mapping == NULL) {
    return false;
  }

  mapping->type   = g_strdup(type);
  mapping->plugin = plugin;
  girara_list_append(plugin_manager->type_plugin_mapping, mapping);
  girara_list_append(plugin_manager->content_types, g_strdup(type));

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
 
  if (plugin->path != NULL) {
    g_free(plugin->path);
  }

  g_module_close(plugin->handle);
  girara_list_free(plugin->content_types);

  g_free(plugin);
}

static void
plugin_add_mimetype(zathura_plugin_t* plugin, const char* mime_type)
{
  if (plugin == NULL || mime_type == NULL) {
    return;
  }

  char* content_type = g_content_type_from_mime_type(mime_type);
  if (content_type == NULL) {
    girara_warning("plugin: unable to convert mime type: %s", mime_type);
  } else {
    girara_list_append(plugin->content_types, content_type);
  }
}

const zathura_plugin_functions_t*
zathura_plugin_get_functions(zathura_plugin_t* plugin)
{
  if (plugin != NULL) {
    return &plugin->functions;
  } else {
    return NULL;
  }
}

const char*
zathura_plugin_get_name(zathura_plugin_t* plugin)
{
  if (plugin != NULL && plugin->definition != NULL) {
    return plugin->definition->name;
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
  if (plugin != NULL && plugin->definition != NULL) {
    return plugin->definition->version;
  }

  zathura_plugin_version_t version = { 0, 0, 0 };
  return version;
}
