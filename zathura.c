/* See LICENSE file for license and copyright information */

#define _BSD_SOURCE || _XOPEN_SOURCE >= 500

#include <regex.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <math.h>

#include <poppler/glib/poppler.h>
#include <cairo.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

/* macros */
#define LENGTH(x) sizeof(x)/sizeof((x)[0])
#define CLEAN(m) (m & ~(GDK_MOD2_MASK) & ~(GDK_BUTTON1_MASK) & ~(GDK_BUTTON2_MASK) & ~(GDK_BUTTON3_MASK) & ~(GDK_BUTTON4_MASK) & ~(GDK_BUTTON5_MASK))

/* enums */
enum { NEXT, PREVIOUS, LEFT, RIGHT, UP, DOWN, BOTTOM, TOP, HIDE, HIGHLIGHT,
  DELETE_LAST_WORD, DELETE_LAST_CHAR, DEFAULT, ERROR, WARNING, NEXT_GROUP,
  PREVIOUS_GROUP, ZOOM_IN, ZOOM_OUT, ZOOM_ORIGINAL, ZOOM_SPECIFIC, FORWARD,
  BACKWARD, ADJUST_BESTFIT, ADJUST_WIDTH, ADJUST_NONE, CONTINUOUS, DELETE_LAST,
  ADD_MARKER, EVAL_MARKER, EXPAND, COLLAPSE, SELECT, GOTO_DEFAULT, GOTO_LABELS,
  GOTO_OFFSET, HALF_UP, HALF_DOWN, FULL_UP, FULL_DOWN, NEXT_CHAR, PREVIOUS_CHAR };

/* define modes */
#define ALL        (1 << 0)
#define FULLSCREEN (1 << 1)
#define INDEX      (1 << 2)
#define INSERT     (1 << 3)
#define NORMAL     (1 << 4)
#define VISUAL     (1 << 5)

/* typedefs */
struct CElement
{
  char            *value;
  char            *description;
  struct CElement *next;
};

typedef struct CElement CompletionElement;

struct CGroup
{
  char              *value;
  CompletionElement *elements;
  struct CGroup     *next;
};

typedef struct CGroup CompletionGroup;

typedef struct
{
  CompletionGroup* groups;
} Completion;

typedef struct
{
  char*      command;
  char*      description;
  int        command_id;
  gboolean   is_group;
  GtkWidget* row;
} CompletionRow;

typedef struct
{
  int   n;
  void *data;
} Argument;

typedef struct
{
  char* name;
  int argument;
} ArgumentName;

typedef struct
{
  int mask;
  int key;
  void (*function)(Argument*);
  int mode;
  Argument argument;
} Shortcut;

typedef struct
{
  char* name;
  int mode;
} ModeName;

typedef struct
{
  char* name;
  void (*function)(Argument*);
} ShortcutName;

typedef struct
{
  int mask;
  int key;
  void (*function)(Argument*);
  Argument argument;
} InputbarShortcut;

typedef struct
{
  int direction;
  void (*function)(Argument*);
  Argument argument;
} MouseScrollEvent;

typedef struct
{
  char* command;
  char* abbr;
  gboolean (*function)(int, char**);
  Completion* (*completion)(char*);
  char* description;
} Command;

typedef struct
{
  char* regex;
  void (*function)(char*, Argument*);
  Argument argument;
} BufferCommand;

typedef struct
{
  char identifier;
  gboolean (*function)(char*, Argument*);
  int always;
  Argument argument;
} SpecialCommand;

struct SCList
{
  Shortcut       element;
  struct SCList *next;
};

typedef struct SCList ShortcutList;

typedef struct
{
  char* identifier;
  int   key;
} GDKKey;

typedef struct
{
  PopplerPage *page;
  int          id;
  char        *label;
} Page;

typedef struct
{
  char* name;
  void* variable;
  char  type;
  gboolean render;
  gboolean reinit;
  char* description;
} Setting;

typedef struct
{
  int id;
  int page;
} Marker;

typedef struct
{
  char* id;
  int page;
} Bookmark;

/* zathura */
struct
{
  struct
  {
    GtkWidget         *window;
    GtkBox            *box;
    GtkBox            *continuous;
    GtkScrolledWindow *view;
    GtkViewport       *viewport;
    GtkWidget         *statusbar;
    GtkBox            *statusbar_entries;
    GtkEntry          *inputbar;
    GtkWidget         *index;
    GtkWidget         *information;
    GtkWidget         *drawing_area;
    GtkWidget         *document;
    GdkNativeWindow    embed;
  } UI;

  struct
  {
    GdkColor default_fg;
    GdkColor default_bg;
    GdkColor inputbar_fg;
    GdkColor inputbar_bg;
    GdkColor statusbar_fg;
    GdkColor statusbar_bg;
    GdkColor completion_fg;
    GdkColor completion_bg;
    GdkColor completion_g_bg;
    GdkColor completion_g_fg;
    GdkColor completion_hl_fg;
    GdkColor completion_hl_bg;
    GdkColor notification_e_fg;
    GdkColor notification_e_bg;
    GdkColor notification_w_fg;
    GdkColor notification_w_bg;
    GdkColor recolor_darkcolor;
    GdkColor recolor_lightcolor;
    GdkColor search_highlight;
    GdkColor select_text;
    PangoFontDescription *font;
  } Style;

  struct
  {
    GtkLabel *status_text;
    GtkLabel *status_buffer;
    GtkLabel *status_state;
    GString  *buffer;
    GList    *history;
    int       mode;
    int       viewing_mode;
    gboolean  recolor;
    gboolean  enable_labelmode;
    int       goto_mode;
    int       adjust_mode;
    gboolean  show_index;
    gboolean  show_statusbar;
    gboolean  show_inputbar;
  } Global;

  struct
  {
    ShortcutList  *sclist;
  } Bindings;

  struct
  {
    gdouble x;
    gdouble y;
  } SelectPoint;

  struct
  {
    char* filename;
    char* pages;
    int scroll_percentage;
  } State;

  struct
  {
    Marker* markers;
    int number_of_markers;
    int last;
  } Marker;

  struct
  {
    GFileMonitor* monitor;
  } FileMonitor;

  struct
  {
    GKeyFile *data;
    char     *file;
    Bookmark *bookmarks;
    int       number_of_bookmarks;
  } Bookmarks;

  struct
  {
    PopplerDocument *document;
    char            *file;
    char            *password;
    Page           **pages;
    int              page_number;
    int              page_offset;
    int              number_of_pages;
    int              scale;
    int              rotate;
    cairo_surface_t *surface;
  } PDF;

  struct
  {
    GStaticMutex pdflib_lock;
    GStaticMutex pdf_obj_lock;
    GStaticMutex search_lock;
    GStaticMutex select_lock;
  } Lock;

  struct
  {
    GList* results;
    int page;
    gboolean draw;
  } Search;

  struct
  {
    GThread* search_thread;
    gboolean search_thread_running;
    GThread* inotify_thread;
  } Thread;

  struct
  {
    guint inputbar_activate;
    guint inputbar_key_press_event;
  } Handler;

} Zathura;

/* function declarations */
void init_look();
void init_directories();
void init_keylist();
void init_settings();
void init_zathura();
void add_marker(int);
void build_index(GtkTreeModel*, GtkTreeIter*, PopplerIndexIter*);
void change_mode(int);
void calculate_offset(GtkWidget*, double*, double*);
void close_file(gboolean);
void enter_password();
void highlight_result(int, PopplerRectangle*);
void draw(int);
void eval_marker(int);
void notify(int, char*);
gboolean open_file(char*, char*);
void open_uri(char*);
void out_of_memory();
void update_status();
void read_configuration();
void recalcRectangle(int, PopplerRectangle*);
void setCompletionRowColor(GtkBox*, int, int);
void set_page(int);
void switch_view(GtkWidget*);
GtkEventBox* createCompletionRow(GtkBox*, char*, char*, gboolean);

Completion* completion_init();
CompletionGroup* completion_group_create(char*);
void completion_add_group(Completion*, CompletionGroup*);
void completion_free(Completion*);
void completion_group_add_element(CompletionGroup*, char*, char*);

/* thread declaration */
void* search(void*);

/* shortcut declarations */
void sc_abort(Argument*);
void sc_adjust_window(Argument*);
void sc_change_buffer(Argument*);
void sc_change_mode(Argument*);
void sc_focus_inputbar(Argument*);
void sc_follow(Argument*);
void sc_navigate(Argument*);
void sc_recolor(Argument*);
void sc_reload(Argument*);
void sc_rotate(Argument*);
void sc_scroll(Argument*);
void sc_search(Argument*);
void sc_switch_goto_mode(Argument*);
void sc_navigate_index(Argument*);
void sc_toggle_index(Argument*);
void sc_toggle_inputbar(Argument*);
void sc_toggle_fullscreen(Argument*);
void sc_toggle_statusbar(Argument*);
void sc_quit(Argument*);
void sc_zoom(Argument*);

/* inputbar shortcut declarations */
void isc_abort(Argument*);
void isc_command_history(Argument*);
void isc_completion(Argument*);
void isc_string_manipulation(Argument*);

/* command declarations */
gboolean cmd_bookmark(int, char**);
gboolean cmd_open_bookmark(int, char**);
gboolean cmd_close(int, char**);
gboolean cmd_correct_offset(int, char**);
gboolean cmd_delete_bookmark(int, char**);
gboolean cmd_export(int, char**);
gboolean cmd_info(int, char**);
gboolean cmd_map(int, char**);
gboolean cmd_open(int, char**);
gboolean cmd_print(int, char**);
gboolean cmd_rotate(int, char**);
gboolean cmd_set(int, char**);
gboolean cmd_quit(int, char**);
gboolean cmd_save(int, char**);

/* completion commands */
Completion* cc_bookmark(char*);
Completion* cc_export(char*);
Completion* cc_open(char*);
Completion* cc_print(char*);
Completion* cc_set(char*);

/* buffer command declarations */
void bcmd_evalmarker(char*, Argument*);
void bcmd_goto(char*, Argument*);
void bcmd_scroll(char*, Argument*);
void bcmd_setmarker(char*, Argument*);
void bcmd_zoom(char*, Argument*);

/* special command delcarations */
gboolean scmd_search(char*, Argument*);

/* callback declarations */
gboolean cb_destroy(GtkWidget*, gpointer);
gboolean cb_draw(GtkWidget*, GdkEventExpose*, gpointer);
gboolean cb_index_row_activated(GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*, gpointer);
gboolean cb_inputbar_kb_pressed(GtkWidget*, GdkEventKey*, gpointer);
gboolean cb_inputbar_activate(GtkEntry*, gpointer);
gboolean cb_inputbar_form_activate(GtkEntry*, gpointer);
gboolean cb_inputbar_password_activate(GtkEntry*, gpointer);
gboolean cb_view_kb_pressed(GtkWidget*, GdkEventKey*, gpointer);
gboolean cb_view_resized(GtkWidget*, GtkAllocation*, gpointer);
gboolean cb_view_button_pressed(GtkWidget*, GdkEventButton*, gpointer);
gboolean cb_view_button_release(GtkWidget*, GdkEventButton*, gpointer);
gboolean cb_view_motion_notify(GtkWidget*, GdkEventMotion*, gpointer);
gboolean cb_view_scrolled(GtkWidget*, GdkEventScroll*, gpointer);
gboolean cb_watch_file(GFileMonitor*, GFile*, GFile*, GFileMonitorEvent, gpointer);

/* configuration */
#include "config.h"

/* function implementation */
void
init_look()
{
  /* parse  */
  gdk_color_parse(default_fgcolor,        &(Zathura.Style.default_fg));
  gdk_color_parse(default_bgcolor,        &(Zathura.Style.default_bg));
  gdk_color_parse(inputbar_fgcolor,       &(Zathura.Style.inputbar_fg));
  gdk_color_parse(inputbar_bgcolor,       &(Zathura.Style.inputbar_bg));
  gdk_color_parse(statusbar_fgcolor,      &(Zathura.Style.statusbar_fg));
  gdk_color_parse(statusbar_bgcolor,      &(Zathura.Style.statusbar_bg));
  gdk_color_parse(completion_fgcolor,     &(Zathura.Style.completion_fg));
  gdk_color_parse(completion_bgcolor,     &(Zathura.Style.completion_bg));
  gdk_color_parse(completion_g_fgcolor,   &(Zathura.Style.completion_g_fg));
  gdk_color_parse(completion_g_fgcolor,   &(Zathura.Style.completion_g_fg));
  gdk_color_parse(completion_hl_fgcolor,  &(Zathura.Style.completion_hl_fg));
  gdk_color_parse(completion_hl_bgcolor,  &(Zathura.Style.completion_hl_bg));
  gdk_color_parse(notification_e_fgcolor, &(Zathura.Style.notification_e_fg));
  gdk_color_parse(notification_e_bgcolor, &(Zathura.Style.notification_e_bg));
  gdk_color_parse(notification_w_fgcolor, &(Zathura.Style.notification_w_fg));
  gdk_color_parse(notification_w_bgcolor, &(Zathura.Style.notification_w_bg));
  gdk_color_parse(recolor_darkcolor,      &(Zathura.Style.recolor_darkcolor));
  gdk_color_parse(recolor_lightcolor,     &(Zathura.Style.recolor_lightcolor));
  gdk_color_parse(search_highlight,       &(Zathura.Style.search_highlight));
  gdk_color_parse(select_text,            &(Zathura.Style.select_text));
  Zathura.Style.font = pango_font_description_from_string(font);

  /* drawing area */
  gtk_widget_modify_bg(GTK_WIDGET(Zathura.UI.drawing_area), GTK_STATE_NORMAL, &(Zathura.Style.default_bg));

  /* statusbar */
  gtk_widget_modify_bg(GTK_WIDGET(Zathura.UI.statusbar), GTK_STATE_NORMAL, &(Zathura.Style.statusbar_bg));

  gtk_widget_modify_fg(GTK_WIDGET(Zathura.Global.status_text),  GTK_STATE_NORMAL, &(Zathura.Style.statusbar_fg));
  gtk_widget_modify_fg(GTK_WIDGET(Zathura.Global.status_state), GTK_STATE_NORMAL, &(Zathura.Style.statusbar_fg));
  gtk_widget_modify_fg(GTK_WIDGET(Zathura.Global.status_buffer), GTK_STATE_NORMAL, &(Zathura.Style.statusbar_fg));

  gtk_widget_modify_font(GTK_WIDGET(Zathura.Global.status_text),  Zathura.Style.font);
  gtk_widget_modify_font(GTK_WIDGET(Zathura.Global.status_state), Zathura.Style.font);
  gtk_widget_modify_font(GTK_WIDGET(Zathura.Global.status_buffer), Zathura.Style.font);

  /* inputbar */
  gtk_widget_modify_base(GTK_WIDGET(Zathura.UI.inputbar), GTK_STATE_NORMAL, &(Zathura.Style.inputbar_bg));
  gtk_widget_modify_text(GTK_WIDGET(Zathura.UI.inputbar), GTK_STATE_NORMAL, &(Zathura.Style.inputbar_fg));
  gtk_widget_modify_font(GTK_WIDGET(Zathura.UI.inputbar),                     Zathura.Style.font);

  /* scrollbars */
  if(show_scrollbars)
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.UI.view), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  else
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.UI.view), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

  /* inputbar */
  if(Zathura.Global.show_inputbar)
    gtk_widget_show(GTK_WIDGET(Zathura.UI.inputbar));
  else
    gtk_widget_hide(GTK_WIDGET(Zathura.UI.inputbar));

  /* statusbar */
  if(Zathura.Global.show_statusbar)
    gtk_widget_show(GTK_WIDGET(Zathura.UI.statusbar));
  else
    gtk_widget_hide(GTK_WIDGET(Zathura.UI.statusbar));
}

