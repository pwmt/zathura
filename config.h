static const float zoom_step   = 0.1;
static const float scroll_step = 40;

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
  {0,                    GDK_colon,     sc_focus_inputbar,   { .data = ":" } },
  {0,                    GDK_o,         sc_focus_inputbar,   { .data = ":open " } },
  {GDK_CONTROL_MASK,     GDK_q,         sc_quit,             {0} },       
};

Command commands[] = {
  // command,         function
  {"o",               cmd_open},
  {"open",            cmd_open},
  {"q",               cmd_quit},
  {"quit",            cmd_quit},
};
