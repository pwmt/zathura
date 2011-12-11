/* See LICENSE file for license and copyright information */

#include <math.h>
#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/session.h>

#include "render.h"
#include "zathura.h"
#include "document.h"

void* render_job(void* data);
bool render(zathura_t* zathura, zathura_page_t* page);

static void
page_calc_height_width(zathura_page_t* page, unsigned int* page_height, unsigned int* page_width, bool rotate)
{
  if (rotate && page->document->rotate % 180) {
    *page_width  = ceil(page->height * page->document->scale);
    *page_height = ceil(page->width  * page->document->scale);
  } else {
    *page_width  = ceil(page->width  * page->document->scale);
    *page_height = ceil(page->height * page->document->scale);
  }
}

void*
render_job(void* data)
{
  render_thread_t* render_thread = (render_thread_t*) data;

  while (true) {
    g_mutex_lock(render_thread->lock);

    if (girara_list_size(render_thread->list) == 0) {
      g_cond_wait(render_thread->cond, render_thread->lock);
    }

    if (girara_list_size(render_thread->list) == 0) {
      /*
       * We've been signaled with g_cond_signal(), but the list
       * is still empty. This means that the signal came from
       * render_free() and current document is being closed.
       * We should unlock the mutex and kill the thread.
       */
      g_mutex_unlock(render_thread->lock);
      g_thread_exit(0);
    }

    zathura_page_t* page = (zathura_page_t*) girara_list_nth(render_thread->list, 0);
    girara_list_remove(render_thread->list, page);
    g_mutex_unlock(render_thread->lock);

    if (render(render_thread->zathura, page) != true) {
      girara_error("Rendering failed\n");
    }
  }

  return NULL;
}

render_thread_t*
render_init(zathura_t* zathura)
{
  render_thread_t* render_thread = g_malloc0(sizeof(render_thread_t));

  /* init */
  render_thread->zathura = zathura;

  /* setup */
  render_thread->list = girara_list_new();

  if (!render_thread->list) {
    goto error_free;
  }

  render_thread->cond = g_cond_new();

  if (!render_thread->cond) {
    goto error_free;
  }

  render_thread->lock = g_mutex_new();

  if (!render_thread->lock) {
    goto error_free;
  }

  render_thread->thread = g_thread_create(render_job, render_thread, TRUE, NULL);

  if (!render_thread->thread) {
    goto error_free;
  }

  return render_thread;

error_free:

  if (render_thread->list) {
    girara_list_free(render_thread->list);
  }

  if (render_thread->cond) {
    g_cond_free(render_thread->cond);
  }

  if (render_thread->lock) {
    g_mutex_free(render_thread->lock);
  }

  g_free(render_thread);

  return NULL;
}

void
render_free(render_thread_t* render_thread)
{
  if (!render_thread) {
    return;
  }

  if (render_thread->list) {
    girara_list_free(render_thread->list);
  }

  if (render_thread->cond) {
    g_cond_signal(render_thread->cond);
    g_thread_join(render_thread->thread);
    g_cond_free(render_thread->cond);
  }

  if (render_thread->lock) {
    g_mutex_free(render_thread->lock);
  }
}

bool
render_page(render_thread_t* render_thread, zathura_page_t* page)
{
  if (!render_thread || !page || !render_thread->list || page->surface) {
    return false;
  }

  g_mutex_lock(render_thread->lock);
  if (!girara_list_contains(render_thread->list, page)) {
    girara_list_append(render_thread->list, page);
  }
  g_cond_signal(render_thread->cond);
  g_mutex_unlock(render_thread->lock);

  return true;
}

