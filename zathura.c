#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <poppler/glib/poppler.h>
#include <cairo.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

/* macros */
#define LENGTH(x) sizeof(x)/sizeof((x)[0])

/* enums */
enum { UP, DOWN, LEFT, RIGHT, ZOOM_IN, ZOOM_OUT, ZOOM_ORIGINAL, 
       NEXT, PREVIOUS, HIDE, ERROR, WARNING, DEFAULT, TOP, BOTTOM, 
       ADJUST_BESTFIT, ADJUST_WIDTH, FORWARD, BACKWARD };

struct
{
  GtkWindow         *window;
  GtkBox            *box;
  GtkScrolledWindow *view;
  GtkWidget         *drawing_area;
  GtkBox            *notification;
  GtkEntry          *inputbar;

  struct
  {
    GdkColor              default_fg;
    GdkColor              default_bg;
    GdkColor              inputbar_fg;
    GdkColor              inputbar_bg;
    GdkColor              notification_fg;
    GdkColor              notification_bg;
    GdkColor              notification_wn_fg;
    GdkColor              notification_wn_bg;
    GdkColor              notification_er_fg;
    GdkColor              notification_er_bg;
    GdkColor              completion_fg;
    GdkColor              completion_bg;
    GdkColor              completion_hl_fg;
    GdkColor              completion_hl_bg;
    GdkColor              search_highlight;
    PangoFontDescription *font;
  } Settings;

  struct
  {
    PopplerDocument *document;
    PopplerPage     *page;
    char            *file;
    cairo_surface_t *surface;
    int              page_number;
    int              number_of_pages;
    double           scale;
    int              rotate;
  } PDF;

} Zathura;

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
  Argument argument;
} Shortcut;

typedef struct
{
  char *command;
  void (*function)(int, char**);
} Command;

/* function declarations */
void init();
void update_title();
void update_status();
void set_page(int);
void draw();
void highlight_result(PopplerRectangle*);
void open_link(char*);
void save_images(int, char*);
void save_attachments(char*);

gboolean complete(Argument*);
gboolean delete_widget(gpointer);

GtkWidget* notify(int, char*);
GtkWidget* update_notification(GtkWidget*, int, char*);

PopplerRectangle* recalcRectangle(PopplerRectangle*);

/* shortcut declarations */
void sc_focus_inputbar(Argument*);
void sc_scroll(Argument*);
void sc_navigate(Argument*);
void sc_zoom(Argument*);
void sc_adjust_window(Argument*);
void sc_rotate(Argument*);
void sc_search(Argument*);
void sc_quit(Argument*);

/* command declarations */
void cmd_export(int, char**);
void cmd_form(int, char**);
void cmd_goto(int, char**);
void cmd_info(int, char**);
void cmd_links(int, char**);
void cmd_open(int, char**);
void cmd_print(int, char**);
void cmd_rotate(int, char**);
void cmd_save(int, char**);
void cmd_quit(int, char**);
void cmd_zoom(int, char**);

/* callbacks declarations */
void cb_draw(GtkWidget*, gpointer);
void cb_destroy(GtkWidget*, gpointer);
void cb_inputbar_activate(GtkEntry*, gpointer);
void cb_inputbar_button_pressed(GtkWidget*, GdkEventButton*, gpointer);
void cb_drawing_area_button_pressed(GtkWidget*, GdkEventButton*, gpointer);

gboolean cb_label_open_link(GtkLabel*, gchar*, gpointer);
gboolean cb_inputbar_key_pressed(GtkEntry*,  GdkEventKey*, gpointer);
gboolean cb_inputbar_key_released(GtkEntry*, GdkEventKey*, gpointer);
gboolean cb_view_key_pressed(GtkWidget*,     GdkEventKey*, gpointer);

/* configuration */
#include "config.h"

