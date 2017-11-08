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
};

static void zathura_plugin_add_mimetype(zathura_plugin_t* plugin, const char* mime_type);
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

static bool
check_suffix(const char* path)
{
  if (g_str_has_suffix(path, ".so") == TRUE) {
    return true;
  }

#ifdef __APPLE__
  if (g_str_has_suffix(path, ".dylib") == TRUE) {
    return true;
  }
#endif

  return false;
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

    if (check_suffix(path) == false) {
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
    const zathura_plugin_definition_t* plugin_definition = NULL;
    if (g_module_symbol(handle, G_STRINGIFY(ZATHURA_PLUGIN_DEFINITION_SYMBOL), (void**) &plugin_definition) == FALSE ||
        plugin_definition == NULL) {
      girara_error("could not find '%s' in plugin %s - is not a plugin or needs to be rebuilt", G_STRINGIFY(ZATHURA_PLUGIN_DEFINITION_SYMBOL), path);
      g_free(path);
      g_module_close(handle);
      continue;
    }

    /* check name */
    if (plugin_definition->name == NULL) {
      girara_error("plugin has no name");
      g_free(path);
      g_free(plugin);
      g_module_close(handle);
      continue;
    }

    /* check mime type */
    if (plugin_definition->mime_types == NULL || plugin_definition->mime_types_size == 0) {
      girara_error("plugin has no mime_types");
      g_free(path);
      g_free(plugin);
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

    plugin->definition = plugin_definition;
    plugin->functions = plugin_definition->functions;
    plugin->content_types = girara_list_new2(g_free);
    plugin->handle = handle;
    plugin->path = path;

    // register mime types
    for (size_t s = 0; s != plugin_definition->mime_types_size; ++s) {
      zathura_plugin_add_mimetype(plugin, plugin_definition->mime_types[s]);
    }
    // register functions
    if (plugin->definition->register_function != NULL) {
      plugin->definition->register_function(&(plugin->functions));
    }

    bool ret = register_plugin(plugin_manager, plugin);
    if (ret == false) {
      girara_error("could not register plugin %s", path);
      zathura_plugin_free(plugin);
    } else {
      girara_debug("successfully loaded plugin from %s", path);
      girara_debug("plugin %s: version %u.%u.%u", plugin_definition->name,
                   plugin_definition->version.major, plugin_definition->version.minor,
                   plugin_definition->version.rev);
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
 
  if (plugin->path != NULL) {
    g_free(plugin->path);
  }

  g_module_close(plugin->handle);
  girara_list_free(plugin->content_types);

  g_free(plugin);
}

static void
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
