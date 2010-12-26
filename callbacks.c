/* See LICENSE file for license and copyright information */

#include <callbacks.h>
#include <girara.h>
#include <stdlib.h>

#include "zathura.h"

gboolean
cb_destroy(GtkWidget* widget, gpointer data)
{
  if(Zathura.UI.session) {
    girara_session_destroy(Zathura.UI.session);
  }

  return TRUE;
}

gboolean
cb_draw(GtkWidget* widget, GdkEventExpose* expose, gpointer data)
{
  if(!widget || !Zathura.document) {
    return false;
  }

  gdk_window_clear(widget->window);
  cairo_t *cairo = gdk_cairo_create(widget->window);

  if(!cairo) {
    return false;
  }

  // FIXME: Split up
  zathura_page_t* page = zathura_page_get(Zathura.document, Zathura.document->current_page_number);
  if(!page) {
    cairo_destroy(cairo);
    goto error_out;
  }

  cairo_surface_t* surface = zathura_page_render(page);
  if(!surface) {
    zathura_page_free(page);
    fprintf(stderr, "error: rendering failed\n");
    goto error_out;
  }

  cairo_set_source_surface(cairo, surface, 0, 0);
  cairo_paint(cairo);
  cairo_destroy(cairo);

  cairo_surface_destroy(surface);
  zathura_page_free(page);

  gtk_widget_set_size_request(Zathura.UI.drawing_area, page->width, page->height);
  gtk_widget_queue_draw(Zathura.UI.drawing_area);

  return true;

error_out:

  return false;
}

void
buffer_changed(girara_session_t* session)
{
  g_return_if_fail(session != NULL);

  char* buffer = girara_buffer_get(session);

  if(buffer) {
    girara_statusbar_item_set_text(session, Zathura.UI.statusbar.buffer, buffer);
    free(buffer);
  } else {
    girara_statusbar_item_set_text(session, Zathura.UI.statusbar.buffer, "");
  }
}
