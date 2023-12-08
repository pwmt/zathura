/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_H
#define ZATHURA_H

#include <stdbool.h>
#include <girara/types.h>
#include <girara/session.h>
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gtk/gtkx.h>
#endif
#include "macros.h"
#include "types.h"
#include "jumplist.h"
#include "file-monitor.h"

enum {
  NEXT,
  PREVIOUS,
  LEFT,
  RIGHT,
  UP,
  DOWN,
  BOTTOM,
  TOP,
  HIDE,
  HIGHLIGHT,
  DELETE_LAST_WORD,
  DELETE_LAST_CHAR,
  DEFAULT,
  ERROR,
  WARNING,
  NEXT_GROUP,
  PREVIOUS_GROUP,
  ZOOM_IN,
  ZOOM_OUT,
  ZOOM_ORIGINAL,
  ZOOM_SPECIFIC,
  FORWARD,
  BACKWARD,
  CONTINUOUS,
  DELETE_LAST,
  EXPAND,
  EXPAND_ALL,
  COLLAPSE_ALL,
  COLLAPSE,
  TOGGLE,
  SELECT,
  GOTO_DEFAULT,
  GOTO_LABELS,
  GOTO_OFFSET,
  HALF_UP,
  HALF_DOWN,
  FULL_UP,
  FULL_DOWN,
  HALF_LEFT,
  HALF_RIGHT,
  FULL_LEFT,
  FULL_RIGHT,
  NEXT_CHAR,
  PREVIOUS_CHAR,
  DELETE_TO_LINE_START,
  APPEND_FILEPATH,
  ROTATE_CW,
  ROTATE_CCW,
  PAGE_BOTTOM,
  PAGE_TOP,
  BIDIRECTIONAL,
  ZOOM_SMOOTH
};

/* unspecified page number */
enum {
  ZATHURA_PAGE_NUMBER_UNSPECIFIED = INT_MIN
};

/* cache constants */
enum {
  ZATHURA_PAGE_CACHE_DEFAULT_SIZE = 15,
  ZATHURA_PAGE_CACHE_MAX_SIZE = 1024,
  ZATHURA_PAGE_THUMBNAIL_DEFAULT_SIZE = 4*1024*1024
};

typedef enum {
  ZATHURA_SANDBOX_NONE,
  ZATHURA_SANDBOX_NORMAL,
  ZATHURA_SANDBOX_STRICT
} zathura_sandbox_t;