void
init_directories()
{
  /* create zathura directory */
  gchar *base_directory = g_build_filename(g_get_home_dir(), ZATHURA_DIR, NULL);
  g_mkdir_with_parents(base_directory,  0771);
  g_free(base_directory);

  /* create or open existing bookmark file */
  Zathura.Bookmarks.data = g_key_file_new();
  char* bookmarks = g_strdup_printf("%s/%s/%s", g_get_home_dir(), ZATHURA_DIR, BOOKMARK_FILE);

  if(!g_file_test(bookmarks, G_FILE_TEST_IS_REGULAR))
  {
    /* file does not exist */
    g_file_set_contents(bookmarks, "# Zathura bookmarks\n", -1, NULL);
  }

  GError* error = NULL;
  if(!g_key_file_load_from_file(Zathura.Bookmarks.data, bookmarks,
        G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error))
  {
    gchar* message = g_strdup_printf("Could not load bookmark file: %s", error->message);
    notify(ERROR, message);
    g_free(message);
  }

  Zathura.Bookmarks.file = g_strdup(bookmarks);
  g_free(bookmarks);
}

void
init_keylist()
{
  ShortcutList* e = NULL;
  ShortcutList* p = NULL;

  int i;
  for(i = 0; i < LENGTH(shortcuts); i++)
  {
    e = malloc(sizeof(ShortcutList));
    if(!e)
      out_of_memory();

    e->element = shortcuts[i];
    e->next    = NULL;

    if(!Zathura.Bindings.sclist)
      Zathura.Bindings.sclist = e;
    if(p)
      p->next = e;

    p = e;
  }
}

void
init_settings()
{
  Zathura.State.filename     = (char*) default_text;
  Zathura.Global.adjust_mode = adjust_open;

  gtk_window_set_default_size(GTK_WINDOW(Zathura.UI.window), default_width, default_height);
}

void
init_zathura()
{
  /* init mutexes */
  g_static_mutex_init(&(Zathura.Lock.pdflib_lock));
  g_static_mutex_init(&(Zathura.Lock.search_lock));
  g_static_mutex_init(&(Zathura.Lock.pdf_obj_lock));
  g_static_mutex_init(&(Zathura.Lock.select_lock));
  g_static_mutex_init(&(Zathura.Lock.select_lock));

  /* other */
  Zathura.Global.mode           = NORMAL;
  Zathura.Global.viewing_mode   = NORMAL;
  Zathura.Global.recolor        = 0;
  Zathura.Global.goto_mode      = GOTO_MODE;
  Zathura.Global.show_index     = FALSE;
  Zathura.Global.show_inputbar  = TRUE;
  Zathura.Global.show_statusbar = TRUE;

  Zathura.State.pages             = g_strdup_printf("");
  Zathura.State.scroll_percentage = 0;

  Zathura.Marker.markers           = NULL;
  Zathura.Marker.number_of_markers =  0;
  Zathura.Marker.last              = -1;

  Zathura.Search.results = NULL;
  Zathura.Search.page    = 0;
  Zathura.Search.draw    = FALSE;

  Zathura.FileMonitor.monitor = NULL;

  /* window */
  if(Zathura.UI.embed)
    Zathura.UI.window = gtk_plug_new(Zathura.UI.embed);
  else
    Zathura.UI.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  /* UI */
  Zathura.UI.box               = GTK_BOX(gtk_vbox_new(FALSE, 0));
  Zathura.UI.continuous        = GTK_BOX(gtk_vbox_new(FALSE, 0));
  Zathura.UI.view              = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
  Zathura.UI.viewport          = GTK_VIEWPORT(gtk_viewport_new(NULL, NULL));
  Zathura.UI.drawing_area      = gtk_drawing_area_new();
  Zathura.UI.statusbar         = gtk_event_box_new();
  Zathura.UI.statusbar_entries = GTK_BOX(gtk_hbox_new(FALSE, 0));
  Zathura.UI.inputbar          = GTK_ENTRY(gtk_entry_new());
  Zathura.UI.document            = gtk_event_box_new();

  /* window */
  gtk_window_set_title(GTK_WINDOW(Zathura.UI.window), "zathura");
  GdkGeometry hints = { 1, 1 };
  gtk_window_set_geometry_hints(GTK_WINDOW(Zathura.UI.window), NULL, &hints, GDK_HINT_MIN_SIZE);
  g_signal_connect(G_OBJECT(Zathura.UI.window), "destroy", G_CALLBACK(cb_destroy), NULL);

  /* box */
  gtk_box_set_spacing(Zathura.UI.box, 0);
  gtk_container_add(GTK_CONTAINER(Zathura.UI.window), GTK_WIDGET(Zathura.UI.box));

  /* continuous */
  gtk_box_set_spacing(Zathura.UI.continuous, 5);

  /* events */
  gtk_container_add(GTK_CONTAINER(Zathura.UI.document), GTK_WIDGET(Zathura.UI.drawing_area));
  gtk_widget_add_events(GTK_WIDGET(Zathura.UI.document), GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  g_signal_connect(G_OBJECT(Zathura.UI.document), "button-press-event",   G_CALLBACK(cb_view_button_pressed), NULL);
  g_signal_connect(G_OBJECT(Zathura.UI.document), "button-release-event", G_CALLBACK(cb_view_button_release), NULL);
  g_signal_connect(G_OBJECT(Zathura.UI.document), "motion-notify-event",  G_CALLBACK(cb_view_motion_notify),  NULL);
  gtk_widget_show(Zathura.UI.document);

  /* view */
  g_signal_connect(G_OBJECT(Zathura.UI.view), "key-press-event",      G_CALLBACK(cb_view_kb_pressed),     NULL);
  g_signal_connect(G_OBJECT(Zathura.UI.view), "size-allocate",        G_CALLBACK(cb_view_resized),        NULL);
  g_signal_connect(G_OBJECT(Zathura.UI.view), "scroll-event",         G_CALLBACK(cb_view_scrolled),       NULL);
  gtk_container_add(GTK_CONTAINER(Zathura.UI.view), GTK_WIDGET(Zathura.UI.viewport));
  gtk_viewport_set_shadow_type(Zathura.UI.viewport, GTK_SHADOW_NONE);

  /* drawing area */
  gtk_widget_show(Zathura.UI.drawing_area);
  g_signal_connect(G_OBJECT(Zathura.UI.drawing_area), "expose-event", G_CALLBACK(cb_draw), NULL);

  /* statusbar */
  Zathura.Global.status_text   = GTK_LABEL(gtk_label_new(NULL));
  Zathura.Global.status_state  = GTK_LABEL(gtk_label_new(NULL));
  Zathura.Global.status_buffer = GTK_LABEL(gtk_label_new(NULL));

  gtk_misc_set_alignment(GTK_MISC(Zathura.Global.status_text),  0.0, 0.0);
  gtk_misc_set_alignment(GTK_MISC(Zathura.Global.status_state), 1.0, 0.0);
  gtk_misc_set_alignment(GTK_MISC(Zathura.Global.status_buffer), 1.0, 0.0);

  gtk_misc_set_padding(GTK_MISC(Zathura.Global.status_text),  2.0, 4.0);
  gtk_misc_set_padding(GTK_MISC(Zathura.Global.status_state), 2.0, 4.0);
  gtk_misc_set_padding(GTK_MISC(Zathura.Global.status_buffer), 2.0, 4.0);

  gtk_label_set_use_markup(Zathura.Global.status_text,  TRUE);
  gtk_label_set_use_markup(Zathura.Global.status_state, TRUE);
  gtk_label_set_use_markup(Zathura.Global.status_buffer, TRUE);

  gtk_box_pack_start(Zathura.UI.statusbar_entries, GTK_WIDGET(Zathura.Global.status_text),  TRUE,  TRUE,  2);
  gtk_box_pack_start(Zathura.UI.statusbar_entries, GTK_WIDGET(Zathura.Global.status_buffer), FALSE, FALSE, 2);
  gtk_box_pack_start(Zathura.UI.statusbar_entries, GTK_WIDGET(Zathura.Global.status_state), FALSE, FALSE, 2);

  gtk_container_add(GTK_CONTAINER(Zathura.UI.statusbar), GTK_WIDGET(Zathura.UI.statusbar_entries));

  /* inputbar */
  gtk_entry_set_inner_border(Zathura.UI.inputbar, NULL);
  gtk_entry_set_has_frame(   Zathura.UI.inputbar, FALSE);
  gtk_editable_set_editable( GTK_EDITABLE(Zathura.UI.inputbar), TRUE);

  Zathura.Handler.inputbar_key_press_event =
    g_signal_connect(G_OBJECT(Zathura.UI.inputbar), "key-press-event",   G_CALLBACK(cb_inputbar_kb_pressed), NULL);
  Zathura.Handler.inputbar_activate =
    g_signal_connect(G_OBJECT(Zathura.UI.inputbar), "activate",          G_CALLBACK(cb_inputbar_activate),   NULL);

  /* packing */
  gtk_box_pack_start(Zathura.UI.box, GTK_WIDGET(Zathura.UI.view),      TRUE,  TRUE,  0);
  gtk_box_pack_start(Zathura.UI.box, GTK_WIDGET(Zathura.UI.statusbar), FALSE, FALSE, 0);
  gtk_box_pack_end(  Zathura.UI.box, GTK_WIDGET(Zathura.UI.inputbar),  FALSE, FALSE, 0);
}

void
add_marker(int id)
{
  if( (id < 0x30) || (id > 0x7A))
    return;

  /* current information */
  int page_number = Zathura.PDF.page_number;

  /* search if entry already exists */
  int i;
  for(i = 0; i < Zathura.Marker.number_of_markers; i++)
  {
    if(Zathura.Marker.markers[i].id == id)
    {
      Zathura.Marker.markers[i].page = page_number;
      Zathura.Marker.last            = page_number;
      return;
    }
  }

  /* add new marker */
  int marker_index = Zathura.Marker.number_of_markers++;
  Zathura.Marker.markers = realloc(Zathura.Marker.markers, sizeof(Marker) *
      (Zathura.Marker.number_of_markers));

  Zathura.Marker.markers[marker_index].id   = id;
  Zathura.Marker.markers[marker_index].page = page_number;
  Zathura.Marker.last                       = page_number;
}

void
build_index(GtkTreeModel* model, GtkTreeIter* parent, PopplerIndexIter* index_iter)
{
  do
  {
    GtkTreeIter       tree_iter;
    PopplerIndexIter *child;
    PopplerAction    *action;
    gchar            *markup;

    action = poppler_index_iter_get_action(index_iter);
    if(!action)
      continue;

    markup = g_markup_escape_text (action->any.title, -1);

    gtk_tree_store_append(GTK_TREE_STORE(model), &tree_iter, parent);
    gtk_tree_store_set(GTK_TREE_STORE(model), &tree_iter, 0, markup, 1, action, -1);
    g_object_weak_ref(G_OBJECT(model), (GWeakNotify) poppler_action_free, action);
    g_free(markup);

    child = poppler_index_iter_get_child(index_iter);
    if(child)
      build_index(model, &tree_iter, child);
    poppler_index_iter_free(child);
  } while(poppler_index_iter_next(index_iter));
}

void
draw(int page_id)
{
  if(!Zathura.PDF.document || page_id < 0 || page_id >= Zathura.PDF.number_of_pages)
    return;

  double page_width, page_height;
  double width, height;

  double scale = ((double) Zathura.PDF.scale / 100.0);

  int rotate = Zathura.PDF.rotate;

  Page *current_page = Zathura.PDF.pages[page_id];

  if(Zathura.PDF.surface)
    cairo_surface_destroy(Zathura.PDF.surface);
  Zathura.PDF.surface = NULL;

  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  poppler_page_get_size(current_page->page, &page_width, &page_height);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

  if(rotate == 0 || rotate == 180)
  {
    width  = page_width  * scale;
    height = page_height * scale;
  }
  else
  {
    width  = page_height * scale;
    height = page_width  * scale;
  }

  cairo_t *cairo;
  Zathura.PDF.surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
  cairo = cairo_create(Zathura.PDF.surface);

  cairo_save(cairo);
  cairo_set_source_rgb(cairo, 1, 1, 1);
  cairo_rectangle(cairo, 0, 0, width, height);
  cairo_fill(cairo);
  cairo_restore(cairo);
  cairo_save(cairo);

  switch(rotate)
  {
    case 90:
      cairo_translate(cairo, width, 0);
      break;
    case 180:
      cairo_translate(cairo, width, height);
      break;
    case 270:
      cairo_translate(cairo, 0, height);
      break;
    default:
      cairo_translate(cairo, 0, 0);
  }

  if(scale != 1.0)
    cairo_scale(cairo, scale, scale);

  if(rotate != 0)
    cairo_rotate(cairo, rotate * G_PI / 180.0);

  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  poppler_page_render(current_page->page, cairo);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

  cairo_restore(cairo);
  cairo_destroy(cairo);

  if(Zathura.Global.recolor)
  {
    unsigned char* image = cairo_image_surface_get_data(Zathura.PDF.surface);
    int x, y;

    int width     = cairo_image_surface_get_width(Zathura.PDF.surface);
    int height    = cairo_image_surface_get_height(Zathura.PDF.surface);
    int rowstride = cairo_image_surface_get_stride(Zathura.PDF.surface);

    /* recolor code based on qimageblitz library flatten() function
    (http://sourceforge.net/projects/qimageblitz/) */

    int r1 = Zathura.Style.recolor_darkcolor.red    / 257;
    int g1 = Zathura.Style.recolor_darkcolor.green  / 257;
    int b1 = Zathura.Style.recolor_darkcolor.blue   / 257;
    int r2 = Zathura.Style.recolor_lightcolor.red   / 257;
    int g2 = Zathura.Style.recolor_lightcolor.green / 257;
    int b2 = Zathura.Style.recolor_lightcolor.blue  / 257;

    int min = 0x00;
    int max = 0xFF;
    int mean;

    float sr = ((float) r2 - r1) / (max - min);
    float sg = ((float) g2 - g1) / (max - min);
    float sb = ((float) b2 - b1) / (max - min);

    for (y = 0; y < height; y++)
    {
      unsigned char* data = image + y * rowstride;

      for (x = 0; x < width; x++)
      {
        mean = (data[0] + data[1] + data[2]) / 3;
        data[2] = sr * (mean - min) + r1 + 0.5;
        data[1] = sg * (mean - min) + g1 + 0.5;
        data[0] = sb * (mean - min) + b1 + 0.5;
        data += 4;
      }
    }
  }

  gtk_widget_set_size_request(Zathura.UI.drawing_area, width, height);
  gtk_widget_queue_draw(Zathura.UI.drawing_area);
}

void
change_mode(int mode)
{
  char* mode_text;

  switch(mode)
  {
    case INSERT:
      mode_text = "-- INSERT --";
      break;
    case VISUAL:
      mode_text = "-- VISUAL --";
      break;
    case ADD_MARKER:
      mode_text = "";
      break;
    case EVAL_MARKER:
      mode_text = "";
      break;
    default:
      mode_text = "";
      mode      = NORMAL;
      break;
  }

  Zathura.Global.mode = mode;
  notify(DEFAULT, mode_text);
}

void
calculate_offset(GtkWidget* widget, double* offset_x, double* offset_y)
{
  double page_width, page_height, width, height;
  double scale = ((double) Zathura.PDF.scale / 100.0);

  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  poppler_page_get_size(Zathura.PDF.pages[Zathura.PDF.page_number]->page, &page_width, &page_height);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

  if(Zathura.PDF.rotate == 0 || Zathura.PDF.rotate == 180)
  {
    width  = page_width  * scale;
    height = page_height * scale;
  }
  else
  {
    width  = page_height * scale;
    height = page_width  * scale;
  }

  int window_x, window_y;
  gdk_drawable_get_size(widget->window, &window_x, &window_y);

  if (window_x > width)
    *offset_x = (window_x - width) / 2;
  else
    *offset_x = 0;

  if (window_y > height)
    *offset_y = (window_y - height) / 2;
  else
    *offset_y = 0;
}

void
close_file(gboolean keep_monitor)
{
  if(!Zathura.PDF.document)
    return;

  /* clean up pages */
  int i;
  for(i = 0; i < Zathura.PDF.number_of_pages; i++)
  {
    Page* current_page = Zathura.PDF.pages[i];
    g_object_unref(current_page->page);
  }

  /* save bookmarks */
  if(Zathura.Bookmarks.data)
  {
    /* set current page */
    g_key_file_set_integer(Zathura.Bookmarks.data, Zathura.PDF.file,
        BM_PAGE_ENTRY, Zathura.PDF.page_number);

    /* set page offset */
    g_key_file_set_integer(Zathura.Bookmarks.data, Zathura.PDF.file,
        BM_PAGE_OFFSET, Zathura.PDF.page_offset);

    /* save bookmarks */
    int i;
    for(i = 0; i < Zathura.Bookmarks.number_of_bookmarks; i++)
      g_key_file_set_integer(Zathura.Bookmarks.data, Zathura.PDF.file,
          Zathura.Bookmarks.bookmarks[i].id, Zathura.Bookmarks.bookmarks[i].page);

    /* convert file and save it */
    gchar* bookmarks = g_key_file_to_data(Zathura.Bookmarks.data, NULL, NULL);
    g_file_set_contents(Zathura.Bookmarks.file, bookmarks, -1, NULL);
    g_free(bookmarks);
  }

  /* inotify */
  if(!keep_monitor)
  {
    g_object_unref(Zathura.FileMonitor.monitor);
    Zathura.FileMonitor.monitor = NULL;
  }

  /* reset values */
  free(Zathura.PDF.pages);
  g_object_unref(Zathura.PDF.document);
  g_free(Zathura.State.pages);
  gtk_window_set_title(GTK_WINDOW(Zathura.UI.window), "zathura");

  Zathura.State.pages         = g_strdup_printf("");
  Zathura.State.filename      = (char*) default_text;;

  g_static_mutex_lock(&(Zathura.Lock.pdf_obj_lock));
  Zathura.PDF.document        = NULL;
  if(!keep_monitor)
  {
    Zathura.PDF.file            = NULL;
    Zathura.PDF.password        = NULL;
    Zathura.PDF.page_number     = 0;
    Zathura.PDF.scale           = 0;
    Zathura.PDF.rotate          = 0;
  }
  Zathura.PDF.number_of_pages = 0;
  Zathura.PDF.page_offset     = 0;
  g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));

  /* destroy index */
  if(Zathura.UI.index)
  {
    gtk_widget_destroy(Zathura.UI.index);
    Zathura.UI.index = NULL;
  }

  /* destroy information */
  if(Zathura.UI.information)
  {
    gtk_widget_destroy(Zathura.UI.information);
    Zathura.UI.information = NULL;
  }

  /* free markers */
  if(Zathura.Marker.markers)
    free(Zathura.Marker.markers);
  Zathura.Marker.number_of_markers =  0;
  Zathura.Marker.last              = -1;

  update_status();
}

