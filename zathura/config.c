/* SPDX-License-Identifier: Zlib */

#include "config.h"
#include "commands.h"
#include "completion.h"
#include "callbacks.h"
#include "shortcuts.h"
#include "zathura.h"
#include "render.h"
#include "marks.h"
#include "utils.h"

#include <girara/settings.h>
#include <girara/session.h>
#include <girara/shortcuts.h>
#include <girara/config.h>
#include <girara/commands.h>
#include <girara/utils.h>
#include <glib/gi18n.h>

#define GLOBAL_RC  "/etc/zathurarc"
#define ZATHURA_RC "zathurarc"

static void
cb_jumplist_change(girara_session_t* session, const char* UNUSED(name),
                   girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  const int* ivalue = value;

  if (*ivalue < 0) {
    zathura->jumplist.max_size = 0;
  } else {
    zathura->jumplist.max_size = *ivalue;
  }

  if (zathura->jumplist.list != NULL && zathura->jumplist.size != 0) {
    zathura_jumplist_trim(zathura);
  }
}

static void
cb_color_change(girara_session_t* session, const char* name,
                girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  g_return_if_fail(name != NULL);
  zathura_t* zathura = session->global.data;

  const char* string_value = (const char*) value;
  if (g_strcmp0(name, "highlight-color") == 0) {
    parse_color(&zathura->ui.colors.highlight_color, string_value);
  } else if (g_strcmp0(name, "highlight-fg") == 0) {
    parse_color(&zathura->ui.colors.highlight_color_fg, string_value);
  } else if (g_strcmp0(name, "highlight-active-color") == 0) {
    parse_color(&zathura->ui.colors.highlight_color_active, string_value);
  } else if (g_strcmp0(name, "recolor-darkcolor") == 0) {
    if (zathura->sync.render_thread != NULL) {
      zathura_renderer_set_recolor_colors_str(zathura->sync.render_thread, NULL, string_value);
    }
  } else if (g_strcmp0(name, "recolor-lightcolor") == 0) {
    if (zathura->sync.render_thread != NULL) {
      zathura_renderer_set_recolor_colors_str(zathura->sync.render_thread, string_value, NULL);
    }
  } else if (g_strcmp0(name, "render-loading-bg") == 0) {
    parse_color(&zathura->ui.colors.render_loading_bg, string_value);
  } else if (g_strcmp0(name, "render-loading-fg") == 0) {
    parse_color(&zathura->ui.colors.render_loading_fg, string_value);
  } else if (g_strcmp0(name, "signature-success-color") == 0) {
    parse_color(&zathura->ui.colors.signature_success, string_value);
  } else if (g_strcmp0(name, "signature-warning-color") == 0) {
    parse_color(&zathura->ui.colors.signature_warning, string_value);
  } else if (g_strcmp0(name, "signature-error-color") == 0) {
    parse_color(&zathura->ui.colors.signature_error, string_value);
  }

  render_all(zathura);
}

static void
cb_nohlsearch_changed(girara_session_t* session, const char* UNUSED(name),
                      girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  const bool* bvalue = value;
  document_draw_search_results(zathura, !*bvalue);
  render_all(zathura);
}

static void
cb_doubleclick_changed(girara_session_t* session, const char* UNUSED(name),
                      girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  zathura->global.double_click_follow = *(const bool*) value;
}

static void
cb_global_modifiers_changed(girara_session_t* session, const char* name,
                            girara_setting_type_t UNUSED(type), const void* value,
                            void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  GdkModifierType* p;
  if (g_strcmp0(name, "synctex-edit-modifier") == 0) {
    p = &(zathura->global.synctex_edit_modmask);
  } else if (g_strcmp0(name, "highlighter-modifier") == 0) {
    p = &(zathura->global.highlighter_modmask);
  } else {
    girara_error("unreachable");
    return;
  }

  const char* modifier = value;
  if (g_strcmp0(modifier, "shift") == 0) {
    *p = GDK_SHIFT_MASK;
  } else if (g_strcmp0(modifier, "ctrl") == 0)  {
    *p = GDK_CONTROL_MASK;
  } else if (g_strcmp0(modifier, "alt") == 0) {
    *p = GDK_MOD1_MASK;
  } else {
    girara_error("Invalid %s option: '%s'", name, modifier);
  }
}

static void
cb_incsearch_changed(girara_session_t* session, const char* UNUSED(name),
                     girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);

  bool inc_search = *(const bool*) value;
  girara_special_command_add(session, '/', cmd_search, inc_search, FORWARD,  NULL);
  girara_special_command_add(session, '?', cmd_search, inc_search, BACKWARD, NULL);
}

static void
cb_sandbox_changed(girara_session_t* session, const char* name,
                   girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  const char* sandbox = value;
  if (g_strcmp0(sandbox, "none") == 0) {
    zathura->global.sandbox = ZATHURA_SANDBOX_NONE;
  } else if (g_strcmp0(sandbox, "normal") == 0)  {
    zathura->global.sandbox = ZATHURA_SANDBOX_NORMAL;
  } else if (g_strcmp0(sandbox, "strict") == 0) {
    zathura->global.sandbox = ZATHURA_SANDBOX_STRICT;
  } else {
    girara_error("Invalid %s option: '%s'", name, sandbox);
  }
}

static void
cb_window_statbusbar_changed(girara_session_t* session, const char* name,
                             girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data))
{
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  const bool is_window_setting = g_str_has_prefix(name, "window-");
  if (is_window_setting) {
    char* formatted_filename = get_formatted_filename(zathura, !is_window_setting);
    girara_set_window_title(zathura->ui.session, formatted_filename);
    g_free(formatted_filename);
  } else {
    statusbar_page_number_update(zathura);
  }
}

static void cb_show_signature_info(girara_session_t* session, const char* UNUSED(name),
                                   girara_setting_type_t UNUSED(type), const void* value, void* UNUSED(data)) {
  g_return_if_fail(value != NULL);
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);
  zathura_t* zathura = session->global.data;

  if (zathura->document == NULL) {
    return;
  }

  zathura_show_signature_information(zathura, *(const bool*)value);
  update_visible_pages(zathura);
}

