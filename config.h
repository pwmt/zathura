/* settings */
static const float ZOOM_STEP         = 0.1;
static const float SCROLL_STEP       = 40;
static const float HL_TRANSPARENCY   = 0.4;
static const int   SHOW_NOTIFICATION = 5;
static const int   DEFAULT_WIDTH     = 800;
static const int   DEFAULT_HEIGHT    = 600;
static const char  BROWSER[]         = "firefox %s";
static const char  PRINTER[]         = "EPSON_AL-CX11_192.168.88.80";
/* look */
static const char font[]                  = "monospace normal 9";
static const char default_bgcolor[]       = "#000000";
static const char default_fgcolor[]       = "#DDDDDD";

static const char notification_fgcolor[]         = "#0F0F0F";
static const char notification_bgcolor[]         = "#F9F9F9";
static const char notification_warning_fgcolor[] = "DDDDDDC";
static const char notification_warning_bgcolor[] = "#D0C54D";
static const char notification_error_fgcolor[]   = "#FFFFFF";
static const char notification_error_bgcolor[]   = "#BC2800";

static const char inputbar_bgcolor[]      = "#141414";
static const char inputbar_fgcolor[]      = "#9FBC00";

static const char completion_fgcolor[]    = "#DDDDDD";
static const char completion_bgcolor[]    = "#232323";
static const char completion_hl_fgcolor[] = "#232323";
static const char completion_hl_bgcolor[] = "#9FBC00";

static const char search_highlight[] = "#9FBC00";

/* shortcuts */
Shortcut shortcuts[] = {
  // mask,               key,           function,            argument
  {GDK_CONTROL_MASK,     GDK_f,         sc_navigate,         { NEXT } },
  {GDK_CONTROL_MASK,     GDK_b,         sc_navigate,         { PREVIOUS } },
  {GDK_CONTROL_MASK,     GDK_plus,      sc_zoom,             { ZOOM_IN } },
  {GDK_CONTROL_MASK,     GDK_minus,     sc_zoom,             { ZOOM_OUT } },
  {GDK_CONTROL_MASK,     GDK_0,         sc_zoom,             { ZOOM_ORIGINAL } },
  {GDK_CONTROL_MASK,     GDK_r,         sc_rotate,           { RIGHT } },
  {GDK_CONTROL_MASK,     GDK_e,         sc_rotate,           { LEFT } },
  {GDK_CONTROL_MASK,     GDK_p,         sc_focus_inputbar,   { .data = ":print all" } },
  {GDK_CONTROL_MASK,     GDK_q,         sc_quit,             {0} },
  {0,                    GDK_n,         sc_search,           { FORWARD } },
  {0,                    GDK_N,         sc_search,           { BACKWARD } },
  {0,                    GDK_h,         sc_scroll,           { LEFT } },
  {0,                    GDK_j,         sc_scroll,           { DOWN } },
  {0,                    GDK_k,         sc_scroll,           { UP } },
  {0,                    GDK_l,         sc_scroll,           { RIGHT } },
  {0,                    GDK_Page_Up,   sc_scroll,           { TOP } },
  {0,                    GDK_Page_Down, sc_scroll,           { BOTTOM } },
  {0,                    GDK_i,         sc_adjust_window,    { ADJUST_BESTFIT } },
  {0,                    GDK_u,         sc_adjust_window,    { ADJUST_WIDTH } },
  {0,                    GDK_colon,     sc_focus_inputbar,   { .data = ":" } },
  {0,                    GDK_o,         sc_focus_inputbar,   { .data = ":open " } },
  {0,                    GDK_r,         sc_focus_inputbar,   { .data = ":rotate " } },
  {0,                    GDK_z,         sc_focus_inputbar,   { .data = ":zoom " } },
  {0,                    GDK_g,         sc_focus_inputbar,   { .data = ":goto " } },
  {0,                    GDK_slash,     sc_focus_inputbar,   { .data = "/" } },
};

/* commands */
Command commands[] = {
  // command,         function
  {"export",          cmd_export},
  {"form",            cmd_form},
  {"goto",            cmd_goto},
  {"info",            cmd_info},
  {"links",           cmd_links},
  {"open",            cmd_open},
  {"print",           cmd_print},
  {"rotate",          cmd_rotate},
  {"save",            cmd_save},
  {"quit",            cmd_quit},
  {"zoom",            cmd_zoom},
};
