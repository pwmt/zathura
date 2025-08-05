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
  girara_list_t* content_types;         /**< List of supported content types */
  zathura_plugin_functions_t functions; /**< Document functions */
  GModule* handle;                      /**< DLL handle */
  char* path;                           /**< Path to the plugin */
  const zathura_plugin_definition_t* definition;
};

/**
 * Plugin mapping
 */
typedef struct zathura_type_plugin_mapping_s {
  char* type;               /**< Plugin type */
  zathura_plugin_t* plugin; /**< Mapped plugin */
} zathura_type_plugin_mapping_t;

/**
 * Plugin manager
 */
struct zathura_plugin_manager_s {
  girara_list_t* plugins;             /**< List of plugins */
  girara_list_t* path;                /**< List of plugin paths */
  girara_list_t* type_plugin_mapping; /**< List of type -> plugin mappings */
  girara_list_t* content_types;       /**< List of all registered content types */
  girara_list_t* utility_plugins;    /**< List of utility plugins */
};

static void zathura_type_plugin_mapping_free(void* data) {
  if (data != NULL) {
    zathura_type_plugin_mapping_t* mapping = data;

    g_free(mapping->type);
    g_free(mapping);
  }
}

static void zathura_plugin_free(void* data) {
  if (data != NULL) {
    zathura_plugin_t* plugin = data;

    g_free(plugin->path);
    g_module_close(plugin->handle);
    girara_list_free(plugin->content_types);
    g_free(plugin);
  }
}

static void zathura_utility_plugin_free(void* data) {
  if (data != NULL) {
    zathura_plugin_t* plugin = data;
    g_free(plugin->path);
    g_module_close(plugin->handle);
    g_free(plugin);
  }
}

static void add_dir(void* data, void* userdata) {
  const char* path                         = data;
  zathura_plugin_manager_t* plugin_manager = userdata;

  girara_list_append(plugin_manager->path, g_strdup(path));
}

static void set_plugin_dir(zathura_plugin_manager_t* plugin_manager, const char* dir) {
  girara_list_t* paths = girara_split_path_array(dir);
  girara_list_foreach(paths, add_dir, plugin_manager);
  girara_list_free(paths);
}

static void set_default_dirs(zathura_plugin_manager_t* plugin_manager) {
#ifdef ZATHURA_PLUGINDIR
  set_plugin_dir(plugin_manager, ZATHURA_PLUGINDIR);
#endif

  const char* env_paths = g_getenv("ZATHURA_PLUGINS_PATH");
  if (env_paths != NULL) {
    set_plugin_dir(plugin_manager, env_paths);
  }
}

zathura_plugin_manager_t* zathura_plugin_manager_new(void) {
  zathura_plugin_manager_t* plugin_manager = g_try_malloc0(sizeof(zathura_plugin_manager_t));
  if (plugin_manager == NULL) {
    return NULL;
  }

  plugin_manager->plugins             = girara_list_new2(zathura_plugin_free);
  plugin_manager->path                = girara_list_new2(g_free);
  plugin_manager->type_plugin_mapping = girara_list_new2(zathura_type_plugin_mapping_free);
  plugin_manager->content_types       = girara_list_new2(g_free);
  plugin_manager->utility_plugins     = girara_list_new2(zathura_utility_plugin_free);

  if (plugin_manager->plugins == NULL || plugin_manager->path == NULL || plugin_manager->type_plugin_mapping == NULL ||
      plugin_manager->content_types == NULL || plugin_manager->utility_plugins == NULL) {
    zathura_plugin_manager_free(plugin_manager);
    return NULL;
  }

  set_default_dirs(plugin_manager);
  return plugin_manager;
}

void zathura_plugin_manager_set_dir(zathura_plugin_manager_t* plugin_manager, const char* dir) {
  g_return_if_fail(plugin_manager != NULL);

  if (dir != NULL) {
    set_plugin_dir(plugin_manager, dir);
  }
}