/* function implementations */ 
void
init()
{
  /* settings */
  gdk_color_parse(default_fgcolor,              &(Zathura.Settings.default_fg));
  gdk_color_parse(default_bgcolor,              &(Zathura.Settings.default_bg));
  gdk_color_parse(inputbar_fgcolor,             &(Zathura.Settings.inputbar_fg));
  gdk_color_parse(inputbar_bgcolor,             &(Zathura.Settings.inputbar_bg));
  gdk_color_parse(notification_fgcolor,         &(Zathura.Settings.notification_fg));
  gdk_color_parse(notification_bgcolor,         &(Zathura.Settings.notification_bg));
  gdk_color_parse(notification_warning_fgcolor, &(Zathura.Settings.notification_wn_fg));
  gdk_color_parse(notification_warning_bgcolor, &(Zathura.Settings.notification_wn_bg));
  gdk_color_parse(notification_error_fgcolor,   &(Zathura.Settings.notification_er_fg));
  gdk_color_parse(notification_error_bgcolor,   &(Zathura.Settings.notification_er_bg));
  gdk_color_parse(completion_fgcolor,           &(Zathura.Settings.completion_fg));
  gdk_color_parse(completion_bgcolor,           &(Zathura.Settings.completion_bg));
  gdk_color_parse(completion_hl_fgcolor,        &(Zathura.Settings.completion_hl_fg));
  gdk_color_parse(completion_hl_bgcolor,        &(Zathura.Settings.completion_hl_bg));
  gdk_color_parse(search_highlight,             &(Zathura.Settings.search_highlight));
  Zathura.Settings.font = pango_font_description_from_string(font);

  /* variables */
  Zathura.window       = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  Zathura.box          = GTK_BOX(gtk_vbox_new(FALSE, 0));
  Zathura.view         = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
  Zathura.drawing_area = gtk_drawing_area_new();
  Zathura.notification = GTK_BOX(gtk_vbox_new(FALSE, 0));
  Zathura.inputbar     = GTK_ENTRY(gtk_entry_new());
  Zathura.PDF.scale    = 1.0;
  Zathura.PDF.rotate   = 0;

  /* window */
  gtk_window_set_title(Zathura.window, "zathura");
  gtk_window_set_default_size(Zathura.window, DEFAULT_WIDTH, DEFAULT_HEIGHT);
  g_signal_connect(G_OBJECT(Zathura.window), "destroy", G_CALLBACK(cb_destroy), NULL);

  /* box */
  gtk_box_set_spacing(Zathura.box, 0);
  gtk_container_add(GTK_CONTAINER(Zathura.window), GTK_WIDGET(Zathura.box));

  /* view */
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(Zathura.view), 
    Zathura.drawing_area);
  gtk_viewport_set_shadow_type((GtkViewport*) gtk_bin_get_child(GTK_BIN(Zathura.view)), GTK_SHADOW_NONE);
  g_signal_connect(G_OBJECT(Zathura.view), "key-press-event", G_CALLBACK(cb_view_key_pressed), NULL);
  
  #ifdef SHOW_SCROLLBARS
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.view), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  #else
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.view), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  #endif

  /* drawing area */
  g_signal_connect(G_OBJECT(Zathura.drawing_area), "expose-event",       G_CALLBACK(cb_draw), NULL);
  g_signal_connect(G_OBJECT(Zathura.drawing_area), "button-press-event", G_CALLBACK(cb_drawing_area_button_pressed), NULL);
  gtk_widget_set_events(Zathura.drawing_area, GDK_BUTTON_PRESS_MASK);
  gtk_widget_modify_bg(GTK_WIDGET(Zathura.drawing_area), GTK_STATE_NORMAL, &(Zathura.Settings.default_bg));

  /* inputbar */
  gtk_entry_set_inner_border(Zathura.inputbar, NULL);
  gtk_entry_set_has_frame(Zathura.inputbar, FALSE);
  gtk_entry_set_editable(Zathura.inputbar,TRUE);
  g_signal_connect(G_OBJECT(Zathura.inputbar), "activate",           G_CALLBACK(cb_inputbar_activate),       NULL);
  g_signal_connect(G_OBJECT(Zathura.inputbar), "button-press-event", G_CALLBACK(cb_inputbar_button_pressed), NULL);
  g_signal_connect(G_OBJECT(Zathura.inputbar), "key-press-event",    G_CALLBACK(cb_inputbar_key_pressed),    NULL);
  g_signal_connect(G_OBJECT(Zathura.inputbar), "key-release-event",  G_CALLBACK(cb_inputbar_key_released),   NULL);

  /* packing */
  gtk_box_pack_start(Zathura.box, GTK_WIDGET(Zathura.view),         TRUE,  TRUE,  0);
  gtk_box_pack_start(Zathura.box, GTK_WIDGET(Zathura.notification), FALSE, FALSE, 0);
  gtk_box_pack_end(Zathura.box,   GTK_WIDGET(Zathura.inputbar),     FALSE, FALSE, 0);

  /* visuals */
  gtk_widget_modify_base(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_bg));
  gtk_widget_modify_text(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_fg));
  gtk_widget_modify_font(GTK_WIDGET(Zathura.inputbar), Zathura.Settings.font);
}

void
update_title()
{
  if(Zathura.PDF.file != NULL)
    gtk_window_set_title(GTK_WINDOW(Zathura.window), Zathura.PDF.file);
  else
    gtk_window_set_title(GTK_WINDOW(Zathura.window), "zathura");
}

GtkWidget*
notify(int level, char* text)
{
  GtkEventBox *view = GTK_EVENT_BOX(gtk_event_box_new());
 
  update_notification(GTK_WIDGET(view), level, text);

  gtk_container_add(GTK_CONTAINER(Zathura.notification), GTK_WIDGET(view));
  
  gtk_widget_show_all(GTK_WIDGET(Zathura.window));
  g_timeout_add_seconds(SHOW_NOTIFICATION, delete_widget, GTK_WIDGET(GTK_WIDGET(view)));

  return GTK_WIDGET(view);
}

