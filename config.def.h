/* settings */
static const int   DEFAULT_WIDTH  = 800;
static const int   DEFAULT_HEIGHT = 600;
static const float ZOOM_STEP      = 10;
static const float ZOOM_MIN       = 10;
static const float ZOOM_MAX       = 400;
static const float SCROLL_STEP    = 40;

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

/* statusbar */
static const char DEFAULT_TEXT[] = "[No Name]";

/* additional settings */
#define SHOW_SCROLLBARS 0

/* shortcuts */
Shortcut shortcuts[] = {
  /* mask,             key,               function,             mode,     argument */
  {GDK_CONTROL_MASK,   GDK_n,             sc_toggle_statusbar,  -1,       {0} },
  {GDK_CONTROL_MASK,   GDK_m,             sc_toggle_inputbar,   -1,       {0} },
  {GDK_CONTROL_MASK,   GDK_q,             sc_quit,              -1,       {0} },
  {GDK_CONTROL_MASK,   GDK_c,             sc_abort,             -1,       {0} },
  {GDK_CONTROL_MASK,   GDK_i,             sc_revert_video,      -1,       {0} },
  {GDK_SHIFT_MASK,     GDK_slash,         sc_focus_inputbar,    -1,       { .data = "/" } },
  {GDK_SHIFT_MASK,     GDK_question,      sc_focus_inputbar,    -1,       { .data = "?" } },
  {0,                  GDK_Tab,           sc_toggle_index,      -1,       {0} },
  {0,                  GDK_J,             sc_navigate,          -1,       { NEXT } },
  {0,                  GDK_K,             sc_navigate,          -1,       { PREVIOUS } },
  {0,                  GDK_Escape,        sc_abort,             -1,       {0} },
  {0,                  GDK_i,             sc_change_mode,       NORMAL,   { INSERT } },
  {0,                  GDK_v,             sc_change_mode,       NORMAL,   { VISUAL } },
  {0,                  GDK_colon,         sc_focus_inputbar,    -1,       { .data = ":" } },
  {0,                  GDK_o,             sc_focus_inputbar,    -1,       { .data = ":open " } },
  {0,                  GDK_r,             sc_rotate,            -1,       {0} },
  {0,                  GDK_h,             sc_scroll,            -1,       { LEFT } },
  {0,                  GDK_j,             sc_scroll,            -1,       { UP } },
  {0,                  GDK_k,             sc_scroll,            -1,       { DOWN } },
  {0,                  GDK_l,             sc_scroll,            -1,       { RIGHT } },
  {0,                  GDK_n,             sc_search,            -1,       { FORWARD } },
  {0,                  GDK_N,             sc_search,            -1,       { BACKWARD } },
  {0,                  GDK_a,             sc_adjust_window,     -1,       { ADJUST_BESTFIT } },
  {0,                  GDK_s,             sc_adjust_window,     -1,       { ADJUST_WIDTH } },
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
  {"close",     "c",            cmd_close,  0,            "Close current file" },
  {"open",      "o",            cmd_open,   cc_open,      "Open a file" },
  {"print",     "p",            cmd_print,  0,            "Print the document" },
  {"rotate",    "r",            cmd_rotate, 0,            "Rotate the page" },
  {"set",       "s",            cmd_set,    cc_set,       "Set an option" },
  {"quit",      "q",            cmd_quit,   0,            "Quit zjui" },
  {"write",     "w",            cmd_save,   0,            "Save the document" },
  {"zoom",      "z",            cmd_zoom,   0,            "Set zoom level" },
};

/* buffer commands */
BufferCommand buffer_commands[] = {
  /* regex,        function,      argument */
  {"^gg$",         bcmd_goto,     { TOP } },
  {"^G$",          bcmd_goto,     { BOTTOM } },
  {"^[0-9]+G$",    bcmd_goto,     {0} },
  {"^zI$",         bcmd_zoom,     { ZOOM_IN } },
  {"^zO$",         bcmd_zoom,     { ZOOM_OUT } },
  {"^z0$",         bcmd_zoom,     { ZOOM_ORIGINAL } },
};

/* special commands */
SpecialCommand special_commands[] = {
  /* identifier,   function,      a,   argument */
  {'/',            scmd_search,   1,   { DOWN } },
  {'?',            scmd_search,   1,   { UP } },
};

/* settings */
Setting settings[] = {
  /* name,         variable,                        type,  render,  description */
  {"revertvideo",  &(Zathura.Global.reverse_video), 'b',   TRUE,    "Invert the image"},
};
