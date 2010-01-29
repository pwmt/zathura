/* See LICENSE file for license and copyright information */

#define _BSD_SOURCE || _XOPEN_SOURCE >= 500

#include <regex.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <pthread.h>

#include <poppler/glib/poppler.h>
#include <cairo.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

/* macros */
#define LENGTH(x) sizeof(x)/sizeof((x)[0])

/* enums */
enum { NEXT, PREVIOUS, LEFT, RIGHT, UP, DOWN,
       BOTTOM, TOP, HIDE, NORMAL, HIGHLIGHT,
       INSERT, VISUAL, DELETE_LAST_WORD, DEFAULT,
       ERROR, WARNING, NEXT_GROUP, PREVIOUS_GROUP,
       ZOOM_IN, ZOOM_OUT, ZOOM_ORIGINAL, ZOOM_SPECIFIC,
       FORWARD, BACKWARD, ADJUST_BESTFIT, ADJUST_WIDTH,
       CONTINUOUS };

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
  int mask;
  int key;
  void (*function)(Argument*);
  int mode;
  Argument argument;
} Shortcut;

typedef struct
{
  int mask;
  int key;
  void (*function)(Argument*);
  Argument argument;
} InputbarShortcut;

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

typedef struct
{
  PopplerPage     *page;
  pthread_mutex_t  lock;
} Page;

typedef struct
{
  char* name;
  void* variable;
  char  type;
  gboolean render;
  char* description;
} Setting;

/* zathura */
struct
{
  struct
  {
    GtkWindow         *window;
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
    GdkColor search_highlight;
    PangoFontDescription *font;
  } Style;

  struct
  {
    GString *buffer;
    GList   *history;
    int      mode;
    int      viewing_mode;
    gboolean reverse_video;
    GtkLabel *status_text;
    GtkLabel *status_buffer;
    GtkLabel *status_state;
  } Global;

  struct
  {
    pthread_mutex_t scale_lock;
    pthread_mutex_t rotate_lock;
    pthread_mutex_t search_lock;
    pthread_mutex_t viewport_lock;
  } Lock;

  struct
  {
    pthread_t      search_thread;
    gboolean       search_thread_running;
  } Thread;

  struct
  {
    char* filename;
    char* pages;
  } State;

  struct
  {
    PopplerDocument *document;
    char            *file;
    Page           **pages;
    int              page_number;
    int              number_of_pages;
    int              scale;
    int              rotate;
    cairo_surface_t *surface;
  } PDF;

} Zathura;

/* function declarations */
void init_zathura();
void build_index(GtkTreeModel*, GtkTreeIter*, PopplerIndexIter*);
void change_mode(int);
void highlight_result(int, PopplerRectangle*);
void draw(int);
void notify(int, char*);
void update_status();
void recalcRectangle(int, PopplerRectangle*);
void setCompletionRowColor(GtkBox*, int, int);
void set_page(int);
void switch_view(GtkWidget*);
GtkEventBox* createCompletionRow(GtkBox*, char*, char*, gboolean);

/* thread declaration */
void* search(void*);

/* shortcut declarations */
void sc_abort(Argument*);
void sc_adjust_window(Argument*);
void sc_change_mode(Argument*);
void sc_focus_inputbar(Argument*);
void sc_navigate(Argument*);
void sc_revert_video(Argument*);
void sc_rotate(Argument*);
void sc_scroll(Argument*);
void sc_search(Argument*);
void sc_toggle_index(Argument*);
void sc_toggle_inputbar(Argument*);
void sc_toggle_statusbar(Argument*);
void sc_quit(Argument*);

/* inputbar shortcut declarations */
void isc_abort(Argument*);
void isc_command_history(Argument*);
void isc_completion(Argument*);
void isc_string_manipulation(Argument*);

/* command declarations */
gboolean cmd_close(int, char**);
gboolean cmd_export(int, char**);
gboolean cmd_info(int, char**);
gboolean cmd_open(int, char**);
gboolean cmd_print(int, char**);
gboolean cmd_rotate(int, char**);
gboolean cmd_set(int, char**);
gboolean cmd_quit(int, char**);
gboolean cmd_save(int, char**);

/* completion commands */
Completion* cc_export(char*);
Completion* cc_open(char*);
Completion* cc_print(char*);
Completion* cc_set(char*);

/* buffer command declarations */
void bcmd_goto(char*, Argument*);
void bcmd_zoom(char*, Argument*);

/* special command delcarations */
gboolean scmd_search(char*, Argument*);

/* callback declarations */
gboolean cb_destroy(GtkWidget*, gpointer);
gboolean cb_draw(GtkWidget*, GdkEventExpose*, gpointer);
gboolean cb_view_kb_pressed(GtkWidget*, GdkEventKey*, gpointer);
gboolean cb_index_selection_changed(GtkTreeSelection*, GtkWidget*);
gboolean cb_inputbar_kb_pressed(GtkWidget*, GdkEventKey*, gpointer);
gboolean cb_inputbar_activate(GtkEntry*, gpointer);

/* configuration */
#include "config.h"