void config_load_default(zathura_t* zathura) {
  if (zathura == NULL || zathura->ui.session == NULL) {
    return;
  }

  int int_value              = 0;
  float float_value          = 0;
  bool bool_value            = false;
  girara_session_t* gsession = zathura->ui.session;

  /* mode settings */
  zathura->modes.normal       = gsession->modes.normal;
  zathura->modes.fullscreen   = girara_mode_add(gsession, "fullscreen");
  zathura->modes.index        = girara_mode_add(gsession, "index");
  zathura->modes.insert       = girara_mode_add(gsession, "insert");
  zathura->modes.presentation = girara_mode_add(gsession, "presentation");

#define NORMAL zathura->modes.normal
#define INSERT zathura->modes.insert
#define INDEX zathura->modes.index
#define FULLSCREEN zathura->modes.fullscreen
#define PRESENTATION zathura->modes.presentation

  const girara_mode_t all_modes[] = {
    NORMAL,
    INSERT,
    INDEX,
    FULLSCREEN,
    PRESENTATION
  };

  /* Set default mode */
  girara_mode_set(gsession, zathura->modes.normal);

  /* zathura settings */
  girara_setting_add(gsession, "database",              "plain",      STRING, true,  _("Database backend"),         NULL, NULL);
  girara_setting_add(gsession, "filemonitor",           "glib",       STRING, true,  _("File monitor backend"),     NULL, NULL);
  int_value = 10;
  girara_setting_add(gsession, "zoom-step",             &int_value,   INT,    false, _("Zoom step"),                NULL, NULL);
  int_value = 1;
  girara_setting_add(gsession, "page-padding",          &int_value,   INT,    false, _("Padding between pages"),    cb_page_layout_value_changed, NULL);
  int_value = 1;
  girara_setting_add(gsession, "pages-per-row",         &int_value,   INT,    false, _("Number of pages per row"),  cb_page_layout_value_changed, NULL);
  int_value = 1;
  girara_setting_add(gsession, "first-page-column",     "1:2",        STRING, false, _("Column of the first page"), cb_page_layout_value_changed, NULL);
  bool_value = false;
  girara_setting_add(gsession, "page-right-to-left",    &bool_value,  BOOLEAN, false, _("Render pages from right to left"),  cb_page_layout_value_changed, NULL);
  float_value = 40;
  girara_setting_add(gsession, "scroll-step",           &float_value, FLOAT,  false, _("Scroll step"),              NULL, NULL);
  float_value = 40;
  girara_setting_add(gsession, "scroll-hstep",          &float_value, FLOAT,  false, _("Horizontal scroll step"),   NULL, NULL);
  float_value = 0.0;
  girara_setting_add(gsession, "scroll-full-overlap",   &float_value, FLOAT,  false, _("Full page scroll overlap"), NULL, NULL);
  int_value = 10;
  girara_setting_add(gsession, "zoom-min",              &int_value,   INT,    false, _("Zoom minimum"), NULL, NULL);
  int_value = 1000;
  girara_setting_add(gsession, "zoom-max",              &int_value,   INT,    false, _("Zoom maximum"), NULL, NULL);
  int_value = ZATHURA_PAGE_CACHE_DEFAULT_SIZE;
  girara_setting_add(gsession, "page-cache-size",       &int_value,   INT,    true,  _("Maximum number of pages to keep in the cache"), NULL, NULL);
  int_value = ZATHURA_PAGE_THUMBNAIL_DEFAULT_SIZE;
  girara_setting_add(gsession, "page-thumbnail-size",   &int_value,   INT,    true,  _("Maximum size in pixels of thumbnails to keep in the cache"), NULL, NULL);
  int_value = 2000;
  girara_setting_add(gsession, "jumplist-size",         &int_value,   INT,    false, _("Number of positions to remember in the jumplist"), cb_jumplist_change, NULL);

  girara_setting_add(gsession, "recolor-darkcolor",      "#FFFFFF", STRING, false, _("Recoloring (dark color)"),            cb_color_change, NULL);
  girara_setting_add(gsession, "recolor-lightcolor",     "#000000", STRING, false, _("Recoloring (light color)"),           cb_color_change, NULL);
  girara_setting_add(gsession, "highlight-color",        NULL,      STRING, false, _("Color for highlighting"),             cb_color_change, NULL);
  girara_setting_set(gsession, "highlight-color",        "#9FBC00");
  girara_setting_add(gsession, "highlight-fg",           NULL,      STRING, false, _("Foreground color for highlighting"),  cb_color_change, NULL);
  girara_setting_set(gsession, "highlight-fg",           "#000000");
  girara_setting_add(gsession, "highlight-active-color", NULL,      STRING, false, _("Color for highlighting (active)"),    cb_color_change, NULL);
  girara_setting_set(gsession, "highlight-active-color", "#00BC00");
  girara_setting_add(gsession, "render-loading-bg",      NULL,      STRING, false, _("'Loading ...' background color"),     cb_color_change, NULL);
  girara_setting_set(gsession, "render-loading-bg",      "#FFFFFF");
  girara_setting_add(gsession, "render-loading-fg",      NULL,      STRING, false, _("'Loading ...' foreground color"),     cb_color_change, NULL);
  girara_setting_set(gsession, "render-loading-fg",      "#000000");

  girara_setting_add(gsession, "index-fg",        "#DDDDDD", STRING, true, _("Index mode foreground color"), NULL, NULL);
  girara_setting_add(gsession, "index-bg",        "#232323", STRING, true, _("Index mode background color"), NULL, NULL);
  girara_setting_add(gsession, "index-active-fg", "#232323", STRING, true, _("Index mode foreground color (active element)"), NULL, NULL);
  girara_setting_add(gsession, "index-active-bg", "#9FBC00", STRING, true, _("Index mode background color (active element)"), NULL, NULL);
  girara_setting_add(gsession, "signature-success-color", NULL, STRING, false,
                     _("Color used to highlight valid signatures"), cb_color_change, NULL);
  girara_setting_set(gsession, "signature-success-color", "rgba(18%,80%,33%,0.9)");
  girara_setting_add(gsession, "signature-warning-color", NULL, STRING, false,
                     _("Color used to highlight signatures with warnings"), cb_color_change, NULL);
  girara_setting_set(gsession, "signature-warning-color", "rgba(100%,84%,0%,0.9)");
  girara_setting_add(gsession, "signature-error-color", NULL, STRING, false,
                     _("Color used to highlight invalid signatures"), cb_color_change, NULL);
  girara_setting_set(gsession, "signature-error-color", "rgba(92%,11%,14%,0.9)");

  bool_value = false;
  girara_setting_add(gsession, "recolor",                &bool_value,  BOOLEAN, false, _("Recolor pages"), cb_setting_recolor_change, NULL);
  bool_value = false;
  girara_setting_add(gsession, "recolor-keephue",        &bool_value,  BOOLEAN, false, _("When recoloring keep original hue and adjust lightness only"), cb_setting_recolor_keep_hue_change, NULL);
  bool_value = false;
  girara_setting_add(gsession, "recolor-reverse-video",  &bool_value,  BOOLEAN, false, _("When recoloring keep original image colors"), cb_setting_recolor_keep_reverse_video_change, NULL);
  bool_value = false;
  girara_setting_add(gsession, "scroll-wrap",            &bool_value,  BOOLEAN, false, _("Wrap scrolling"), NULL, NULL);
  bool_value = false;
  girara_setting_add(gsession, "scroll-page-aware",      &bool_value,  BOOLEAN, false, _("Page aware scrolling"), NULL, NULL);
  bool_value = true;
  girara_setting_add(gsession, "advance-pages-per-row",  &bool_value,  BOOLEAN, false, _("Advance number of pages per row"), NULL, NULL);
  bool_value = false;
  girara_setting_add(gsession, "zoom-center",            &bool_value,  BOOLEAN, false, _("Horizontally centered zoom"), NULL, NULL);
  bool_value = false;
  girara_setting_add(gsession, "vertical-center",        &bool_value,  BOOLEAN, false, _("Vertically center pages"), NULL, NULL);
  bool_value = true;
  girara_setting_add(gsession, "link-hadjust",           &bool_value,  BOOLEAN, false, _("Align link target to the left"), NULL, NULL);
  bool_value = true;
  girara_setting_add(gsession, "link-zoom",              &bool_value,  BOOLEAN, false, _("Let zoom be changed when following links"), NULL, NULL);
  bool_value = true;
  girara_setting_add(gsession, "search-hadjust",         &bool_value,  BOOLEAN, false, _("Center result horizontally"), NULL, NULL);
  float_value = 0.5;
  girara_setting_add(gsession, "highlight-transparency", &float_value, FLOAT,   false, _("Transparency for highlighting"), NULL, NULL);
  bool_value = true;
  girara_setting_add(gsession, "render-loading",         &bool_value,  BOOLEAN, false, _("Render 'Loading ...'"), NULL, NULL);
  bool_value = true;
  girara_setting_add(gsession, "smooth-reload",          &bool_value,  BOOLEAN, false, _("Smooth over flicker when reloading file"), NULL, NULL);
  girara_setting_add(gsession, "adjust-open",            "best-fit",   STRING,  false, _("Adjust to when opening file"), NULL, NULL);
  bool_value = false;
  girara_setting_add(gsession, "show-hidden",            &bool_value,  BOOLEAN, false, _("Show hidden files and directories"), NULL, NULL);
  bool_value = true;
  girara_setting_add(gsession, "show-directories",       &bool_value,  BOOLEAN, false, _("Show directories"), NULL, NULL);
  int_value = 10;
  girara_setting_add(gsession, "show-recent",            &int_value,   INT,     false, _("Show recent files"), NULL, NULL);
  bool_value = false;
  girara_setting_add(gsession, "open-first-page",        &bool_value,  BOOLEAN, false, _("Always open on first page"), NULL, NULL);
  bool_value = false;
  girara_setting_add(gsession, "nohlsearch",             &bool_value,  BOOLEAN, false, _("Highlight search results"), cb_nohlsearch_changed, NULL);
  bool_value = true;
  girara_setting_add(gsession, "double-click-follow",    &bool_value,  BOOLEAN, false, _("Double click to follow links"), cb_doubleclick_changed, NULL);
#define INCREMENTAL_SEARCH false
  bool_value = INCREMENTAL_SEARCH;
  girara_setting_add(gsession, "incremental-search",     &bool_value,  BOOLEAN, false, _("Enable incremental search"), cb_incsearch_changed, NULL);
  bool_value = true;
  girara_setting_add(gsession, "abort-clear-search",     &bool_value,  BOOLEAN, false, _("Clear search results on abort"), NULL, NULL);
  bool_value = false;
  girara_setting_add(gsession, "window-title-basename",  &bool_value,  BOOLEAN, false, _("Use basename of the file in the window title"), cb_window_statbusbar_changed, NULL);
  bool_value = false;
  girara_setting_add(gsession, "window-title-home-tilde",  &bool_value,  BOOLEAN, false, _("Use ~ instead of $HOME in the filename in the window title"), cb_window_statbusbar_changed, NULL);
  bool_value = false;
  girara_setting_add(gsession, "window-title-page",      &bool_value,  BOOLEAN, false, _("Display the page number in the window title"), cb_window_statbusbar_changed, NULL);
  bool_value = false;
  girara_setting_add(gsession, "window-icon-document",   &bool_value,  BOOLEAN, false, _("Use first page of a document as window icon"), NULL, NULL);
  bool_value = false;
  girara_setting_add(gsession, "statusbar-basename",     &bool_value,  BOOLEAN, false, _("Use basename of the file in the statusbar"), cb_window_statbusbar_changed, NULL);
  bool_value = false;
  girara_setting_add(gsession, "statusbar-home-tilde",  &bool_value,   BOOLEAN, false, _("Use ~ instead of $HOME in the filename in the statusbar"), cb_window_statbusbar_changed, NULL);
  bool_value = false;
  girara_setting_add(gsession, "statusbar-page-percent",  &bool_value, BOOLEAN, false, _("Display (current page / total pages) as a percent in the statusbar"), cb_window_statbusbar_changed, NULL);
  bool_value = true;
  girara_setting_add(gsession, "synctex",                &bool_value,  BOOLEAN, false, _("Enable synctex support"), NULL, NULL);
  girara_setting_add(gsession, "synctex-editor-command", "",           STRING,  false, _("Synctex editor command"), NULL, NULL);
  girara_setting_add(gsession, "synctex-edit-modifier",  "ctrl",       STRING,  false, _("Synctex edit modifier"), cb_global_modifiers_changed, NULL);
  girara_setting_add(gsession, "highlighter-modifier",   "shift",      STRING,  false, _("Highlighter modifier"), cb_global_modifiers_changed, NULL);
  bool_value = true;
  girara_setting_add(gsession, "dbus-service",           &bool_value,  BOOLEAN, false, _("Enable D-Bus service"), NULL, NULL);
  girara_setting_add(gsession, "dbus-raise-window",      &bool_value,  BOOLEAN, false, _("Raise window on certain D-Bus commands"), NULL, NULL);
  bool_value = false;
  girara_setting_add(gsession, "continuous-hist-save",   &bool_value,  BOOLEAN, false, _("Save history at each page change"), NULL, NULL);
  girara_setting_add(gsession, "selection-clipboard",    "primary",    STRING,  false, _("The clipboard into which mouse-selected data will be written"), NULL, NULL);
  bool_value = true;
  girara_setting_add(gsession, "selection-notification", &bool_value,  BOOLEAN, false, _("Enable notification after selecting text"), NULL, NULL);
  /* default to no sandbox when running in WSL */
  const char* string_value = running_under_wsl() ? "none" : "normal";
  girara_setting_add(gsession, "sandbox",                string_value, STRING, true,   _("Sandbox level"), cb_sandbox_changed, NULL);
  bool_value = false;
  girara_setting_add(gsession, "show-signature-information", &bool_value, BOOLEAN, false,
                     _("Disable additional information for signatures embedded in the document."),
                     cb_show_signature_info, NULL);

#define DEFAULT_SHORTCUTS(mode)                                                                                        \
  girara_shortcut_add(gsession, 0, GDK_KEY_a, NULL, sc_adjust_window, (mode), ZATHURA_ADJUST_BESTFIT, NULL);           \
  girara_shortcut_add(gsession, 0, GDK_KEY_s, NULL, sc_adjust_window, (mode), ZATHURA_ADJUST_WIDTH, NULL);             \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_F, NULL, sc_display_link, (mode), 0, NULL);                                 \
  girara_shortcut_add(gsession, 0, GDK_KEY_c, NULL, sc_copy_link, (mode), 0, NULL);                                    \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_slash, NULL, sc_focus_inputbar, (mode), 0, &("/"));                         \
  girara_shortcut_add(gsession, GDK_SHIFT_MASK, GDK_KEY_slash, NULL, sc_focus_inputbar, (mode), 0, &("/"));            \
  girara_shortcut_add(gsession, 0, GDK_KEY_question, NULL, sc_focus_inputbar, (mode), 0, &("?"));                      \
  girara_shortcut_add(gsession, 0, GDK_KEY_colon, NULL, sc_focus_inputbar, (mode), 0, &(":"));                         \
  girara_shortcut_add(gsession, 0, GDK_KEY_o, NULL, sc_focus_inputbar, (mode), 0, &(":open "));                        \
  girara_shortcut_add(gsession, 0, GDK_KEY_O, NULL, sc_focus_inputbar, (mode), APPEND_FILEPATH, &(":open "));          \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_f, NULL, sc_follow, (mode), 0, NULL);                                       \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, 0, "gg", sc_goto, (mode), TOP, NULL);                                               \
  girara_shortcut_add(gsession, 0, 0, "G", sc_goto, (mode), BOTTOM, NULL);                                             \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_m, NULL, sc_mark_add, (mode), 0, NULL);                                     \
  girara_shortcut_add(gsession, 0, GDK_KEY_apostrophe, NULL, sc_mark_evaluate, (mode), 0, NULL);                       \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_J, NULL, sc_navigate, (mode), NEXT, NULL);                                  \
  girara_shortcut_add(gsession, 0, GDK_KEY_K, NULL, sc_navigate, (mode), PREVIOUS, NULL);                              \
  girara_shortcut_add(gsession, GDK_MOD1_MASK, GDK_KEY_Right, NULL, sc_navigate, (mode), NEXT, NULL);                  \
  girara_shortcut_add(gsession, GDK_MOD1_MASK, GDK_KEY_Left, NULL, sc_navigate, (mode), PREVIOUS, NULL);               \
  girara_shortcut_add(gsession, 0, GDK_KEY_Page_Down, NULL, sc_navigate, (mode), NEXT, NULL);                          \
  girara_shortcut_add(gsession, 0, GDK_KEY_Page_Up, NULL, sc_navigate, (mode), PREVIOUS, NULL);                        \
                                                                                                                       \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_p, NULL, sc_print, (mode), 0, NULL);                         \
                                                                                                                       \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_r, NULL, sc_recolor, (mode), 0, NULL);                       \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_R, NULL, sc_reload, (mode), 0, NULL);                                       \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_r, NULL, sc_rotate, (mode), ROTATE_CW, NULL);                               \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_h, NULL, sc_scroll, (mode), LEFT, NULL);                                    \
  girara_shortcut_add(gsession, 0, GDK_KEY_j, NULL, sc_scroll, (mode), DOWN, NULL);                                    \
  girara_shortcut_add(gsession, 0, GDK_KEY_k, NULL, sc_scroll, (mode), UP, NULL);                                      \
  girara_shortcut_add(gsession, 0, GDK_KEY_l, NULL, sc_scroll, (mode), RIGHT, NULL);                                   \
  girara_shortcut_add(gsession, 0, GDK_KEY_Left, NULL, sc_scroll, (mode), LEFT, NULL);                                 \
  girara_shortcut_add(gsession, 0, GDK_KEY_Up, NULL, sc_scroll, (mode), UP, NULL);                                     \
  girara_shortcut_add(gsession, 0, GDK_KEY_Down, NULL, sc_scroll, (mode), DOWN, NULL);                                 \
  girara_shortcut_add(gsession, 0, GDK_KEY_H, NULL, sc_scroll, (mode), PAGE_TOP, NULL);                                \
  girara_shortcut_add(gsession, 0, GDK_KEY_L, NULL, sc_scroll, (mode), PAGE_BOTTOM, NULL);                             \
  girara_shortcut_add(gsession, 0, GDK_KEY_Right, NULL, sc_scroll, (mode), RIGHT, NULL);                               \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_t, NULL, sc_scroll, (mode), HALF_LEFT, NULL);                \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_d, NULL, sc_scroll, (mode), HALF_DOWN, NULL);                \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_u, NULL, sc_scroll, (mode), HALF_UP, NULL);                  \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_y, NULL, sc_scroll, (mode), HALF_RIGHT, NULL);               \
  girara_shortcut_add(gsession, 0, GDK_KEY_t, NULL, sc_scroll, (mode), FULL_LEFT, NULL);                               \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_f, NULL, sc_scroll, (mode), FULL_DOWN, NULL);                \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_b, NULL, sc_scroll, (mode), FULL_UP, NULL);                  \
  girara_shortcut_add(gsession, 0, GDK_KEY_y, NULL, sc_scroll, (mode), FULL_RIGHT, NULL);                              \
  girara_shortcut_add(gsession, 0, GDK_KEY_space, NULL, sc_scroll, (mode), FULL_DOWN, NULL);                           \
  girara_shortcut_add(gsession, GDK_SHIFT_MASK, GDK_KEY_space, NULL, sc_scroll, (mode), FULL_UP, NULL);                \
  girara_shortcut_add(gsession, 0, GDK_KEY_Y, NULL, sc_copy_filepath, (mode), 0, NULL);                                \
                                                                                                                       \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_o, NULL, sc_jumplist, (mode), BACKWARD, NULL);               \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_i, NULL, sc_jumplist, (mode), FORWARD, NULL);                \
                                                                                                                       \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_j, NULL, sc_bisect, (mode), FORWARD, NULL);                  \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_k, NULL, sc_bisect, (mode), BACKWARD, NULL);                 \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_n, NULL, sc_search, (mode), FORWARD, NULL);                                 \
  girara_shortcut_add(gsession, 0, GDK_KEY_N, NULL, sc_search, (mode), BACKWARD, NULL);                                \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_P, NULL, sc_snap_to_page, (mode), 0, NULL);                                 \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_Tab, NULL, sc_toggle_index, (mode), 0, NULL);                               \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_n, NULL, girara_sc_toggle_statusbar, (mode), 0, NULL);       \
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_m, NULL, girara_sc_toggle_inputbar, (mode), 0, NULL);        \
  girara_shortcut_add(gsession, 0, GDK_KEY_d, NULL, sc_toggle_page_mode, (mode), 0, NULL);                             \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_q, NULL, sc_quit, (mode), 0, NULL);                                         \
                                                                                                                       \
  girara_shortcut_add(gsession, 0, GDK_KEY_plus, NULL, sc_zoom, (mode), ZOOM_IN, NULL);                                \
  girara_shortcut_add(gsession, 0, GDK_KEY_KP_Add, NULL, sc_zoom, (mode), ZOOM_IN, NULL);                              \
  girara_shortcut_add(gsession, 0, GDK_KEY_minus, NULL, sc_zoom, (mode), ZOOM_OUT, NULL);                              \
  girara_shortcut_add(gsession, 0, GDK_KEY_KP_Subtract, NULL, sc_zoom, (mode), ZOOM_OUT, NULL);                        \
  girara_shortcut_add(gsession, 0, GDK_KEY_equal, NULL, sc_zoom, (mode), ZOOM_SPECIFIC, NULL);                         \
  girara_shortcut_add(gsession, 0, 0, "zi", sc_zoom, (mode), ZOOM_IN, NULL);                                           \
  girara_shortcut_add(gsession, 0, 0, "zI", sc_zoom, (mode), ZOOM_IN, NULL);                                           \
  girara_shortcut_add(gsession, 0, 0, "zo", sc_zoom, (mode), ZOOM_OUT, NULL);                                          \
  girara_shortcut_add(gsession, 0, 0, "zO", sc_zoom, (mode), ZOOM_OUT, NULL);                                          \
  girara_shortcut_add(gsession, 0, 0, "z0", sc_zoom, (mode), ZOOM_ORIGINAL, NULL);                                     \
  girara_shortcut_add(gsession, 0, 0, "zz", sc_zoom, (mode), ZOOM_SPECIFIC, NULL);                                     \
  girara_shortcut_add(gsession, 0, 0, "zZ", sc_zoom, (mode), ZOOM_SPECIFIC, NULL);

