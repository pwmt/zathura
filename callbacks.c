/* See LICENSE file for license and copyright information */

#include <girara.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "zathura.h"
#include "render.h"
#include "document.h"
#include "utils.h"

gboolean
cb_destroy(GtkWidget* widget, gpointer data)
{
  zathura_free(data);

  return TRUE;
}

void
buffer_changed(girara_session_t* session)
{
  g_return_if_fail(session != NULL);
  g_return_if_fail(session->global.data != NULL);

  zathura_t* zathura = session->global.data;

  char* buffer = girara_buffer_get(session);

  if (buffer) {
    girara_statusbar_item_set_text(session, zathura->ui.statusbar.buffer, buffer);
    free(buffer);
  } else {
    girara_statusbar_item_set_text(session, zathura->ui.statusbar.buffer, "");
  }
}

void
cb_view_vadjustment_value_changed(GtkAdjustment *adjustment, gpointer data)
{
  zathura_t* zathura = data;
  if (!zathura || !zathura->document || !zathura->document->pages || !zathura->ui.page_view) {
    return;
  }

  /* get current adjustment values */
  gdouble lower = gtk_adjustment_get_value(adjustment);
  gdouble upper = lower + gtk_adjustment_get_page_size(adjustment);

  /* find page that fits */
  for (unsigned int page_id = 0; page_id < zathura->document->number_of_pages; page_id++)
  {
    zathura_page_t* page = zathura->document->pages[page_id];

    page_offset_t* offset = page_calculate_offset(page);
    if (offset == NULL) {
      continue;
    }

    double begin = offset->y;
    double end   = offset->y + page->height;

    if (   ( (begin >= lower) && (end <= upper) ) /* [> page is in viewport <]*/
        || ( (begin <= lower) && (end >= lower) && (end <= upper) ) /* [> end of the page is in viewport <] */
        || ( (begin >= lower) && (end >= upper) && (begin <= upper) ) /* [> begin of the page is in viewport <] */
      ) {
      page->visible = true;
      if (page->surface == NULL) {
        render_page(zathura->sync.render_thread, page);
      }
    } else {
      page->visible = false;
      cairo_surface_destroy(page->surface);
      page->surface = NULL;
    }
    
    free(offset);
  }
}

void
cb_pages_per_row_value_changed(girara_session_t* session, girara_setting_t* setting)
{
  int pages_per_row = setting->value.i;
  zathura_t* zathura = setting->data;

  if (pages_per_row < 1) {
    pages_per_row = 1;
  }

  page_view_set_mode(zathura, pages_per_row);
}
