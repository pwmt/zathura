/* SPDX-License-Identifier: Zlib */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <gtk/gtk.h>
#include <girara/types.h>

#include "document.h"
#include "zathura/types.h"

#define LENGTH(x) (sizeof(x) / sizeof((x)[0]))

typedef struct page_offset_s {
  int x;
  int y;
} page_offset_t;

/**
 * This function checks if the file has a valid extension. A extension is
 * evaluated as valid if it matches a supported filetype.
 *
 * @param zathura Zathura object
 * @param path The path to the file
 * @return true if the extension is valid, otherwise false
 */
bool file_valid_extension(zathura_t* zathura, const char* path);

/**
 * Generates the document index based upon the list retrieved from the document
 * object.
 *
 * @param session The session
 * @param model The tree model
 * @param parent The tree iterator parent
 * @param tree The Tree iterator
 */
void document_index_build(girara_session_t* session, GtkTreeModel* model, GtkTreeIter* parent,
                          girara_tree_node_t* tree);

/**
 * Generates the file explorer based upon the list retrieved from the system call
 *
 * @param session The session
 * @param model The tree model
 * @param parent The tree iterator parent
 * @param tree The Tree iterator
 */
void file_explorer_build(girara_session_t* session, GtkTreeModel* model, GtkTreeIter* parent,
                          girara_tree_node_t* tree);

/**
 * A custom search equal function for the index tree view, so that
 * when interactively searching, the string will be recursively compared
 * to all the children of visible entries
 *
 * @param model The tree model
 * @param column The column of the entry
 * @param key The keyword to be compared
 * @param iter The tree iterator
 * @param search_data User data pointer
 */
gboolean search_equal_func_index(GtkTreeModel* model, gint column, const gchar* key, GtkTreeIter* iter,
                                 gpointer search_data);
/**
 * Scrolls the document index to the current page
 *
 * @param zathura The zathura instance
 */
void index_scroll_to_current_page(zathura_t* zathura);

/**
 * Calculates the new coordinates based on the rotation and scale level of the
 * document for the given rectangle
 *
 * @param page Page where the rectangle should be
 * @param rectangle The rectangle
 * @return New rectangle
 */
zathura_rectangle_t recalc_rectangle(zathura_page_t* page, zathura_rectangle_t rectangle);

/**
 * Returns the page widget of the page
 *
 * @param zathura The zathura instance
 * @param page The page object
 * @return The page widget of the page
 * @return NULL if an error occurred
 */
GtkWidget* zathura_page_get_widget(zathura_t* zathura, zathura_page_t* page);

/**
 * Set if the search results should be drawn or not
 *
 * @param zathura Zathura instance
 * @param value true if they should be drawn, otherwise false
 */
void document_draw_search_results(zathura_t* zathura, bool value);

/**
 * Create zathura version string
 *
 * @param plugin_manager The plugin manager
 * @param markup Enable markup
 * @return Version string
 */
char* zathura_get_version_string(const zathura_plugin_manager_t* plugin_manager, bool markup);

/**
 * Get a pointer to the GdkAtom of the current clipboard.
 *
 * @param zathura The zathura instance
 *
 * @return A pointer to a GdkAtom object correspoinding to the current
 * clipboard, or NULL.
 */
GdkAtom* get_selection(zathura_t* zathura);

/**
 * Returns the valid zoom value which needs to lie in the interval of zoom_min
 * and zoom_max specified in the girara session
 *
 * @param[in] session The session
 * @param[in] zoom The proposed zoom value
 *
 * @return The corrected zoom value
 */
double zathura_correct_zoom_value(girara_session_t* session, const double zoom);

/**
 * Write a list of 'pages per row to first column' values as a colon separated string.
 *
 * For valid settings list, this is the inverse of parse_first_page_column_list.
 *
 * @param[in] first_page_columns The settings vector
 * @param[in] size The size of the settings vector
 *
 * @return The new settings string
 */
char* write_first_page_column(unsigned int* first_page_columns, unsigned int size);

/**
 * Parse a 'pages per row to first column' settings list.
 *
 * For valid settings list, this is the inverse of write_first_page_column_list.
 *
 * @param[in] first_page_column_list The settings list
 * @param[in] size A cell to return the size of the result, mandatory
 *
 * @return The values from the settings list as a new vector
 */
unsigned int* parse_first_page_column(const char* first_page_column_list, unsigned int* size);

/**
 * Extracts the column the first page should be rendered in from the specified
 * list of settings corresponding to the specified pages per row
 *
 * @param[in] first_page_column_list The settings list
 * @param[in] pages_per_row The current pages per row
 *
 * @return The column the first page should be rendered in
 */
unsigned int find_first_page_column(const char* first_page_column_list, const unsigned int pages_per_row);

/**
 * Cycle the column the first page should be rendered in.
 *
 * @param[in] first_page_column_list The settings list
 * @param[in] pages_per_row The current pages per row
 * @param[in] incr The value added to the current first page column setting
 *
 * @return The new modified settings list
 */
char* increment_first_page_column(const char* first_page_column_list, const unsigned int pages_per_row, int incr);

/**
 * Parse color string and print warning if color cannot be parsed.
 *
 * @param[out] color The color
 * @param[in] str Color string
 *
 * @return True if color string can be parsed, false otherwise.
 */
bool parse_color(GdkRGBA* color, const char* str);

/**
 * Flatten list of overlapping rectangles.
 *
 * @param[in] rectangles A list of rectangles
 *
 * @return List of rectangles
 */
girara_list_t* flatten_rectangles(girara_list_t* rectangles);

girara_tree_node_t* zathura_explorer_generate(girara_session_t* session, zathura_error_t* error);

bool zathura_explorer_generate_r(zathura_t* zathura, zathura_error_t* error, girara_tree_node_t* root, char *path, int trace);

#endif // UTILS_H