void
enter_password()
{
  /* replace default inputbar handler */
  g_signal_handler_disconnect((gpointer) Zathura.UI.inputbar, Zathura.Handler.inputbar_activate);
  Zathura.Handler.inputbar_activate = g_signal_connect(G_OBJECT(Zathura.UI.inputbar), "activate", G_CALLBACK(cb_inputbar_password_activate), NULL);

  Argument argument;
  argument.data = "Enter password: ";
  sc_focus_inputbar(&argument);
}

void
eval_marker(int id)
{
  /* go to last marker */
  if(id == 0x27)
  {
    int current_page = Zathura.PDF.page_number;
    set_page(Zathura.Marker.last);
    Zathura.Marker.last = current_page;
    return;
  }

  /* search markers */
  int i;
  for(i = 0; i < Zathura.Marker.number_of_markers; i++)
  {
    if(Zathura.Marker.markers[i].id == id)
    {
      set_page(Zathura.Marker.markers[i].page);
      return;
    }
  }
}

void
highlight_result(int page_id, PopplerRectangle* rectangle)
{
  PopplerRectangle* trect = poppler_rectangle_copy(rectangle);
  cairo_t *cairo = cairo_create(Zathura.PDF.surface);
  cairo_set_source_rgba(cairo, Zathura.Style.search_highlight.red, Zathura.Style.search_highlight.green,
      Zathura.Style.search_highlight.blue, transparency);

  recalcRectangle(page_id, trect);
  cairo_rectangle(cairo, trect->x1, trect->y1, (trect->x2 - trect->x1), (trect->y2 - trect->y1));
  poppler_rectangle_free(trect);
  cairo_fill(cairo);
}

void notify(int level, char* message)
{
  switch(level)
  {
    case ERROR:
      gtk_widget_modify_base(GTK_WIDGET(Zathura.UI.inputbar), GTK_STATE_NORMAL, &(Zathura.Style.notification_e_bg));
      gtk_widget_modify_text(GTK_WIDGET(Zathura.UI.inputbar), GTK_STATE_NORMAL, &(Zathura.Style.notification_e_fg));
      break;
    case WARNING:
      gtk_widget_modify_base(GTK_WIDGET(Zathura.UI.inputbar), GTK_STATE_NORMAL, &(Zathura.Style.notification_w_bg));
      gtk_widget_modify_text(GTK_WIDGET(Zathura.UI.inputbar), GTK_STATE_NORMAL, &(Zathura.Style.notification_w_fg));
      break;
    default:
      gtk_widget_modify_base(GTK_WIDGET(Zathura.UI.inputbar), GTK_STATE_NORMAL, &(Zathura.Style.inputbar_bg));
      gtk_widget_modify_text(GTK_WIDGET(Zathura.UI.inputbar), GTK_STATE_NORMAL, &(Zathura.Style.inputbar_fg));
      break;
  }

  if(message)
    gtk_entry_set_text(Zathura.UI.inputbar, message);
}

gboolean
open_file(char* path, char* password)
{
  g_static_mutex_lock(&(Zathura.Lock.pdf_obj_lock));

  /* specify path max */
  size_t pm;
#ifdef PATH_MAX
  pm = PATH_MAX;
#else
  pm = pathconf(path,_PC_PATH_MAX);
  if(pm <= 0)
    pm = 4096;
#endif

  /* get filename */
  char* file = (char*) calloc(sizeof(char), pm);
  if(!file || !realpath(path, file))
  {
    notify(ERROR, "File does not exist");
    if(file)
      free(file);
    return FALSE;
  }

  if(path[0] == '~')
  {
    char* home_path = getenv("HOME");
    int file_len = strlen(home_path) + strlen(path) - 1;
    file = malloc(file_len);
    if(!file)
      out_of_memory();

    snprintf(file, file_len, "%s%s", getenv("HOME"), path + 1);
  }

  /* check if file exists */
  if(!g_file_test(file, G_FILE_TEST_IS_REGULAR))
  {
    notify(ERROR, "File does not exist");
    free(file);
    g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));
    return FALSE;
  }

  /* close old file */
  g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));
  close_file(FALSE);
  g_static_mutex_lock(&(Zathura.Lock.pdf_obj_lock));

  /* format path */
  GError* error = NULL;
  char* file_uri = g_filename_to_uri(file, NULL, &error);
  if (!file_uri)
  {
    char* message = g_strdup_printf("Can not open file: %s", error->message);
    notify(ERROR, message);
    g_free(message);
    g_error_free(error);
    g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));
    return FALSE;
  }

  /* open file */
  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  Zathura.PDF.document = poppler_document_new_from_file(file_uri, password, &error);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

  if(!Zathura.PDF.document)
  {
    if(error->code == 1)
    {
      g_free(file_uri);
      g_error_free(error);
      Zathura.PDF.file = file;
      g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));
      enter_password();
      return FALSE;
    }
    else
    {
      char* message = g_strdup_printf("Can not open file: %s", error->message);
      notify(ERROR, message);
      g_free(file_uri);
      g_free(message);
      g_error_free(error);
      g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));
      return FALSE;
    }
  }

  /* save password */
  Zathura.PDF.password = password;

  /* inotify */
  if(!Zathura.FileMonitor.monitor)
  {
    GFile* file = g_file_new_for_uri(file_uri);

    if(file)
    {
      Zathura.FileMonitor.monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, NULL, NULL);
      if(Zathura.FileMonitor.monitor)
        g_signal_connect(G_OBJECT(Zathura.FileMonitor.monitor), "changed", G_CALLBACK(cb_watch_file), NULL);
    }
  }

  g_free(file_uri);

  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  Zathura.PDF.number_of_pages = poppler_document_get_n_pages(Zathura.PDF.document);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));
  Zathura.PDF.file            = file;
  Zathura.PDF.scale           = 100;
  Zathura.PDF.rotate          = 0;
  Zathura.State.filename      = g_markup_escape_text(file, -1);
  Zathura.PDF.pages           = malloc(Zathura.PDF.number_of_pages * sizeof(Page*));

  if(!Zathura.PDF.pages)
    out_of_memory();

  /* get pages and check label mode */
  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  Zathura.Global.enable_labelmode = FALSE;

  int i;
  for(i = 0; i < Zathura.PDF.number_of_pages; i++)
  {
    Zathura.PDF.pages[i] = malloc(sizeof(Page));
    if(!Zathura.PDF.pages[i])
      out_of_memory();

    Zathura.PDF.pages[i]->id = i + 1;
    Zathura.PDF.pages[i]->page = poppler_document_get_page(Zathura.PDF.document, i);
    g_object_get(G_OBJECT(Zathura.PDF.pages[i]->page), "label", &(Zathura.PDF.pages[i]->label), NULL);

    /* check if it is necessary to use the label mode */
    int label_int = atoi(Zathura.PDF.pages[i]->label);
    if(label_int == 0 || label_int != (i+1))
      Zathura.Global.enable_labelmode = TRUE;
  }
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

  /* set correct goto mode */
  if(!Zathura.Global.enable_labelmode && GOTO_MODE == GOTO_LABELS)
    Zathura.Global.goto_mode = GOTO_DEFAULT;

  /* start page */
  int start_page          = 0;
  Zathura.PDF.page_offset = 0;

  /* bookmarks */
  if(Zathura.Bookmarks.data && g_key_file_has_group(Zathura.Bookmarks.data, file))
  {
    /* get last opened page */
    if(g_key_file_has_key(Zathura.Bookmarks.data, file, BM_PAGE_ENTRY, NULL))
      start_page = g_key_file_get_integer(Zathura.Bookmarks.data, file, BM_PAGE_ENTRY, NULL);

    /* get page offset */
    if(g_key_file_has_key(Zathura.Bookmarks.data, file, BM_PAGE_OFFSET, NULL))
      Zathura.PDF.page_offset = g_key_file_get_integer(Zathura.Bookmarks.data, file, BM_PAGE_OFFSET, NULL);
    if((Zathura.PDF.page_offset != 0) && (Zathura.PDF.page_offset != GOTO_OFFSET))
      Zathura.PDF.page_offset = GOTO_OFFSET;

    /* open and read bookmark file */
    gsize i              = 0;
    gsize number_of_keys = 0;
    char** keys          = g_key_file_get_keys(Zathura.Bookmarks.data, file, &number_of_keys, NULL);

    for(i = 0; i < number_of_keys; i++)
    {
      if(strcmp(keys[i], BM_PAGE_ENTRY) && strcmp(keys[i], BM_PAGE_OFFSET))
      {
        Zathura.Bookmarks.bookmarks = realloc(Zathura.Bookmarks.bookmarks,
            (Zathura.Bookmarks.number_of_bookmarks + 1) * sizeof(Bookmark));

        Zathura.Bookmarks.bookmarks[Zathura.Bookmarks.number_of_bookmarks].id   = keys[i];
        Zathura.Bookmarks.bookmarks[Zathura.Bookmarks.number_of_bookmarks].page =
          g_key_file_get_integer(Zathura.Bookmarks.data, file, keys[i], NULL);

        Zathura.Bookmarks.number_of_bookmarks++;
      }
    }
  }

  /* set window title */
  gtk_window_set_title(GTK_WINDOW(Zathura.UI.window), basename(file));

  /* show document */
  set_page(start_page);
  update_status();

  g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));
  isc_abort(NULL);
  return TRUE;
}

void open_uri(char* uri)
{
  char* uri_cmd = g_strdup_printf(uri_command, uri);
  system(uri_cmd);
  g_free(uri_cmd);
}

void out_of_memory()
{
  printf("error: out of memory\n");
  exit(-1);
}

void
update_status()
{
  /* update text */
  gtk_label_set_markup((GtkLabel*) Zathura.Global.status_text, Zathura.State.filename);

  /* update pages */
  if( Zathura.PDF.document && Zathura.PDF.pages )
  {
    int page = Zathura.PDF.page_number;
    g_free(Zathura.State.pages);

    Zathura.State.pages = g_strdup_printf("[%i/%i]", page + 1, Zathura.PDF.number_of_pages);
  }

  /* update state */
  char* zoom_level  = (Zathura.PDF.scale != 0) ? g_strdup_printf("%d%%", Zathura.PDF.scale) : g_strdup("");
  char* goto_mode   = (Zathura.Global.goto_mode == GOTO_LABELS) ? "L" :
    (Zathura.Global.goto_mode == GOTO_OFFSET) ? "O" : "D";
  char* status_text = g_strdup_printf("%s [%s] %s", zoom_level, goto_mode, Zathura.State.pages);
  gtk_label_set_markup((GtkLabel*) Zathura.Global.status_state, status_text);
  g_free(status_text);
  g_free(zoom_level);
}

void
read_configuration()
{
  char* zathurarc = g_strdup_printf("%s/%s/%s", g_get_home_dir(), ZATHURA_DIR, ZATHURA_RC);

  if(!zathurarc)
    return;

  if(g_file_test(zathurarc, G_FILE_TEST_IS_REGULAR))
  {
    char* content = NULL;

    if(g_file_get_contents(zathurarc, &content, NULL, NULL))
    {
      gchar **lines = g_strsplit(content, "\n", -1);
      int     n     = g_strv_length(lines) - 1;

      int i;
      for(i = 0; i <= n; i++)
      {
        if(!strlen(lines[i]))
          continue;

        gchar **tokens = g_strsplit(lines[i], " ", -1);
        int     length = g_strv_length(tokens);

        if(!strcmp(tokens[0], "set"))
          cmd_set(length - 1, tokens + 1);
        else if(!strcmp(tokens[0], "map"))
          cmd_map(length - 1, tokens + 1);
      }
    }
  }

  g_free(zathurarc);
}

void
recalcRectangle(int page_id, PopplerRectangle* rectangle)
{
  double page_width, page_height;
  double x1 = rectangle->x1;
  double x2 = rectangle->x2;
  double y1 = rectangle->y1;
  double y2 = rectangle->y2;

  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  poppler_page_get_size(Zathura.PDF.pages[page_id]->page, &page_width, &page_height);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

  double scale = ((double) Zathura.PDF.scale / 100.0);

  int rotate = Zathura.PDF.rotate;

  switch(rotate)
  {
    case 90:
      rectangle->x1 = y2 * scale;
      rectangle->y1 = x1 * scale;
      rectangle->x2 = y1 * scale;
      rectangle->y2 = x2 * scale;
      break;
    case 180:
      rectangle->x1 = (page_width  - x2) * scale;
      rectangle->y1 = y2 * scale;
      rectangle->x2 = (page_width  - x1) * scale;
      rectangle->y2 = y1 * scale;
      break;
    case 270:
      rectangle->x1 = (page_height - y1) * scale;
      rectangle->y1 = (page_width  - x2) * scale;
      rectangle->x2 = (page_height - y2) * scale;
      rectangle->y2 = (page_width  - x1) * scale;
      break;
    default:
      rectangle->x1 = x1  * scale;
      rectangle->y1 = (page_height - y1) * scale;
      rectangle->x2 = x2  * scale;
      rectangle->y2 = (page_height - y2) * scale;
  }
}

