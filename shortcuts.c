/* See LICENSE file for license and copyright information */

#include <girara.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "shortcuts.h"
#include "zathura.h"

bool
sc_abort(girara_session_t* session, girara_argument_t* argument)
{
  g_return_val_if_fail(session != NULL, false);

  girara_mode_set(session, NORMAL);

  return false;
}

bool
sc_adjust_window(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_change_buffer(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_change_mode(girara_session_t* session, girara_argument_t* argument)
{
  g_return_val_if_fail(session != NULL, false);

  girara_mode_set(session, argument->n);

  return false;
}

bool
sc_focus_inputbar(girara_session_t* session, girara_argument_t* argument)
{
  g_return_val_if_fail(session != NULL, false);

  if (!(GTK_WIDGET_VISIBLE(GTK_WIDGET(session->gtk.inputbar)))) {
    gtk_widget_show(GTK_WIDGET(session->gtk.inputbar));
  }

  if (argument->data) {
    gtk_entry_set_text(session->gtk.inputbar, (char*) argument->data);
    gtk_widget_grab_focus(GTK_WIDGET(session->gtk.inputbar));
    gtk_editable_set_position(GTK_EDITABLE(session->gtk.inputbar), -1);
  }

  return false;
}

bool
sc_follow(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_goto(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_navigate(girara_session_t* session, girara_argument_t* argument)
{
  if (!session || !argument || !Zathura.document) {
    return false;
  }

  unsigned int number_of_pages = Zathura.document->number_of_pages;
  unsigned int new_page        = Zathura.document->current_page_number;

  if (argument->n == NEXT) {
    new_page = (new_page + 1) % number_of_pages;
  } else if (argument->n == PREVIOUS) {
    new_page = (new_page + number_of_pages - 1) % number_of_pages;
  }

  page_set(new_page);

  return false;
}

bool
sc_recolor(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_reload(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_rotate(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_scroll(girara_session_t* session, girara_argument_t* argument)
{
  GtkAdjustment* adjustment = NULL;
  if ( (argument->n == LEFT) || (argument->n == RIGHT) )
    adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(Zathura.UI.session->gtk.view));
  else
    adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(Zathura.UI.session->gtk.view));

  gdouble view_size  = gtk_adjustment_get_page_size(adjustment);
  gdouble value      = gtk_adjustment_get_value(adjustment);
  gdouble max        = gtk_adjustment_get_upper(adjustment) - view_size;
  gdouble scroll_step = 40;

  gdouble new_value;
  switch(argument->n)
  {
    case FULL_UP:
      new_value = (value - view_size) < 0 ? 0 : (value - view_size);
      break;
    case FULL_DOWN:
      new_value = (value + view_size) > max ? max : (value + view_size);
      break;
    case HALF_UP:
      new_value = (value - (view_size / 2)) < 0 ? 0 : (value - (view_size / 2));
      break;
    case HALF_DOWN:
      new_value = (value + (view_size / 2)) > max ? max : (value + (view_size / 2));
      break;
    case LEFT:
    case UP:
      new_value = (value - scroll_step) < 0 ? 0 : (value - scroll_step);
      break;
    case RIGHT:
    case DOWN:
      new_value = (value + scroll_step) > max ? max : (value + scroll_step);
      break;
    case TOP:
      new_value = 0;
      break;
    case BOTTOM:
      new_value = max;
      break;
    default:
      new_value = 0;
  }

  gtk_adjustment_set_value(adjustment, new_value);

  return false;
}

bool
sc_search(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_switch_goto_mode(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_navigate_index(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_toggle_index(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_toggle_inputbar(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_toggle_fullscreen(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_toggle_statusbar(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}

bool
sc_quit(girara_session_t* session, girara_argument_t* argument)
{
  girara_argument_t arg = { GIRARA_HIDE, NULL };
  girara_isc_completion(session, &arg);

  cb_destroy(NULL, NULL);

  gtk_main_quit();

  return false;
}

bool
sc_zoom(girara_session_t* session, girara_argument_t* argument)
{
  return false;
}