/* function implementation */
void
init_zathura()
{
  /* mutexes */
  pthread_mutex_init(&(Zathura.Lock.scale_lock),    NULL);
  pthread_mutex_init(&(Zathura.Lock.rotate_lock),   NULL);
  pthread_mutex_init(&(Zathura.Lock.search_lock),   NULL);
  pthread_mutex_init(&(Zathura.Lock.viewport_lock), NULL);

  /* look */
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
  gdk_color_parse(search_highlight,       &(Zathura.Style.search_highlight));
  Zathura.Style.font = pango_font_description_from_string(font);

  /* other */
  Zathura.Global.mode          = NORMAL;
  Zathura.Global.viewing_mode  = NORMAL;
  Zathura.Global.reverse_video = FALSE;

  Zathura.State.filename = (char*) DEFAULT_TEXT;
  Zathura.State.pages = "";

  /* UI */
  Zathura.UI.window            = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  Zathura.UI.box               = GTK_BOX(gtk_vbox_new(FALSE, 0));
  Zathura.UI.continuous        = GTK_BOX(gtk_vbox_new(FALSE, 0));
  Zathura.UI.view              = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
  Zathura.UI.viewport          = GTK_VIEWPORT(gtk_viewport_new(NULL, NULL));
  Zathura.UI.drawing_area      = gtk_drawing_area_new();
  Zathura.UI.statusbar         = gtk_event_box_new();
  Zathura.UI.statusbar_entries = GTK_BOX(gtk_hbox_new(FALSE, 0));
  Zathura.UI.inputbar          = GTK_ENTRY(gtk_entry_new());

  /* window */
  gtk_window_set_title(Zathura.UI.window, "zathura");
  GdkGeometry hints = { 1, 1 };
  gtk_window_set_geometry_hints(Zathura.UI.window, NULL, &hints, GDK_HINT_MIN_SIZE);
  gtk_window_set_default_size(Zathura.UI.window, DEFAULT_WIDTH, DEFAULT_HEIGHT);
  g_signal_connect(G_OBJECT(Zathura.UI.window), "destroy", G_CALLBACK(cb_destroy), NULL);

  /* box */
  gtk_box_set_spacing(Zathura.UI.box, 0);
  gtk_container_add(GTK_CONTAINER(Zathura.UI.window), GTK_WIDGET(Zathura.UI.box));

  /* continuous */
  gtk_box_set_spacing(Zathura.UI.continuous, 5);

  /* view */
  g_signal_connect(G_OBJECT(Zathura.UI.view), "key-press-event", G_CALLBACK(cb_view_kb_pressed), NULL);
  gtk_container_add(GTK_CONTAINER(Zathura.UI.view), GTK_WIDGET(Zathura.UI.viewport));
  gtk_viewport_set_shadow_type(Zathura.UI.viewport, GTK_SHADOW_NONE);
  
  #if SHOW_SCROLLBARS
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.UI.view), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  #else
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.UI.view), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  #endif

  /* drawing area */
  gtk_widget_modify_bg(GTK_WIDGET(Zathura.UI.drawing_area), GTK_STATE_NORMAL, &(Zathura.Style.default_bg));
  gtk_widget_show(Zathura.UI.drawing_area);
  g_signal_connect(G_OBJECT(Zathura.UI.drawing_area), "expose-event", G_CALLBACK(cb_draw), NULL);

  /* statusbar */
  gtk_widget_modify_bg(GTK_WIDGET(Zathura.UI.statusbar), GTK_STATE_NORMAL, &(Zathura.Style.statusbar_bg));

  Zathura.Global.status_text   = GTK_LABEL(gtk_label_new(NULL));
  Zathura.Global.status_state  = GTK_LABEL(gtk_label_new(NULL));
  Zathura.Global.status_buffer = GTK_LABEL(gtk_label_new(NULL));

  gtk_widget_modify_fg(GTK_WIDGET(Zathura.Global.status_text),  GTK_STATE_NORMAL, &(Zathura.Style.statusbar_fg));
  gtk_widget_modify_fg(GTK_WIDGET(Zathura.Global.status_state), GTK_STATE_NORMAL, &(Zathura.Style.statusbar_fg));
  gtk_widget_modify_fg(GTK_WIDGET(Zathura.Global.status_buffer), GTK_STATE_NORMAL, &(Zathura.Style.statusbar_fg));

  gtk_widget_modify_font(GTK_WIDGET(Zathura.Global.status_text),  Zathura.Style.font);
  gtk_widget_modify_font(GTK_WIDGET(Zathura.Global.status_state), Zathura.Style.font);
  gtk_widget_modify_font(GTK_WIDGET(Zathura.Global.status_buffer), Zathura.Style.font);

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

  gtk_widget_modify_base(GTK_WIDGET(Zathura.UI.inputbar), GTK_STATE_NORMAL, &(Zathura.Style.inputbar_bg));
  gtk_widget_modify_text(GTK_WIDGET(Zathura.UI.inputbar), GTK_STATE_NORMAL, &(Zathura.Style.inputbar_fg));
  gtk_widget_modify_font(GTK_WIDGET(Zathura.UI.inputbar),                     Zathura.Style.font);

  g_signal_connect(G_OBJECT(Zathura.UI.inputbar), "key-press-event",   G_CALLBACK(cb_inputbar_kb_pressed), NULL);
  g_signal_connect(G_OBJECT(Zathura.UI.inputbar), "activate",          G_CALLBACK(cb_inputbar_activate),   NULL);

  /* packing */
  gtk_box_pack_start(Zathura.UI.box, GTK_WIDGET(Zathura.UI.view),      TRUE,  TRUE,  0);
  gtk_box_pack_start(Zathura.UI.box, GTK_WIDGET(Zathura.UI.statusbar), FALSE, FALSE, 0);
  gtk_box_pack_end(  Zathura.UI.box, GTK_WIDGET(Zathura.UI.inputbar),  FALSE, FALSE, 0);
}

