/* See LICENSE file for license and copyright information */

#ifndef ZATHURA_H
#define ZATHURA_H

#include <stdbool.h>
#include <girara/types.h>
#include <gtk/gtk.h>
#include "macros.h"
#include "types.h"

enum { NEXT, PREVIOUS, LEFT, RIGHT, UP, DOWN, BOTTOM, TOP, HIDE, HIGHLIGHT,
  DELETE_LAST_WORD, DELETE_LAST_CHAR, DEFAULT, ERROR, WARNING, NEXT_GROUP,
  PREVIOUS_GROUP, ZOOM_IN, ZOOM_OUT, ZOOM_ORIGINAL, ZOOM_SPECIFIC, FORWARD,
  BACKWARD, CONTINUOUS, DELETE_LAST, EXPAND, EXPAND_ALL, COLLAPSE_ALL, COLLAPSE,
  SELECT, GOTO_DEFAULT, GOTO_LABELS, GOTO_OFFSET, HALF_UP, HALF_DOWN, FULL_UP,
  FULL_DOWN, HALF_LEFT, HALF_RIGHT, FULL_LEFT, FULL_RIGHT, NEXT_CHAR,
  PREVIOUS_CHAR, DELETE_TO_LINE_START, APPEND_FILEPATH, ROTATE_CW, ROTATE_CCW };

/* forward declaration for types form database.h */
typedef struct _ZathuraDatabase zathura_database_t;

/* forward declaration for types from render.h */
struct render_thread_s;
typedef struct render_thread_s render_thread_t;

struct zathura_s
{
  struct
  {
    girara_session_t* session; /**< girara interface session */

    struct
    {
      girara_statusbar_item_t* buffer; /**< buffer statusbar entry */
      girara_statusbar_item_t* file; /**< file statusbar entry */
      girara_statusbar_item_t* page_number; /**< page number statusbar entry */
    } statusbar;

    struct
    {
      GdkColor recolor_dark_color; /**< Dark color for recoloring */
      GdkColor recolor_light_color; /**< Light color for recoloring */
      GdkColor highlight_color; /**< Color for highlighting */
      GdkColor highlight_color_active; /** Color for highlighting */
    } colors;

    GtkWidget *page_widget_alignment;
    GtkWidget *page_widget; /**< Widget that contains all rendered pages */
    GtkWidget *index; /**< Widget to show the index of the document */
  } ui;

  struct
  {
    render_thread_t* render_thread; /**< The thread responsible for rendering the pages */
  } sync;

  struct
  {
    void* manager; /**< Plugin manager */
  } plugins;

  struct
  {
    gchar* config_dir; /**< Path to the configuration directory */
    gchar* data_dir; /**< Path to the data directory */
  } config;

  struct
  {
    bool enabled;
    gchar* editor;
  } synctex;

  struct
  {
    GtkPrintSettings* settings; /**< Print settings */
    GtkPageSetup* page_setup; /**< Saved page setup */
  } print;

  struct
  {
    bool recolor_keep_hue; /**< Keep hue when recoloring */
    bool recolor; /**< Recoloring mode switch */
    bool update_page_number; /**< Update current page number */
    girara_list_t* marks; /**< Marker */
    char** arguments; /**> Arguments that were passed at startup */
  } global;

  struct
  {
    girara_mode_t normal; /**< Normal mode */
    girara_mode_t fullscreen; /**< Fullscreen mode */
    girara_mode_t index; /**< Index mode */
    girara_mode_t insert; /**< Insert mode */
  } modes;

  struct
  {
    gchar* file; /**< bookmarks file */
    girara_list_t* bookmarks; /**< bookmarks */
  } bookmarks;

  struct
  {
    gchar* file;
  } stdin_support;

  zathura_document_t* document; /**< The current document */
  GtkWidget** pages; /**< The page widgets */
  zathura_database_t* database; /**< The database */

  /**
   * File monitor
   */
  struct {
    GFileMonitor* monitor; /**< File monitor */
    GFile* file; /**< File for file monitor */
    gchar* file_path; /**< Save file path */
    gchar* password; /**< Save password */
  } file_monitor;
};

/**
 * Initializes zathura
 *
 * @param argc Number of arguments
 * @param argv Values of arguments
 * @return zathura session object or NULL if zathura could not been initialized
 */
zathura_t* zathura_init(int argc, char* argv[]);

/**
 * Free zathura session
 *
 * @param zathura The zathura session
 */
void zathura_free(zathura_t* zathura);

/**
 * Opens a file
 *
 * @param zathura The zathura session
 * @param path The path to the file
 * @param password The password of the file
 *
 * @return If no error occured true, otherwise false, is returned.
 */
bool document_open(zathura_t* zathura, const char* path, const char* password);

/**
 * Save a open file
 *
 * @param zathura The zathura session
 * @param path The path
 * @param overwrite Overwrite existing file
 *
 * @return If no error occured true, otherwise false, is returned.
 */
bool document_save(zathura_t* zathura, const char* path, bool overwrite);

/**
 * Closes the current opened document
 *
 * @param zathura The zathura session
 * @param keep_monitor Set to true if monitor should be kept (sc_reload)
 * @return If no error occured true, otherwise false, is returned.
 */
bool document_close(zathura_t* zathura, bool keep_monitor);

/**
 * Opens the page with the given number
 *
 * @param zathura The zathura session
 * @param page_id The id of the page that should be set
 * @return If no error occured true, otherwise false, is returned.
 */
bool page_set(zathura_t* zathura, unsigned int page_id);

/**
 * Opens the page with the given number (delayed)
 *
 * @param zathura The zathura session
 * @param page_id The id of the page that should be set
 * @return If no error occured true, otherwise false, is returned.
 */
bool page_set_delayed(zathura_t* zathura, unsigned int page_id);

/**
 * Moves to the given position
 *
 * @param zathura Zathura session
 * @param position_x X coordinate
 * @param position_y Y coordinate
 * @return If no error occured true, otherwise false, is returned.
 */
bool position_set_delayed(zathura_t* zathura, double position_x, double position_y);

/**
 * Builds the box structure to show the rendered pages
 *
 * @param zathura The zathura session
 * @param pages_per_row Number of shown pages per row
 * @param first_page_column Column on which first page start
 */
void page_widget_set_mode(zathura_t* zathura, unsigned int pages_per_row, unsigned int first_page_column);

/**
 * Updates the page number in the statusbar. Note that 1 will be added to the
 * displayed number
 *
 * @param zathura The zathura session
 */
void statusbar_page_number_update(zathura_t* zathura);

#endif // ZATHURA_H
