/* settings */
static const int DEFAULT_WIDTH  = 800;
static const int DEFAULT_HEIGHT = 600;

/* completion */
static const char FORMAT_COMMAND[]     = "<b>%s</b>";
static const char FORMAT_DESCRIPTION[] = "<i>%s</i>";

/* look */
static const char font[]                   = "monospace normal 9";
static const char default_bgcolor[]        = "#000000";
static const char default_fgcolor[]        = "#DDDDDD";
static const char inputbar_bgcolor[]       = "#141414";
static const char inputbar_fgcolor[]       = "#9FBC00";
static const char statusbar_bgcolor[]      = "#000000";
static const char statusbar_fgcolor[]      = "#FFFFFF";
static const char completion_fgcolor[]     = "#DDDDDD";
static const char completion_bgcolor[]     = "#232323";
static const char completion_g_fgcolor[]   = "#DEDEDE";
static const char completion_g_bgcolor[]   = "#FF00FF";
static const char completion_hl_fgcolor[]  = "#232323";
static const char completion_hl_bgcolor[]  = "#9FBC00";
static const char notification_e_bgcolor[] = "#FF1212";
static const char notification_e_fgcolor[] = "#FFFFFF";
static const char notification_w_bgcolor[] = "#FFF712";
static const char notification_w_fgcolor[] = "#000000";

/* shortcuts */
Shortcut shortcuts[] = {
  /* mask,             key,               function,             mode,     argument */
  {0,                  GDK_i,             sc_change_mode,       NORMAL,   { INSERT } },
  {0,                  GDK_v,             sc_change_mode,       NORMAL,   { VISUAL } },
  {GDK_CONTROL_MASK,   GDK_q,             sc_quit,              -1,       {0} },
  {GDK_SHIFT_MASK,     GDK_slash,         sc_focus_inputbar,    -1,       { .data = "/" } },
  {GDK_SHIFT_MASK,     GDK_question,      sc_focus_inputbar,    -1,       { .data = "?" } },
  {0,                  GDK_colon,         sc_focus_inputbar,    -1,       { .data = ":" } },
  {0,                  GDK_Escape,        sc_abort,             -1,       {0} },
};

/* inputbar shortcuts */
InputbarShortcut inputbar_shortcuts[] = {
  /* mask,             key,               function,                  argument */
  {0,                  GDK_Escape,        isc_abort,                 {0} },
  {0,                  GDK_Up,            isc_command_history,       {0} },
  {0,                  GDK_Down,          isc_command_history,       {0} },
  {0,                  GDK_Tab,           isc_completion,            { NEXT } },
  {GDK_CONTROL_MASK,   GDK_Tab,           isc_completion,            { NEXT_GROUP } },
  {0,                  GDK_ISO_Left_Tab,  isc_completion,            { PREVIOUS } },
  {GDK_CONTROL_MASK,   GDK_ISO_Left_Tab,  isc_completion,            { PREVIOUS_GROUP } },
  {GDK_CONTROL_MASK,   GDK_w,             isc_string_manipulation,   { DELETE_LAST_WORD } },
};

/* commands */
Command commands[] = {
  /* command,   abbreviation,   function,   completion,   description  */
  {"open",      "o",            cmd_quit,   cc_open,      "Open a file" },
  {"quit",      "q",            cmd_quit,   0,            "Quit zjui" },
};

/* buffer commands */
BufferCommand buffer_commands[] = {
  /* regex,        function,      argument */
  {"^gg$",         bcmd_goto,     { TOP } },
  {"^[0-9]+G$",    bcmd_goto,     {0} },
};

/* special commands */
SpecialCommand special_commands[] = {
  /* identifier,   function,      a,   argument */
  {'/',            scmd_search,   1,   { DOWN } },
  {'?',            scmd_search,   1,   { UP } },
};