void
build_index(GtkTreeModel* model, GtkTreeIter* parent, PopplerIndexIter* index_iter)
{
  do
  {
    GtkTreeIter       tree_iter;
    PopplerIndexIter *child;
    PopplerAction    *action;
    gboolean          expand;
    gchar            *markup;

    action = poppler_index_iter_get_action(index_iter);
    expand = poppler_index_iter_is_open(index_iter);

    if(!action)
      continue;

    markup = g_markup_escape_text(action->any.title, -1);

    gtk_tree_store_append(GTK_TREE_STORE(model), &tree_iter, parent);
    gtk_tree_store_set(GTK_TREE_STORE(model), &tree_iter, 0, markup,
      1, action, -1);
    g_object_weak_ref(G_OBJECT(model), (GWeakNotify) poppler_action_free, action);
    g_free(markup);

    child = poppler_index_iter_get_child(index_iter);
    if(child)
      build_index(model, &tree_iter, child);
    poppler_index_iter_free(child);
  }
  while(poppler_index_iter_next(index_iter));
}

void
draw(int page_id)
{
  if(!Zathura.PDF.document || page_id < 0 || page_id > Zathura.PDF.number_of_pages)
    return;

  double page_width, page_height;
  double width, height;

  pthread_mutex_lock(&(Zathura.Lock.scale_lock));
  double scale = ((double) Zathura.PDF.scale / 100.0);
  pthread_mutex_unlock(&(Zathura.Lock.scale_lock));

  pthread_mutex_lock(&(Zathura.Lock.rotate_lock));
  int rotate = Zathura.PDF.rotate;
  pthread_mutex_unlock(&(Zathura.Lock.rotate_lock));

  pthread_mutex_lock(&(Zathura.PDF.pages[page_id]->lock));
  Page *current_page = Zathura.PDF.pages[page_id];

  if(Zathura.PDF.surface)
    cairo_surface_destroy(Zathura.PDF.surface);
  Zathura.PDF.surface = NULL;

  poppler_page_get_size(current_page->page, &page_width, &page_height);

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

  poppler_page_render(current_page->page, cairo);

  cairo_restore(cairo);
  cairo_destroy(cairo);

  if(Zathura.Global.reverse_video)
  {
    unsigned char* image = cairo_image_surface_get_data(Zathura.PDF.surface);
    int x, y, z = 0;

    for(x = 0; x < cairo_image_surface_get_width(Zathura.PDF.surface); x++)
      for(y = 0; y < cairo_image_surface_get_height(Zathura.PDF.surface) * 4; y++)
        image[z++] ^= 0x00FFFFFF;
  }

  gtk_widget_set_size_request(Zathura.UI.drawing_area, width, height);
  gtk_widget_queue_draw(Zathura.UI.drawing_area);
  pthread_mutex_unlock(&(Zathura.PDF.pages[page_id]->lock));
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
    default:
      mode_text = "";
      mode      = NORMAL;
      break;
  }

  Zathura.Global.mode = mode;
  notify(DEFAULT, mode_text);
}