/* forward declaration for types from database.h */
typedef struct _ZathuraDatabase zathura_database_t;
/* forward declaration for types from content-type.h */
typedef struct zathura_content_type_context_s zathura_content_type_context_t;

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

    struct {
      GdkRGBA highlight_color;        /**< Color for highlighting */
      GdkRGBA highlight_color_fg;     /**< Color for highlighting (foreground) */
      GdkRGBA highlight_color_active; /** Color for highlighting */
      GdkRGBA render_loading_bg;      /**< Background color for render "Loading..." */
      GdkRGBA render_loading_fg;      /**< Foreground color for render "Loading..." */
      GdkRGBA signature_success;      /**> Color for highlighing valid signatures */
      GdkRGBA signature_warning;      /**> Color for highlighing  signatures with warnings */
      GdkRGBA signature_error;        /**> Color for highlighing invalid signatures */
    } colors;

    GtkWidget *page_widget; /**< Widget that contains all rendered pages */
    GtkWidget *index; /**< Widget to show the index of the document */
  } ui;

  struct
  {
    ZathuraRenderer* render_thread; /**< The thread responsible for rendering the pages */
  } sync;

  struct
  {
    void* manager; /**< Plugin manager */
  } plugins;

  struct
  {
    gchar* config_dir; /**< Path to the configuration directory */
    gchar* data_dir; /**< Path to the data directory */
    gchar* cache_dir; /**< Path to the cache directory */
  } config;

  struct
  {
    GtkPrintSettings* settings; /**< Print settings */
    GtkPageSetup* page_setup; /**< Saved page setup */
  } print;

  struct
  {
    girara_list_t* marks; /**< Marker */
    char** arguments; /**> Arguments that were passed at startup */
    int search_direction; /**< Current search direction (FORWARD or BACKWARD) */
    GdkModifierType synctex_edit_modmask; /**< Modifier to trigger synctex edit */
    GdkModifierType highlighter_modmask; /**< Modifier to draw with a highlighter */
    zathura_sandbox_t sandbox; /**< Sandbox mode */
    bool double_click_follow; /**< Double/Single click to follow link */
  } global;

  struct
  {
    girara_mode_t normal; /**< Normal mode */
    girara_mode_t fullscreen; /**< Fullscreen mode */
    girara_mode_t index; /**< Index mode */
    girara_mode_t insert; /**< Insert mode */
    girara_mode_t presentation; /**< Presentation mode */
  } modes;

  struct
  {
    gchar* file; /**< bookmarks file */
    girara_list_t* bookmarks; /**< bookmarks */
  } bookmarks;

  zathura_jumplist_t jumplist;

  struct
  {
    guint refresh_view;
#ifdef G_OS_UNIX
    guint sigterm;
#endif

    gulong monitors_changed_handler; /**< Signal handler for monitors-changed */
  } signals;

  struct
  {
    gchar* file;
  } stdin_support;

  zathura_document_t* document; /**< The current document */
  zathura_document_t* predecessor_document; /**< The document from before a reload */
  GtkWidget** pages; /**< The page widgets */
  GtkWidget** predecessor_pages; /**< The page widgets from before a reload */
  zathura_database_t* database; /**< The database */
  ZathuraDbus* dbus; /**< D-Bus service */
  ZathuraRenderRequest* window_icon_render_request; /**< Render request for window icon */

  /**
   * File monitor
   */
  struct {
    ZathuraFileMonitor* monitor; /**< File monitor */
    gchar* password; /**< Save password */
  } file_monitor;

  /**
   * Bisect stage
   */
  struct {
    unsigned int last_jump; /**< Page jumped to by bisect */
    unsigned int start; /**< Bisection range - start */
    unsigned int end; /**< Bisection range - end */
  } bisect;

  /**
   * Storage for shortcuts.
   */
  struct {
    struct {
      int x;
      int y;
    } mouse;
    struct {
      int pages;
    } toggle_page_mode;
    struct {
      int pages;
      char* first_page_column_list;
      double zoom;
    } toggle_presentation_mode;
  } shortcut;

  /**
   * Storage for gestures.
   */
  struct {
    double initial_zoom;
  } gesture;

  /**
   * Context for MIME type detection
   */
  zathura_content_type_context_t* content_type_context;
};

/**
 * Creates a zathura session
 *
 * @return zathura session object or NULL if zathura could not be creeated
 */
zathura_t* zathura_create(void);

/**
 * Initializes zathura
 *
 * @param zathura The zathura session
 * @return true if initialization has been successful
 */
bool zathura_init(zathura_t* zathura);

/**
 * Free zathura session
 *
 * @param zathura The zathura session
 */
void zathura_free(zathura_t* zathura);

/**
 * Set parent window id. This does not have an effect if the underlying Gtk
 * backend is not X11.
 *
 * @param zathura The zathura session
 * @param xid The window id
 */
void zathura_set_xid(zathura_t* zathura, Window xid);

/**
 * Set the path to the configuration directory
 *
 * @param zathura The zathura session
 * @param dir Directory path
 */
void zathura_set_config_dir(zathura_t* zathura, const char* dir);

/**
 * Set the path to the data directory
 *
 * @param zathura The zathura session
 * @param dir Directory path
 */
void zathura_set_data_dir(zathura_t* zathura, const char* dir);

/**
 * Set the path to the cache directory.
 *
 * @param zathura The Zathura session
 * @param dir Directory path
 */
void zathura_set_cache_dir(zathura_t* zathura, const char* dir);

/**
 * Set the path to the plugin directory
 *
 * @param zathura The zathura session
 * @param dir Directory path
 */
void zathura_set_plugin_dir(zathura_t* zathura, const char* dir);

/**
 * Sets the program parameters
 *
 * @param zathura The zathura session
 * @param argv List of arguments
 */