#define DEFAULT_MOUSE_EVENTS(mode)                                                                                     \
  girara_mouse_event_add(gsession, 0, 0, sc_mouse_scroll, (mode), GIRARA_EVENT_SCROLL_UP, UP, NULL);                   \
  girara_mouse_event_add(gsession, 0, 0, sc_mouse_scroll, (mode), GIRARA_EVENT_SCROLL_DOWN, DOWN, NULL);               \
  girara_mouse_event_add(gsession, 0, 0, sc_mouse_scroll, (mode), GIRARA_EVENT_SCROLL_LEFT, LEFT, NULL);               \
  girara_mouse_event_add(gsession, 0, 0, sc_mouse_scroll, (mode), GIRARA_EVENT_SCROLL_RIGHT, RIGHT, NULL);             \
  girara_mouse_event_add(gsession, 0, 0, sc_mouse_scroll, (mode), GIRARA_EVENT_SCROLL_BIDIRECTIONAL, BIDIRECTIONAL,    \
                         NULL);                                                                                        \
                                                                                                                       \
  girara_mouse_event_add(gsession, GDK_SHIFT_MASK, 0, sc_mouse_scroll, (mode), GIRARA_EVENT_SCROLL_UP, LEFT, NULL);    \
  girara_mouse_event_add(gsession, GDK_SHIFT_MASK, 0, sc_mouse_scroll, (mode), GIRARA_EVENT_SCROLL_DOWN, RIGHT, NULL); \
                                                                                                                       \
  girara_mouse_event_add(gsession, GDK_CONTROL_MASK, 0, sc_mouse_zoom, (mode), GIRARA_EVENT_SCROLL_UP, UP, NULL);      \
  girara_mouse_event_add(gsession, GDK_CONTROL_MASK, 0, sc_mouse_zoom, (mode), GIRARA_EVENT_SCROLL_DOWN, DOWN, NULL);  \
  girara_mouse_event_add(gsession, GDK_CONTROL_MASK, 0, sc_mouse_zoom, (mode), GIRARA_EVENT_SCROLL_BIDIRECTIONAL,      \
                         BIDIRECTIONAL, NULL);                                                                         \
  girara_mouse_event_add(gsession, 0, GIRARA_MOUSE_BUTTON2, sc_mouse_scroll, (mode), GIRARA_EVENT_BUTTON_PRESS, 0,     \
                         NULL);                                                                                        \
  girara_mouse_event_add(gsession, GDK_BUTTON2_MASK, GIRARA_MOUSE_BUTTON2, sc_mouse_scroll, (mode),                    \
                         GIRARA_EVENT_BUTTON_RELEASE, 0, NULL);                                                        \
  girara_mouse_event_add(gsession, GDK_BUTTON2_MASK, 0, sc_mouse_scroll, (mode), GIRARA_EVENT_MOTION_NOTIFY, 0, NULL);

  /* Define mode-less shortcuts
   * girara adds them only for normal mode, so passing 0 as mode is currently
   * not enough. We need to add/override for every mode. */
  for (size_t idx = 0; idx != LENGTH(all_modes); ++idx) {
    girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_c,      NULL, sc_abort, all_modes[idx], 0, NULL);
    girara_shortcut_add(gsession, 0,                GDK_KEY_Escape, NULL, sc_abort, all_modes[idx], 0, NULL);
  }

  /* Normal mode */
  girara_shortcut_add(gsession, 0, GDK_KEY_F5,  NULL, sc_toggle_presentation, NORMAL, 0, NULL);
  girara_shortcut_add(gsession, 0, GDK_KEY_F11, NULL, sc_toggle_fullscreen,   NORMAL, 0, NULL);

  DEFAULT_SHORTCUTS(NORMAL)

  /* Normal mode - Mouse events */
  DEFAULT_MOUSE_EVENTS(NORMAL)

  /* Fullscreen mode */
  girara_shortcut_add(gsession, 0, GDK_KEY_F11, NULL, sc_toggle_fullscreen, FULLSCREEN, 0, NULL);

  DEFAULT_SHORTCUTS(FULLSCREEN)

  /* Fullscreen mode - Mouse events */
  DEFAULT_MOUSE_EVENTS(FULLSCREEN)

  /* Index mode */
  girara_shortcut_add(gsession, 0,                GDK_KEY_Tab,         NULL, sc_toggle_index,        INDEX,        0,            NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_k,           NULL, sc_navigate_index,      INDEX,        UP,           NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_j,           NULL, sc_navigate_index,      INDEX,        DOWN,         NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_h,           NULL, sc_navigate_index,      INDEX,        COLLAPSE,     NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_l,           NULL, sc_navigate_index,      INDEX,        EXPAND,       NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_L,           NULL, sc_navigate_index,      INDEX,        EXPAND_ALL,   NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_H,           NULL, sc_navigate_index,      INDEX,        COLLAPSE_ALL, NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_Up,          NULL, sc_navigate_index,      INDEX,        UP,           NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_Down,        NULL, sc_navigate_index,      INDEX,        DOWN,         NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_Left,        NULL, sc_navigate_index,      INDEX,        COLLAPSE,     NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_Right,       NULL, sc_navigate_index,      INDEX,        EXPAND,       NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_space,       NULL, sc_navigate_index,      INDEX,        SELECT,       NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_Return,      NULL, sc_navigate_index,      INDEX,        SELECT,       NULL);
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_j,           NULL, sc_navigate_index,      INDEX,        SELECT,       NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_q,           NULL, sc_quit,                INDEX,        0,            NULL);
  girara_shortcut_add(gsession, 0,                0,                   "gg", sc_navigate_index,      INDEX,        TOP,          NULL);
  girara_shortcut_add(gsession, 0,                0,                   "G",  sc_navigate_index,      INDEX,        BOTTOM,       NULL);
  girara_shortcut_add(gsession, 0,                GDK_KEY_Escape,      NULL, sc_toggle_index,        INDEX,        0,            NULL);
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_bracketleft, NULL, sc_toggle_index,        INDEX,        0,            NULL);
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_c,           NULL, sc_toggle_index,        INDEX,        0,            NULL);

  /* Presentation mode */
  girara_shortcut_add(gsession, 0,              GDK_KEY_J,         NULL, sc_navigate,            PRESENTATION, NEXT,         NULL);
  girara_shortcut_add(gsession, 0,              GDK_KEY_Down,      NULL, sc_navigate,            PRESENTATION, NEXT,         NULL);
  girara_shortcut_add(gsession, 0,              GDK_KEY_Right,     NULL, sc_navigate,            PRESENTATION, NEXT,         NULL);
  girara_shortcut_add(gsession, 0,              GDK_KEY_Page_Down, NULL, sc_navigate,            PRESENTATION, NEXT,         NULL);
  girara_shortcut_add(gsession, 0,              GDK_KEY_space,     NULL, sc_navigate,            PRESENTATION, NEXT,         NULL);
  girara_shortcut_add(gsession, 0,              GDK_KEY_K,         NULL, sc_navigate,            PRESENTATION, PREVIOUS,     NULL);
  girara_shortcut_add(gsession, 0,              GDK_KEY_Left,      NULL, sc_navigate,            PRESENTATION, PREVIOUS,     NULL);
  girara_shortcut_add(gsession, 0,              GDK_KEY_Up,        NULL, sc_navigate,            PRESENTATION, PREVIOUS,     NULL);
  girara_shortcut_add(gsession, 0,              GDK_KEY_Page_Up,   NULL, sc_navigate,            PRESENTATION, PREVIOUS,     NULL);
  girara_shortcut_add(gsession, GDK_SHIFT_MASK, GDK_KEY_space,     NULL, sc_navigate,            PRESENTATION, PREVIOUS,     NULL);
  girara_shortcut_add(gsession, 0,              GDK_KEY_BackSpace, NULL, sc_navigate,            PRESENTATION, PREVIOUS,     NULL);

  girara_shortcut_add(gsession, 0,              GDK_KEY_F5,        NULL, sc_toggle_presentation, PRESENTATION, 0,            NULL);

  girara_shortcut_add(gsession, 0,              GDK_KEY_q,         NULL, sc_quit,                PRESENTATION, 0,            NULL);

  /* Presentation mode - Mouse events */
  girara_mouse_event_add(gsession, 0,                0,                    sc_mouse_scroll, PRESENTATION, GIRARA_EVENT_SCROLL_UP,    UP,       NULL);
  girara_mouse_event_add(gsession, 0,                0,                    sc_mouse_scroll, PRESENTATION, GIRARA_EVENT_SCROLL_DOWN,  DOWN,     NULL);
  girara_mouse_event_add(gsession, 0,                0,                    sc_mouse_scroll, PRESENTATION, GIRARA_EVENT_SCROLL_LEFT,  LEFT,     NULL);
  girara_mouse_event_add(gsession, 0,                0,                    sc_mouse_scroll, PRESENTATION, GIRARA_EVENT_SCROLL_RIGHT, RIGHT,    NULL);

  girara_mouse_event_add(gsession, 0,                GIRARA_MOUSE_BUTTON1, sc_navigate,     PRESENTATION, GIRARA_EVENT_BUTTON_PRESS, NEXT,     NULL);
  girara_mouse_event_add(gsession, 0,                GIRARA_MOUSE_BUTTON3, sc_navigate,     PRESENTATION, GIRARA_EVENT_BUTTON_PRESS, PREVIOUS, NULL);

  girara_mouse_event_add(gsession, GDK_SHIFT_MASK,   0,                    sc_mouse_scroll, PRESENTATION, GIRARA_EVENT_SCROLL_UP,    LEFT,     NULL);
  girara_mouse_event_add(gsession, GDK_SHIFT_MASK,   0,                    sc_mouse_scroll, PRESENTATION, GIRARA_EVENT_SCROLL_DOWN,  RIGHT,    NULL);

  girara_mouse_event_add(gsession, GDK_CONTROL_MASK, 0,                    sc_mouse_zoom,   PRESENTATION, GIRARA_EVENT_SCROLL_UP,    UP,       NULL);
  girara_mouse_event_add(gsession, GDK_CONTROL_MASK, 0,                    sc_mouse_zoom,   PRESENTATION, GIRARA_EVENT_SCROLL_DOWN,  DOWN,     NULL);

  /* inputbar shortcuts */
  girara_inputbar_shortcut_add(gsession, 0,                GDK_KEY_Escape, sc_abort, 0, NULL);
  girara_inputbar_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_KEY_c,      sc_abort, 0, NULL);

  /* define default inputbar commands */
  girara_inputbar_command_add(gsession, "bmark",      NULL,   cmd_bookmark_create, NULL,         _("Add a bookmark"));
  girara_inputbar_command_add(gsession, "bdelete",    NULL,   cmd_bookmark_delete, cc_bookmarks, _("Delete a bookmark"));
  girara_inputbar_command_add(gsession, "blist",      NULL,   cmd_bookmark_open,   cc_bookmarks, _("List all bookmarks"));
  girara_inputbar_command_add(gsession, "close",      NULL,   cmd_close,           NULL,         _("Close current file"));
  girara_inputbar_command_add(gsession, "info",       NULL,   cmd_info,            NULL,         _("Show file information"));
  girara_inputbar_command_add(gsession, "exec",       NULL,   cmd_exec,            NULL,         _("Execute a command"));
  girara_inputbar_command_add(gsession, "!",          NULL,   cmd_exec,            NULL,         _("Execute a command")); /* like vim */
  girara_inputbar_command_add(gsession, "help",       NULL,   cmd_help,            NULL,         _("Show help"));
  girara_inputbar_command_add(gsession, "open",       "o",    cmd_open,            cc_open,      _("Open document"));
  girara_inputbar_command_add(gsession, "quit",       "q",    cmd_quit,            NULL,         _("Close zathura"));
  girara_inputbar_command_add(gsession, "print",      NULL,   cmd_print,           NULL,         _("Print document"));
  girara_inputbar_command_add(gsession, "save",       NULL,   cmd_save,            cc_write,     _("Save document"));
  girara_inputbar_command_add(gsession, "save!",      NULL,   cmd_savef,           cc_write,     _("Save document (and force overwriting)"));
  girara_inputbar_command_add(gsession, "write",      NULL,   cmd_save,            cc_write,     _("Save document"));
  girara_inputbar_command_add(gsession, "write!",     NULL,   cmd_savef,           cc_write,     _("Save document (and force overwriting)"));
  girara_inputbar_command_add(gsession, "export",     NULL,   cmd_export,          cc_export,    _("Save attachments"));
  girara_inputbar_command_add(gsession, "offset",     NULL,   cmd_offset,          NULL,         _("Set page offset"));
  girara_inputbar_command_add(gsession, "mark",       NULL,   cmd_marks_add,       NULL,         _("Mark current location within the document"));
  girara_inputbar_command_add(gsession, "delmarks",   "delm", cmd_marks_delete,    NULL,         _("Delete the specified marks"));
  girara_inputbar_command_add(gsession, "nohlsearch", "nohl", cmd_nohlsearch,      NULL,         _("Don't highlight current search results"));
  girara_inputbar_command_add(gsession, "hlsearch",   NULL,   cmd_hlsearch,        NULL,         _("Highlight current search results"));
  girara_inputbar_command_add(gsession, "version",    NULL,   cmd_version,         NULL,         _("Show version information"));
  girara_inputbar_command_add(gsession, "source",     NULL,   cmd_source,          NULL,         _("Source config file"));

  girara_special_command_add(gsession, '/', cmd_search, INCREMENTAL_SEARCH, FORWARD,  NULL);
  girara_special_command_add(gsession, '?', cmd_search, INCREMENTAL_SEARCH, BACKWARD, NULL);

  /* add shortcut mappings */
  girara_shortcut_mapping_add(gsession, "abort",               sc_abort);
  girara_shortcut_mapping_add(gsession, "adjust_window",       sc_adjust_window);
  girara_shortcut_mapping_add(gsession, "bisect",              sc_bisect);
  girara_shortcut_mapping_add(gsession, "change_mode",         sc_change_mode);
  girara_shortcut_mapping_add(gsession, "display_link",        sc_display_link);
  girara_shortcut_mapping_add(gsession, "copy_link",           sc_copy_link);
  girara_shortcut_mapping_add(gsession, "copy_filepath",       sc_copy_filepath);
  girara_shortcut_mapping_add(gsession, "exec",                sc_exec);
  girara_shortcut_mapping_add(gsession, "focus_inputbar",      sc_focus_inputbar);
  girara_shortcut_mapping_add(gsession, "follow",              sc_follow);
  girara_shortcut_mapping_add(gsession, "goto",                sc_goto);
  girara_shortcut_mapping_add(gsession, "jumplist",            sc_jumplist);
  girara_shortcut_mapping_add(gsession, "mark_add",            sc_mark_add);
  girara_shortcut_mapping_add(gsession, "mark_evaluate",       sc_mark_evaluate);
  girara_shortcut_mapping_add(gsession, "navigate",            sc_navigate);
  girara_shortcut_mapping_add(gsession, "navigate_index",      sc_navigate_index);
  girara_shortcut_mapping_add(gsession, "nohlsearch",          sc_nohlsearch);
  girara_shortcut_mapping_add(gsession, "print",               sc_print);
  girara_shortcut_mapping_add(gsession, "quit",                sc_quit);
  girara_shortcut_mapping_add(gsession, "recolor",             sc_recolor);
  girara_shortcut_mapping_add(gsession, "reload",              sc_reload);
  girara_shortcut_mapping_add(gsession, "rotate",              sc_rotate);
  girara_shortcut_mapping_add(gsession, "scroll",              sc_scroll);
  girara_shortcut_mapping_add(gsession, "search",              sc_search);
  girara_shortcut_mapping_add(gsession, "snap_to_page",        sc_snap_to_page);
  girara_shortcut_mapping_add(gsession, "toggle_fullscreen",   sc_toggle_fullscreen);
  girara_shortcut_mapping_add(gsession, "toggle_index",        sc_toggle_index);
  girara_shortcut_mapping_add(gsession, "toggle_page_mode",    sc_toggle_page_mode);
  girara_shortcut_mapping_add(gsession, "toggle_presentation", sc_toggle_presentation);
  girara_shortcut_mapping_add(gsession, "zoom",                sc_zoom);

  /* add argument mappings */
  girara_argument_mapping_add(gsession, "backward",     BACKWARD);
  girara_argument_mapping_add(gsession, "bottom",       BOTTOM);
  girara_argument_mapping_add(gsession, "default",      DEFAULT);
  girara_argument_mapping_add(gsession, "collapse",     COLLAPSE);
  girara_argument_mapping_add(gsession, "collapse-all", COLLAPSE_ALL);
  girara_argument_mapping_add(gsession, "down",         DOWN);
  girara_argument_mapping_add(gsession, "expand",       EXPAND);
  girara_argument_mapping_add(gsession, "expand-all",   EXPAND_ALL);
  girara_argument_mapping_add(gsession, "select",       SELECT);
  girara_argument_mapping_add(gsession, "toggle",       TOGGLE);
  girara_argument_mapping_add(gsession, "forward",      FORWARD);
  girara_argument_mapping_add(gsession, "full-down",    FULL_DOWN);
  girara_argument_mapping_add(gsession, "full-up",      FULL_UP);
  girara_argument_mapping_add(gsession, "half-down",    HALF_DOWN);
  girara_argument_mapping_add(gsession, "half-up",      HALF_UP);
  girara_argument_mapping_add(gsession, "full-right",   FULL_RIGHT);
  girara_argument_mapping_add(gsession, "full-left",    FULL_LEFT);
  girara_argument_mapping_add(gsession, "half-right",   HALF_RIGHT);
  girara_argument_mapping_add(gsession, "half-left",    HALF_LEFT);
  girara_argument_mapping_add(gsession, "in",           ZOOM_IN);
  girara_argument_mapping_add(gsession, "left",         LEFT);
  girara_argument_mapping_add(gsession, "next",         NEXT);
  girara_argument_mapping_add(gsession, "out",          ZOOM_OUT);
  girara_argument_mapping_add(gsession, "page-top",     PAGE_TOP);
  girara_argument_mapping_add(gsession, "page-bottom",  PAGE_BOTTOM);
  girara_argument_mapping_add(gsession, "previous",     PREVIOUS);
  girara_argument_mapping_add(gsession, "right",        RIGHT);
  girara_argument_mapping_add(gsession, "specific",     ZOOM_SPECIFIC);
  girara_argument_mapping_add(gsession, "top",          TOP);
  girara_argument_mapping_add(gsession, "up",           UP);
  girara_argument_mapping_add(gsession, "best-fit",     ZATHURA_ADJUST_BESTFIT);
  girara_argument_mapping_add(gsession, "width",        ZATHURA_ADJUST_WIDTH);
  girara_argument_mapping_add(gsession, "rotate-cw",    ROTATE_CW);
  girara_argument_mapping_add(gsession, "rotate-ccw",   ROTATE_CCW);
}

void
config_load_files(zathura_t* zathura)
{
  /* load global configuration files */
  char* config_path = girara_get_xdg_path(XDG_CONFIG_DIRS);
  girara_list_t* config_dirs = girara_split_path_array(config_path);
  ssize_t size = girara_list_size(config_dirs) - 1;
  for (; size >= 0; --size) {
    const char* dir = girara_list_nth(config_dirs, size);
    char* file = g_build_filename(dir, ZATHURA_RC, NULL);
    girara_config_parse(zathura->ui.session, file);
    g_free(file);
  }
  girara_list_free(config_dirs);
  g_free(config_path);

  girara_config_parse(zathura->ui.session, GLOBAL_RC);

  /* load local configuration files */
  char* configuration_file = g_build_filename(zathura->config.config_dir, ZATHURA_RC, NULL);
  girara_config_parse(zathura->ui.session, configuration_file);
  g_free(configuration_file);
}