GtkWidget*
update_notification(GtkWidget* view, int level, char* text)
{
  GtkLabel *message = GTK_LABEL(gtk_label_new(""));
  gtk_label_set_markup(message, text);
  g_signal_connect(G_OBJECT(message), "activate-link", G_CALLBACK(cb_label_open_link), NULL);

  gtk_misc_set_alignment(GTK_MISC(message), 0, 0);
  gtk_widget_modify_font(GTK_WIDGET(message), Zathura.Settings.font);

  if(level == WARNING)
  {
    gtk_widget_modify_fg(GTK_WIDGET(message), GTK_STATE_NORMAL, &(Zathura.Settings.notification_wn_fg));
    gtk_widget_modify_bg(GTK_WIDGET(view),    GTK_STATE_NORMAL, &(Zathura.Settings.notification_wn_bg));
  }
  else if(level == ERROR)
  {
    gtk_widget_modify_fg(GTK_WIDGET(message), GTK_STATE_NORMAL, &(Zathura.Settings.notification_er_fg));
    gtk_widget_modify_bg(GTK_WIDGET(view),    GTK_STATE_NORMAL, &(Zathura.Settings.notification_er_bg));
  }
  else
  {
    gtk_widget_modify_fg(GTK_WIDGET(message), GTK_STATE_NORMAL, &(Zathura.Settings.notification_fg));
    gtk_widget_modify_bg(GTK_WIDGET(view),    GTK_STATE_NORMAL, &(Zathura.Settings.notification_bg));
  }

  if(gtk_bin_get_child(GTK_BIN(view)))
    gtk_container_remove(GTK_CONTAINER(view), gtk_bin_get_child(GTK_BIN(view)));

  gtk_container_add(GTK_CONTAINER(view), GTK_WIDGET(message));

  return GTK_WIDGET(view);
}

PopplerRectangle*
recalcRectangle(PopplerRectangle* rectangle)
{
  double page_width, page_height;
  double x1 = rectangle->x1;
  double x2 = rectangle->x2;
  double y1 = rectangle->y1;
  double y2 = rectangle->y2;

  poppler_page_get_size(Zathura.PDF.page, &page_width, &page_height);
  
  switch(Zathura.PDF.rotate)
  {
    case 90:
      rectangle->x1 = (page_height - y2) * Zathura.PDF.scale;
      rectangle->y1 = x1 * Zathura.PDF.scale;
      rectangle->x2 = (page_height - y1) * Zathura.PDF.scale;
      rectangle->y2 = x2 * Zathura.PDF.scale;
      break;
    case 180:
      rectangle->x1 = (page_width  - x2) * Zathura.PDF.scale;
      rectangle->y1 = (page_height - y2) * Zathura.PDF.scale;
      rectangle->x2 = (page_width  - x1) * Zathura.PDF.scale;
      rectangle->y2 = (page_height - y1) * Zathura.PDF.scale;
      break;
    case 270:
      rectangle->x1 = y1 * Zathura.PDF.scale;
      rectangle->y1 = (page_width  - x2) * Zathura.PDF.scale;
      rectangle->x2 = y2 * Zathura.PDF.scale;
      rectangle->y2 = (page_width  - x1) * Zathura.PDF.scale;
      break;
    default:
      rectangle->x1 = x1 * Zathura.PDF.scale;
      rectangle->y1 = y1 * Zathura.PDF.scale;
      rectangle->x2 = x2 * Zathura.PDF.scale;
      rectangle->y2 = y2 * Zathura.PDF.scale;
  }

  return rectangle;
}

void
update_status()
{
  char* text = "";

  if(Zathura.PDF.document && Zathura.PDF.page)
    text = g_strdup_printf("[%i/%i] %s", Zathura.PDF.page_number + 1, Zathura.PDF.number_of_pages, Zathura.PDF.file);

  gtk_entry_set_text(Zathura.inputbar, text);
}

void set_page(int page_number)
{
  if(page_number > Zathura.PDF.number_of_pages) 
  {
    notify(WARNING, "Could not open page");
    return;
  }
  
  Zathura.PDF.page_number = page_number;
  Zathura.PDF.page = poppler_document_get_page(Zathura.PDF.document, page_number);
}

