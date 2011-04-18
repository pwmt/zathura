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

    zathura_page_t* page = (zathura_page_t*) girara_list_nth(render_thread->list, 0);
    girara_list_remove(render_thread->list, page);
    g_mutex_unlock(render_thread->lock);

    if (render(render_thread->zathura, page) != true) {
      girara_error("Rendering failed\n");
    }

    printf("Rendered %d\n", page->number);
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

  render_thread->thread = g_thread_create(render_job, render_thread, TRUE, NULL);

  if (!render_thread->thread) {
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
    g_cond_free(render_thread->cond);
  }

  if (render_thread->lock) {
    g_mutex_free(render_thread->lock);
  }
}

bool
render_page(render_thread_t* render_thread, zathura_page_t* page)
{
  if (!render_thread || !page || !render_thread->list || page->rendered) {
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

  /* remove old image */
  GtkWidget* widget = gtk_bin_get_child(GTK_BIN(page->event_box));
  if (widget != NULL) {
    gtk_container_remove(GTK_CONTAINER(page->event_box), widget);
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
      dst[0] = src[0];
      dst[1] = src[1];
      dst[2] = src[2];
      dst += 3;
    }
  }

  /* draw to gtk widget */
  page->surface = surface;

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
    zathura->document->pages[page_id]->rendered = false;
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

  if (page->surface == NULL) {
    cairo_set_source_surface(cairo, page->surface, 0, 0);
    cairo_paint(cairo);
    cairo_destroy(cairo);

    cairo_surface_destroy(page->surface);
    page->surface = NULL;
  }

  g_static_mutex_unlock(&(page->lock));

  return TRUE;
}
