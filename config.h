static const float ZOOM_STEP   = 0.1;
static const float SCROLL_STEP = 40;

static const char font[]                  = "monospace normal 9";
static const char default_bgcolor[]       = "#141414";
static const char default_fgcolor[]       = "#CCCCCC";

static const char inputbar_bgcolor[]      = "#141414";
static const char inputbar_fgcolor[]      = "#9FBC00";
static const char inputbar_wn_fgcolor[]   = "#CCCCCC";
static const char inputbar_wn_bgcolor[]   = "#B82121";

static const char completion_fgcolor[]    = "#CCCCCC";
static const char completion_bgcolor[]    = "#232323";
static const char completion_hl_fgcolor[] = "#232323";
static const char completion_hl_bgcolor[] = "#9FBC00";

Shortcut shortcuts[] = {
  // mask,               key,           function,            argument
  {GDK_CONTROL_MASK,     GDK_f,         sc_navigate,         { NEXT } },
  {GDK_CONTROL_MASK,     GDK_b,         sc_navigate,         { PREVIOUS } },
  {GDK_CONTROL_MASK,     GDK_plus,      sc_zoom,             { ZOOM_IN } },
  {GDK_CONTROL_MASK,     GDK_minus,     sc_zoom,             { ZOOM_OUT } },
  {GDK_CONTROL_MASK,     GDK_0,         sc_zoom,             { ZOOM_ORIGINAL } },
  {GDK_CONTROL_MASK,     GDK_r,         sc_rotate,           { RIGHT } },
  {GDK_CONTROL_MASK,     GDK_e,         sc_rotate,           { LEFT } },
  {GDK_CONTROL_MASK,     GDK_q,         sc_quit,             {0} },       
  {0,                    GDK_h,         sc_scroll,           { LEFT } },
  {0,                    GDK_j,         sc_scroll,           { DOWN } },
  {0,                    GDK_k,         sc_scroll,           { UP } },
  {0,                    GDK_l,         sc_scroll,           { RIGHT } },
  {0,                    GDK_i,         sc_adjust_window,    { ADJUST_BESTFIT } },
  {0,                    GDK_u,         sc_adjust_window,    { ADJUST_WIDTH } },
  {0,                    GDK_colon,     sc_focus_inputbar,   { .data = ":" } },
  {0,                    GDK_o,         sc_focus_inputbar,   { .data = ":open " } },
  {0,                    GDK_r,         sc_focus_inputbar,   { .data = ":rotate " } },
  {0,                    GDK_z,         sc_focus_inputbar,   { .data = ":zoom " } },
  {0,                    GDK_g,         sc_focus_inputbar,   { .data = ":goto " } },
  {0,                    GDK_slash,     sc_focus_inputbar,   { .data = "/" } },
};

Command commands[] = {
  // command,         function
  {"g",               cmd_goto},
  {"goto",            cmd_goto},
  {"o",               cmd_open},
  {"open",            cmd_open},
  {"r",               cmd_rotate},
  {"rotate",          cmd_rotate},
  {"q",               cmd_quit},
  {"quit",            cmd_quit},
  {"z",               cmd_zoom},
  {"zoom",            cmd_zoom},
};