void
draw()
{
  if(!Zathura.PDF.document || !Zathura.PDF.page)
    return;

  double page_width, page_height;
  double width, height;

  if(Zathura.PDF.surface)
    cairo_surface_destroy(Zathura.PDF.surface);
  Zathura.PDF.surface = NULL;

  poppler_page_get_size(Zathura.PDF.page, &page_width, &page_height);
 
  if(Zathura.PDF.rotate == 0 || Zathura.PDF.rotate == 180)
  {
    width  = page_width  * Zathura.PDF.scale;
    height = page_height * Zathura.PDF.scale;
  }
  else
  {
    width  = page_height * Zathura.PDF.scale;
    height = page_width  * Zathura.PDF.scale;
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

  switch(Zathura.PDF.rotate)
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

  if(Zathura.PDF.scale != 1.0)
    cairo_scale(cairo, Zathura.PDF.scale, Zathura.PDF.scale);

  if(Zathura.PDF.rotate != 0)
    cairo_rotate(cairo, Zathura.PDF.rotate * G_PI / 180.0);

  poppler_page_render(Zathura.PDF.page, cairo);
  cairo_restore(cairo);
  cairo_destroy(cairo);

  gtk_widget_set_size_request(Zathura.drawing_area, width, height);
  gtk_widget_queue_draw(Zathura.drawing_area);
}

void 
highlight_result(PopplerRectangle* rectangle)
{
  cairo_t *cairo = cairo_create(Zathura.PDF.surface);
  cairo_set_source_rgba(cairo, Zathura.Settings.search_highlight.red, Zathura.Settings.search_highlight.green, 
      Zathura.Settings.search_highlight.blue, HL_TRANSPARENCY);

  rectangle = recalcRectangle(rectangle);
 
  cairo_rectangle(cairo, rectangle->x1, rectangle->y1, (rectangle->x2 - rectangle->x1), (rectangle->y2 - rectangle->y1));
  cairo_fill(cairo);
}

void
open_link(char* link)
{
  char* start_browser = g_strdup_printf(BROWSER, link);
  system(start_browser);
}

void
save_images(int page, char* directory)
{
  GList           *image_list;
  GList           *images;
  cairo_surface_t *image;
  PopplerPage     *document_page;

  document_page = poppler_document_get_page(Zathura.PDF.document, page);
  image_list = poppler_page_get_image_mapping(document_page);

  for(images = image_list; images; images = g_list_next(images))
  {
    PopplerImageMapping *image_mapping;
    PopplerRectangle     image_field;
    gint                 image_id;
    char*                file;

    image_mapping = (PopplerImageMapping*) images->data;
    image_field   = image_mapping->area;
    image_id      = image_mapping->image_id;
 
    image = poppler_page_get_image(document_page, image_id);
    file  = g_strdup_printf("%sp%i_i%i.png", directory, page + 1, image_id);

    cairo_surface_write_to_png(image, file);

    g_free(file);
  }
}

void
save_attachments(char* directory)
{
  GList *attachment_list;
  GList *attachments;
  char  *file;

  attachment_list = poppler_document_get_attachments(Zathura.PDF.document);

  for(attachments = attachment_list; attachments; attachments = g_list_next(attachments))
  {
    PopplerAttachment *attachment = (PopplerAttachment*) attachments->data;
    file = g_strdup_printf("%s%s", directory, attachment->name);
    
    poppler_attachment_save(attachment, file, NULL);

    g_free(file);
  }
}

gboolean
delete_widget(gpointer data)
{
  gtk_widget_destroy(GTK_WIDGET(data));

  return FALSE;
}

gboolean
complete(Argument* argument)
{
  static GtkBox     *results;
  static GtkWidget **result_rows;
  static char      **command_names;
  static int         n = 0;
  static int         current = -1; 

  char *command = (char*) gtk_entry_get_text(Zathura.inputbar);
  int   command_length  = strlen(command);
  int   length = 0;
  int   i = 0;

  if( (command_length == 0 || command[0] != ':') && argument->n != HIDE )
    return TRUE;

  if(argument->n != HIDE && n > 0 && results && current != -1 && !strcmp(&command[1], command_names[current]) )
  {
    GtkEventBox *current_box   = (GtkEventBox*) g_list_nth_data(gtk_container_get_children(GTK_CONTAINER(results)),     current);
    GtkLabel    *current_label = (GtkLabel*)    g_list_nth_data(gtk_container_get_children(GTK_CONTAINER(current_box)), 0);
   
    gtk_widget_modify_fg(GTK_WIDGET(current_label), GTK_STATE_NORMAL, &(Zathura.Settings.completion_fg));
    gtk_widget_modify_bg(GTK_WIDGET(current_box),   GTK_STATE_NORMAL, &(Zathura.Settings.completion_bg));

    if(argument->n == NEXT)
      current = (current + n + 1) % n;
    else if (argument->n == PREVIOUS)
      current = (current + n - 1) % n;

    current_box   = (GtkEventBox*) g_list_nth_data(gtk_container_get_children(GTK_CONTAINER(results)),     current);
    current_label = (GtkLabel*)    g_list_nth_data(gtk_container_get_children(GTK_CONTAINER(current_box)), 0);

    gtk_widget_modify_fg(GTK_WIDGET(current_label), GTK_STATE_NORMAL, &(Zathura.Settings.completion_hl_fg));
    gtk_widget_modify_bg(GTK_WIDGET(current_box),   GTK_STATE_NORMAL, &(Zathura.Settings.completion_hl_bg));

    char* temp_string = g_strconcat(":", command_names[current], NULL);
    gtk_entry_set_text(GTK_ENTRY(Zathura.inputbar), temp_string);
    gtk_editable_set_position(GTK_EDITABLE(Zathura.inputbar), -1);
    g_free(temp_string);
  }
  else
  {
    if(results != NULL)
      gtk_widget_destroy(GTK_WIDGET(results));
 
    free(result_rows);
    free(command_names);

    n             = 0;
    current       = -1;
    results       = NULL;
    result_rows   = NULL;
    command_names = NULL;

    if(argument->n == HIDE)
      return TRUE;
  }

  if(!results)
  {
    results       = GTK_BOX(gtk_vbox_new(FALSE, 0));
    result_rows   = malloc(LENGTH(commands) * sizeof(GtkWidget*));
    command_names = malloc(LENGTH(commands) * sizeof(char*));

    for(i = 0; i < LENGTH(commands); i++)
    {
      length = strlen(commands[i].command);
      
      if( (command_length - 1 <= length) && !strncmp(&command[1], commands[i].command, command_length - 1) )
      {
        GtkLabel *show_command = GTK_LABEL(gtk_label_new(commands[i].command));
        GtkEventBox *event_box = GTK_EVENT_BOX(gtk_event_box_new());

        gtk_misc_set_alignment(GTK_MISC(show_command), 0, 0);
        gtk_widget_modify_fg(  GTK_WIDGET(show_command), GTK_STATE_NORMAL, &(Zathura.Settings.completion_fg));
        gtk_widget_modify_bg(  GTK_WIDGET(event_box),    GTK_STATE_NORMAL, &(Zathura.Settings.completion_bg));
        gtk_widget_modify_font(GTK_WIDGET(show_command), Zathura.Settings.font);

        gtk_container_add(GTK_CONTAINER(event_box), GTK_WIDGET(show_command));
        gtk_box_pack_start(results, GTK_WIDGET(event_box), FALSE, FALSE, 0);
        
        command_names[n] = commands[i].command;
        result_rows[n++] = GTK_WIDGET(event_box);
      }
    }

    command_names = realloc(command_names, n * sizeof(char*));
    result_rows   = realloc(result_rows,   n * sizeof(GtkWidget*));
    
    gtk_box_pack_start(Zathura.box, GTK_WIDGET(results), FALSE, FALSE, 0);
    gtk_widget_show_all(GTK_WIDGET(Zathura.window));

    current = (argument->n == NEXT) ? 0 : (n - 1);
  }

  if(current != -1 && n > 0)
  {
    GtkEventBox *current_box   = (GtkEventBox*) g_list_nth_data(gtk_container_get_children(GTK_CONTAINER(results)),     current);
    GtkLabel    *current_label = (GtkLabel*)    g_list_nth_data(gtk_container_get_children(GTK_CONTAINER(current_box)), 0);
   
    gtk_widget_modify_fg(GTK_WIDGET(current_label), GTK_STATE_NORMAL, &(Zathura.Settings.completion_hl_fg));
    gtk_widget_modify_bg(GTK_WIDGET(current_box),   GTK_STATE_NORMAL, &(Zathura.Settings.completion_hl_bg));

    char* temp_string = g_strconcat(":", command_names[current], NULL);
    gtk_entry_set_text(GTK_ENTRY(Zathura.inputbar), temp_string);
    gtk_editable_set_position(GTK_EDITABLE(Zathura.inputbar), -1);
    g_free(temp_string); 
  }

  return TRUE;
}

/* shortcut implementations */
void
sc_focus_inputbar(Argument *argument)
{
  gtk_entry_set_text(Zathura.inputbar, argument->data);
  gtk_widget_modify_text(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_fg));
  gtk_widget_modify_base(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_bg));
  
  gtk_widget_grab_focus(GTK_WIDGET(Zathura.inputbar));
  gtk_editable_set_position(GTK_EDITABLE(Zathura.inputbar), -1);
}