GtkEventBox*
createCompletionRow(GtkBox* results, char* command, char* description, gboolean group)
{
  GtkBox      *col = GTK_BOX(gtk_hbox_new(FALSE, 0));
  GtkEventBox *row = GTK_EVENT_BOX(gtk_event_box_new());

  GtkLabel *show_command     = GTK_LABEL(gtk_label_new(NULL));
  GtkLabel *show_description = GTK_LABEL(gtk_label_new(NULL));

  gtk_misc_set_alignment(GTK_MISC(show_command),     0.0, 0.0);
  gtk_misc_set_alignment(GTK_MISC(show_description), 0.0, 0.0);

  if(group)
  {
    gtk_misc_set_padding(GTK_MISC(show_command),     2.0, 4.0);
    gtk_misc_set_padding(GTK_MISC(show_description), 2.0, 4.0);
  }
  else
  {
    gtk_misc_set_padding(GTK_MISC(show_command),     1.0, 1.0);
    gtk_misc_set_padding(GTK_MISC(show_description), 1.0, 1.0);
  }

  gtk_label_set_use_markup(show_command,     TRUE);
  gtk_label_set_use_markup(show_description, TRUE);

  gtk_label_set_markup(show_command,     g_markup_printf_escaped(FORMAT_COMMAND,     command ? command : ""));
  gtk_label_set_markup(show_description, g_markup_printf_escaped(FORMAT_DESCRIPTION, description ? description : ""));

  if(group)
  {
    gtk_widget_modify_fg(GTK_WIDGET(show_command),     GTK_STATE_NORMAL, &(Zathura.Style.completion_g_fg));
    gtk_widget_modify_fg(GTK_WIDGET(show_description), GTK_STATE_NORMAL, &(Zathura.Style.completion_g_fg));
    gtk_widget_modify_bg(GTK_WIDGET(row),              GTK_STATE_NORMAL, &(Zathura.Style.completion_g_bg));
  }
  else
  {
    gtk_widget_modify_fg(GTK_WIDGET(show_command),     GTK_STATE_NORMAL, &(Zathura.Style.completion_fg));
    gtk_widget_modify_fg(GTK_WIDGET(show_description), GTK_STATE_NORMAL, &(Zathura.Style.completion_fg));
    gtk_widget_modify_bg(GTK_WIDGET(row),              GTK_STATE_NORMAL, &(Zathura.Style.completion_bg));
  }

  gtk_widget_modify_font(GTK_WIDGET(show_command),     Zathura.Style.font);
  gtk_widget_modify_font(GTK_WIDGET(show_description), Zathura.Style.font);

  gtk_box_pack_start(GTK_BOX(col), GTK_WIDGET(show_command),     TRUE,  TRUE,  2);
  gtk_box_pack_start(GTK_BOX(col), GTK_WIDGET(show_description), FALSE, FALSE, 2);

  gtk_container_add(GTK_CONTAINER(row), GTK_WIDGET(col));

  gtk_box_pack_start(results, GTK_WIDGET(row), FALSE, FALSE, 0);

  return row;
}

void
setCompletionRowColor(GtkBox* results, int mode, int id)
{
  GtkEventBox *row   = (GtkEventBox*) g_list_nth_data(gtk_container_get_children(GTK_CONTAINER(results)), id);

  if(row)
  {
    GtkBox      *col   = (GtkBox*)      g_list_nth_data(gtk_container_get_children(GTK_CONTAINER(row)), 0);
    GtkLabel    *cmd   = (GtkLabel*)    g_list_nth_data(gtk_container_get_children(GTK_CONTAINER(col)), 0);
    GtkLabel    *cdesc = (GtkLabel*)    g_list_nth_data(gtk_container_get_children(GTK_CONTAINER(col)), 1);

    if(mode == NORMAL)
    {
      gtk_widget_modify_fg(GTK_WIDGET(cmd),   GTK_STATE_NORMAL, &(Zathura.Style.completion_fg));
      gtk_widget_modify_fg(GTK_WIDGET(cdesc), GTK_STATE_NORMAL, &(Zathura.Style.completion_fg));
      gtk_widget_modify_bg(GTK_WIDGET(row),   GTK_STATE_NORMAL, &(Zathura.Style.completion_bg));
    }
    else
    {
      gtk_widget_modify_fg(GTK_WIDGET(cmd),   GTK_STATE_NORMAL, &(Zathura.Style.completion_hl_fg));
      gtk_widget_modify_fg(GTK_WIDGET(cdesc), GTK_STATE_NORMAL, &(Zathura.Style.completion_hl_fg));
      gtk_widget_modify_bg(GTK_WIDGET(row),   GTK_STATE_NORMAL, &(Zathura.Style.completion_hl_bg));
    }
  }
}

void
set_page(int page)
{
  if(page >= Zathura.PDF.number_of_pages || page < 0)
  {
    notify(WARNING, "Could not open page");
    return;
  }

  Zathura.PDF.page_number = page;
  Zathura.Search.draw     = FALSE;

  Argument argument;
  argument.n = TOP;

  switch_view(Zathura.UI.document);
  draw(page);
  sc_scroll(&argument);
}

void
switch_view(GtkWidget* widget)
{
  GtkWidget* child = gtk_bin_get_child(GTK_BIN(Zathura.UI.viewport));
  if(child)
  {
    g_object_ref(child);
    gtk_container_remove(GTK_CONTAINER(Zathura.UI.viewport), child);
  }

  gtk_container_add(GTK_CONTAINER(Zathura.UI.viewport), GTK_WIDGET(widget));
}

Completion*
completion_init()
{
  Completion *completion = malloc(sizeof(Completion));
  if(!completion)
    out_of_memory();

  completion->groups = NULL;

  return completion;
}

CompletionGroup*
completion_group_create(char* name)
{
  CompletionGroup* group = malloc(sizeof(CompletionGroup));
  if(!group)
    out_of_memory();

  group->value    = name;
  group->elements = NULL;
  group->next     = NULL;

  return group;
}

void
completion_add_group(Completion* completion, CompletionGroup* group)
{
  CompletionGroup* cg = completion->groups;

  while(cg && cg->next)
    cg = cg->next;

  if(cg)
    cg->next = group;
  else
    completion->groups = group;
}

void completion_free(Completion* completion)
{
  CompletionGroup* group = completion->groups;
  CompletionElement *element;

  while(group)
  {
    element = group->elements;
    while(element)
    {
      CompletionElement* ne = element->next;
      free(element);
      element = ne;
    }

    CompletionGroup *ng = group->next;
    free(group);
    group = ng;
  }
}

void completion_group_add_element(CompletionGroup* group, char* name, char* description)
{
  CompletionElement* el = group->elements;

  while(el && el->next)
    el = el->next;

  CompletionElement* new_element = malloc(sizeof(CompletionElement));
  if(!new_element)
    out_of_memory();

  new_element->value       = name;
  new_element->description = description;
  new_element->next        = NULL;

  if(el)
    el->next = new_element;
  else
    group->elements = new_element;
}


/* thread implementation */
void*
search(void* parameter)
{
  Argument* argument = (Argument*) parameter;

  static char* search_item;
  static int direction;
  static int next_page = 0;
  int page_counter;
  GList* results = NULL;

  if(argument->data)
  {
    if(search_item)
      g_free(search_item);

    search_item = g_strdup((char*) argument->data);
  }

  g_static_mutex_lock(&(Zathura.Lock.pdf_obj_lock));
  if(!Zathura.PDF.document || !search_item || !strlen(search_item))
  {
    g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));
    g_static_mutex_lock(&(Zathura.Lock.search_lock));
    Zathura.Thread.search_thread_running = FALSE;
    g_static_mutex_unlock(&(Zathura.Lock.search_lock));
    g_thread_exit(NULL);
  }

  /* delete old results */
  if(Zathura.Search.results)
  {
    g_list_free(Zathura.Search.results);
    Zathura.Search.results = NULL;
  }

  /* search document */
  if(argument->n)
    direction = (argument->n == BACKWARD) ? -1 : 1;

  int number_of_pages = Zathura.PDF.number_of_pages;
  int page_number     = Zathura.PDF.page_number;

  g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));

  for(page_counter = 1; page_counter <= number_of_pages; page_counter++)
  {
    g_static_mutex_lock(&(Zathura.Lock.search_lock));
    if(Zathura.Thread.search_thread_running == FALSE)
    {
      g_static_mutex_unlock(&(Zathura.Lock.search_lock));
      g_thread_exit(NULL);
    }
    g_static_mutex_unlock(&(Zathura.Lock.search_lock));

    next_page = (number_of_pages + page_number +
        page_counter * direction) % number_of_pages;

    g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
    PopplerPage* page = poppler_document_get_page(Zathura.PDF.document, next_page);
    g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

    if(!page)
      g_thread_exit(NULL);

    g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
    results = poppler_page_find_text(page, search_item);
    g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

    if(results)
      break;
  }

  /* draw results */
  if(results)
  {
    gdk_threads_enter();

    set_page(next_page);

    Zathura.Search.results = results;
    Zathura.Search.page    = next_page;
    Zathura.Search.draw    = TRUE;

    gdk_threads_leave();
  }

  g_static_mutex_lock(&(Zathura.Lock.search_lock));
  Zathura.Thread.search_thread_running = FALSE;
  g_static_mutex_unlock(&(Zathura.Lock.search_lock));

  g_thread_exit(NULL);
  return NULL;
}

/* shortcut implementation */
void
sc_abort(Argument* argument)
{
  /* Clear buffer */
  if(Zathura.Global.buffer)
  {
    g_string_free(Zathura.Global.buffer, TRUE);
    Zathura.Global.buffer = NULL;
    gtk_label_set_markup((GtkLabel*) Zathura.Global.status_buffer, "");
  }

  /* Set back to normal mode */
  change_mode(NORMAL);
  switch_view(Zathura.UI.document);
}

void
sc_adjust_window(Argument* argument)
{
  if(!Zathura.PDF.document)
    return;

  Zathura.Global.adjust_mode = argument->n;

  GtkAdjustment* adjustment;
  double view_size;
  double page_width;
  double page_height;

  if(argument->n == ADJUST_BESTFIT)
    adjustment = gtk_scrolled_window_get_vadjustment(Zathura.UI.view);
  else if(argument->n == ADJUST_WIDTH)
    adjustment = gtk_scrolled_window_get_hadjustment(Zathura.UI.view);
  else
    return;

  view_size  = gtk_adjustment_get_page_size(adjustment);

  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  poppler_page_get_size(Zathura.PDF.pages[Zathura.PDF.page_number]->page, &page_width, &page_height);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

  if ((Zathura.PDF.rotate == 90) || (Zathura.PDF.rotate == 270))
  {
    double swap = page_width;
    page_width  = page_height;
    page_height = swap;
  }

  if(argument->n == ADJUST_BESTFIT)
    Zathura.PDF.scale = (view_size / page_height) * 100;
  else
    Zathura.PDF.scale = (view_size / page_width) * 100;

  draw(Zathura.PDF.page_number);
  update_status();
}

void
sc_change_buffer(Argument* argument)
{
  if(!Zathura.Global.buffer)
    return;

  int buffer_length = Zathura.Global.buffer->len;

  if(argument->n == DELETE_LAST)
  {
    if((buffer_length - 1) == 0)
    {
      g_string_free(Zathura.Global.buffer, TRUE);
      Zathura.Global.buffer = NULL;
      gtk_label_set_markup((GtkLabel*) Zathura.Global.status_buffer, "");
    }
    else
    {
      GString* temp = g_string_new_len(Zathura.Global.buffer->str, buffer_length - 1);
      g_string_free(Zathura.Global.buffer, TRUE);
      Zathura.Global.buffer = temp;
      gtk_label_set_markup((GtkLabel*) Zathura.Global.status_buffer, Zathura.Global.buffer->str);
    }
  }
}

void
sc_change_mode(Argument* argument)
{
  if(argument)
    change_mode(argument->n);
}

void
sc_focus_inputbar(Argument* argument)
{
  if(!(GTK_WIDGET_VISIBLE(GTK_WIDGET(Zathura.UI.inputbar))))
    gtk_widget_show(GTK_WIDGET(Zathura.UI.inputbar));

  if(argument->data)
  {
    notify(DEFAULT, argument->data);
    gtk_widget_grab_focus(GTK_WIDGET(Zathura.UI.inputbar));
    gtk_editable_set_position(GTK_EDITABLE(Zathura.UI.inputbar), -1);
  }
}

void
sc_follow(Argument* argument)
{
  if(!Zathura.PDF.document)
    return;

  Page* current_page = Zathura.PDF.pages[Zathura.PDF.page_number];
  int number_of_links = 0, link_id = 1;

  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  GList *link_list = poppler_page_get_link_mapping(current_page->page);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));
  link_list = g_list_reverse(link_list);

  if((number_of_links = g_list_length(link_list)) <= 0)
    return;

  GList *links;
  for(links = link_list; links; links = g_list_next(links))
  {
    PopplerLinkMapping *link_mapping = (PopplerLinkMapping*) links->data;
    PopplerRectangle* link_rectangle = &link_mapping->area;
    PopplerAction            *action = link_mapping->action;

    /* only handle URI and internal links */
    if(action->type == POPPLER_ACTION_URI || action->type == POPPLER_ACTION_GOTO_DEST)
    {
      highlight_result(Zathura.PDF.page_number, link_rectangle);

      /* draw text */
      recalcRectangle(Zathura.PDF.page_number, link_rectangle);
      cairo_t *cairo = cairo_create(Zathura.PDF.surface);
      cairo_select_font_face(cairo, font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(cairo, 10);
      cairo_move_to(cairo, link_rectangle->x1 + 1, link_rectangle->y1 - 1);
      char* link_number = g_strdup_printf("%i", link_id++);
      cairo_show_text(cairo, link_number);
      g_free(link_number);
    }
  }

  gtk_widget_queue_draw(Zathura.UI.drawing_area);
  poppler_page_free_link_mapping(link_list);

  /* replace default inputbar handler */
  g_signal_handler_disconnect((gpointer) Zathura.UI.inputbar, Zathura.Handler.inputbar_activate);
  Zathura.Handler.inputbar_activate = g_signal_connect(G_OBJECT(Zathura.UI.inputbar), "activate", G_CALLBACK(cb_inputbar_form_activate), NULL);

  argument->data = "Follow hint: ";
  sc_focus_inputbar(argument);
}

void
sc_navigate(Argument* argument)
{
  if(!Zathura.PDF.document)
    return;

  int number_of_pages = Zathura.PDF.number_of_pages;
  int new_page = Zathura.PDF.page_number;

  if(argument->n == NEXT)
    new_page = (new_page + number_of_pages + 1) % number_of_pages;
  else if(argument->n == PREVIOUS)
    new_page = (new_page + number_of_pages - 1) % number_of_pages;

  set_page(new_page);
  update_status();
}

void
sc_recolor(Argument* argument)
{
  Zathura.Global.recolor = !Zathura.Global.recolor;
  draw(Zathura.PDF.page_number);
}

void
sc_reload(Argument* argument)
{
  draw(Zathura.PDF.page_number);

  GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(Zathura.UI.view);
  GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment(Zathura.UI.view);

  /* save old information */
  g_static_mutex_lock(&(Zathura.Lock.pdf_obj_lock));
  char* path     = Zathura.PDF.file ? strdup(Zathura.PDF.file) : NULL;
  char* password = Zathura.PDF.password ? strdup(Zathura.PDF.password) : NULL;
  int scale      = Zathura.PDF.scale;
  int page       = Zathura.PDF.page_number;
  int rotate     = Zathura.PDF.rotate;
  gdouble va     = gtk_adjustment_get_value(vadjustment);
  gdouble ha     = gtk_adjustment_get_value(hadjustment);
  g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));

  /* reopen and restore settings */
  close_file(TRUE);
  open_file(path, password);

  g_static_mutex_lock(&(Zathura.Lock.pdf_obj_lock));
  Zathura.PDF.scale  = scale;
  Zathura.PDF.rotate = rotate;
  gtk_adjustment_set_value(vadjustment, va);
  gtk_adjustment_set_value(hadjustment, ha);
  g_static_mutex_unlock(&(Zathura.Lock.pdf_obj_lock));

  draw(page);

  if(path)
    free(path);
  if(password)
    free(password);
}