bool
render(zathura_t* zathura, zathura_page_t* page)
{
  if (zathura == NULL || page == NULL) {
    return false;
  }

  gdk_threads_enter();
  g_static_mutex_lock(&(page->lock));

  /* create cairo surface */
  unsigned int page_width  = 0;
  unsigned int page_height = 0;
  page_calc_height_width(page, &page_height, &page_width, false);

  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, page_width, page_height);

  if (surface == NULL) {
    g_static_mutex_unlock(&(page->lock));
    gdk_threads_leave();
    return false;
  }

  cairo_t* cairo = cairo_create(surface);

  if (cairo == NULL) {
    cairo_surface_destroy(surface);
    g_static_mutex_unlock(&(page->lock));
    gdk_threads_leave();
    return false;
  }

  cairo_save(cairo);
  cairo_set_source_rgb(cairo, 1, 1, 1);
  cairo_rectangle(cairo, 0, 0, page_width, page_height);
  cairo_fill(cairo);
  cairo_restore(cairo);
  cairo_save(cairo);

  if (fabs(zathura->document->scale - 1.0f) > FLT_EPSILON) {
    cairo_scale(cairo, zathura->document->scale, zathura->document->scale);
  }

  if (zathura_page_render(page, cairo) == false) {
    cairo_destroy(cairo);
    cairo_surface_destroy(surface);
    g_static_mutex_unlock(&(page->lock));
    gdk_threads_leave();
    return false;
  }

  cairo_restore(cairo);
  cairo_destroy(cairo);

  const int rowstride  = cairo_image_surface_get_stride(surface);
  unsigned char* image = cairo_image_surface_get_data(surface);

  /* recolor */
  if (zathura->global.recolor) {
    /* recolor code based on qimageblitz library flatten() function
    (http://sourceforge.net/projects/qimageblitz/) */

    int r1 = zathura->ui.colors.recolor_dark_color.red    / 257;
    int g1 = zathura->ui.colors.recolor_dark_color.green  / 257;
    int b1 = zathura->ui.colors.recolor_dark_color.blue   / 257;
    int r2 = zathura->ui.colors.recolor_light_color.red   / 257;
    int g2 = zathura->ui.colors.recolor_light_color.green / 257;
    int b2 = zathura->ui.colors.recolor_light_color.blue  / 257;

    int min  = 0x00;
    int max  = 0xFF;
    int mean = 0x00;

    float sr = ((float) r2 - r1) / (max - min);
    float sg = ((float) g2 - g1) / (max - min);
    float sb = ((float) b2 - b1) / (max - min);

    for (unsigned int y = 0; y < page_height; y++) {
      unsigned char* data = image + y * rowstride;

      for (unsigned int x = 0; x < page_width; x++) {
        mean = (data[0] + data[1] + data[2]) / 3;
        data[2] = sr * (mean - min) + r1 + 0.5;
        data[1] = sg * (mean - min) + g1 + 0.5;
        data[0] = sb * (mean - min) + b1 + 0.5;
        data += 4;
      }
    }
  }

  /* draw to gtk widget */
  page->surface = surface;
  gtk_widget_queue_draw(page->drawing_area);

  g_static_mutex_unlock(&(page->lock));
  gdk_threads_leave();

  return true;
}

void
render_all(zathura_t* zathura)
{
  if (zathura->document == NULL) {
    return;
  }

  /* unmark all pages */
  for (unsigned int page_id = 0; page_id < zathura->document->number_of_pages; page_id++) {
    zathura_page_t* page = zathura->document->pages[page_id];
    cairo_surface_destroy(page->surface);
    page->surface = NULL;

    unsigned int page_height = 0, page_width = 0;
    page_calc_height_width(page, &page_height, &page_width, true);

    gtk_widget_set_size_request(page->drawing_area, page_width, page_height);
    gtk_widget_queue_resize(page->drawing_area);
  }
}

gboolean
page_expose_event(GtkWidget* UNUSED(widget), GdkEventExpose* UNUSED(event),
    gpointer data)
{
  zathura_page_t* page = data;
  if (page == NULL) {
    return FALSE;
  }

  g_static_mutex_lock(&(page->lock));

  cairo_t* cairo = gdk_cairo_create(page->drawing_area->window);

  if (cairo == NULL) {
    girara_error("Could not retreive cairo object");
    g_static_mutex_unlock(&(page->lock));
    return FALSE;
  }

  unsigned int page_height = 0, page_width = 0;
  page_calc_height_width(page, &page_height, &page_width, true);

  if (page->surface != NULL) {
    cairo_save(cairo);

    switch(page->document->rotate) {
      case 90:
        cairo_translate(cairo, page_width, 0);
        break;
      case 180:
        cairo_translate(cairo, page_width, page_height);
        break;
      case 270:
        cairo_translate(cairo, 0, page_height);
        break;
    }

    if (page->document->rotate != 0) {
      cairo_rotate(cairo, page->document->rotate * G_PI / 180.0);
    }

    cairo_set_source_surface(cairo, page->surface, 0, 0);
    cairo_paint(cairo);

    cairo_restore(cairo);
  } else {
    /* set background color */
    cairo_set_source_rgb(cairo, 255, 255, 255);
    cairo_rectangle(cairo, 0, 0, page_width, page->height * page->height);
    cairo_fill(cairo);

    /* write text */
    cairo_set_source_rgb(cairo, 0, 0, 0);
    const char* text = "Loading...";
    cairo_select_font_face(cairo, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo, 16.0);
    cairo_text_extents_t extents;
    cairo_text_extents(cairo, text, &extents);
    double x = page_width * 0.5 - (extents.width * 0.5 + extents.x_bearing);
    double y = page_height * 0.5 - (extents.height * 0.5 + extents.y_bearing);
    cairo_move_to(cairo, x, y);
    cairo_show_text(cairo, text);

    /* render real page */
    render_page(page->document->zathura->sync.render_thread, page);

    /* update statusbar */
  }
  cairo_destroy(cairo);

  page->document->current_page_number = page->number;
  statusbar_page_number_update(page->document->zathura);

  g_static_mutex_unlock(&(page->lock));

  return TRUE;
}
