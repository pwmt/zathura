#include "render.h"
#include "zathura.h"
#include "document.h"

void* render_job(void* data);
bool render(zathura_t* zathura, zathura_page_t* page);

void*
render_job(void* data)
{
  render_thread_t* render_thread = (render_thread_t*) data;

  while (true) {
    g_mutex_lock(render_thread->lock);

    if (girara_list_size(render_thread->list) <= 0) {
      g_cond_wait(render_thread->cond, render_thread->lock);
    }

    if (girara_list_size(render_thread->list) <= 0) {
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
  render_thread_t* render_thread = malloc(sizeof(render_thread_t));

  if (!render_thread) {
    goto error_ret;
  }

  /* init */
  render_thread->list    = NULL;
  render_thread->thread  = NULL;
  render_thread->cond    = NULL;
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

  free(render_thread);

error_ret:

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
  zathura_image_buffer_t* image_buffer = zathura_page_render(page);

  if (image_buffer == NULL) {
    g_static_mutex_unlock(&(page->lock));
    gdk_threads_leave();
    return false;
  }

  /* create cairo surface */
  unsigned int page_width  = page->width  * zathura->document->scale;
  unsigned int page_height = page->height * zathura->document->scale;

  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, page_width, page_height);

  int rowstride        = cairo_image_surface_get_stride(surface);
  unsigned char* image = cairo_image_surface_get_data(surface);

  for (unsigned int y = 0; y < page_height; y++) {
    unsigned char* dst = image + y * rowstride;
    unsigned char* src = image_buffer->data + y * image_buffer->rowstride;

    for (unsigned int x = 0; x < page_width; x++) {
      dst[0] = src[2];
      dst[1] = src[1];
      dst[2] = src[0];
      src += 3;
      dst += 4;
    }
  }

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
  gtk_widget_set_size_request(page->drawing_area, page_width, page_height);
  gtk_widget_queue_draw(page->drawing_area);

  zathura_image_buffer_free(image_buffer);
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
    cairo_surface_destroy(zathura->document->pages[page_id]->surface);
    zathura->document->pages[page_id]->surface = NULL;
  }

  /* redraw current page */
  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.session->gtk.view));
  cb_view_vadjustment_value_changed(view_vadjustment, zathura);
}

gboolean
page_expose_event(GtkWidget* widget, GdkEventExpose* event, gpointer data)
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

  if (page->surface != NULL) {
    cairo_set_source_surface(cairo, page->surface, 0, 0);
    cairo_paint(cairo);
  } else {
    /* set background color */
    cairo_set_source_rgb(cairo, 255, 255, 255);
    cairo_rectangle(cairo, 0, 0, page->width * page->document->scale, page->height * page->document->scale);
    cairo_fill(cairo);

    /* write text */
    cairo_set_source_rgb(cairo, 0, 0, 0);
    const char* text = "Loading...";
    cairo_select_font_face(cairo, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo, 16.0);
    cairo_text_extents_t extents;
    cairo_text_extents(cairo, text, &extents);
    double x = (page->width  * page->document->scale) / 2 - (extents.width  / 2 + extents.x_bearing);
    double y = (page->height * page->document->scale) / 2 - (extents.height / 2 + extents.y_bearing);
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