void
sc_scroll(Argument *argument)
{
  GtkAdjustment* adjustment;

  if( (argument->n == LEFT) || (argument->n == RIGHT) )
    adjustment = gtk_scrolled_window_get_hadjustment(Zathura.view);
  else
    adjustment = gtk_scrolled_window_get_vadjustment(Zathura.view);

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
sc_navigate(Argument *argument)
{
  int number_of_pages = poppler_document_get_n_pages(Zathura.PDF.document);
  int new_page = Zathura.PDF.page_number;

  if(argument->n == NEXT)
    new_page = abs( (new_page + number_of_pages + 1) % number_of_pages);
  else if(argument->n == PREVIOUS)
    new_page = abs( (new_page + number_of_pages - 1) % number_of_pages);

  set_page(new_page);

  Argument reset;
  reset.n = TOP;
  sc_scroll(&reset);
  
  draw();

  update_status();
}

void
sc_zoom(Argument *argument)
{
  if(argument->n == ZOOM_IN)
    Zathura.PDF.scale += ZOOM_STEP;
  else if(argument->n == ZOOM_OUT)
    Zathura.PDF.scale -= ZOOM_STEP;
  else if(argument->n == ZOOM_ORIGINAL)
    Zathura.PDF.scale = 1.0;

  draw();
}

void
sc_adjust_window(Argument *argument)
{
  GtkAdjustment* adjustment;
  double view_size;
  double page_width;
  double page_height;

  if(argument->n == ADJUST_WIDTH)
    adjustment = gtk_scrolled_window_get_vadjustment(Zathura.view);
  else
    adjustment = gtk_scrolled_window_get_hadjustment(Zathura.view);

  view_size  = gtk_adjustment_get_page_size(adjustment);
  poppler_page_get_size(Zathura.PDF.page, &page_width, &page_height);

  if(argument->n == ADJUST_WIDTH)
    Zathura.PDF.scale = view_size / page_height;
  else
    Zathura.PDF.scale = view_size / page_width;
  
  draw();
}

void
sc_rotate(Argument *argument)
{
  if(argument->n == LEFT)
    Zathura.PDF.rotate = abs((Zathura.PDF.rotate - 90) % 360);
  else if(argument->n == RIGHT)
    Zathura.PDF.rotate = abs((Zathura.PDF.rotate + 90) % 360);

  draw();
}

void
sc_search(Argument *argument)
{
  static char* search_item;
  static int direction;
  int page_counter;
  GList* results;
  GList* list;

  if(argument->data)
    search_item = (char*) argument->data;

  if(!Zathura.PDF.document || !Zathura.PDF.page || !search_item)
    return;

  /* search document */
  GtkWidget* search_status = notify(DEFAULT, "Searching...");
  
  if(argument->n)
    direction = (argument->n == BACKWARD) ? -1 : 1;

  for(page_counter = 0; page_counter < Zathura.PDF.number_of_pages; page_counter++)
  {
    int next_page = (Zathura.PDF.page_number + page_counter * direction) % Zathura.PDF.number_of_pages;
    set_page(next_page);
    
    results = poppler_page_find_text(Zathura.PDF.page, search_item);
    if(results)
      break;
  }

  /* draw results */
  if(results)
  {
    double page_width, page_height;
    poppler_page_get_size(Zathura.PDF.page, &page_width, &page_height);
    draw();
    
    update_notification(GTK_WIDGET(search_status), DEFAULT, g_strdup_printf("%s found on page %i for %i time(s)", 
      search_item, Zathura.PDF.page_number, g_list_length(results)));
    
    for(list = results; list && list->data; list = g_list_next(list))
    {
      PopplerRectangle* result = (PopplerRectangle*) list->data;
      result->y1 = page_height - result->y1;
      result->y2 = page_height - result->y2;
      highlight_result(result);
    }
  }
  else
    update_notification(search_status, DEFAULT, g_strdup_printf("No match for %s", search_item));
}

void
sc_quit(Argument *argument)
{
  cb_destroy(NULL, NULL);
}

/* command implementations */
void
cmd_open(int argc, char** argv)
{
  if(argc == 0 || (int) strlen(argv[0]) == 0)
    return;

  char* file = realpath(argv[0], NULL);

  if(!g_file_test(file, G_FILE_TEST_IS_REGULAR))
  {
    notify(ERROR, "File does not exist");
    return;
  }

  Zathura.PDF.document = poppler_document_new_from_file(g_strdup_printf("file://%s", file), 
      (argc == 2) ? argv[1] : NULL, NULL);
  
  if(!Zathura.PDF.document)
  {
    notify(WARNING, "Can not open file");
    return;
  }

  Zathura.PDF.number_of_pages = poppler_document_get_n_pages(Zathura.PDF.document);
  Zathura.PDF.file = file;
  
  set_page(0);
  draw();  
 
  update_status();
  update_title();
}

void
cmd_print(int argc, char** argv)
{
  if(argc == 0 || !Zathura.PDF.document)
    return;

  char* print_command;
  char* sites;

  if(strcmp(argv[0], "all") == 0)
    sites = g_strdup_printf("%i", Zathura.PDF.number_of_pages);
  else
    sites = argv[0];

  print_command = g_strdup_printf("lp -d '%s' -P %s %s", PRINTER, sites, Zathura.PDF.file);

  system(print_command);
}

void
cmd_export(int argc, char** argv)
{
  if(argc == 0 || !Zathura.PDF.document)
    return;
 
  if(argc < 2)
  {
    notify(WARNING, "No export path specified");
    return;
  }
  
  /* export images */
  if(strcmp(argv[0], "images") == 0)
  {
    int page_counter;
   
    if(argc == 3)
    {
      int page_number = atoi(argv[2]) - 1;

      if(page_number < 0 || page_number > Zathura.PDF.number_of_pages)
        notify(WARNING, "Page does not exist");
      else
        save_images(page_number, argv[1]);

      return;
    }
    
    for(page_counter = 0; page_counter < Zathura.PDF.number_of_pages; page_counter++)
    {
      save_images(page_counter, argv[1]);
    }
  }

  /* export attachments */
  else if(strcmp(argv[0], "attachments") == 0)
  {
    if(!poppler_document_has_attachments(Zathura.PDF.document))
      notify(WARNING, "PDF file has no attachments");
    else
      save_attachments(argv[1]);
  }
  else
  {
    notify(DEFAULT, "export [images|attachments]");
    return;
  }
}

void
cmd_form(int argc, char** argv)
{
  if(argc == 0 || !Zathura.PDF.document || !Zathura.PDF.page)
    return;

  if(strcmp(argv[0], "show") == 0)
  {
    double page_width, page_height;
    int form_id = 0;
    GList* form_mapping = g_list_reverse(poppler_page_get_form_field_mapping(Zathura.PDF.page));
    GList* forms;

    poppler_page_get_size(Zathura.PDF.page, &page_width, &page_height);

    for(forms = form_mapping; forms; forms = g_list_next(forms))
    {
      /* draw rectangle */
      PopplerFormFieldMapping *form = (PopplerFormFieldMapping*) forms->data;
      PopplerRectangle *form_rectangle = &form->area;

      if(!poppler_form_field_is_read_only(form->field))
      {
        form_rectangle->y1 = page_height - form_rectangle->y1;
        form_rectangle->y2 = page_height - form_rectangle->y2;

        highlight_result(form_rectangle);

        /* draw text */
        cairo_t *cairo = cairo_create(Zathura.PDF.surface);
        cairo_select_font_face(cairo, font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cairo, 10);
        cairo_move_to(cairo, form_rectangle->x1 + 1, form_rectangle->y1 - 1);
        cairo_show_text(cairo, g_strdup_printf("%i", form_id));
      }
  
      form_id++;
    }
    
    gtk_widget_queue_draw(Zathura.drawing_area);
    poppler_page_free_form_field_mapping(form_mapping);
  }
  else if(strcmp(argv[0], "set") == 0)
  {
    if(argc <= 2)
      return;

    GList* form_mapping    = g_list_reverse(poppler_page_get_form_field_mapping(Zathura.PDF.page));
    int    form_id         = atoi(argv[1]);
    
    PopplerFormFieldMapping *form = (PopplerFormFieldMapping*) g_list_nth_data(form_mapping, form_id);
    PopplerFormFieldType type     = poppler_form_field_get_field_type(form->field);

    if(type == POPPLER_FORM_FIELD_TEXT)
    {
      char *text = "";
      int i;

      for(i = 2; i < argc; i++)
        text = g_strdup_printf("%s %s", text, argv[i]);
      
      poppler_form_field_text_set_text(form->field, text);
    }
    else if(type == POPPLER_FORM_FIELD_BUTTON)
    {
      if(argv[2][0] == '0')
        poppler_form_field_button_set_state(form->field, FALSE);
      else
        poppler_form_field_button_set_state(form->field, TRUE);
    }
  
   draw();
  }
}

void
cmd_goto(int argc, char** argv)
{
  if(argc == 0)
    return;

  int page = atoi(argv[0]);;
  set_page(page - 1);
  
  Argument argument;
  argument.n = TOP;
  sc_scroll(&argument);
  
  draw();

  update_status();
}

void
cmd_info(int argc, char** argv)
{
  if(!Zathura.PDF.document)
    return;

  gchar *info;
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

  info = g_strconcat(
      "<b>Title:</b> ",    title    ? title    : "", "\n", 
      "<b>Author:</b> ",   author   ? author   : "", "\n",
      "<b>Subject:</b> ",  subject  ? subject  : "", "\n",
      "<b>Keywords:</b> ", keywords ? keywords : "", "\n",
      "<b>Creator:</b> ",  creator  ? creator  : "", "\n",
      "<b>Producer:</b> ", producer ? producer : "",
      NULL);

  notify(DEFAULT, info);
}

void
cmd_links(int argc, char** argv)
{
  if(!Zathura.PDF.document && !Zathura.PDF.page)
    return;

  GList *link_list;
  GList *links;
  int    number_of_links;

  link_list       = poppler_page_get_link_mapping(Zathura.PDF.page);
  number_of_links = g_list_length(link_list);

  if(number_of_links > 0)
    notify(DEFAULT, g_strdup_printf("%d links found", number_of_links));
  else
    notify(WARNING, "No links found");

  for(links = link_list; links; links = g_list_next(links))
  {
    PopplerLinkMapping *link_mapping;
    PopplerAction      *action;
    
    link_mapping = (PopplerLinkMapping*) links->data;
    action       = poppler_action_copy(link_mapping->action);
  
    if(action->type == POPPLER_ACTION_URI)
    {
      char* link_name = poppler_page_get_text(Zathura.PDF.page, POPPLER_SELECTION_WORD, &link_mapping->area);
      link_name       = g_strdup_printf("<a href=\"%s\">%s</a>", action->uri.uri, link_name);
     
      notify(DEFAULT, g_strdup_printf("<b>%s:</b> %s", link_name, action->uri.uri));
    }
  }

  poppler_page_free_link_mapping(link_list);
}

void
cmd_rotate(int argc, char** argv)
{
  Argument argument;
  
  if(argc == 0)
    argument.n = RIGHT;
  else 
  {
    if(!strncmp(argv[0], "left", strlen(argv[0]) - 1))
      argument.n = LEFT;
    else
      argument.n = RIGHT;
  }

  sc_rotate(&argument);
}

void
cmd_save(int argc, char** argv)
{
  if(argc == 0 || !Zathura.PDF.document)
    return;

  poppler_document_save(Zathura.PDF.document, g_strdup_printf("file://%s", argv[0]), NULL);
}

void
cmd_zoom(int argc, char** argv)
{
  if(argc == 0)
    return;

  int zoom = atoi(argv[0]);
  if(zoom < 1 || zoom >= 1000)
    return;

  Zathura.PDF.scale = (float) zoom / 100;
  draw();
}

void
cmd_quit(int argc, char** argv)
{
  cb_destroy(NULL, NULL);
}

/* callback implementations */
void
cb_draw(GtkWidget *widget, gpointer data)
{
  gdk_window_clear(widget->window);

  cairo_t *cairo = gdk_cairo_create(widget->window);
  cairo_set_source_surface(cairo, Zathura.PDF.surface, 0, 0);
  cairo_paint(cairo);
  cairo_destroy(cairo);
}

void
cb_destroy(GtkWidget *widget, gpointer data)
{
  pango_font_description_free(Zathura.Settings.font);
  gtk_main_quit();
}

gboolean
cb_view_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  int i;
  for(i = 0; i < LENGTH(shortcuts); i++)
  {
    if (event->keyval == shortcuts[i].key && 
      (((event->state & shortcuts[i].mask) == shortcuts[i].mask) || shortcuts[i].mask == 0))
    {
      shortcuts[i].function(&(shortcuts[i].argument));
      return TRUE;
    }
  }
  return FALSE;
}