void
highlight_result(int page_id, PopplerRectangle* rectangle)
{
  cairo_t *cairo = cairo_create(Zathura.PDF.surface);
  cairo_set_source_rgba(cairo, Zathura.Style.search_highlight.red, Zathura.Style.search_highlight.green,
      Zathura.Style.search_highlight.blue, TRANSPARENCY);

  recalcRectangle(page_id, rectangle);
  cairo_rectangle(cairo, rectangle->x1, rectangle->y1, (rectangle->x2 - rectangle->x1), (rectangle->y2 - rectangle->y1));
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

void
update_status()
{
  /* update text */
  gtk_label_set_markup((GtkLabel*) Zathura.Global.status_text, Zathura.State.filename);

  /* update state */
  pthread_mutex_lock(&(Zathura.Lock.scale_lock));
  char* zoom_level   = (Zathura.PDF.scale != 0) ? g_strdup_printf("%d%%", Zathura.PDF.scale) : "";
  pthread_mutex_unlock(&(Zathura.Lock.scale_lock));
  char* status_text  = g_strdup_printf("%s %s", zoom_level, Zathura.State.pages);
  gtk_label_set_markup((GtkLabel*) Zathura.Global.status_state, status_text);
}

void
recalcRectangle(int page_id, PopplerRectangle* rectangle)
{
  double page_width, page_height;
  double x1 = rectangle->x1;
  double x2 = rectangle->x2;
  double y1 = rectangle->y1;
  double y2 = rectangle->y2;

  pthread_mutex_lock(&(Zathura.PDF.pages[page_id]->lock));
  poppler_page_get_size(Zathura.PDF.pages[page_id]->page, &page_width, &page_height);
  pthread_mutex_unlock(&(Zathura.PDF.pages[page_id]->lock));

  pthread_mutex_lock(&(Zathura.Lock.scale_lock));
  double scale = ((double) Zathura.PDF.scale / 100.0);
  pthread_mutex_unlock(&(Zathura.Lock.scale_lock));

  pthread_mutex_lock(&(Zathura.Lock.rotate_lock));
  int rotate = Zathura.PDF.rotate;
  pthread_mutex_unlock(&(Zathura.Lock.rotate_lock));

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
  if(page > Zathura.PDF.number_of_pages || page < 0)
  {
    notify(WARNING, "Could not open page");
    return;
  }

  Zathura.PDF.page_number = page;
  Zathura.State.pages = g_strdup_printf("[%i/%i]", page + 1, Zathura.PDF.number_of_pages);
  switch_view(Zathura.UI.drawing_area);
  draw(page);
}

void
switch_view(GtkWidget* widget)
{
  pthread_mutex_lock(&(Zathura.Lock.viewport_lock));
  GtkWidget* child = gtk_bin_get_child(GTK_BIN(Zathura.UI.viewport));
  if(child)
  {
    g_object_ref(child);
    gtk_container_remove(GTK_CONTAINER(Zathura.UI.viewport), child);
  }
  pthread_mutex_unlock(&(Zathura.Lock.viewport_lock));

  gtk_container_add(GTK_CONTAINER(Zathura.UI.viewport), GTK_WIDGET(widget));
}

/* thread implementation */
void*
search(void* parameter)
{
  Zathura.Thread.search_thread_running = TRUE;
  Argument* argument = (Argument*) parameter;

  static char* search_item;
  static int direction;
  static int next_page = 0;
  int page_counter;
  GList* results;
  GList* list;

  if(argument->data)
    search_item = g_strdup((char*) argument->data);

  if(!Zathura.PDF.document || !search_item || !strlen(search_item))
  {
    Zathura.Thread.search_thread_running = FALSE;
    pthread_exit(NULL);
  }

  /* search document */
  if(argument->n)
    direction = (argument->n == BACKWARD) ? -1 : 1;

  for(page_counter = 1; page_counter <= Zathura.PDF.number_of_pages; page_counter++)
  {
    next_page = (Zathura.PDF.number_of_pages + Zathura.PDF.page_number +
        page_counter * direction) % Zathura.PDF.number_of_pages;

    PopplerPage* page = poppler_document_get_page(Zathura.PDF.document, next_page);
    if(!page)
      pthread_exit(NULL);
    results = poppler_page_find_text(page, search_item);

    if(results)
      break;
  }

  /* draw results */
  if(results)
  {
    for(list = results; list && list->data; list = g_list_next(list))
      highlight_result(next_page, (PopplerRectangle*) list->data);

    set_page(next_page);
  }
  else
  {
    printf("Nothing found for %s\n", search_item);
  }

  Zathura.Thread.search_thread_running = FALSE;
  pthread_exit(NULL);
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
}

void
sc_adjust_window(Argument* argument)
{
  GtkAdjustment* adjustment;
  double view_size;
  double page_width;
  double page_height;

  if(argument->n == ADJUST_WIDTH)
    adjustment = gtk_scrolled_window_get_vadjustment(Zathura.UI.view);
  else
    adjustment = gtk_scrolled_window_get_hadjustment(Zathura.UI.view);

  view_size  = gtk_adjustment_get_page_size(adjustment);

  pthread_mutex_lock(&(Zathura.PDF.pages[Zathura.PDF.page_number]->lock));
  poppler_page_get_size(Zathura.PDF.pages[Zathura.PDF.page_number]->page, &page_width, &page_height);
  pthread_mutex_unlock(&(Zathura.PDF.pages[Zathura.PDF.page_number]->lock));

  if ((Zathura.PDF.rotate == 90) || (Zathura.PDF.rotate == 270))
  {
    double swap = page_width;
    page_width  = page_height;
    page_height = swap;
  }

  if(argument->n == ADJUST_WIDTH)
    Zathura.PDF.scale = (view_size / page_height) * 100;
  else
    Zathura.PDF.scale = (view_size / page_width) * 100;

  draw(Zathura.PDF.page_number);
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
  if(argument->data)
  {
    notify(DEFAULT, argument->data);
    gtk_widget_grab_focus(GTK_WIDGET(Zathura.UI.inputbar));
    gtk_editable_set_position(GTK_EDITABLE(Zathura.UI.inputbar), -1);
  }
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
sc_revert_video(Argument* argument)
{
  Zathura.Global.reverse_video = !Zathura.Global.reverse_video;
  draw(Zathura.PDF.page_number);
}

void
sc_rotate(Argument* argument)
{
  pthread_mutex_lock(&(Zathura.Lock.rotate_lock));
  Zathura.PDF.rotate = (Zathura.PDF.rotate + 90) % 360;
  pthread_mutex_unlock(&(Zathura.Lock.rotate_lock));

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

  if( (argument->n == LEFT) || (argument->n == DOWN))
    gtk_adjustment_set_value(adjustment, (value - SCROLL_STEP) < 0 ? 0 : (value - SCROLL_STEP));
  else if (argument->n == TOP)
    gtk_adjustment_set_value(adjustment, 0);
  else if(argument->n == BOTTOM)
    gtk_adjustment_set_value(adjustment, max);
  else
    gtk_adjustment_set_value(adjustment, (value + SCROLL_STEP) > max ? max : (value + SCROLL_STEP));
}

void
sc_search(Argument* argument)
{
  pthread_mutex_lock(&(Zathura.Lock.search_lock));
  if(Zathura.Thread.search_thread_running)
  {
    if(pthread_cancel(Zathura.Thread.search_thread) == 0)
      pthread_join(Zathura.Thread.search_thread, NULL);

    Zathura.Thread.search_thread_running = FALSE;
  }

  pthread_create(&(Zathura.Thread.search_thread), NULL, search, (gpointer) argument);
  pthread_mutex_unlock(&(Zathura.Lock.search_lock));
}

void
sc_toggle_index(Argument* argument)
{
  if(!Zathura.PDF.document)
    return;

  if(!Zathura.UI.index)
  {
    GtkWidget        *treeview;
    GtkTreeModel     *model;
    PopplerIndexIter *index_iter;
    GtkCellRenderer  *renderer;
    GtkTreeSelection *selection;

    Zathura.UI.index = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.UI.index),
      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    index_iter = poppler_index_iter_new(Zathura.PDF.document);

    if(index_iter)
    {
      model = GTK_TREE_MODEL(gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_POINTER));
      build_index(model, NULL, index_iter);
      poppler_index_iter_free(index_iter);
      treeview = gtk_tree_view_new_with_model(model);
      g_object_unref(model);

      renderer = gtk_cell_renderer_text_new();
      gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), 0, "Title", renderer,
        "markup", 0, NULL);
      gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);

      selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
      g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(cb_index_selection_changed),
        NULL);

      gtk_container_add(GTK_CONTAINER(Zathura.UI.index), treeview);
      gtk_widget_show(GTK_WIDGET(treeview));
    }
    else
    {
      notify(WARNING, "This document does not contain any index");
      Zathura.UI.index = NULL;
      return;
    }
  }

  static gboolean show = TRUE;

  if(show)
  {
    gtk_widget_show(Zathura.UI.index);
    switch_view(Zathura.UI.index);
  }
  else
  {
    switch_view(Zathura.UI.drawing_area);
  }

  show = !show;
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