void
sc_rotate(Argument* argument)
{
  Zathura.PDF.rotate  = (Zathura.PDF.rotate + 90) % 360;
  Zathura.Search.draw = TRUE;

  draw(Zathura.PDF.page_number);
}

void
sc_scroll(Argument* argument)
{
  GtkAdjustment* adjustment;

  if( (argument->n == LEFT) || (argument->n == RIGHT) )
    adjustment = gtk_scrolled_window_get_hadjustment(Zathura.UI.view);
  else
    adjustment = gtk_scrolled_window_get_vadjustment(Zathura.UI.view);

  gdouble view_size  = gtk_adjustment_get_page_size(adjustment);
  gdouble value      = gtk_adjustment_get_value(adjustment);
  gdouble max        = gtk_adjustment_get_upper(adjustment) - view_size;
  gdouble new_value  = value;
  gboolean static ss = FALSE;

  if((argument->n == UP || argument->n == HALF_UP || argument->n == FULL_UP) && value == 0)
  {
    Argument arg;
    arg.n = PREVIOUS;
    sc_navigate(&arg);
    arg.n = BOTTOM;
    ss = TRUE;
    sc_scroll(&arg);
    return;
  }
  else if((argument->n == DOWN || argument->n == HALF_DOWN || argument->n == FULL_DOWN) && value == max)
  {
    Argument arg;
    arg.n = NEXT;
    ss = TRUE;
    sc_navigate(&arg);
    return;
  }
  else if(argument->n == FULL_UP)
    new_value = (value - view_size) < 0 ? 0 : (value - view_size);
  else if(argument->n == FULL_DOWN)
    new_value = (value + view_size) > max ? max : (value + view_size);
  else if(argument->n == HALF_UP)
    new_value = (value - (view_size / 2)) < 0 ? 0 : (value - (view_size / 2));
  else if(argument->n == HALF_DOWN)
    new_value = (value + (view_size / 2)) > max ? max : (value + (view_size / 2));
  else if((argument->n == LEFT) || (argument->n == UP))
    new_value = (value - scroll_step) < 0 ? 0 : (value - scroll_step);
  else if(argument->n == TOP)
    new_value = 0;
  else if(argument->n == BOTTOM)
    new_value = max;
  else
    new_value = (value + scroll_step) > max ? max : (value + scroll_step);

  if(smooth_scrolling && !ss)
  {
    gdouble i;
    if(new_value > value)
      for(i = value; (i + smooth_scrolling) < new_value; i += smooth_scrolling)
        gtk_adjustment_set_value(adjustment, i);
    else
      for(i = value; (i + smooth_scrolling) > new_value; i -= smooth_scrolling)
        gtk_adjustment_set_value(adjustment, i);
  }

  gtk_adjustment_set_value(adjustment, new_value);
  ss = FALSE;

  update_status();
}

void
sc_search(Argument* argument)
{
  g_static_mutex_lock(&(Zathura.Lock.search_lock));
  if(Zathura.Thread.search_thread_running)
  {
    Zathura.Thread.search_thread_running = FALSE;
    g_static_mutex_unlock(&(Zathura.Lock.search_lock));
    gdk_threads_leave();
    g_thread_join(Zathura.Thread.search_thread);
    gdk_threads_enter();
    g_static_mutex_lock(&(Zathura.Lock.search_lock));
  }

  Zathura.Thread.search_thread_running = TRUE;
  Zathura.Thread.search_thread = g_thread_create(search, (gpointer) argument, TRUE, NULL);
  g_static_mutex_unlock(&(Zathura.Lock.search_lock));
}

void
sc_switch_goto_mode(Argument* argument)
{
  switch(Zathura.Global.goto_mode)
  {
    case GOTO_LABELS:
      Zathura.Global.goto_mode = GOTO_OFFSET;
      break;
    case GOTO_OFFSET:
      Zathura.Global.goto_mode = GOTO_DEFAULT;
      break;
    default:
      if(Zathura.Global.enable_labelmode)
        Zathura.Global.goto_mode = GOTO_LABELS;
      else
        Zathura.Global.goto_mode = GOTO_OFFSET;
      break;
  }

  update_status();
}

gboolean cb_index_row_activated(GtkTreeView* treeview, GtkTreePath* path,
  GtkTreeViewColumn* column, gpointer user_data)
{
  GtkTreeModel  *model;
  GtkTreeIter   iter;

  g_object_get(treeview, "model", &model, NULL);

  if(gtk_tree_model_get_iter(model, &iter, path))
  {
    PopplerAction* action;
    PopplerDest*   destination;

    gtk_tree_model_get(model, &iter, 1, &action, -1);
    if(!action)
      return TRUE;

    if(action->type == POPPLER_ACTION_GOTO_DEST)
    {
      destination = action->goto_dest.dest;
      int page_number = destination->page_num;

      if(action->goto_dest.dest->type == POPPLER_DEST_NAMED)
      {
        PopplerDest* d = poppler_document_find_dest(Zathura.PDF.document, action->goto_dest.dest->named_dest);
        if(d)
        {
          page_number = d->page_num;
          poppler_dest_free(d);
        }
      }

      set_page(page_number - 1);
      update_status();
      Zathura.Global.show_index = FALSE;
      gtk_widget_grab_focus(GTK_WIDGET(Zathura.UI.document));
    }
  }

  Zathura.Global.mode = NORMAL;
  g_object_unref(model);

  return TRUE;
}

void
sc_navigate_index(Argument* argument)
{
  GtkTreeView *treeview = gtk_container_get_children(GTK_CONTAINER(Zathura.UI.index))->data;
  GtkTreePath *path;

  gtk_tree_view_get_cursor(treeview, &path, NULL);
  if(!path)
    return;

  GtkTreeModel *model = gtk_tree_view_get_model(treeview);
  GtkTreeIter   iter;
  GtkTreeIter   child_iter;

  gboolean is_valid_path = TRUE;

  switch(argument->n)
  {
    case UP:
      if(!gtk_tree_path_prev(path))
        is_valid_path = gtk_tree_path_up(path);
      else /* row above */
      {
        while(gtk_tree_view_row_expanded(treeview, path)) {
          gtk_tree_model_get_iter(model, &iter, path);
          /* select last child */
          gtk_tree_model_iter_nth_child(model, &child_iter, &iter,
            gtk_tree_model_iter_n_children(model, &iter)-1);
          gtk_tree_path_free(path);
          path = gtk_tree_model_get_path(model, &child_iter);
        }
      }
      break;
    case COLLAPSE:
      if(!gtk_tree_view_collapse_row(treeview, path)
        && gtk_tree_path_get_depth(path) > 1)
      {
        gtk_tree_path_up(path);
        gtk_tree_view_collapse_row(treeview, path);
      }
      break;
    case DOWN:
      if(gtk_tree_view_row_expanded(treeview, path))
        gtk_tree_path_down(path);
      else
      {
        do
        {
          gtk_tree_model_get_iter(model, &iter, path);
          if (gtk_tree_model_iter_next(model, &iter))
          {
            path = gtk_tree_model_get_path(model, &iter);
            break;
          }
        }
        while((is_valid_path = (gtk_tree_path_get_depth(path) > 1))
          && gtk_tree_path_up(path));
      }
      break;
    case EXPAND:
      if(gtk_tree_view_expand_row(treeview, path, FALSE))
        gtk_tree_path_down(path);
      break;
    case SELECT:
      cb_index_row_activated(treeview, path, NULL, NULL);
      return;
  }

  if (is_valid_path)
    gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);

  gtk_tree_path_free(path);
}

void
sc_toggle_index(Argument* argument)
{
  if(!Zathura.PDF.document)
    return;

  GtkWidget        *treeview;
  GtkTreeModel     *model;
  GtkCellRenderer  *renderer;
  PopplerIndexIter *iter;

  if(!Zathura.UI.index)
  {
    Zathura.UI.index = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.UI.index),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    if((iter = poppler_index_iter_new(Zathura.PDF.document)))
    {
      model = GTK_TREE_MODEL(gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_POINTER));
      g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
      build_index(model, NULL, iter);
      g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));
      poppler_index_iter_free(iter);
    }
    else
    {
      notify(WARNING, "This document does not contain any index");
      Zathura.UI.index = NULL;
      return;
    }

    treeview = gtk_tree_view_new_with_model (model);
    g_object_unref(model);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (treeview), 0, "Title",
        renderer, "markup", 0, NULL);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_object_set(G_OBJECT(gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0)), "expand", TRUE, NULL);

    gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), gtk_tree_path_new_first(), NULL, FALSE);
    g_signal_connect(G_OBJECT(treeview), "row-activated", G_CALLBACK(cb_index_row_activated), NULL);

    gtk_container_add (GTK_CONTAINER (Zathura.UI.index), treeview);
    gtk_widget_show (treeview);
    gtk_widget_show(Zathura.UI.index);
  }

  if(!Zathura.Global.show_index)
  {
    switch_view(Zathura.UI.index);
    Zathura.Global.mode = INDEX;
  }
  else
  {
    switch_view(Zathura.UI.document);
    Zathura.Global.mode = NORMAL;
  }

  Zathura.Global.show_index = !Zathura.Global.show_index;
}

void
sc_toggle_inputbar(Argument* argument)
{
  if(GTK_WIDGET_VISIBLE(GTK_WIDGET(Zathura.UI.inputbar)))
    gtk_widget_hide(GTK_WIDGET(Zathura.UI.inputbar));
  else
    gtk_widget_show(GTK_WIDGET(Zathura.UI.inputbar));
}

void
sc_toggle_fullscreen(Argument* argument)
{
  static gboolean fs = TRUE;

  if(fs)
  {
    gtk_window_fullscreen(GTK_WINDOW(Zathura.UI.window));
    gtk_widget_hide(GTK_WIDGET(Zathura.UI.inputbar));
    gtk_widget_hide(GTK_WIDGET(Zathura.UI.statusbar));

    Argument arg;
    arg.n = ADJUST_BESTFIT;
    sc_adjust_window(&arg);

    Zathura.Global.mode = FULLSCREEN;
    fs = FALSE;
  }
  else
  {
    gtk_window_unfullscreen(GTK_WINDOW(Zathura.UI.window));
    gtk_widget_show(GTK_WIDGET(Zathura.UI.inputbar));
    gtk_widget_show(GTK_WIDGET(Zathura.UI.statusbar));

    Zathura.Global.mode = NORMAL;
    fs = TRUE;
  }
}

void
sc_toggle_statusbar(Argument* argument)
{
  if(GTK_WIDGET_VISIBLE(GTK_WIDGET(Zathura.UI.statusbar)))
    gtk_widget_hide(GTK_WIDGET(Zathura.UI.statusbar));
  else
    gtk_widget_show(GTK_WIDGET(Zathura.UI.statusbar));
}

void
sc_quit(Argument* argument)
{
  cb_destroy(NULL, NULL);
}

void
sc_zoom(Argument* argument)
{
  bcmd_zoom(NULL, argument);
}

/* inputbar shortcut declarations */
void
isc_abort(Argument* argument)
{
  Argument arg = { HIDE };
  isc_completion(&arg);

  notify(DEFAULT, "");
  gtk_widget_grab_focus(GTK_WIDGET(Zathura.UI.view));

  if(!Zathura.Global.show_inputbar)
    gtk_widget_hide(GTK_WIDGET(Zathura.UI.inputbar));
}

void
isc_command_history(Argument* argument)
{
  static int current = 0;
  int        length  = g_list_length(Zathura.Global.history);

  if(length > 0)
  {
    if(argument->n == NEXT)
      current = (length + current + 1) % length;
    else
      current = (length + current - 1) % length;

    gchar* command = (gchar*) g_list_nth_data(Zathura.Global.history, current);
    notify(DEFAULT, command);
    gtk_widget_grab_focus(GTK_WIDGET(Zathura.UI.inputbar));
    gtk_editable_set_position(GTK_EDITABLE(Zathura.UI.inputbar), -1);
  }
}