void
cb_inputbar_activate(GtkEntry* entry, gpointer data)
{
  gchar **tokens   = g_strsplit(gtk_entry_get_text(entry), " ", gtk_entry_get_text_length(entry));
  gchar  *command  = tokens[0] + 1;
  int     length   = g_strv_length(tokens);
  int          i   = 0;
  gboolean success = FALSE;
  Argument argument;

  argument.n = HIDE;
  complete(&argument);

  if(length < 1)
    return;

  // special command
  if(*tokens[0] == '/')
  {
    Argument argument;
    argument.data = (char*) gtk_entry_get_text(entry) + 1;
    sc_search(&argument);
    g_strfreev(tokens);
    update_status();
    gtk_widget_grab_focus(GTK_WIDGET(Zathura.view));
    return;
  }

  // other
  for(i = 0; i < LENGTH(commands); i++)
  {
    if(g_strcmp0(command, commands[i].command) == 0)
    {
      commands[i].function(length - 1, tokens + 1);
      success = TRUE;
      break;
    }
  }

  if(!success)
    notify(ERROR, g_strdup_printf("\"%s\" is not a valid command", command));

  update_status();
  g_strfreev(tokens);
  gtk_widget_grab_focus(GTK_WIDGET(Zathura.view));
}

void 
cb_inputbar_button_pressed(GtkWidget* widget, GdkEventButton* event, gpointer data)
{
  if(event->type == GDK_BUTTON_PRESS)
  {
    gtk_widget_modify_text(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_fg));
    gtk_widget_modify_base(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_bg));
    gtk_widget_grab_focus(GTK_WIDGET(Zathura.inputbar));
    gtk_entry_set_text(Zathura.inputbar, "");
    gtk_editable_set_position(GTK_EDITABLE(Zathura.inputbar), -1);
  }
}