/* inputbar shortcut declarations */
void
isc_abort(Argument* argument)
{
  Argument arg = { HIDE };
  isc_completion(&arg);

  notify(DEFAULT, "");
  gtk_widget_grab_focus(GTK_WIDGET(Zathura.UI.view));
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
      group = result->groups;

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
    /* create list based on commands */
    else
    {
      int i = 0;
      command_mode = TRUE;

      rows = malloc(LENGTH(commands) * sizeof(CompletionRow));

      for(i = 0; i < LENGTH(commands); i++)
      {
        int abbr_length = strlen(commands[i].abbr);
        int cmd_length  = strlen(commands[i].command);

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

    if(command_mode)
    {
      char* cp = (current_parameter) ? g_strconcat(" ", current_parameter, NULL) : 0;
      temp = g_strconcat(":", rows[current_item].command, cp, NULL);
    }
    else
    {
      temp = g_strconcat(":", previous_command, " ", rows[current_item].command, NULL);
    }

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
  if(argument->n == DELETE_LAST_WORD)
  {
    gchar *input  = gtk_editable_get_chars(GTK_EDITABLE(Zathura.UI.inputbar), 0, -1);
    int    length = strlen(input);
    int    i      = 0;

    for(i = length; i > 0; i--)
    {
      if( (input[i] == ' ') ||
          (input[i] == '/') )
      {
        if(i == (length - 1))
          continue;

        i = (input[i] == ' ') ? (i - 1) : i;
        break;
      }
    }

    notify(DEFAULT, g_strndup(input, i + 1));
    gtk_editable_set_position(GTK_EDITABLE(Zathura.UI.inputbar), -1);
  }
}

/* command implementation */
gboolean
cmd_close(int argc, char** argv)
{
  if(!Zathura.PDF.document)
  {
    if(argc != -1)
      notify(ERROR, "No file has been opened");
    return FALSE;
  }

  /* clean up pages */
  int i;
  for(i = 0; i < Zathura.PDF.number_of_pages; i++)
  {
    Page* current_page = Zathura.PDF.pages[i];
    g_object_unref(current_page->page);
    pthread_mutex_destroy(&current_page->lock);
  }

  /* reset values */
  free(Zathura.PDF.pages);
  g_object_unref(Zathura.PDF.document);

  Zathura.State.pages         = "";
  Zathura.State.filename      = (char*) DEFAULT_TEXT;
  Zathura.PDF.document        = NULL;
  Zathura.PDF.file            = "";
  Zathura.PDF.page_number     = 0;
  Zathura.PDF.number_of_pages = 0;
  Zathura.PDF.scale           = 0;
  Zathura.PDF.rotate          = 0;

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

  update_status();

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

      pthread_mutex_lock(&(Zathura.PDF.pages[page_number]->lock));
      image_list = poppler_page_get_image_mapping(Zathura.PDF.pages[page_number]->page);
      pthread_mutex_unlock(&(Zathura.PDF.pages[page_number]->lock));

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

        pthread_mutex_lock(&(Zathura.PDF.pages[page_number]->lock));
        image     = poppler_page_get_image(Zathura.PDF.pages[page_number]->page, image_id);

        if(!image)
          continue;

        filename  = g_strdup_printf("%s_p%i_i%i.png", Zathura.PDF.file, page_number + 1, image_id);

        if(argv[1][0] == '~')
        {
          file = malloc(((int) strlen(filename) + (int) strlen(argv[1]) 
            + (int) strlen(getenv("HOME")) - 1) * sizeof(char));
          file = g_strdup_printf("%s%s%s", getenv("HOME"), argv[1] + 1, filename);
        }
        else
          file = g_strdup_printf("%s%s", argv[1], filename);

        cairo_surface_write_to_png(image, file);
        pthread_mutex_unlock(&(Zathura.PDF.pages[page_number]->lock));

        g_free(file);
      }
    }
  }
  else if(!strcmp(argv[0], "attachments"))
  {
    if(!poppler_document_has_attachments(Zathura.PDF.document))
    {
      notify(WARNING, "PDF file has no attachments");
      return FALSE;
    }

    GList *attachment_list = poppler_document_get_attachments(Zathura.PDF.document);
    GList *attachments;
    char  *file;

    for(attachments = attachment_list; attachments; attachments = g_list_next(attachments))
    {
      PopplerAttachment *attachment = (PopplerAttachment*) attachments->data;

      if(argv[1][0] == '~')
      {
        file = malloc(((int) strlen(attachment->name) + (int) strlen(argv[1])
          + (int) strlen(getenv("HOME")) - 1) * sizeof(char));
        file = g_strdup_printf("%s%s%s", getenv("HOME"), argv[1] + 1, attachment->name);
      }
      else
        file = g_strdup_printf("%s%s", argv[1], attachment->name);

      poppler_attachment_save(attachment, file, NULL);

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

  static gboolean show = TRUE;

  if(show)
    switch_view(Zathura.UI.information);
  else
  {
    switch_view(Zathura.UI.drawing_area);
  }

  show = !show;

  return FALSE;
}

gboolean
cmd_open(int argc, char** argv)
{
  if(argc == 0 || strlen(argv[0]) == 0)
    return FALSE;
  
  /* get filename */
  char* file = realpath(argv[0], NULL);

  if(argv[0][0] == '~')
    file = g_strdup_printf("%s%s", getenv("HOME"), argv[0] + 1);

  /* check if file exists */
  if(!g_file_test(file, G_FILE_TEST_IS_REGULAR))
  {
    notify(ERROR, "File does not exist");
    return FALSE;
  }

  /* close old file */
  cmd_close(-1, NULL);

  /* open file */
  Zathura.PDF.document = poppler_document_new_from_file(g_strdup_printf("file://%s", file),
      (argc == 2) ? argv[1] : NULL, NULL);

  if(!Zathura.PDF.document)
  {
    notify(ERROR, "Can not open file");
    return FALSE;
  }

  Zathura.PDF.number_of_pages = poppler_document_get_n_pages(Zathura.PDF.document);
  Zathura.PDF.file            = file;
  pthread_mutex_lock(&(Zathura.Lock.scale_lock));
  Zathura.PDF.scale           = 100;
  pthread_mutex_unlock(&(Zathura.Lock.scale_lock));
  pthread_mutex_lock(&(Zathura.Lock.rotate_lock));
  Zathura.PDF.rotate          = 0;
  pthread_mutex_unlock(&(Zathura.Lock.rotate_lock));
  Zathura.PDF.pages           = malloc(Zathura.PDF.number_of_pages * sizeof(Page*));
  Zathura.State.filename      = file;

  /* get pages */
  int i;
  for(i = 0; i < Zathura.PDF.number_of_pages; i++)
  {
    Zathura.PDF.pages[i] = malloc(sizeof(Page));
    Zathura.PDF.pages[i]->page = poppler_document_get_page(Zathura.PDF.document, i);
    pthread_mutex_init(&(Zathura.PDF.pages[i]->lock), NULL);
  }

  /* render pages */
  draw(0);

  set_page(0);
  update_status();

  return TRUE;
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
  char* sites   = (argc == 2) ? argv[1] : g_strdup_printf("%i", Zathura.PDF.number_of_pages);
  char* print_command = g_strdup_printf(PRINT_COMMAND, printer, sites, Zathura.PDF.file);
  system(print_command);

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
  if(argc <= 0 || argc >= 3)
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
            *x = TRUE;
          else
            *x = FALSE;
        }
      }
      else if(settings[i].type == 'i')
      {
        if(argc != 2)
          return FALSE;

        int *x = (int*) (settings[i].variable);
        if(argv[1])
          *x = atoi(argv[1]);
      }
      else if(settings[i].type == 's')
      {
        if(argc != 2)
          return FALSE;

        char **x = (char**) settings[i].variable;
        if(argv[1])
          *x = argv[1];
      }
      else if(settings[i].type == 'c')
      {
        if(argc != 2)
          return FALSE;

        char *x = (char*) (settings[i].variable);
        if(argv[1])
          *x = argv[1][0];
      }

      /* render */
      if(settings[i].render)
      {
        if(!Zathura.PDF.document)
          return FALSE;

        draw(Zathura.PDF.page_number);
      }
    }
  }

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
  return TRUE;
}

