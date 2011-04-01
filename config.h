/* See LICENSE file for license and copyright information */

#ifndef CONFIG_H
#define CONFIG_H

#define GLOBAL_RC  "/etc/zathurarc"
#define ZATHURA_RC "zathurarc"
#define CONFIG_DIR "~/.config/zathura"

/**
 * This function loads the default values of the configuration
 */
void config_load_default(void);

/**
 * Loads and evaluates a configuration file
 *
 * @param path Path to the configuration file
 */
void config_load_file(char* path);

#endif // CONFIG_H
