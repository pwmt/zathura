/* See LICENSE file for license and copyright information */

#ifndef ZATHURA_H
#define ZATHURA_H

#include <stdbool.h>
#include <girara.h>

enum { NEXT, PREVIOUS, LEFT, RIGHT, UP, DOWN, BOTTOM, TOP, HIDE, HIGHLIGHT,
  DELETE_LAST_WORD, DELETE_LAST_CHAR, DEFAULT, ERROR, WARNING, NEXT_GROUP,
  PREVIOUS_GROUP, ZOOM_IN, ZOOM_OUT, ZOOM_ORIGINAL, ZOOM_SPECIFIC, FORWARD,
  BACKWARD, ADJUST_BESTFIT, ADJUST_WIDTH, ADJUST_NONE, CONTINUOUS, DELETE_LAST,
  ADD_MARKER, EVAL_MARKER, EXPAND, COLLAPSE, SELECT, GOTO_DEFAULT, GOTO_LABELS,
  GOTO_OFFSET, HALF_UP, HALF_DOWN, FULL_UP, FULL_DOWN, NEXT_CHAR, PREVIOUS_CHAR,
  DELETE_TO_LINE_START, APPEND_FILEPATH };

/* define modes */
#define ALL        (1 << 0)
#define FULLSCREEN (1 << 1)
#define INDEX      (1 << 2)
#define NORMAL     (1 << 3)
#define INSERT     (1 << 4)

/* forward declaration for types from document.h */
struct zathura_document_s;
struct zathura_page_s;
typedef struct zathura_document_s zathura_document_t;
typedef struct zathura_page_s zathura_page_t;

/* forward declaration for types from render.h */
struct render_thread_s;
typedef struct render_thread_s render_thread_t;

typedef struct zathura_s
{
  struct
  {
    girara_session_t* session; /**> girara interface session */

    struct
    {
      girara_statusbar_item_t* buffer; /**> buffer statusbar entry */
      girara_statusbar_item_t* file; /**> file statusbar entry */
      girara_statusbar_item_t* page_number; /**> page number statusbar entry */
    } statusbar;

    GtkWidget *page_view; /**> Widget that contains all rendered pages */
    GtkWidget *index; /**> Widget to show the index of the document */
  } ui;

  struct
  {
    render_thread_t* render_thread; /**> The thread responsible for rendering the pages */
  } sync;

  struct
  {
    girara_list_t* plugins;
    girara_list_t* path;
  } plugins;

  struct
  {
    gchar* config_dir;
    gchar* data_dir;
  } config;

  struct
  {
    GtkPrintSettings* settings;
    GtkPageSetup* page_setup;
  } print;

  struct
  {
    unsigned int page_padding;
  } global;

  zathura_document_t* document; /**> The current document */
} zathura_t;

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
 * Closes the current opened document
 *
 * @param zathura The zathura session
 * @return If no error occured true, otherwise false, is returned.
 */
bool document_close(zathura_t* zathura);

/**
 * Opens the page with the given number
 *
 * @param zathura The zathura session
 * @return If no error occured true, otherwise false, is returned.
 */
bool page_set(zathura_t* zathura, unsigned int page_id);

/**
 * Builds the box structure to show the rendered pages
 *
 * @param zathura The zathura session
 * @param pages_per_row Number of shown pages per row
 */
void page_view_set_mode(zathura_t* zathura, unsigned int pages_per_row);

/**
 * Updates the page number in the statusbar. Note that 1 will be added to the
 * displayed number
 *
 * @param zathura The zathura session
 */
void statusbar_page_number_update(zathura_t* zathura);

#endif // ZATHURA_H