void 
cb_drawing_area_button_pressed(GtkWidget* widget, GdkEventButton* event, gpointer data)
{
  if(!Zathura.PDF.document && !Zathura.PDF.page)
    return;

  // left click
  if(event->button == 1)
  {
    // check for links
    GList *link_list = poppler_page_get_link_mapping(Zathura.PDF.page);
    GList *links;
    int number_of_links = g_list_length(link_list);

    if(number_of_links <= 0)
      return;

    for(links = link_list; links; links = g_list_next(links))
    {
      PopplerLinkMapping *link_mapping = (PopplerLinkMapping*) links->data;
      PopplerAction      *action       = poppler_action_copy(link_mapping->action);

      if(action->type == POPPLER_ACTION_URI)
      {
        double page_width, page_height;
        poppler_page_get_size(Zathura.PDF.page, &page_width, &page_height);
        PopplerRectangle* link_rectangle = &link_mapping->area;

        if(Zathura.PDF.rotate == 0 || Zathura.PDF.rotate == 180)
          event->y = page_height - event->y;
        else if(Zathura.PDF.rotate == 90 || Zathura.PDF.rotate == 270)
          event->x = page_height - event->x;
          
        link_rectangle = recalcRectangle(link_rectangle);
        
        // check if click is in url area
        if( (link_rectangle->x1 <= event->x)
         && (link_rectangle->x2 >= event->x)
         && (link_rectangle->y1 <= event->y)
         && (link_rectangle->y2 >= event->y)
        )
          open_link(action->uri.uri);
      }
    }
    
    poppler_page_free_link_mapping(link_list);
  }
}