void
isc_completion(Argument* argument)
{
  gchar *input      = gtk_editable_get_chars(GTK_EDITABLE(Zathura.UI.inputbar), 1, -1);
  gchar  identifier = gtk_editable_get_chars(GTK_EDITABLE(Zathura.UI.inputbar), 0,  1)[0];
  int    length     = strlen(input);

  if(!length && !identifier)
    return;

  /* get current information*/
  char* first_space = strstr(input, " ");
  char* current_command;
  char* current_parameter;
  int   current_command_length;
  int   current_parameter_length;

  if(!first_space)
  {
    current_command          = input;
    current_command_length   = length;
    current_parameter        = NULL;
    current_parameter_length = 0;
  }
  else
  {
    int offset               = abs(input - first_space);
    current_command          = g_strndup(input, offset);
    current_command_length   = strlen(current_command);
    current_parameter        = input + offset + 1;
    current_parameter_length = strlen(current_parameter);
  }

  /* if the identifier does not match the command sign and
   * the completion should not be hidden, leave this function */
  if((identifier != ':') && (argument->n != HIDE))
    return;

  /* static elements */
  static GtkBox        *results = NULL;
  static CompletionRow *rows    = NULL;

  static int current_item = 0;
  static int n_items      = 0;

  static char *previous_command   = NULL;
  static char *previous_parameter = NULL;
  static int   previous_id        = 0;
  static int   previous_length    = 0;

  static gboolean command_mode = TRUE;

  /* delete old list iff
   *   the completion should be hidden
   *   the current command differs from the previous one
   *   the current parameter differs from the previous one
   */
  if( (argument->n == HIDE) ||
      (current_parameter && previous_parameter && strcmp(current_parameter, previous_parameter)) ||
      (current_command && previous_command && strcmp(current_command, previous_command)) ||
      (previous_length != length)
    )
  {
    if(results)
      gtk_widget_destroy(GTK_WIDGET(results));

    results = NULL;

    if(rows)
      free(rows);

    rows         = NULL;
    current_item = 0;
    n_items      = 0;
    command_mode = TRUE;

    if(argument->n == HIDE)
      return;
  }

  /* create new list iff
   *  there is no current list
   *  the current command differs from the previous one
   *  the current parameter differs from the previous one
   */
  if( (!results) )
  {
    results = GTK_BOX(gtk_vbox_new(FALSE, 0));

    /* create list based on parameters iff
     *  there is a current parameter given
     *  there is an old list with commands
     *  the current command does not differ from the previous one
     *  the current command has an completion function
     */
    if( (previous_command) && (current_parameter) && !strcmp(current_command, previous_command) )
    {
      if(previous_id < 0 || !commands[previous_id].completion)
        return;

      Completion *result = commands[previous_id].completion(current_parameter);

      if(!result || !result->groups)
        return;

      command_mode               = FALSE;
      CompletionGroup* group     = NULL;
      CompletionElement* element = NULL;

      rows = malloc(sizeof(CompletionRow));
      if(!rows)
        out_of_memory();

      for(group = result->groups; group != NULL; group = group->next)
      {
        int group_elements = 0;

        for(element = group->elements; element != NULL; element = element->next)
        {
          if( (element->value) &&
              (current_parameter_length <= strlen(element->value)) && !strncmp(current_parameter, element->value, current_parameter_length)
            )
          {
            rows = realloc(rows, (n_items + 1) * sizeof(CompletionRow));
            rows[n_items].command     = element->value;
            rows[n_items].description = element->description;
            rows[n_items].command_id  = previous_id;
            rows[n_items].is_group    = FALSE;
            rows[n_items++].row       = GTK_WIDGET(createCompletionRow(results, element->value, element->description, FALSE));
            group_elements++;
          }
        }

        if(group->value && group_elements > 0)
        {
          rows = realloc(rows, (n_items + 1) * sizeof(CompletionRow));
          rows[n_items].command     = group->value;
          rows[n_items].description = NULL;
          rows[n_items].command_id  = -1;
          rows[n_items].is_group    = TRUE;
          rows[n_items].row       = GTK_WIDGET(createCompletionRow(results, group->value, NULL, TRUE));

          /* Swap group element with first element of the list */
          CompletionRow temp = rows[n_items - group_elements];
          gtk_box_reorder_child(results, rows[n_items].row, n_items - group_elements);
          rows[n_items - group_elements] = rows[n_items];
          rows[n_items] = temp;

          n_items++;
        }
      }

      /* clean up */
      completion_free(result);
    }
    /* create list based on commands */
    else
    {
      int i = 0;
      command_mode = TRUE;

      rows = malloc(LENGTH(commands) * sizeof(CompletionRow));
      if(!rows)
        out_of_memory();

      for(i = 0; i < LENGTH(commands); i++)
      {
        int abbr_length = commands[i].abbr ? strlen(commands[i].abbr) : 0;
        int cmd_length  = commands[i].command ? strlen(commands[i].command) : 0;

        /* add command to list iff
         *  the current command would match the command
         *  the current command would match the abbreviation
         */
        if( ((current_command_length <= cmd_length)  && !strncmp(current_command, commands[i].command, current_command_length)) ||
            ((current_command_length <= abbr_length) && !strncmp(current_command, commands[i].abbr,    current_command_length))
          )
        {
          rows[n_items].command     = commands[i].command;
          rows[n_items].description = commands[i].description;
          rows[n_items].command_id  = i;
          rows[n_items].is_group    = FALSE;
          rows[n_items++].row       = GTK_WIDGET(createCompletionRow(results, commands[i].command, commands[i].description, FALSE));
        }
      }

      rows = realloc(rows, n_items * sizeof(CompletionRow));
    }

    gtk_box_pack_start(Zathura.UI.box, GTK_WIDGET(results), FALSE, FALSE, 0);
    gtk_widget_show_all(GTK_WIDGET(Zathura.UI.window));

    current_item = (argument->n == NEXT) ? -1 : 0;
  }

  /* update coloring iff
   *  there is a list with items
   */
  if( (results) && (n_items > 0) )
  {
    setCompletionRowColor(results, NORMAL, current_item);
    char* temp;
    int i = 0, next_group = 0;

    for(i = 0; i < n_items; i++)
    {
      if(argument->n == NEXT || argument->n == NEXT_GROUP)
        current_item = (current_item + n_items + 1) % n_items;
      else if(argument->n == PREVIOUS || argument->n == PREVIOUS_GROUP)
        current_item = (current_item + n_items - 1) % n_items;

      if(rows[current_item].is_group)
      {
        if(!command_mode && (argument->n == NEXT_GROUP || argument->n == PREVIOUS_GROUP))
          next_group = 1;
        continue;
      }
      else
      {
        if(!command_mode && (next_group == 0) && (argument->n == NEXT_GROUP || argument->n == PREVIOUS_GROUP))
          continue;
        break;
      }
    }

    setCompletionRowColor(results, HIGHLIGHT, current_item);

    /* hide other items */
    int uh = ceil(n_completion_items / 2);
    int lh = floor(n_completion_items / 2);

    for(i = 0; i < n_items; i++)
    {
     if((i >= (current_item - lh) && (i <= current_item + uh)) ||
        (i < n_completion_items && current_item < lh) ||
        (i >= (n_items - n_completion_items) && (current_item >= (n_items - uh)))
       )
        gtk_widget_show(rows[i].row);
      else
        gtk_widget_hide(rows[i].row);
    }

    if(command_mode)
      temp = g_strconcat(":", rows[current_item].command, NULL);
    else
      temp = g_strconcat(":", previous_command, " ", rows[current_item].command, NULL);

    gtk_entry_set_text(Zathura.UI.inputbar, temp);
    gtk_editable_set_position(GTK_EDITABLE(Zathura.UI.inputbar), -1);
    g_free(temp);

    previous_command   = (command_mode) ? rows[current_item].command : current_command;
    previous_parameter = (command_mode) ? current_parameter : rows[current_item].command;
    previous_length    = strlen(previous_command) + ((command_mode) ? (length - current_command_length) : (strlen(previous_parameter) + 1));
    previous_id        = rows[current_item].command_id;
  }
}

void
isc_string_manipulation(Argument* argument)
{
 gchar *input  = gtk_editable_get_chars(GTK_EDITABLE(Zathura.UI.inputbar), 0, -1);
 int    length = strlen(input);
 int pos       = gtk_editable_get_position(GTK_EDITABLE(Zathura.UI.inputbar));

  if(argument->n == DELETE_LAST_WORD)
  {
    int i = pos - 1;

    if(!pos)
      return;

    /* remove trailing spaces */
    for(; i >= 0 && input[i] == ' '; i--);

    /* find the beginning of the word */
    while((i > 0) && (input[i] != ' ') && (input[i] != '/'))
      i--;

    gtk_editable_delete_text(GTK_EDITABLE(Zathura.UI.inputbar),  i, pos);
    gtk_editable_set_position(GTK_EDITABLE(Zathura.UI.inputbar), i);
  }
  else if(argument->n == DELETE_LAST_CHAR)
  {
    if((length - 1) <= 0)
      isc_abort(NULL);

    gtk_editable_delete_text(GTK_EDITABLE(Zathura.UI.inputbar), pos - 1, pos);
  }
  else if(argument->n == NEXT_CHAR)
    gtk_editable_set_position(GTK_EDITABLE(Zathura.UI.inputbar), pos+1);
  else if(argument->n == PREVIOUS_CHAR)
    gtk_editable_set_position(GTK_EDITABLE(Zathura.UI.inputbar), (pos == 0) ? 0 : pos - 1);
}

/* command implementation */
gboolean
cmd_bookmark(int argc, char** argv)
{
  if(!Zathura.PDF.document || argc < 1)
    return TRUE;

  /* get id */
  int i;
  GString *id = g_string_new("");

  for(i = 0; i < argc; i++)
  {
    if(i != 0)
      id = g_string_append_c(id, ' ');

    id = g_string_append(id, argv[i]);
  }

  /* check for existing bookmark to overwrite */
  for(i = 0; i < Zathura.Bookmarks.number_of_bookmarks; i++)
  {
    if(!strcmp(id->str, Zathura.Bookmarks.bookmarks[i].id))
    {
      Zathura.Bookmarks.bookmarks[i].page = Zathura.PDF.page_number;
      return TRUE;
    }
  }

  /* add new bookmark */
  Zathura.Bookmarks.bookmarks = realloc(Zathura.Bookmarks.bookmarks,
      (Zathura.Bookmarks.number_of_bookmarks + 1) * sizeof(Bookmark));

  Zathura.Bookmarks.bookmarks[Zathura.Bookmarks.number_of_bookmarks].id   = id->str;
  Zathura.Bookmarks.bookmarks[Zathura.Bookmarks.number_of_bookmarks].page = Zathura.PDF.page_number;
  Zathura.Bookmarks.number_of_bookmarks++;

  return TRUE;
}

gboolean
cmd_open_bookmark(int argc, char** argv)
{
  if(!Zathura.PDF.document || argc < 1)
    return TRUE;

  /* get id */
  int i;
  GString *id = g_string_new("");

  for(i = 0; i < argc; i++)
  {
    if(i != 0)
      id = g_string_append_c(id, ' ');

    id = g_string_append(id, argv[i]);
  }

  /* find bookmark */
  for(i = 0; i < Zathura.Bookmarks.number_of_bookmarks; i++)
  {
    if(!strcmp(id->str, Zathura.Bookmarks.bookmarks[i].id))
    {
      set_page(Zathura.Bookmarks.bookmarks[i].page);
      return TRUE;
    }
  }

  notify(WARNING, "No matching bookmark found");
  return FALSE;
}

gboolean
cmd_close(int argc, char** argv)
{
  close_file(FALSE);

  return TRUE;
}

gboolean
cmd_correct_offset(int argc, char** argv)
{
  if(!Zathura.PDF.document || argc == 0)
    return TRUE;

  Zathura.PDF.page_offset = (Zathura.PDF.page_number + 1) - atoi(argv[0]);

  if(Zathura.PDF.page_offset != 0)
    Zathura.Global.goto_mode = GOTO_OFFSET;
  else
    Zathura.Global.goto_mode = GOTO_MODE;

  update_status();

  return TRUE;
}

gboolean
cmd_delete_bookmark(int argc, char** argv)
{
  if(!Zathura.PDF.document || argc < 1)
    return TRUE;

  /* get id */
  int i;
  GString *id = g_string_new("");

  for(i = 0; i < argc; i++)
  {
    if(i != 0)
      id = g_string_append_c(id, ' ');

    id = g_string_append(id, argv[i]);
  }

  /* check for bookmark to delete */
  for(i = 0; i < Zathura.Bookmarks.number_of_bookmarks; i++)
  {
    if(!strcmp(id->str, Zathura.Bookmarks.bookmarks[i].id))
    {
      /* update key file */
      g_key_file_remove_key(Zathura.Bookmarks.data, Zathura.PDF.file, Zathura.Bookmarks.bookmarks[i].id, NULL);

      /* update bookmarks */
      Zathura.Bookmarks.bookmarks[i].id   = Zathura.Bookmarks.bookmarks[Zathura.Bookmarks.number_of_bookmarks - 1].id;
      Zathura.Bookmarks.bookmarks[i].page = Zathura.Bookmarks.bookmarks[Zathura.Bookmarks.number_of_bookmarks - 1].page;
      Zathura.Bookmarks.bookmarks = realloc(Zathura.Bookmarks.bookmarks,
          Zathura.Bookmarks.number_of_bookmarks * sizeof(Bookmark));

      Zathura.Bookmarks.number_of_bookmarks--;

      return TRUE;
    }
  }

  return TRUE;
}

gboolean
cmd_export(int argc, char** argv)
{
  if(argc == 0 || !Zathura.PDF.document)
    return TRUE;

  if(argc < 2)
  {
    notify(WARNING, "No export path specified");
    return FALSE;
  }

  /* export images */
  if(!strcmp(argv[0], "images"))
  {
    int page_number;
    for(page_number = 0; page_number < Zathura.PDF.number_of_pages; page_number++)
    {
      GList           *image_list;
      GList           *images;
      cairo_surface_t *image;

      g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
      image_list = poppler_page_get_image_mapping(Zathura.PDF.pages[page_number]->page);
      g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

      if(!g_list_length(image_list))
      {
        notify(WARNING, "This document does not contain any images");
        return FALSE;
      }

      for(images = image_list; images; images = g_list_next(images))
      {
        PopplerImageMapping *image_mapping;
        PopplerRectangle     image_field;
        gint                 image_id;
        char*                file;
        char*                filename;

        image_mapping = (PopplerImageMapping*) images->data;
        image_field   = image_mapping->area;
        image_id      = image_mapping->image_id;

        g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
        image     = poppler_page_get_image(Zathura.PDF.pages[page_number]->page, image_id);
        g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

        if(!image)
          continue;

        filename  = g_strdup_printf("%s_p%i_i%i.png", Zathura.PDF.file, page_number + 1, image_id);

        if(argv[1][0] == '~')
        {
          file = malloc(((int) strlen(filename) + (int) strlen(argv[1])
            + (int) strlen(getenv("HOME")) - 1) * sizeof(char));
          if(!file)
            out_of_memory();

          file = g_strdup_printf("%s%s%s", getenv("HOME"), argv[1] + 1, filename);
        }
        else
          file = g_strdup_printf("%s%s", argv[1], filename);

        cairo_surface_write_to_png(image, file);

        g_free(file);
      }
    }
  }
  else if(!strcmp(argv[0], "attachments"))
  {
    g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
    if(!poppler_document_has_attachments(Zathura.PDF.document))
    {
      notify(WARNING, "PDF file has no attachments");
      g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));
      return FALSE;
    }

    GList *attachment_list = poppler_document_get_attachments(Zathura.PDF.document);
    g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

    GList *attachments;
    char  *file;

    for(attachments = attachment_list; attachments; attachments = g_list_next(attachments))
    {
      PopplerAttachment *attachment = (PopplerAttachment*) attachments->data;

      if(argv[1][0] == '~')
      {
        file = malloc(((int) strlen(attachment->name) + (int) strlen(argv[1])
          + (int) strlen(getenv("HOME")) - 1) * sizeof(char));
        if(!file)
          out_of_memory();

        file = g_strdup_printf("%s%s%s", getenv("HOME"), argv[1] + 1, attachment->name);
      }
      else
        file = g_strdup_printf("%s%s", argv[1], attachment->name);

      g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
      poppler_attachment_save(attachment, file, NULL);
      g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

      g_free(file);
    }
  }

  return TRUE;
}

gboolean
cmd_info(int argc, char** argv)
{
  if(!Zathura.PDF.document)
    return TRUE;

  static gboolean visible = FALSE;

  if(!Zathura.UI.information)
  {
    GtkListStore      *list;
    GtkTreeIter        iter;
    GtkCellRenderer   *renderer;
    GtkTreeSelection  *selection;

    list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

    /* read document information */
    gchar *title,   *author;
    gchar *subject, *keywords;
    gchar *creator, *producer;
    GTime  creation_date, modification_date;

    g_object_get(Zathura.PDF.document,
        "title",         &title,
        "author",        &author,
        "subject",       &subject,
        "keywords",      &keywords,
        "creator",       &creator,
        "producer",      &producer,
        "creation-date", &creation_date,
        "mod-date",      &modification_date,
        NULL);

    /* append information to list */
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter, 0,  "Author",   1, author   ? author   : "", -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter, 0,  "Title",    1, title    ? title    : "", -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter, 0,  "Subject",  1, subject  ? subject  : "", -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter, 0,  "Keywords", 1, keywords ? keywords : "", -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter, 0,  "Creator",  1, creator  ? creator  : "", -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter, 0,  "Producer", 1, producer ? producer : "", -1);

    Zathura.UI.information = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list));
    renderer = gtk_cell_renderer_text_new();

    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(Zathura.UI.information), -1,
      "Name", renderer, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(Zathura.UI.information), -1,
      "Value", renderer, "text", 1, NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(Zathura.UI.information));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    gtk_widget_show_all(Zathura.UI.information);
  }

  if(!visible)
    switch_view(Zathura.UI.information);
  else
    switch_view(Zathura.UI.document);

  visible = !visible;

  return FALSE;
}

