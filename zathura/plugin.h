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
 * Add colon-seperated list of directories to the plugin manager's plugin search path
 *
 * @param plugin_manager  The plain manager
 * @param dir Colon-seperated list of directories
 */
void zathura_plugin_manager_set_dir(zathura_plugin_manager_t* plugin_manager, const char* dir);

/**
 * Loads all plugins available in the previously given directories
 *
 * @param plugin_manager The plugin manager
 * @return Success if some plugins have been loaded, false otherwise
 */
bool zathura_plugin_manager_load(zathura_plugin_manager_t* plugin_manager);

/**
 * Returns the (if available) associated plugin
 *
 * @param plugin_manager The plugin manager
 * @param type The document type
 * @return The plugin or NULL if no matching plugin is available
 */
const zathura_plugin_t* zathura_plugin_manager_get_plugin(const zathura_plugin_manager_t* plugin_manager,
                                                          const char* type);

/**
 * Initialize utility plugins
 *
 * @param plugin_manager The plugin manager
 * @param zathura The zathura instance
 */
void zathura_plugin_manager_init_utility_plugins(const zathura_plugin_manager_t* plugin_manager, zathura_t* zathura);

/**
 * Returns a list with the plugin objects
 *
 * @param plugin_manager The plugin manager
 * @return List of plugins or NULL
 */
girara_list_t* zathura_plugin_manager_get_plugins(const zathura_plugin_manager_t* plugin_manager);

/**
 * Return a list of supported content types
 *
 * @param plugin_manager The plugin manager
 * @return List of plugins or NULL
 */
girara_list_t* zathura_plugin_manager_get_content_types(const zathura_plugin_manager_t* plugin_manager);

/**
 * Returns the plugin functions
 *
 * @param plugin The plugin
 * @return The plugin functions
 */
const zathura_plugin_functions_t* zathura_plugin_get_functions(const zathura_plugin_t* plugin);

/**
 * Returns the name of the plugin
 *
 * @param plugin The plugin
 * @return The name of the plugin or NULL
 */
const char* zathura_plugin_get_name(const zathura_plugin_t* plugin);

/**
 * Returns the path to the plugin
 *
 * @param plugin The plugin
 * @return The path of the plugin or NULL
 */
const char* zathura_plugin_get_path(const zathura_plugin_t* plugin);

/**
 * Returns the version information of the plugin
 *
 * @param plugin The plugin
 * @return The version information of the plugin
 */
zathura_plugin_version_t zathura_plugin_get_version(const zathura_plugin_t* plugin);

#endif // PLUGIN_H
