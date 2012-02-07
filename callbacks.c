/* See LICENSE file for license and copyright information */

#include <girara/statusbar.h>
#include <girara/session.h>
#include <girara/settings.h>
#include <girara/utils.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "callbacks.h"
#include "zathura.h"
#include "render.h"
#include "document.h"
#include "utils.h"
#include "shortcuts.h"
#include "page_view_widget.h"

gboolean
cb_destroy(GtkWidget* UNUSED(widget), gpointer UNUSED(data))
{
  gtk_main_quit();
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
cb_view_vadjustment_value_changed(GtkAdjustment* GIRARA_UNUSED(adjustment), gpointer data)
{
  zathura_t* zathura = data;
  if (!zathura || !zathura->document || !zathura->document->pages || !zathura->ui.page_view) {
    return;
  }

  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  GtkAdjustment* view_hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));

  GdkRectangle view_rect;
  /* get current adjustment values */
  view_rect.y = gtk_adjustment_get_value(view_vadjustment);
  view_rect.height = gtk_adjustment_get_page_size(view_vadjustment);
  view_rect.x = gtk_adjustment_get_value(view_hadjustment);
  view_rect.width = gtk_adjustment_get_page_size(view_hadjustment);

  int page_padding = 1;
  girara_setting_get(zathura->ui.session, "page-padding", &page_padding);

  GdkRectangle center;
  center.x = view_rect.x + (view_rect.width + 1) / 2;
  center.y = view_rect.y + (view_rect.height + 1) / 2;
  center.height = center.width = 2*page_padding + 1;

  bool updated = false;
  /* find page that fits */
  for (unsigned int page_id = 0; page_id < zathura->document->number_of_pages; page_id++)
  {
    zathura_page_t* page = zathura->document->pages[page_id];

    page_offset_t offset;
    page_calculate_offset(page, &offset);

    GdkRectangle page_rect;
    page_rect.x = offset.x;
    page_rect.y = offset.y;
    page_rect.width = page->width * zathura->document->scale;
    page_rect.height = page->height * zathura->document->scale;

    if (gdk_rectangle_intersect(&view_rect, &page_rect, NULL) == TRUE) {
      page->visible = true;
      if (updated == false && gdk_rectangle_intersect(&center, &page_rect, NULL) == TRUE) {
        zathura->document->current_page_number = page_id;
        updated = true;
      }
    } else {
      page->visible = false;
    }
  }

  statusbar_page_number_update(zathura);
}

void
cb_pages_per_row_value_changed(girara_session_t* UNUSED(session), const char* UNUSED(name), girara_setting_type_t UNUSED(type), void* value, void* data)
{
  g_return_if_fail(value != NULL);

  int pages_per_row = *(int*) value;
  zathura_t* zathura = data;

  if (pages_per_row < 1) {
    pages_per_row = 1;
  }

  page_view_set_mode(zathura, pages_per_row);
}

void
cb_index_row_activated(GtkTreeView* tree_view, GtkTreePath* path,
    GtkTreeViewColumn* UNUSED(column), void* data)
{
  zathura_t* zathura = data;
  if (tree_view == NULL || zathura == NULL || zathura->ui.session == NULL) {
    return;
  }

  GtkTreeModel  *model;
  GtkTreeIter   iter;

  g_object_get(tree_view, "model", &model, NULL);

  if(gtk_tree_model_get_iter(model, &iter, path))
  {
    zathura_index_element_t* index_element;
    gtk_tree_model_get(model, &iter, 2, &index_element, -1);

    if (index_element == NULL) {
      return;
    }

    if (index_element->type == ZATHURA_LINK_TO_PAGE) {
      sc_toggle_index(zathura->ui.session, NULL, NULL, 0);
      page_set_delayed(zathura, index_element->target.page_number);
    } else if (index_element->type == ZATHURA_LINK_EXTERNAL) {
      if (girara_xdg_open(index_element->target.uri) == false) {
        girara_notify(zathura->ui.session, GIRARA_ERROR, "Failed to run xdg-open.");
      }
    }
  }

  g_object_unref(model);
}

bool
cb_sc_follow(GtkEntry* entry, girara_session_t* session)
{
  g_return_val_if_fail(session != NULL, FALSE);
  g_return_val_if_fail(session->global.data != NULL, FALSE);

  zathura_t* zathura = session->global.data;

  char* input = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
  if (input == NULL) {
    goto error_ret;
  } else if (strlen(input) == 0) {
    goto error_free;
  }

  int index = atoi(input);
  if (index == 0 && g_strcmp0(input, "0") != 0) {
    girara_notify(session, GIRARA_WARNING, "Invalid input '%s' given.", input);
    goto error_free;
  }
  index = index-1;

  /* set pages to draw links */
  bool invalid_index = true;
  for (unsigned int page_id = 0; page_id < zathura->document->number_of_pages; page_id++) {
    zathura_page_t* page = zathura->document->pages[page_id];
    if (page == NULL || page->visible == false) {
      continue;
    }

    g_object_set(page->drawing_area, "draw-links", FALSE, NULL);

    zathura_link_t* link = zathura_page_view_link_get(ZATHURA_PAGE_VIEW(page->drawing_area), index);
    if (link != NULL) {
      switch (link->type) {
        case ZATHURA_LINK_TO_PAGE:
          girara_info("page number: %d", link->target.page_number);
          page_set_delayed(zathura, link->target.page_number);
          break;
        case ZATHURA_LINK_EXTERNAL:
          girara_info("target: %s", link->target.value);
          girara_xdg_open(link->target.value);
          break;
      }

      invalid_index = false;
      break;
    }
  }

  if (invalid_index == true) {
    girara_notify(session, GIRARA_WARNING, "Invalid index '%s' given.", input);
  }

  g_free(input);

  return TRUE;

error_free:

  g_free(input);

error_ret:

  return FALSE;
}