gboolean
cmd_map(int argc, char** argv)
{
  if(argc < 2)
    return TRUE;

  char* ks = argv[0];

  /* search for the right shortcut function */
  int sc_id = -1;

  int sc_c;
  for(sc_c = 0; sc_c < LENGTH(shortcut_names); sc_c++)
  {
    if(!strcmp(argv[1], shortcut_names[sc_c].name))
    {
      sc_id = sc_c;
      break;
    }
  }

  if(sc_id == -1)
  {
    notify(WARNING, "No such shortcut function exists");
    return FALSE;
  }

  /* parse modifier and key */
  int mask = 0;
  int key  = 0;
  int keyl = strlen(ks);
  int mode = NORMAL;

  // single key (e.g.: g)
  if(keyl == 1)
    key = ks[0];

  // modifier and key (e.g.: <S-g>
  // special key or modifier and key/special key (e.g.: <S-g>, <Space>)

  else if(keyl >= 3 && ks[0] == '<' && ks[keyl-1] == '>')
  {
    char* specialkey = NULL;

    /* check for modifier */
    if(keyl >= 5 && ks[2] == '-')
    {
      /* evaluate modifier */
      switch(ks[1])
      {
        case 'S':
          mask = GDK_SHIFT_MASK;
          break;
        case 'C':
          mask = GDK_CONTROL_MASK;
          break;
      }

      /* no valid modifier */
      if(!mask)
      {
        notify(WARNING, "No valid modifier given.");
        return FALSE;
      }

      /* modifier and special key */
      if(keyl > 5)
        specialkey = g_strndup(ks + 3, keyl - 4);
      else
        key = ks[3];
    }
    else
      specialkey = ks;

    /* search special key */
    int g_c;
    for(g_c = 0; specialkey && g_c < LENGTH(gdk_keys); g_c++)
    {
      if(!strcmp(specialkey, gdk_keys[g_c].identifier))
      {
        key = gdk_keys[g_c].key;
        break;
      }
    }

    if(specialkey)
      g_free(specialkey);
  }

  if(!key)
  {
    notify(WARNING, "No valid key binding given.");
    return FALSE;
  }

  /* parse argument */
  Argument arg = {0, 0};

  if(argc >= 3)
  {
    int arg_id = -1;

    /* compare argument with given argument names... */
    int arg_c;
    for(arg_c = 0; arg_c < LENGTH(argument_names); arg_c++)
    {
      if(!strcmp(argv[2], argument_names[arg_c].name))
      {
        arg_id = argument_names[arg_c].argument;
        break;
      }
    }

    /* if not, save it do .data */
    if(arg_id == -1)
      arg.data = argv[2];
    else
      arg.n = arg_id;
  }

  /* parse mode */
  if(argc >= 4)
  {
    int mode_c;
    for(mode_c = 0; mode_c < LENGTH(mode_names); mode_c++)
    {
      if(!strcmp(argv[3], mode_names[mode_c].name))
      {
        mode = mode_names[mode_c].mode;
        break;
      }
    }
  }

  /* search for existing binding to overwrite it */
  ShortcutList* sc = Zathura.Bindings.sclist;
  while(sc && sc->next != NULL)
  {
    if(sc->element.key == key && sc->element.mask == mask
        && sc->element.mode == mode)
    {
      sc->element.function = shortcut_names[sc_id].function;
      sc->element.argument = arg;
      return TRUE;
    }

    sc = sc->next;
  }

  /* create new entry */
  ShortcutList* entry = malloc(sizeof(ShortcutList));
  if(!entry)
    out_of_memory();

  entry->element.mask     = mask;
  entry->element.key      = key;
  entry->element.function = shortcut_names[sc_id].function;
  entry->element.mode     = mode;
  entry->element.argument = arg;
  entry->next             = NULL;

  /* append to list */
  if(!Zathura.Bindings.sclist)
    Zathura.Bindings.sclist = entry;

  if(sc)
    sc->next = entry;

  return TRUE;
}

gboolean
cmd_open(int argc, char** argv)
{
  if(argc == 0 || strlen(argv[0]) == 0)
    return TRUE;

  /* assembly the arguments back to one string */
  int i = 0;
  GString *filepath = g_string_new("");
  for(i = 0; i < argc; i++)
  {
    if(i != 0)
      filepath = g_string_append_c(filepath, ' ');

    filepath = g_string_append(filepath, argv[i]);
  }

  return open_file(filepath->str, NULL);
}

gboolean
cmd_print(int argc, char** argv)
{
  if(!Zathura.PDF.document)
    return TRUE;

  if(argc == 0)
  {
    notify(WARNING, "No printer specified");
    return FALSE;
  }

  char* printer = argv[0];
  char* sites   = (argc == 2) ? g_strdup(argv[1]) : g_strdup_printf("1-%i", Zathura.PDF.number_of_pages);
  char* command = g_strdup_printf(print_command, printer, sites, Zathura.PDF.file);
  system(command);

  g_free(sites);
  g_free(command);

  return TRUE;
}

gboolean
cmd_rotate(int argc, char** argv)
{
  return TRUE;
}

gboolean
cmd_set(int argc, char** argv)
{
  if(argc <= 0)
    return FALSE;

  int i;
  for(i = 0; i < LENGTH(settings); i++)
  {
    if(!strcmp(argv[0], settings[i].name))
    {
      /* check var type */
      if(settings[i].type == 'b')
      {
        gboolean *x = (gboolean*) (settings[i].variable);
        *x = !(*x);

        if(argv[1])
        {
          if(!strcmp(argv[1], "false") || !strcmp(argv[1], "0"))
            *x = FALSE;
          else
            *x = TRUE;
        }
      }
      else if(settings[i].type == 'i')
      {
        if(argc != 2)
          return FALSE;

        int *x = (int*) (settings[i].variable);

        int id = -1;
        int arg_c;
        for(arg_c = 0; arg_c < LENGTH(argument_names); arg_c++)
        {
          if(!strcmp(argv[1], argument_names[arg_c].name))
          {
            id = argument_names[arg_c].argument;
            break;
          }
        }

        if(id == -1)
          id = atoi(argv[1]);

        *x = id;
      }
      else if(settings[i].type == 'f')
      {
        if(argc != 2)
          return FALSE;

        float *x = (float*) (settings[i].variable);
        if(argv[1])
          *x = atof(argv[1]);
      }
      else if(settings[i].type == 's')
      {
        if(argc < 2)
          return FALSE;

        /* assembly the arguments back to one string */
        int j;
        GString *s = g_string_new("");
        for(j = 1; j < argc; j++)
        {
          if(j != 1)
            s = g_string_append_c(s, ' ');

          s = g_string_append(s, argv[j]);
        }

        char **x = (char**) settings[i].variable;
        *x = s->str;
      }
      else if(settings[i].type == 'c')
      {
        if(argc != 2)
          return FALSE;

        char *x = (char*) (settings[i].variable);
        if(argv[1])
          *x = argv[1][0];
      }

      /* re-init */
      if(settings[i].reinit)
        init_look();

      /* render */
      if(settings[i].render)
      {
        if(!Zathura.PDF.document)
          return FALSE;

        draw(Zathura.PDF.page_number);
      }
    }
  }

  update_status();
  return TRUE;
}

gboolean
cmd_quit(int argc, char** argv)
{
  cb_destroy(NULL, NULL);
  return TRUE;
}

gboolean
cmd_save(int argc, char** argv)
{
  if(argc == 0 || !Zathura.PDF.document)
    return TRUE;

  char* path = g_strdup_printf("file://%s", argv[0]);
  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  poppler_document_save(Zathura.PDF.document, path, NULL);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));
  g_free(path);

  return TRUE;
}

/* completion command implementation */
Completion*
cc_bookmark(char* input)
{
  Completion* completion = completion_init();
  CompletionGroup* group = completion_group_create(NULL);

  completion_add_group(completion, group);

  int i = 0;
  int input_length = input ? strlen(input) : 0;

  for(i = 0; i < Zathura.Bookmarks.number_of_bookmarks; i++)
  {
    if( (input_length <= strlen(Zathura.Bookmarks.bookmarks[i].id)) &&
          !strncmp(input, Zathura.Bookmarks.bookmarks[i].id, input_length) )
    {
      completion_group_add_element(group, Zathura.Bookmarks.bookmarks[i].id, g_strdup_printf("Page %d", Zathura.Bookmarks.bookmarks[i].page));
    }
  }

  return completion;
}

Completion*
cc_export(char* input)
{
  Completion* completion = completion_init();
  CompletionGroup* group = completion_group_create(NULL);

  completion_add_group(completion, group);

  completion_group_add_element(group, "images",      "Export images");
  completion_group_add_element(group, "attachments", "Export attachments");

  return completion;
}

Completion*
cc_open(char* input)
{
  Completion* completion = completion_init();
  CompletionGroup* group = completion_group_create(NULL);

  completion_add_group(completion, group);

  /* read dir */
  char* path        = "/";
  char* file        = "";
  int   file_length = 0;

  /* ~ */
  if(input[0] == '~')
  {
    char *file = g_strdup_printf(":open %s/%s", getenv("HOME"), input + 1);
    gtk_entry_set_text(Zathura.UI.inputbar, file);
    gtk_editable_set_position(GTK_EDITABLE(Zathura.UI.inputbar), -1);
    return NULL;
  }

  /* parse input string */
  if(input && strlen(input) > 0)
  {
    char* path_temp = dirname(strdup(input));
    char* file_temp = basename(strdup(input));
    char  last_char = input[strlen(input) - 1];

    if( !strcmp(path_temp, "/") && !strcmp(file_temp, "/") )
      file = "";
    else if( !strcmp(path_temp, "/") && strcmp(file_temp, "/") && last_char != '/')
      file = file_temp;
    else if( !strcmp(path_temp, "/") && strcmp(file_temp, "/") && last_char == '/')
      path = g_strdup_printf("/%s/", file_temp);
    else if(last_char == '/')
      path = input;
    else
    {
      path = g_strdup_printf("%s/", path_temp);
      file = file_temp;
    }
  }

  file_length = strlen(file);

  /* open directory */
  GDir* dir = g_dir_open(path, 0, NULL);
  if(!dir)
    return NULL;

  /* create element list */
  char* name = NULL;

  while((name = (char*) g_dir_read_name(dir)) != NULL)
  {
    char* d_name   = g_filename_display_name(name);
    int   d_length = strlen(d_name);

    if( ((file_length <= d_length) && !strncmp(file, d_name, file_length)) ||
        (file_length == 0) )
    {
      char* d = g_strdup_printf("%s%s", path, d_name);
      if(g_file_test(d, G_FILE_TEST_IS_DIR))
      {
        gchar *subdir = d;
        d = g_strdup_printf("%s/", subdir);
        g_free(subdir);
      }
      completion_group_add_element(group, d, NULL);
    }
  }

  g_dir_close(dir);

  return completion;
}

Completion*
cc_print(char* input)
{
  Completion* completion = completion_init();
  CompletionGroup* group = completion_group_create(NULL);

  completion_add_group(completion, group);

  int input_length = input ? strlen(input) : 0;

  /* read printers */
  char *current_line = NULL, current_char;
  int count = 0;
  FILE *fp;

  fp = popen(list_printer_command, "r");

  if(!fp)
  {
    completion_free(completion);
    return NULL;
  }

  while((current_char = fgetc(fp)) != EOF)
  {
    if(!current_line)
      current_line = malloc(sizeof(char));
    if(!current_line)
      out_of_memory();

    current_line = realloc(current_line, (count + 1) * sizeof(char));

    if(current_char != '\n')
      current_line[count++] = current_char;
    else
    {
      current_line[count] = '\0';
      int line_length = strlen(current_line);

      if( (input_length <= line_length) ||
          (!strncmp(input, current_line, input_length)) )
      {
        completion_group_add_element(group, g_strdup(current_line), NULL);
      }

      free(current_line);
      current_line = NULL;
      count = 0;
    }
  }

  pclose(fp);

  return completion;
}

Completion*
cc_set(char* input)
{
  Completion* completion = completion_init();
  CompletionGroup* group = completion_group_create(NULL);

  completion_add_group(completion, group);

  int i = 0;
  int input_length = input ? strlen(input) : 0;

  for(i = 0; i < LENGTH(settings); i++)
  {
    if( (input_length <= strlen(settings[i].name)) &&
          !strncmp(input, settings[i].name, input_length) )
    {
      completion_group_add_element(group, settings[i].name, settings[i].description);
    }
  }

  return completion;
}

/* buffer command implementation */
void
bcmd_goto(char* buffer, Argument* argument)
{
  if(!Zathura.PDF.document)
    return;

  int b_length = strlen(buffer);
  if(b_length < 1)
    return;

  if(!strcmp(buffer, "gg"))
    set_page(0);
  else if(!strcmp(buffer, "G"))
    set_page(Zathura.PDF.number_of_pages - 1);
  else
  {
    char* id = g_strndup(buffer, b_length - 1);
    int  pid = atoi(id);

    if(Zathura.Global.goto_mode == GOTO_LABELS)
    {
      int i;
      for(i = 0; i < Zathura.PDF.number_of_pages; i++)
        if(!strcmp(id, Zathura.PDF.pages[i]->label))
          pid = Zathura.PDF.pages[i]->id;
    }
    else if(Zathura.Global.goto_mode == GOTO_OFFSET)
      pid += Zathura.PDF.page_offset;

    set_page(pid - 1);
    g_free(id);
   }

  update_status();
}

void
bcmd_scroll(char* buffer, Argument* argument)
{
  int b_length = strlen(buffer);
  if(b_length < 1)
    return;

  int percentage = atoi(g_strndup(buffer, b_length - 1));
  percentage = (percentage < 0) ? 0 : ((percentage > 100) ? 100 : percentage);

  GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(Zathura.UI.view);

  gdouble view_size  = gtk_adjustment_get_page_size(adjustment);
  gdouble max        = gtk_adjustment_get_upper(adjustment) - view_size;
  gdouble nvalue     = (percentage * max) / 100;

  Zathura.State.scroll_percentage = percentage;
  gtk_adjustment_set_value(adjustment, nvalue);
  update_status();
}

void
bcmd_zoom(char* buffer, Argument* argument)
{
  Zathura.Global.adjust_mode = ADJUST_NONE;

  if(argument->n == ZOOM_IN)
  {
    if((Zathura.PDF.scale + zoom_step) <= zoom_max)
      Zathura.PDF.scale += zoom_step;
    else
      Zathura.PDF.scale = zoom_max;
  }
  else if(argument->n == ZOOM_OUT)
  {
    if((Zathura.PDF.scale - zoom_step) >= zoom_min)
      Zathura.PDF.scale -= zoom_step;
    else
      Zathura.PDF.scale = zoom_min;
  }
  else if(argument->n == ZOOM_SPECIFIC)
  {
    int b_length = strlen(buffer);
    if(b_length < 1)
      return;

    int value = atoi(g_strndup(buffer, b_length - 1));
    if(value <= zoom_min)
      Zathura.PDF.scale = zoom_min;
    else if(value >= zoom_max)
      Zathura.PDF.scale = zoom_max;
    else
      Zathura.PDF.scale = value;
  }
  else
    Zathura.PDF.scale = 100;

  Zathura.Search.draw = TRUE;
  draw(Zathura.PDF.page_number);
  update_status();
}

/* special command implementation */
gboolean
scmd_search(char* input, Argument* argument)
{
  if(!strlen(input))
    return TRUE;

  argument->data = input;
  sc_search(argument);

  return TRUE;
}

/* callback implementation */
gboolean
cb_destroy(GtkWidget* widget, gpointer data)
{
  pango_font_description_free(Zathura.Style.font);

  if(Zathura.PDF.document)
    close_file(FALSE);

  /* clean up other variables */
  g_free(Zathura.Bookmarks.file);

  /* inotify */
  if(Zathura.FileMonitor.monitor)
    g_object_unref(Zathura.FileMonitor.monitor);

  g_list_free(Zathura.Global.history);

  /* clean shortcut list */
  ShortcutList* sc = Zathura.Bindings.sclist;

  while(sc)
  {
    ShortcutList* ne = sc->next;
    free(sc);
    sc = ne;
  }

  gtk_main_quit();

  return TRUE;
}