/* completion command implementation */
Completion*
cc_export(char* input)
{
  /* init completion group */
  Completion *completion = malloc(sizeof(Completion));
  CompletionGroup* group = malloc(sizeof(CompletionGroup));

  group->value    = NULL;
  group->next     = NULL;
  group->elements = NULL;

  completion->groups = group;

  /* export images */
  CompletionElement *export_images = malloc(sizeof(CompletionElement));
  export_images->value = "images";
  export_images->description = "Export images";
  export_images->next = NULL;

  /* export attachmants */
  CompletionElement *export_attachments = malloc(sizeof(CompletionElement));
  export_attachments->value = "attachments";
  export_attachments->description = "Export attachments";
  export_attachments->next = NULL;

  /* connect */
  group->elements = export_attachments;
  export_attachments->next = export_images;

  return completion;
}

Completion*
cc_open(char* input)
{
  /* init completion group */
  Completion *completion = malloc(sizeof(Completion));
  CompletionGroup* group = malloc(sizeof(CompletionGroup));

  group->value    = NULL;
  group->next     = NULL;
  group->elements = NULL;

  completion->groups = group;

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
  CompletionElement *last_element = NULL;
  char* name            = NULL;
  int   element_counter = 0;

  while((name = (char*) g_dir_read_name(dir)) != NULL)
  {
    char* d_name   = g_filename_display_name(name);
    int   d_length = strlen(d_name);

    if( ((file_length <= d_length) && !strncmp(file, d_name, file_length)) ||
        (file_length == 0) )
    {
      CompletionElement* el = malloc(sizeof(CompletionElement));
      el->value = g_strdup_printf("%s%s", path, d_name);
      el->description = NULL;
      el->next = NULL;

      if(element_counter++ != 0)
        last_element->next = el;
      else
        group->elements = el;

      last_element = el;
    }
  }

  g_dir_close(dir);

  return completion;
}

