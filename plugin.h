/* See LICENSE file for license and copyright information */

#ifndef PLUGIN_H
#define PLUGIN_H

#include <girara/types.h>

#include "types.h"
#include "plugin-api.h"
#include "version.h"
#include "zathura.h"

#define PLUGIN_REGISTER_FUNCTION    "zathura_plugin_register"
#define PLUGIN_API_VERSION_FUNCTION "zathura_plugin_api_version"
#define PLUGIN_ABI_VERSION_FUNCTION "zathura_plugin_abi_version"

/**
 * Document plugin structure
 */
struct zathura_plugin_s
{
  girara_list_t* content_types; /**< List of supported content types */
  zathura_plugin_register_function_t register_function; /**< Document open function */
  zathura_plugin_functions_t functions; /**< Document functions */
  void* handle; /**< DLL handle */
};

typedef struct zathura_plugin_manager_s zathura_plugin_manager_t;

/**
 * Creates a new instance of the plugin manager
 *
 * @return A plugin manager object or NULL if an error occured
 */
zathura_plugin_manager_t* zathura_plugin_manager_new();

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
 * Frees the plugin manager
 *
 * @param plugin_manager
 */
void zathura_plugin_manager_free(zathura_plugin_manager_t* plugin_manager);

/**
 * Plugin mapping
 */
typedef struct zathura_type_plugin_mapping_s
{
  const gchar* type; /**< Plugin type */
  zathura_plugin_t* plugin; /**< Mapped plugin */
} zathura_type_plugin_mapping_t;

/**
 * Function prototype that is called to register a document plugin
 *
 * @param The document plugin
 */
typedef void (*zathura_plugin_register_service_t)(zathura_plugin_t*);

/**
 * Function prototype that is called to get the plugin's API version.
 *
 * @return plugin's API version
 */
typedef unsigned int (*zathura_plugin_api_version_t)();

/**
 * Function prototype that is called to get the ABI version the plugin is built
 * against.
 *
 * @return plugin's ABI version
 */
typedef unsigned int (*zathura_plugin_abi_version_t)();

#endif // PLUGIN_H
