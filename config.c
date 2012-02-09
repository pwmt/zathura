/* See LICENSE file for license and copyright information */

#include "config.h"
#include "commands.h"
#include "completion.h"
#include "callbacks.h"
#include "shortcuts.h"
#include "zathura.h"

#include <girara/settings.h>
#include <girara/session.h>
#include <girara/shortcuts.h>
#include <girara/config.h>
#include <girara/commands.h>

void
config_load_default(zathura_t* zathura)
{
  if (zathura == NULL || zathura->ui.session == NULL) {
    return;
  }

  int int_value              = 0;
  float float_value          = 0;
  char* string_value         = NULL;
  bool bool_value            = false;
  girara_session_t* gsession = zathura->ui.session;

  /* mode settings */
  zathura->modes.normal     = gsession->modes.normal;
  zathura->modes.fullscreen = girara_mode_add(gsession, "fullscreen");
  zathura->modes.index      = girara_mode_add(gsession, "index");
  zathura->modes.insert     = girara_mode_add(gsession, "insert");

#define NORMAL zathura->modes.normal
#define INSERT zathura->modes.insert
#define INDEX zathura->modes.index
#define FULLSCREEN zathura->modes.fullscreen

  girara_mode_set(gsession, zathura->modes.normal);

  /* zathura settings */
  int_value = 10;
  girara_setting_add(gsession, "zoom-step",     &int_value,   INT,   false, "Zoom step",               NULL, NULL);
  int_value = 1;
  girara_setting_add(gsession, "page-padding",  &int_value,   INT,   true,  "Padding between pages",   NULL, NULL);
  int_value = 1;
  girara_setting_add(gsession, "pages-per-row", &int_value,   INT,   false, "Number of pages per row", cb_pages_per_row_value_changed, zathura);
  float_value = 40;
  girara_setting_add(gsession, "scroll-step",   &float_value, FLOAT, false, "Scroll step",             NULL, NULL);

  string_value = "#FFFFFF";
  girara_setting_add(gsession, "recolor-darkcolor",  string_value, STRING, false, "Recoloring (dark color)",  NULL, NULL);
  string_value = "#000000";
  girara_setting_add(gsession, "recolor-lightcolor", string_value, STRING, false, "Recoloring (light color)", NULL, NULL);

  string_value = "#9FBC00";
  girara_setting_add(gsession, "highlight-color",        string_value, STRING, false, "Color for highlighting",        NULL, NULL);
  float_value = 0.5;
  girara_setting_add(gsession, "highlight-transparency", &float_value, FLOAT,  false, "Transparency for highlighting", NULL, NULL);
  bool_value = true;
  girara_setting_add(gsession, "render-loading",         &bool_value,  BOOLEAN, false, "Render 'Loading ...'", NULL, NULL);

  /* define default shortcuts */
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_c,          NULL, sc_abort,                    0,          0,               NULL);
  girara_shortcut_add(gsession, 0,                GDK_Escape,     NULL, sc_abort,                    0,          0,               NULL);
  girara_shortcut_add(gsession, 0,                GDK_a,          NULL, sc_adjust_window,            NORMAL,     ADJUST_BESTFIT,  NULL);
  girara_shortcut_add(gsession, 0,                GDK_s,          NULL, sc_adjust_window,            NORMAL,     ADJUST_WIDTH,    NULL);
  girara_shortcut_add(gsession, 0,                GDK_i,          NULL, sc_change_mode,              NORMAL,     INSERT,          NULL);
  girara_shortcut_add(gsession, 0,                GDK_m,          NULL, sc_change_mode,              NORMAL,     ADD_MARKER,      NULL);
  girara_shortcut_add(gsession, 0,                GDK_apostrophe, NULL, sc_change_mode,              NORMAL,     EVAL_MARKER,     NULL);
  girara_shortcut_add(gsession, 0,                GDK_slash,      NULL, girara_sc_focus_inputbar,    NORMAL,     0,               &("/"));
  girara_shortcut_add(gsession, GDK_SHIFT_MASK,   GDK_slash,      NULL, girara_sc_focus_inputbar,    NORMAL,     0,               &("/"));
  girara_shortcut_add(gsession, 0,                GDK_question,   NULL, girara_sc_focus_inputbar,    NORMAL,     0,               &("?"));
  girara_shortcut_add(gsession, 0,                GDK_colon,      NULL, girara_sc_focus_inputbar,    NORMAL,     0,               &(":"));
  girara_shortcut_add(gsession, 0,                GDK_o,          NULL, girara_sc_focus_inputbar,    NORMAL,     0,               &(":open "));
  girara_shortcut_add(gsession, 0,                GDK_O,          NULL, girara_sc_focus_inputbar,    NORMAL,     APPEND_FILEPATH, &(":open "));
  girara_shortcut_add(gsession, 0,                GDK_f,          NULL, sc_follow,                   NORMAL,     0,               NULL);
  girara_shortcut_add(gsession, 0,                0,              "gg", sc_goto,                     NORMAL,     TOP,             NULL);
  girara_shortcut_add(gsession, 0,                0,              "gg", sc_goto,                     FULLSCREEN, TOP,             NULL);
  girara_shortcut_add(gsession, 0,                0,              "G",  sc_goto,                     NORMAL,     BOTTOM,          NULL);
  girara_shortcut_add(gsession, 0,                0,              "G",  sc_goto,                     FULLSCREEN, BOTTOM,          NULL);
  girara_shortcut_add(gsession, 0,                GDK_J,          NULL, sc_navigate,                 NORMAL,     NEXT,            NULL);
  girara_shortcut_add(gsession, 0,                GDK_K,          NULL, sc_navigate,                 NORMAL,     PREVIOUS,        NULL);
  girara_shortcut_add(gsession, GDK_MOD1_MASK,    GDK_Right,      NULL, sc_navigate,                 NORMAL,     NEXT,            NULL);
  girara_shortcut_add(gsession, GDK_MOD1_MASK,    GDK_Left,       NULL, sc_navigate,                 NORMAL,     PREVIOUS,        NULL);
  girara_shortcut_add(gsession, 0,                GDK_Left,       NULL, sc_navigate,                 FULLSCREEN, PREVIOUS,        NULL);
  girara_shortcut_add(gsession, 0,                GDK_Up,         NULL, sc_navigate,                 FULLSCREEN, PREVIOUS,        NULL);
  girara_shortcut_add(gsession, 0,                GDK_Down,       NULL, sc_navigate,                 FULLSCREEN, NEXT,            NULL);
  girara_shortcut_add(gsession, 0,                GDK_Right,      NULL, sc_navigate,                 FULLSCREEN, NEXT,            NULL);
  girara_shortcut_add(gsession, 0,                GDK_k,          NULL, sc_navigate_index,           INDEX,      UP,              NULL);
  girara_shortcut_add(gsession, 0,                GDK_j,          NULL, sc_navigate_index,           INDEX,      DOWN,            NULL);
  girara_shortcut_add(gsession, 0,                GDK_h,          NULL, sc_navigate_index,           INDEX,      COLLAPSE,        NULL);
  girara_shortcut_add(gsession, 0,                GDK_l,          NULL, sc_navigate_index,           INDEX,      EXPAND,          NULL);
  girara_shortcut_add(gsession, 0,                GDK_space,      NULL, sc_navigate_index,           INDEX,      SELECT,          NULL);
  girara_shortcut_add(gsession, 0,                GDK_Return,     NULL, sc_navigate_index,           INDEX,      SELECT,          NULL);
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_i,          NULL, sc_recolor,                  NORMAL,     0,               NULL);
  girara_shortcut_add(gsession, 0,                GDK_R,          NULL, sc_reload,                   NORMAL,     0,               NULL);
  girara_shortcut_add(gsession, 0,                GDK_r,          NULL, sc_rotate,                   NORMAL,     0,               NULL);
  girara_shortcut_add(gsession, 0,                GDK_h,          NULL, sc_scroll,                   NORMAL,     LEFT,            NULL);
  girara_shortcut_add(gsession, 0,                GDK_j,          NULL, sc_scroll,                   NORMAL,     DOWN,            NULL);
  girara_shortcut_add(gsession, 0,                GDK_k,          NULL, sc_scroll,                   NORMAL,     UP,              NULL);
  girara_shortcut_add(gsession, 0,                GDK_l,          NULL, sc_scroll,                   NORMAL,     RIGHT,           NULL);
  girara_shortcut_add(gsession, 0,                GDK_Left,       NULL, sc_scroll,                   NORMAL,     LEFT,            NULL);
  girara_shortcut_add(gsession, 0,                GDK_Up,         NULL, sc_scroll,                   NORMAL,     UP,              NULL);
  girara_shortcut_add(gsession, 0,                GDK_Down,       NULL, sc_scroll,                   NORMAL,     DOWN,            NULL);
  girara_shortcut_add(gsession, 0,                GDK_Right,      NULL, sc_scroll,                   NORMAL,     RIGHT,           NULL);
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_d,          NULL, sc_scroll,                   NORMAL,     HALF_DOWN,       NULL);
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_u,          NULL, sc_scroll,                   NORMAL,     HALF_UP,         NULL);
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_f,          NULL, sc_scroll,                   NORMAL,     FULL_DOWN,       NULL);
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_b,          NULL, sc_scroll,                   NORMAL,     FULL_UP,         NULL);
  girara_shortcut_add(gsession, 0,                GDK_space,      NULL, sc_scroll,                   NORMAL,     FULL_DOWN,       NULL);
  girara_shortcut_add(gsession, GDK_SHIFT_MASK,   GDK_space,      NULL, sc_scroll,                   NORMAL,     FULL_UP,         NULL);
  girara_shortcut_add(gsession, 0,                GDK_n,          NULL, sc_search,                   NORMAL,     FORWARD,         NULL);
  girara_shortcut_add(gsession, 0,                GDK_N,          NULL, sc_search,                   NORMAL,     BACKWARD,        NULL);
  girara_shortcut_add(gsession, 0,                GDK_Tab,        NULL, sc_toggle_index,             NORMAL,     0,               NULL);
  girara_shortcut_add(gsession, 0,                GDK_Tab,        NULL, sc_toggle_index,             INDEX,      0,               NULL);
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_m,          NULL, girara_sc_toggle_inputbar,   NORMAL,     0,               NULL);
  girara_shortcut_add(gsession, 0,                GDK_F5,         NULL, sc_toggle_fullscreen,        NORMAL,     0,               NULL);
  girara_shortcut_add(gsession, 0,                GDK_F5,         NULL, sc_toggle_fullscreen,        FULLSCREEN, 0,               NULL);
  girara_shortcut_add(gsession, GDK_CONTROL_MASK, GDK_n,          NULL, girara_sc_toggle_statusbar,  NORMAL,     0,               NULL);
  girara_shortcut_add(gsession, 0,                GDK_q,          NULL, sc_quit,                     NORMAL,     0,               NULL);
  girara_shortcut_add(gsession, 0,                GDK_plus,       NULL, sc_zoom,                     NORMAL,     ZOOM_IN,         NULL);
  girara_shortcut_add(gsession, 0,                GDK_plus,       NULL, sc_zoom,                     FULLSCREEN, ZOOM_IN,         NULL);
  girara_shortcut_add(gsession, 0,                GDK_minus,      NULL, sc_zoom,                     NORMAL,     ZOOM_OUT,        NULL);
  girara_shortcut_add(gsession, 0,                GDK_minus,      NULL, sc_zoom,                     FULLSCREEN, ZOOM_OUT,        NULL);
  girara_shortcut_add(gsession, 0,                GDK_equal,      NULL, sc_zoom,                     NORMAL,     ZOOM_ORIGINAL,   NULL);
  girara_shortcut_add(gsession, 0,                GDK_equal,      NULL, sc_zoom,                     FULLSCREEN, ZOOM_ORIGINAL,   NULL);
  girara_shortcut_add(gsession, 0,                0,              "zI", sc_zoom,                     NORMAL,     ZOOM_IN,         NULL);
  girara_shortcut_add(gsession, 0,                0,              "zI", sc_zoom,                     FULLSCREEN, ZOOM_IN,         NULL);
  girara_shortcut_add(gsession, 0,                0,              "zO", sc_zoom,                     NORMAL,     ZOOM_OUT,        NULL);
  girara_shortcut_add(gsession, 0,                0,              "zO", sc_zoom,                     FULLSCREEN, ZOOM_OUT,        NULL);
  girara_shortcut_add(gsession, 0,                0,              "z0", sc_zoom,                     NORMAL,     ZOOM_ORIGINAL,   NULL);
  girara_shortcut_add(gsession, 0,                0,              "z0", sc_zoom,                     FULLSCREEN, ZOOM_ORIGINAL,   NULL);
  girara_shortcut_add(gsession, 0,                GDK_equal,      NULL, sc_zoom,                     NORMAL,     ZOOM_SPECIFIC,   NULL);
  girara_shortcut_add(gsession, 0,                GDK_equal,      NULL, sc_zoom,                     FULLSCREEN, ZOOM_SPECIFIC,   NULL);

  /* mouse events */
  girara_mouse_event_add(gsession, 0, 0,                                   sc_mouse_scroll, NORMAL,     GIRARA_EVENT_SCROLL,         0, NULL);
  girara_mouse_event_add(gsession, 0, 0,                                   sc_mouse_scroll, FULLSCREEN, GIRARA_EVENT_SCROLL,         0, NULL);
  girara_mouse_event_add(gsession, GDK_CONTROL_MASK, 0,                    sc_mouse_zoom,   NORMAL,     GIRARA_EVENT_SCROLL,         0, NULL);
  girara_mouse_event_add(gsession, GDK_CONTROL_MASK, 0,                    sc_mouse_zoom,   FULLSCREEN, GIRARA_EVENT_SCROLL,         0, NULL);
  girara_mouse_event_add(gsession, 0,                GIRARA_MOUSE_BUTTON2, sc_mouse_scroll, NORMAL,     GIRARA_EVENT_BUTTON_PRESS,   0, NULL);
  girara_mouse_event_add(gsession, GDK_BUTTON2_MASK, GIRARA_MOUSE_BUTTON2, sc_mouse_scroll, NORMAL,     GIRARA_EVENT_BUTTON_RELEASE, 0, NULL);
  girara_mouse_event_add(gsession, GDK_BUTTON2_MASK, 0,                    sc_mouse_scroll, NORMAL,     GIRARA_EVENT_MOTION_NOTIFY,  0, NULL);

  /* define default inputbar commands */
  girara_inputbar_command_add(gsession, "bmark",   NULL, cmd_bookmark_create, NULL,         "Add a bookmark");
  girara_inputbar_command_add(gsession, "bdelete", NULL, cmd_bookmark_delete, cc_bookmarks, "Delete a bookmark");
  girara_inputbar_command_add(gsession, "blist",   NULL, cmd_bookmark_open,   cc_bookmarks, "List all bookmarks");
  girara_inputbar_command_add(gsession, "close",   NULL, cmd_close,           NULL,         "Close current file");
  girara_inputbar_command_add(gsession, "info",    NULL, cmd_info,            NULL,         "Show file information");
  girara_inputbar_command_add(gsession, "help",    NULL, cmd_help,            NULL,         "Show help");
  girara_inputbar_command_add(gsession, "open",    "o",  cmd_open,            cc_open,      "Open document");
  girara_inputbar_command_add(gsession, "print",   NULL, cmd_print,           NULL,         "Print document");
  girara_inputbar_command_add(gsession, "write",   NULL, cmd_save,            NULL,         "Save document");
  girara_inputbar_command_add(gsession, "write!",  NULL, cmd_savef,           NULL,         "Save document (and force overwriting)");
  girara_inputbar_command_add(gsession, "export",  NULL, cmd_export,          cc_export,    "Save attachments");

  girara_special_command_add(gsession, '/', cmd_search, true, FORWARD,  NULL);
  girara_special_command_add(gsession, '?', cmd_search, true, BACKWARD, NULL);

  /* add shortcut mappings */
  girara_shortcut_mapping_add(gsession, "abort",             sc_abort);
  girara_shortcut_mapping_add(gsession, "adjust_window",     sc_adjust_window);
  girara_shortcut_mapping_add(gsession, "change_mode",       sc_change_mode);
  girara_shortcut_mapping_add(gsession, "follow",            sc_follow);
  girara_shortcut_mapping_add(gsession, "goto",              sc_goto);
  girara_shortcut_mapping_add(gsession, "index_navigate",    sc_navigate_index);
  girara_shortcut_mapping_add(gsession, "navigate",          sc_navigate);
  girara_shortcut_mapping_add(gsession, "quit",              sc_quit);
  girara_shortcut_mapping_add(gsession, "recolor",           sc_recolor);
  girara_shortcut_mapping_add(gsession, "reload",            sc_reload);
  girara_shortcut_mapping_add(gsession, "rotate",            sc_rotate);
  girara_shortcut_mapping_add(gsession, "scroll",            sc_scroll);
  girara_shortcut_mapping_add(gsession, "search",            sc_search);
  girara_shortcut_mapping_add(gsession, "toggle_fullscreen", sc_toggle_fullscreen);
  girara_shortcut_mapping_add(gsession, "toggle_index",      sc_toggle_index);
  girara_shortcut_mapping_add(gsession, "toggle_inputbar",   girara_sc_toggle_inputbar);
  girara_shortcut_mapping_add(gsession, "toggle_statusbar",  girara_sc_toggle_statusbar);
  girara_shortcut_mapping_add(gsession, "zoom",              sc_zoom);

  /* add argument mappings */
  girara_argument_mapping_add(gsession, "bottom",     BOTTOM);
  girara_argument_mapping_add(gsession, "default",    DEFAULT);
  girara_argument_mapping_add(gsession, "down",       DOWN);
  girara_argument_mapping_add(gsession, "full_down",  FULL_DOWN);
  girara_argument_mapping_add(gsession, "full_up",    FULL_UP);
  girara_argument_mapping_add(gsession, "half_down",  HALF_DOWN);
  girara_argument_mapping_add(gsession, "half_up",    HALF_UP);
  girara_argument_mapping_add(gsession, "in",         ZOOM_IN);
  girara_argument_mapping_add(gsession, "left",       LEFT);
  girara_argument_mapping_add(gsession, "next",       NEXT);
  girara_argument_mapping_add(gsession, "out",        ZOOM_OUT);
  girara_argument_mapping_add(gsession, "previous",   PREVIOUS);
  girara_argument_mapping_add(gsession, "right",      RIGHT);
  girara_argument_mapping_add(gsession, "specific",   ZOOM_SPECIFIC);
  girara_argument_mapping_add(gsession, "top",        TOP);
  girara_argument_mapping_add(gsession, "up",         UP);
}

void
config_load_file(zathura_t* zathura, char* path)
{
  if (zathura == NULL) {
    return;
  }

  girara_config_parse(zathura->ui.session, path);
}