Completion*
cc_print(char* input)
{
  /* init completion group */
  Completion *completion = malloc(sizeof(Completion));
  CompletionGroup* group = malloc(sizeof(CompletionGroup));

  group->value    = NULL;
  group->next     = NULL;
  group->elements = NULL;

  completion->groups = group;
  CompletionElement *last_element = NULL;
  int element_counter = 0;
  int input_length = input ? strlen(input) : 0;

  /* read printers */
  char *current_line = NULL, current_char;
  int count = 0;
  FILE *fp;

  fp = popen(LIST_PRINTER_COMMAND, "r");

  if(!fp)
    return NULL;

  while((current_char = fgetc(fp)) != EOF)
  {
    if(!current_line)
      current_line = malloc(sizeof(char));

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
        CompletionElement* el = malloc(sizeof(CompletionElement));
        el->value       = g_strdup(current_line);
        el->description = NULL;
        el->next        = NULL;

        if(element_counter++ != 0)
          last_element->next = el;
        else
          group->elements = el;

        last_element = el;
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
  /* init completion group */
  Completion *completion = malloc(sizeof(Completion));
  CompletionGroup* group = malloc(sizeof(CompletionGroup));

  group->value    = NULL;
  group->next     = NULL;
  group->elements = NULL;

  completion->groups = group;
  CompletionElement *last_element = NULL;
  int element_counter = 0;
  int i = 0;
  int input_length = input ? strlen(input) : 0;

  for(i = 0; i < LENGTH(settings); i++)
  {
    if( (input_length <= strlen(settings[i].name)) &&
          !strncmp(input, settings[i].name, input_length) )
    {
      CompletionElement* el = malloc(sizeof(CompletionElement));
      el->value = settings[i].name;
      el->description = settings[i].description;
      el->next = NULL;

      if(element_counter++ != 0)
        last_element->next = el;
      else
        group->elements = el;

      last_element = el;
    }
  }

  return completion;
}

/* buffer command implementation */
void 
bcmd_goto(char* buffer, Argument* argument)
{
  int b_length = strlen(buffer);
  if(b_length < 1)
    return;

  if(!strcmp(buffer, "gg"))
    set_page(0);
  else if(!strcmp(buffer, "G"))
    set_page(Zathura.PDF.number_of_pages - 1);
  else
    set_page(atoi(g_strndup(buffer, b_length - 1)) - 1);

  update_status();
}

void
bcmd_zoom(char* buffer, Argument* argument)
{
  pthread_mutex_lock(&(Zathura.Lock.scale_lock));
  if(argument->n == ZOOM_IN)
  {
    if((Zathura.PDF.scale + ZOOM_STEP) <= ZOOM_MAX)
      Zathura.PDF.scale += ZOOM_STEP;
    else
      Zathura.PDF.scale = ZOOM_MAX;
  }
  else if(argument->n == ZOOM_OUT)
  {
    if((Zathura.PDF.scale - ZOOM_STEP) >= ZOOM_MIN)
      Zathura.PDF.scale -= ZOOM_STEP;
    else
      Zathura.PDF.scale = ZOOM_MIN;
  }
  else if(argument->n == ZOOM_SPECIFIC)
  {
    int b_length = strlen(buffer);
    if(b_length < 1)
      return;

    int value = atoi(g_strndup(buffer, b_length - 1));
    if(value <= ZOOM_MIN)
      Zathura.PDF.scale = ZOOM_MIN;
    else if(value >= ZOOM_MAX)
      Zathura.PDF.scale = ZOOM_MAX;
    else
      Zathura.PDF.scale = value;
  }
  else
    Zathura.PDF.scale = 100;
  pthread_mutex_unlock(&(Zathura.Lock.scale_lock));

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
    cmd_close(0, NULL);

  /* mutexes */
  pthread_mutex_destroy(&(Zathura.Lock.scale_lock));
  pthread_mutex_destroy(&(Zathura.Lock.rotate_lock));
  pthread_mutex_destroy(&(Zathura.Lock.search_lock));
  pthread_mutex_destroy(&(Zathura.Lock.viewport_lock));

  gtk_main_quit();

  return TRUE;
}

gboolean cb_draw(GtkWidget* widget, GdkEventExpose* expose, gpointer data)
{
  /*intptr_t t = (intptr_t) data;*/
  int page_id = Zathura.PDF.page_number; /* (int) t; */

  if(page_id < 0 || page_id > Zathura.PDF.number_of_pages)
    return FALSE;

  gdk_window_clear(widget->window);
  cairo_t *cairo = gdk_cairo_create(widget->window);

  double page_width, page_height, width, height;
  pthread_mutex_lock(&(Zathura.Lock.scale_lock));
  double scale = ((double) Zathura.PDF.scale / 100.0);
  pthread_mutex_unlock(&(Zathura.Lock.scale_lock));

  pthread_mutex_lock(&(Zathura.PDF.pages[page_id]->lock));
  poppler_page_get_size(Zathura.PDF.pages[page_id]->page, &page_width, &page_height);
  pthread_mutex_unlock(&(Zathura.PDF.pages[page_id]->lock));

  pthread_mutex_lock(&(Zathura.Lock.rotate_lock));
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
  pthread_mutex_unlock(&(Zathura.Lock.rotate_lock));

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


  cairo_set_source_surface(cairo, Zathura.PDF.surface, offset_x, offset_y);
  cairo_paint(cairo);
  cairo_destroy(cairo);

  return TRUE;
}

gboolean
cb_view_kb_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  int i;
  for(i = 0; i < LENGTH(shortcuts); i++)
  {
    if (event->keyval == shortcuts[i].key && 
      (((event->state & shortcuts[i].mask) == shortcuts[i].mask) || shortcuts[i].mask == 0)
      && (Zathura.Global.mode == shortcuts[i].mode || shortcuts[i].mode == -1))
    {
      shortcuts[i].function(&(shortcuts[i].argument));
      return TRUE;
    }
  }

  /* append only numbers and characters to buffer */
  if( (event->keyval >= 0x30) && (event->keyval <= 0x7A))
  {
    if(!Zathura.Global.buffer)
      Zathura.Global.buffer = g_string_new("");

    Zathura.Global.buffer = g_string_append_c(Zathura.Global.buffer, event->keyval);
    gtk_label_set_markup((GtkLabel*) Zathura.Global.status_buffer, Zathura.Global.buffer->str);
  }

  /* search buffer commands */
  if(Zathura.Global.buffer)
  {
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
cb_index_selection_changed(GtkTreeSelection* treeselection, GtkWidget* action_view)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;

  if(gtk_tree_selection_get_selected(treeselection, &model, &iter))
  {
    PopplerAction* action;
    PopplerDest*   destination;

    gtk_tree_model_get(model, &iter, 1, &action, -1);

    if(action->type == POPPLER_ACTION_GOTO_DEST)
    {
      destination = action->goto_dest.dest;

      if(destination->type == POPPLER_DEST_NAMED)
      {
        destination = poppler_document_find_dest(Zathura.PDF.document, destination->named_dest);

        if(destination)
        {
          set_page(destination->page_num - 1);
          update_status();
        }
      }
    }
  }

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

/* main function */
int main(int argc, char* argv[])
{
  gtk_init(&argc, &argv);

  init_zathura();
  update_status();

  if(argc >= 2)
    cmd_open(2, &argv[1]);

  gtk_widget_show_all(GTK_WIDGET(Zathura.UI.window));

  Argument arg;
  arg.n = ADJUST_OPEN;
  sc_adjust_window(&arg);

  gtk_main();

  return 0;
}
