/* See LICENSE file for license and copyright information */

#ifndef CONFIG_H
#define CONFIG_H

#define GLOBAL_RC  "/etc/zathurarc"
#define ZATHURA_RC "zathurarc"

#include "zathura.h"

/**
 * This function loads the default values of the configuration
 *
 * @param zathura The zathura session
 */
void config_load_default(zathura_t* zathura);

/**
 * Loads and evaluates a configuration file
 *
 * @param zathura The zathura session
 * @param path Path to the configuration file
 */
void config_load_file(zathura_t* zathura, const char* path);

#endif // CONFIG_H