void zathura_set_argv(zathura_t* zathura, char** argv);

/**
 * Calculate and store the monitor PPI for the view widget
 *
 * @param zathura The zathura session
 */
void zathura_update_view_ppi(zathura_t* zathura);

/**
 * Opens a file
 *
 * @param zathura The zathura session
 * @param path The path to the file
 * @param password The password of the file
 * @param page_number Open given page number
 *
 * @return If no error occurred true, otherwise false, is returned.
 */
bool document_open(zathura_t* zathura, const char* path, const char* uri, const char* password,
                   int page_number);

/**
 * Opens a file
 *
 * @param zathura The zathura session
 * @param path The path to the file
 * @param password The password of the file
 * @param synctex Open at the given SyncTeX string
 *
 * @return If no error occurred true, otherwise false, is returned.
 */
bool document_open_synctex(zathura_t* zathura, const char* path, const char* uri,
                           const char* password, const char* synctex);

/**
 * Opens a file (idle)
 *
 * @param zathura The zathura session
 * @param path The path to the file
 * @param password The password of the file
 * @param page_number Open given page number
 * @param mode Open in given page mode
 * @param synctex SyncTeX string
 */
void document_open_idle(zathura_t* zathura, const char* path,
                        const char* password, int page_number,
                        const char* mode, const char* synctex,
                        const char* bookmark_name, const char *search_string);

/**
 * Save a open file
 *
 * @param zathura The zathura session
 * @param path The path
 * @param overwrite Overwrite existing file
 *
 * @return If no error occurred true, otherwise false, is returned.
 */
bool document_save(zathura_t* zathura, const char* path, bool overwrite);

/**
 * Frees the "predecessor" buffers used for smooth-reload
 *
 * @param zathura The zathura session
 * @return If no error occurred true, otherwise false, is returned.
 */
bool document_predecessor_free(zathura_t* zathura);

/**
 * Closes the current opened document
 *
 * @param zathura The zathura session
 * @param keep_monitor Set to true if monitor should be kept (sc_reload)
 * @return If no error occurred true, otherwise false, is returned.
 */
bool document_close(zathura_t* zathura, bool keep_monitor);

/**
 * Opens the page with the given number
 *
 * @param zathura The zathura session
 * @param page_id The id of the page that should be set
 * @return If no error occurred true, otherwise false, is returned.
 */
bool page_set(zathura_t* zathura, unsigned int page_id);

/**
 * Moves to the given position
 *
 * @param zathura Zathura session
 * @param position_x X coordinate
 * @param position_y Y coordinate
 * @return If no error occurred true, otherwise false, is returned.
 */
bool position_set(zathura_t* zathura, double position_x, double position_y);

/**
 * Refresh the page view
 *
 * @param zathura Zathura session
 */
void refresh_view(zathura_t* zathura);

/**
 * Recompute the scale according to settings
 *
 * @param zathura Zathura session
 */
bool adjust_view(zathura_t* zathura);

/**
 * Builds the box structure to show the rendered pages
 *
 * @param zathura The zathura session
 * @param page_padding padding in pixels between pages
 * @param pages_per_row Number of shown pages per row
 * @param first_page_column Column on which first page start
 * @param page_right_to_left Render pages right to left
 */
void page_widget_set_mode(zathura_t* zathura, unsigned int page_padding,
                          unsigned int pages_per_row, unsigned int first_page_column,
                          bool page_right_to_left);

/**
 * Updates the page number in the statusbar. Note that 1 will be added to the
 * displayed number
 *
 * @param zathura The zathura session
 */
void statusbar_page_number_update(zathura_t* zathura);

/**
 * Gets the nicely formatted filename of the loaded document according to settings
 *
 * @param zathura The zathura session
 * @param statusbar Whether return value will be dispalyed in status bar
 *
 * return Printable filename. Free with g_free.
 */
char* get_formatted_filename(zathura_t* zathura, bool statusbar);

/**
 * Show additional signature information
 *
 * @param zathura The zathura session
 * @param show Whether to show the signature information
 */
void zathura_show_signature_information(zathura_t* zathura, bool show);

#endif // ZATHURA_H
