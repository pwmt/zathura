#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

/* macros */
#define LENGTH(x) sizeof(x)/sizeof((x)[0])

/* enums */
enum { NEXT, PREVIOUS, HIDE, WARNING, DEFAULT };

struct
{
  GtkWindow         *window;
  GtkBox            *box;
  GtkScrolledWindow *view;
  GtkEntry          *inputbar;

  struct
  {
    GdkColor              default_fg;
    GdkColor              default_bg;
    GdkColor              inputbar_fg;
    GdkColor              inputbar_bg;
    GdkColor              inputbar_wn_fg;
    GdkColor              inputbar_wn_bg;
    GdkColor              completion_fg;
    GdkColor              completion_bg;
    GdkColor              completion_hl_fg;
    GdkColor              completion_hl_bg;
    PangoFontDescription *font;
  } Settings;
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
void update_status(int, char*);

gboolean complete(Argument*);

/* shortcut declarations */
void sc_focus_inputbar(Argument*);
void sc_quit(Argument*);

/* command declarations */
void cmd_open(int, char**);
void cmd_quit(int, char**);

/* callbacks declarations */
void cb_destroy(GtkWidget*, gpointer);
void cb_inputbar_activate(GtkEntry*, gpointer);
void cb_inputbar_button_pressed(GtkWidget*, GdkEventButton*, gpointer);

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
  gdk_color_parse(default_fgcolor,       &(Zathura.Settings.default_fg));
  gdk_color_parse(default_bgcolor,       &(Zathura.Settings.default_bg));
  gdk_color_parse(inputbar_fgcolor,      &(Zathura.Settings.inputbar_fg));
  gdk_color_parse(inputbar_bgcolor,      &(Zathura.Settings.inputbar_bg));
  gdk_color_parse(inputbar_wn_fgcolor,   &(Zathura.Settings.inputbar_wn_fg));
  gdk_color_parse(inputbar_wn_bgcolor,   &(Zathura.Settings.inputbar_wn_bg));
  gdk_color_parse(completion_fgcolor,    &(Zathura.Settings.completion_fg));
  gdk_color_parse(completion_bgcolor,    &(Zathura.Settings.completion_bg));
  gdk_color_parse(completion_hl_fgcolor, &(Zathura.Settings.completion_hl_fg));
  gdk_color_parse(completion_hl_bgcolor, &(Zathura.Settings.completion_hl_bg));
  Zathura.Settings.font = pango_font_description_from_string(font);

  /* variables */
  Zathura.window   = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  Zathura.box      = GTK_BOX(gtk_vbox_new(FALSE, 0));
  Zathura.view     = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
  Zathura.inputbar = GTK_ENTRY(gtk_entry_new());

  /* window */
  gtk_window_set_title(Zathura.window, "title");
  g_signal_connect(G_OBJECT(Zathura.window), "destroy",         G_CALLBACK(cb_destroy),     NULL);

  /* box */
  gtk_box_set_spacing(Zathura.box, 0);
  gtk_container_add(GTK_CONTAINER(Zathura.window), GTK_WIDGET(Zathura.box));

  /* view */
  g_signal_connect(G_OBJECT(Zathura.view), "key-press-event", G_CALLBACK(cb_view_key_pressed), NULL);

  #ifdef SHOW_SCROLLBARS
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.view), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  #else
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Zathura.view), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  #endif

  /* inputbar */
  gtk_entry_set_inner_border(Zathura.inputbar, NULL);
  gtk_entry_set_has_frame(Zathura.inputbar, FALSE);
  gtk_entry_set_editable(Zathura.inputbar,TRUE);
  g_signal_connect(G_OBJECT(Zathura.inputbar), "activate",           G_CALLBACK(cb_inputbar_activate),       NULL);
  g_signal_connect(G_OBJECT(Zathura.inputbar), "button-press-event", G_CALLBACK(cb_inputbar_button_pressed), NULL);
  g_signal_connect(G_OBJECT(Zathura.inputbar), "key-press-event",    G_CALLBACK(cb_inputbar_key_pressed),    NULL);
  g_signal_connect(G_OBJECT(Zathura.inputbar), "key-release-event",  G_CALLBACK(cb_inputbar_key_released),   NULL);

  /* packing */
  gtk_box_pack_start(Zathura.box, GTK_WIDGET(Zathura.view),     TRUE,  TRUE,  0);
  gtk_box_pack_end(Zathura.box,   GTK_WIDGET(Zathura.inputbar), FALSE, FALSE, 0);

  /* visuals */
  gtk_widget_modify_base(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_bg));
  gtk_widget_modify_text(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_fg));
  gtk_widget_modify_font(GTK_WIDGET(Zathura.inputbar), Zathura.Settings.font);
}

void
update_title()
{

}

void
update_status(int level, char* text)
{
  if(level == WARNING)
  {
    gtk_widget_modify_text(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_wn_fg));
    gtk_widget_modify_base(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_wn_bg));
  }
  else if(level == DEFAULT)
  {
    gtk_widget_modify_text(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_fg));
    gtk_widget_modify_base(GTK_WIDGET(Zathura.inputbar), GTK_STATE_NORMAL, &(Zathura.Settings.inputbar_bg));
  }

  gtk_entry_set_text(Zathura.inputbar, text);
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
sc_quit(Argument *argument)
{
  cb_destroy(NULL, NULL);
}

/* command implementations */
void
cmd_open(int argc, char** argv)
{

}

void
cmd_quit(int argc, char** argv)
{
  cb_destroy(NULL, NULL);
}

/* callback implementations */
void
cb_destroy(GtkWidget *widget, gpointer data)
{
  gtk_main_quit();
}

gboolean
cb_view_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  int i;
  for(i = 0; i < LENGTH(shortcuts); i++)
  {
    if(event->keyval == shortcuts[i].key && (event->state == shortcuts[i].mask || shortcuts[i].mask == 0))
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
    return;

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
    update_status(WARNING, "Not a valid command");
  else
    update_status(DEFAULT, "");

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
    gtk_editable_set_position(GTK_EDITABLE(Zathura.inputbar), -1);
    gtk_widget_grab_focus(GTK_WIDGET(Zathura.inputbar));
  }
}

gboolean
cb_inputbar_key_pressed(GtkEntry* entry, GdkEventKey *event, gpointer data)
{
  Argument argument;

  switch(event->keyval)
  {
    case GDK_Escape:
      gtk_widget_grab_focus(GTK_WIDGET(Zathura.view));
      update_status(DEFAULT, "");
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
  int length = gtk_entry_get_text_length(entry);
  Argument argument;

  if(!length)
  {
    argument.n = HIDE;
    complete(&argument);
  }

  return FALSE;
}

/* main function */
int
main(int argc, char* argv[])
{
  gtk_init(&argc, &argv);

  init();

  gtk_widget_show_all(GTK_WIDGET(Zathura.window));
  gtk_main();
  
  return 0;
}

