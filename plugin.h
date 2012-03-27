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
 * Plugin mapping
 */
typedef struct zathura_type_plugin_mapping_s
{
  const gchar* type; /**< Plugin type */
  zathura_plugin_t* plugin; /**< Mapped plugin */
} zathura_type_plugin_mapping_t;

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

/**
 * Load all document plugins
 *
 * @param zathura the zathura session
 */
void zathura_document_plugins_load(zathura_t* zathura);

/**
 * Free a document plugin
 *
 * @param plugin The plugin
 */
void zathura_document_plugin_free(zathura_plugin_t* plugin);

/**
 * Add type -> plugin mapping
 * @param zathura zathura instance
 * @param type content type
 * @param plugin plugin instance
 * @return true on success, false if another plugin is already registered for
 * that type
 */
bool zathura_type_plugin_mapping_new(zathura_t* zathura, const gchar* type, zathura_plugin_t* plugin);

/**
 * Free type -> plugin mapping
 * @param mapping To be freed
 */
void zathura_type_plugin_mapping_free(zathura_type_plugin_mapping_t* mapping);

#endif // PLUGIN_H