gboolean cb_draw(GtkWidget* widget, GdkEventExpose* expose, gpointer data)
{
  if(!Zathura.PDF.document)
    return FALSE;

  int page_id = Zathura.PDF.page_number;
  if(page_id < 0 || page_id > Zathura.PDF.number_of_pages)
    return FALSE;

  gdk_window_clear(widget->window);
  cairo_t *cairo = gdk_cairo_create(widget->window);

  double page_width, page_height, width, height;
  double scale = ((double) Zathura.PDF.scale / 100.0);

  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  poppler_page_get_size(Zathura.PDF.pages[page_id]->page, &page_width, &page_height);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

  if(Zathura.PDF.rotate == 0 || Zathura.PDF.rotate == 180)
  {
    width  = page_width  * scale;
    height = page_height * scale;
  }
  else
  {
    width  = page_height * scale;
    height = page_width  * scale;
  }

  int window_x, window_y;
  gdk_drawable_get_size(widget->window, &window_x, &window_y);

  int offset_x, offset_y;

  if (window_x > width)
    offset_x = (window_x - width) / 2;
  else
    offset_x = 0;

  if (window_y > height)
    offset_y = (window_y - height) / 2;
  else
    offset_y = 0;

  if(Zathura.Search.draw)
  {
    GList* list;
    for(list = Zathura.Search.results; list && list->data; list = g_list_next(list))
      highlight_result(Zathura.Search.page, (PopplerRectangle*) list->data);
    Zathura.Search.draw = FALSE;
  }

  cairo_set_source_surface(cairo, Zathura.PDF.surface, offset_x, offset_y);
  cairo_paint(cairo);
  cairo_destroy(cairo);

  return TRUE;
}

gboolean
cb_inputbar_kb_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  int i;

  /* inputbar shortcuts */
  for(i = 0; i < LENGTH(inputbar_shortcuts); i++)
  {
    if(event->keyval == inputbar_shortcuts[i].key &&
      (((event->state & inputbar_shortcuts[i].mask) == inputbar_shortcuts[i].mask)
       || inputbar_shortcuts[i].mask == 0))
    {
      inputbar_shortcuts[i].function(&(inputbar_shortcuts[i].argument));
      return TRUE;
    }
  }

  /* special commands */
  char identifier = gtk_editable_get_chars(GTK_EDITABLE(Zathura.UI.inputbar), 0, 1)[0];
  for(i = 0; i < LENGTH(special_commands); i++)
  {
    if((identifier == special_commands[i].identifier) &&
       (special_commands[i].always == 1))
    {
      gchar *input  = gtk_editable_get_chars(GTK_EDITABLE(Zathura.UI.inputbar), 1, -1);
      special_commands[i].function(input, &(special_commands[i].argument));
      return FALSE;
    }
  }

  return FALSE;
}

gboolean
cb_inputbar_activate(GtkEntry* entry, gpointer data)
{
  gchar  *input  = gtk_editable_get_chars(GTK_EDITABLE(entry), 1, -1);
  gchar **tokens = g_strsplit(input, " ", -1);
  gchar *command = tokens[0];
  int     length = g_strv_length(tokens);
  int          i = 0;
  gboolean  retv = FALSE;
  gboolean  succ = FALSE;

  /* no input */
  if(length < 1)
  {
    isc_abort(NULL);
    return FALSE;
  }

  /* append input to the command history */
  Zathura.Global.history = g_list_append(Zathura.Global.history, g_strdup(gtk_entry_get_text(entry)));

  /* special commands */
  char identifier = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, 1)[0];
  for(i = 0; i < LENGTH(special_commands); i++)
  {
    if(identifier == special_commands[i].identifier)
    {
      /* special commands that are evaluated every key change are not
       * called here */
      if(special_commands[i].always == 1)
      {
        isc_abort(NULL);
        return TRUE;
      }

      retv = special_commands[i].function(input, &(special_commands[i].argument));
      if(retv) isc_abort(NULL);
      gtk_widget_grab_focus(GTK_WIDGET(Zathura.UI.view));
      return TRUE;
    }
  }

  /* search commands */
  for(i = 0; i < LENGTH(commands); i++)
  {
    if((g_strcmp0(command, commands[i].command) == 0) ||
       (g_strcmp0(command, commands[i].abbr)    == 0))
    {
      retv = commands[i].function(length - 1, tokens + 1);
      succ = TRUE;
      break;
    }
  }

  if(retv)
    isc_abort(NULL);

  if(!succ)
    notify(ERROR, "Unknown command.");

  Argument arg = { HIDE };
  isc_completion(&arg);
  gtk_widget_grab_focus(GTK_WIDGET(Zathura.UI.view));

  return TRUE;
}

gboolean
cb_inputbar_form_activate(GtkEntry* entry, gpointer data)
{
  if(!Zathura.PDF.document)
    return TRUE;

  Page* current_page = Zathura.PDF.pages[Zathura.PDF.page_number];
  int number_of_links = 0, link_id = 1, new_page_id = Zathura.PDF.page_number;

  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  GList *link_list = poppler_page_get_link_mapping(current_page->page);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));
  link_list = g_list_reverse(link_list);

  if((number_of_links = g_list_length(link_list)) <= 0)
    return FALSE;

  /* parse entry */
  gchar *input = gtk_editable_get_chars(GTK_EDITABLE(entry), 1, -1);
  gchar *token = input + strlen("Follow hint: ") - 1;
  if(!token)
    return FALSE;

  int li = atoi(token);
  if(li <= 0 || li > number_of_links)
  {
    set_page(Zathura.PDF.page_number);
    isc_abort(NULL);
    notify(WARNING, "Invalid hint");
    return TRUE;
  }

  /* compare entry */
  GList *links;
  for(links = link_list; links; links = g_list_next(links))
  {
    PopplerLinkMapping *link_mapping = (PopplerLinkMapping*) links->data;
    PopplerAction            *action = link_mapping->action;

    /* only handle URI and internal links */
    if(action->type == POPPLER_ACTION_URI)
    {
      if(li == link_id)
        open_uri(action->uri.uri);
    }
    else if(action->type == POPPLER_ACTION_GOTO_DEST)
    {
      if(li == link_id)
      {
        if(action->goto_dest.dest->type == POPPLER_DEST_NAMED)
        {
          PopplerDest* destination = poppler_document_find_dest(Zathura.PDF.document, action->goto_dest.dest->named_dest);
          if(destination)
          {
            new_page_id = destination->page_num - 1;
            poppler_dest_free(destination);
          }
        }
        else
          new_page_id = action->goto_dest.dest->page_num - 1;
      }
    }
    else
      continue;

    link_id++;
  }

  poppler_page_free_link_mapping(link_list);

  /* replace default inputbar handler */
  g_signal_handler_disconnect((gpointer) Zathura.UI.inputbar, Zathura.Handler.inputbar_activate);
  Zathura.Handler.inputbar_activate = g_signal_connect(G_OBJECT(Zathura.UI.inputbar), "activate", G_CALLBACK(cb_inputbar_activate), NULL);

  /* reset all */
  set_page(new_page_id);
  isc_abort(NULL);

  return TRUE;
}

gboolean
cb_inputbar_password_activate(GtkEntry* entry, gpointer data)
{
  gchar *input = gtk_editable_get_chars(GTK_EDITABLE(entry), 1, -1);
  gchar *token = input + strlen("Enter password: ") - 1;
  if(!token)
    return FALSE;

 if(!open_file(Zathura.PDF.file, token))
 {
   enter_password();
   return TRUE;
 }

  /* replace default inputbar handler */
  g_signal_handler_disconnect((gpointer) Zathura.UI.inputbar, Zathura.Handler.inputbar_activate);
  Zathura.Handler.inputbar_activate = g_signal_connect(G_OBJECT(Zathura.UI.inputbar), "activate", G_CALLBACK(cb_inputbar_activate), NULL);

  isc_abort(NULL);

  return TRUE;
}

gboolean
cb_view_kb_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  ShortcutList* sc = Zathura.Bindings.sclist;
  while(sc)
  {
    if(
       event->keyval == sc->element.key
       && (CLEAN(event->state) == sc->element.mask || (sc->element.key >= 0x21
       && sc->element.key <= 0x7E && CLEAN(event->state) == GDK_SHIFT_MASK))
       && (Zathura.Global.mode & sc->element.mode || sc->element.mode == ALL)
       && sc->element.function
      )
    {
      if(!(Zathura.Global.buffer && strlen(Zathura.Global.buffer->str)) || (sc->element.mask == GDK_CONTROL_MASK) ||
         (sc->element.key <= 0x21 || sc->element.key >= 0x7E)
        )
      {
        sc->element.function(&(sc->element.argument));
        return TRUE;
      }
    }

    sc = sc->next;
  }

  if(Zathura.Global.mode == ADD_MARKER)
  {
    add_marker(event->keyval);
    change_mode(NORMAL);
    return TRUE;
  }
  else if(Zathura.Global.mode == EVAL_MARKER)
  {
    eval_marker(event->keyval);
    change_mode(NORMAL);
    return TRUE;
  }

  /* append only numbers and characters to buffer */
  if( (event->keyval >= 0x21) && (event->keyval <= 0x7E))
  {
    if(!Zathura.Global.buffer)
      Zathura.Global.buffer = g_string_new("");

    Zathura.Global.buffer = g_string_append_c(Zathura.Global.buffer, event->keyval);
    gtk_label_set_markup((GtkLabel*) Zathura.Global.status_buffer, Zathura.Global.buffer->str);
  }

  /* search buffer commands */
  if(Zathura.Global.buffer)
  {
    int i;
    for(i = 0; i < LENGTH(buffer_commands); i++)
    {
      regex_t regex;
      int     status;

      regcomp(&regex, buffer_commands[i].regex, REG_EXTENDED);
      status = regexec(&regex, Zathura.Global.buffer->str, (size_t) 0, NULL, 0);
      regfree(&regex);

      if(status == 0)
      {
        buffer_commands[i].function(Zathura.Global.buffer->str, &(buffer_commands[i].argument));
        g_string_free(Zathura.Global.buffer, TRUE);
        Zathura.Global.buffer = NULL;
        gtk_label_set_markup((GtkLabel*) Zathura.Global.status_buffer, "");

        return TRUE;
      }
    }
  }

  return FALSE;
}

gboolean
cb_view_resized(GtkWidget* widget, GtkAllocation* allocation, gpointer data)
{
  Argument arg;
  arg.n = Zathura.Global.adjust_mode;
  sc_adjust_window(&arg);

  return TRUE;
}

gboolean
cb_view_button_pressed(GtkWidget* widget, GdkEventButton* event, gpointer data)
{
  if(!Zathura.PDF.document)
    return FALSE;

  /* clean page */
  draw(Zathura.PDF.page_number);
  g_static_mutex_lock(&(Zathura.Lock.select_lock));
  Zathura.SelectPoint.x = event->x;
  Zathura.SelectPoint.y = event->y;
  g_static_mutex_unlock(&(Zathura.Lock.select_lock));

  return TRUE;
}

gboolean
cb_view_button_release(GtkWidget* widget, GdkEventButton* event, gpointer data)
{
  if(!Zathura.PDF.document)
    return FALSE;

  double scale, offset_x, offset_y, page_width, page_height;
  PopplerRectangle rectangle;
  cairo_t* cairo;

  /* build selection rectangle */
  rectangle.x1 = event->x;
  rectangle.y1 = event->y;

  g_static_mutex_lock(&(Zathura.Lock.select_lock));
  rectangle.x2 = Zathura.SelectPoint.x;
  rectangle.y2 = Zathura.SelectPoint.y;
  g_static_mutex_unlock(&(Zathura.Lock.select_lock));

  /* calculate offset */
  calculate_offset(widget, &offset_x, &offset_y);

  /* draw selection rectangle */
  cairo = cairo_create(Zathura.PDF.surface);
  cairo_set_source_rgba(cairo, Zathura.Style.select_text.red, Zathura.Style.select_text.green,
      Zathura.Style.select_text.blue, transparency);
  cairo_rectangle(cairo, rectangle.x1 - offset_x, rectangle.y1 - offset_y,
      (rectangle.x2 - rectangle.x1), (rectangle.y2 - rectangle.y1));
  cairo_fill(cairo);
  gtk_widget_queue_draw(Zathura.UI.drawing_area);

  /* resize selection rectangle to document page */
  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  poppler_page_get_size(Zathura.PDF.pages[Zathura.PDF.page_number]->page, &page_width, &page_height);
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));

  scale        = ((double) Zathura.PDF.scale / 100.0);
  rectangle.x1 = (rectangle.x1 - offset_x) / scale;
  rectangle.y1 = (rectangle.y1 - offset_y) / scale;
  rectangle.x2 = (rectangle.x2 - offset_x) / scale;
  rectangle.y2 = (rectangle.y2 - offset_y) / scale;

  /* rotation */
  int rotate = Zathura.PDF.rotate;
  double x1 = rectangle.x1;
  double x2 = rectangle.x2;
  double y1 = rectangle.y1;
  double y2 = rectangle.y2;

  switch(rotate)
  {
    case 90:
      rectangle.x1 = y1;
      rectangle.y1 = page_height - x2;
      rectangle.x2 = y2;
      rectangle.y2 = page_height - x1;
      break;
    case 180:
      rectangle.x1 = (page_height - y1);
      rectangle.y1 = (page_width  - x2);
      rectangle.x2 = (page_height - y2);
      rectangle.y2 = (page_width  - x1);
      break;
    case 270:
      rectangle.x1 = page_width - y2;
      rectangle.y1 = x1;
      rectangle.x2 = page_width - y1;
      rectangle.y2 = x2;
      break;
  }

  /* reset points of the rectangle so that p1 is in the top-left corner
   * and p2 is in the bottom right corner */
  if(rectangle.x1 > rectangle.x2)
  {
    double d = rectangle.x1;
    rectangle.x1 = rectangle.x2;
    rectangle.x2 = d;
  }
  if(rectangle.y2 > rectangle.y1)
  {
    double d = rectangle.y1;
    rectangle.y1 = rectangle.y2;
    rectangle.y2 = d;
  }

  /* adapt y coordinates */
  rectangle.y1 = page_height - rectangle.y1;
  rectangle.y2 = page_height - rectangle.y2;

  /* get selected text */
  g_static_mutex_lock(&(Zathura.Lock.pdflib_lock));
  char* selected_text = poppler_page_get_text(
      Zathura.PDF.pages[Zathura.PDF.page_number]->page,SELECTION_STYLE,
      &rectangle);

  if(selected_text)
  {
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), selected_text, -1);
    g_free(selected_text);
  }
  g_static_mutex_unlock(&(Zathura.Lock.pdflib_lock));


  return TRUE;
}

gboolean
cb_view_motion_notify(GtkWidget* widget, GdkEventMotion* event, gpointer data)
{
  return TRUE;
}

gboolean
cb_view_scrolled(GtkWidget* widget, GdkEventScroll* event, gpointer data)
{
  int i;
  for(i = 0; i < LENGTH(mouse_scroll_events); i++)
  {
    if(event->direction == mouse_scroll_events[i].direction)
    {
      mouse_scroll_events[i].function(&(mouse_scroll_events[i].argument));
      return TRUE;
    }
  }

  return FALSE;
}

gboolean
cb_watch_file(GFileMonitor* monitor, GFile* file, GFile* other_file, GFileMonitorEvent event, gpointer data)
{
  if(event != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    return FALSE;

  sc_reload(NULL);

  return TRUE;
}

/* main function */
int main(int argc, char* argv[])
{
  /* embed */
  Zathura.UI.embed = 0;

  /* parse arguments */
  int i;
  for(i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++)
  {
    switch(argv[i][1])
    {
      case 'e':
        if(++i < argc)
          Zathura.UI.embed = atoi(argv[i]);
        break;
    }
  }

  g_thread_init(NULL);
  gdk_threads_init();

  gtk_init(&argc, &argv);

  init_zathura();
  init_keylist();
  read_configuration();
  init_settings();
  init_look();
  init_directories();

  if(argc >= i+1)
    open_file(argv[i], (argc == i+2) ? argv[i+1] : NULL);

  switch_view(Zathura.UI.document);
  update_status();

  gtk_widget_show_all(GTK_WIDGET(Zathura.UI.window));
  gtk_widget_grab_focus(GTK_WIDGET(Zathura.UI.view));

  if(!Zathura.Global.show_inputbar)
    gtk_widget_hide(GTK_WIDGET(Zathura.UI.inputbar));

  if(!Zathura.Global.show_statusbar)
    gtk_widget_hide(GTK_WIDGET(Zathura.UI.statusbar));

  gdk_threads_enter();
  gtk_main();
  gdk_threads_leave();

  return 0;
}
