/* SPDX-License-Identifier: Zlib */

#ifndef PLUGIN_H
#define PLUGIN_H

#include <girara/types.h>
#include <gmodule.h>

#include "types.h"
#include "plugin-api.h"
#include "zathura-version.h"
#include "zathura.h"

/**
 * Creates a new instance of the plugin manager
 *
 * @return A plugin manager object or NULL if an error occurred
 */
zathura_plugin_manager_t* zathura_plugin_manager_new(void);

/**
 * Frees the plugin manager
 *
 * @param plugin_manager
 */
void zathura_plugin_manager_free(zathura_plugin_manager_t* plugin_manager);

/**
 * Adds a plugin directory to the plugin manager
 *
 * @param plugin_manager The plugin manager
 * @param dir Path to a directory with plugins
 */
void zathura_plugin_manager_add_dir(zathura_plugin_manager_t* plugin_manager, const char* dir);

/**
 * Loads all plugins available in the previously given directories
 *
 * @param plugin_manager The plugin manager
 */
void zathura_plugin_manager_load(zathura_plugin_manager_t* plugin_manager);

/**
 * Returns the (if available) associated plugin
 *
 * @param plugin_manager The plugin manager
 * @param type The document type
 * @return The plugin or NULL if no matching plugin is available
 */
zathura_plugin_t* zathura_plugin_manager_get_plugin(zathura_plugin_manager_t* plugin_manager, const char* type);

/**
 * Returns a list with the plugin objects
 *
 * @param plugin_manager The plugin manager
 * @return List of plugins or NULL
 */
girara_list_t* zathura_plugin_manager_get_plugins(zathura_plugin_manager_t* plugin_manager);

/**
 * Returns the plugin functions
 *
 * @param plugin The plugin
 * @return The plugin functions
 */
const zathura_plugin_functions_t* zathura_plugin_get_functions(zathura_plugin_t* plugin);

/**
 * Returns the name of the plugin
 *
 * @param plugin The plugin
 * @return The name of the plugin or NULL
 */
const char* zathura_plugin_get_name(zathura_plugin_t* plugin);

/**
 * Returns the path to the plugin
 *
 * @param plugin The plugin
 * @return The path of the plugin or NULL
 */
char* zathura_plugin_get_path(zathura_plugin_t* plugin);

/**
 * Returns the version information of the plugin
 *
 * @param plugin The plugin
 * @return The version information of the plugin
 */
zathura_plugin_version_t zathura_plugin_get_version(zathura_plugin_t* plugin);

#endif // PLUGIN_H