static bool check_suffix(const char* path) {
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

static void plugin_add_mimetype(zathura_plugin_t* plugin, const char* mime_type) {
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

static bool register_utility_plugin(zathura_plugin_manager_t* plugin_manager, zathura_plugin_t* plugin) {
  if (plugin == NULL || plugin_manager == NULL || plugin_manager->utility_plugins == NULL) {
    girara_error("utility plugin: could not register");
    return false;
  }

  girara_list_append(plugin_manager->utility_plugins, plugin);
  girara_debug("utility plugin: registered %s", plugin->definition ? plugin->definition->name : "unknown");
  return true;
}

static void load_utility_plugin(zathura_plugin_manager_t* plugin_manager, const char* plugindir, const char* name) {
  char* path = g_build_filename(plugindir, name, NULL);
  if (g_file_test(path, G_FILE_TEST_IS_REGULAR) == 0) {
    g_free(path);
    return;
  }

  if (check_suffix(path) == false) {
    g_free(path);
    return;
  }

  /* load plugin */
  GModule* handle = g_module_open(path, G_MODULE_BIND_LOCAL);
  if (handle == NULL) {
    g_free(path);
    return;
  }

  /* check for utility plugin symbol */
  const zathura_utility_plugin_definition_t* utility_plugin_definition = NULL;
  if (g_module_symbol(handle, G_STRINGIFY(ZATHURA_UTILITY_PLUGIN_DEFINITION_SYMBOL), (void**)&utility_plugin_definition) == FALSE ||
      utility_plugin_definition == NULL) {
    /* Not a utility plugin, continue with regular plugin loading */
    g_module_close(handle);
    g_free(path);
    return;
  }

  /* check name */
  if (utility_plugin_definition->name == NULL) {
    girara_error("Utility plugin has no name.");
    g_free(path);
    g_module_close(handle);
    return;
  }

  /* check init function */
  if (utility_plugin_definition->init_function == NULL) {
    girara_error("Utility plugin is missing init function.");
    g_free(path);
    g_module_close(handle);
    return;
  }

  zathura_plugin_t* plugin = g_try_malloc0(sizeof(zathura_plugin_t));
  if (plugin == NULL) {
    girara_error("Failed to allocate memory for utility plugin.");
    g_free(path);
    g_module_close(handle);
    return;
  }

  plugin->definition    = (const zathura_plugin_definition_t*)utility_plugin_definition;
  plugin->content_types = NULL; /* Utility plugins don't handle content types */
  plugin->handle        = handle;
  plugin->path          = path;

  bool ret = register_utility_plugin(plugin_manager, plugin);
  if (ret == false) {
    girara_error("Could not register utility plugin '%s'.", path);
    zathura_utility_plugin_free(plugin);
  } else {
    girara_debug("Successfully loaded utility plugin from '%s'.", path);
    girara_debug("utility plugin %s: version %u.%u.%u", utility_plugin_definition->name, 
                 utility_plugin_definition->version.major,
                 utility_plugin_definition->version.minor, 
                 utility_plugin_definition->version.rev);
  }
}

static void load_plugin(zathura_plugin_manager_t* plugin_manager, const char* plugindir, const char* name) {
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
  if (g_module_symbol(handle, G_STRINGIFY(ZATHURA_PLUGIN_DEFINITION_SYMBOL), (void**)&plugin_definition) == FALSE ||
      plugin_definition == NULL) {
    girara_error("Could not find '%s' in plugin %s - is not a plugin or needs to be rebuilt.",
                 G_STRINGIFY(ZATHURA_PLUGIN_DEFINITION_SYMBOL), path);
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

  if (plugin_definition->functions.document_open == NULL || plugin_definition->functions.document_free == NULL ||
      plugin_definition->functions.page_init == NULL || plugin_definition->functions.page_clear == NULL ||
      plugin_definition->functions.page_render_cairo == NULL) {
    girara_error("Plugin is missing required functions.");
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

  plugin->definition    = plugin_definition;
  plugin->functions     = plugin_definition->functions;
  plugin->content_types = girara_list_new2(g_free);
  plugin->handle        = handle;
  plugin->path          = path;

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
    girara_debug("plugin %s: version %u.%u.%u", plugin_definition->name, plugin_definition->version.major,
                 plugin_definition->version.minor, plugin_definition->version.rev);
  }
}

static void load_dir(void* data, void* userdata) {
  const char* plugindir                    = data;
  zathura_plugin_manager_t* plugin_manager = userdata;

  GDir* dir = g_dir_open(plugindir, 0, NULL);
  if (dir == NULL) {
    girara_debug("Could not open plugin directory: %s", plugindir);
  } else {
    const char* name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
      /* First try to load as utility plugin */
      load_utility_plugin(plugin_manager, plugindir, name);
      /* Then try to load as regular document plugin */
      load_plugin(plugin_manager, plugindir, name);
    }
    g_dir_close(dir);
  }
}

bool zathura_plugin_manager_load(zathura_plugin_manager_t* plugin_manager) {
  if (plugin_manager == NULL || plugin_manager->path == NULL) {
    return false;
  }

  /* read all files in the plugin directory */
  girara_list_foreach(plugin_manager->path, load_dir, plugin_manager);
  return girara_list_size(plugin_manager->plugins) > 0;
}

const zathura_plugin_t* zathura_plugin_manager_get_plugin(const zathura_plugin_manager_t* plugin_manager,
                                                          const char* type) {
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

girara_list_t* zathura_plugin_manager_get_plugins(const zathura_plugin_manager_t* plugin_manager) {
  if (plugin_manager == NULL) {
    return NULL;
  }

  return plugin_manager->plugins;
}

girara_list_t* zathura_plugin_manager_get_content_types(const zathura_plugin_manager_t* plugin_manager) {
  if (plugin_manager == NULL) {
    return NULL;
  }

  return plugin_manager->content_types;
}

void zathura_plugin_manager_free(zathura_plugin_manager_t* plugin_manager) {
  if (plugin_manager != NULL) {
    girara_list_free(plugin_manager->content_types);
    girara_list_free(plugin_manager->type_plugin_mapping);
    girara_list_free(plugin_manager->path);
    girara_list_free(plugin_manager->plugins);
    girara_list_free(plugin_manager->utility_plugins);

    g_free(plugin_manager);
  }
}

const zathura_plugin_functions_t* zathura_plugin_get_functions(const zathura_plugin_t* plugin) {
  if (plugin != NULL) {
    return &plugin->functions;
  } else {
    return NULL;
  }
}

const char* zathura_plugin_get_name(const zathura_plugin_t* plugin) {
  if (plugin != NULL && plugin->definition != NULL) {
    return plugin->definition->name;
  } else {
    return NULL;
  }
}

const char* zathura_plugin_get_path(const zathura_plugin_t* plugin) {
  if (plugin != NULL) {
    return plugin->path;
  } else {
    return NULL;
  }
}

zathura_plugin_version_t zathura_plugin_get_version(const zathura_plugin_t* plugin) {
  if (plugin != NULL && plugin->definition != NULL) {
    return plugin->definition->version;
  }

  zathura_plugin_version_t version = {0, 0, 0};
  return version;
}

void zathura_plugin_manager_init_utility_plugins(const zathura_plugin_manager_t* plugin_manager, zathura_t* zathura) {
  if (plugin_manager == NULL || plugin_manager->utility_plugins == NULL || zathura == NULL) {
    return;
  }

  for (size_t idx = 0; idx != girara_list_size(plugin_manager->utility_plugins); ++idx) {
    zathura_plugin_t* plugin = girara_list_nth(plugin_manager->utility_plugins, idx);
    const zathura_utility_plugin_definition_t* def = (const zathura_utility_plugin_definition_t*)plugin->definition;
    
    if (def->init_function != NULL) {
      girara_debug("Initializing utility plugin: %s", def->name);
      def->init_function(zathura);
    }
  }
}