gboolean
cb_label_open_link(GtkLabel* label, gchar* link, gpointer data)
{
  open_link(link);
  return TRUE;
}

gboolean
cb_inputbar_key_pressed(GtkEntry* entry, GdkEventKey *event, gpointer data)
{
  Argument argument;

  switch(event->keyval)
  {
    case GDK_Escape:
      gtk_widget_grab_focus(GTK_WIDGET(Zathura.view));
      update_status();
      argument.n = HIDE;
      return complete(&argument);
    case GDK_Tab:
      argument.n = NEXT;
      return complete(&argument);
    case GDK_ISO_Left_Tab:
      argument.n = PREVIOUS;
      return complete(&argument);
  }
  
  return FALSE;
}

gboolean
cb_inputbar_key_released(GtkEntry *entry, GdkEventKey *event, gpointer data)
{
  int  length = gtk_entry_get_text_length(entry);
  char*  text = (char*) gtk_entry_get_text(entry);
  Argument argument;

  if(!length)
  {
    argument.n = HIDE;
    complete(&argument);
  }
  else if(length > 1 && text[0] == '/')
  {
    Argument argument;
    argument.data = (char*) text + 1;
    sc_search(&argument);
    gtk_widget_grab_focus(GTK_WIDGET(Zathura.inputbar));
    gtk_editable_set_position(GTK_EDITABLE(Zathura.inputbar), -1);
  }

  return FALSE;
}

/* main function */
int
main(int argc, char* argv[])
{
  gtk_init(&argc, &argv);

  init();
  
  if(argc >= 2)
    cmd_open(2, &argv[1]);

  gtk_widget_show_all(GTK_WIDGET(Zathura.window));
  gtk_main();
  
  return 0;
}

