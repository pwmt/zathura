/* See LICENSE file for license and copyright information */

#include <math.h>
#include <girara/datastructures.h>
#include <girara/utils.h>
#include <girara/session.h>
#include <girara/settings.h>

#include "render.h"
#include "zathura.h"
#include "document.h"
#include "page.h"
#include "page-widget.h"
#include "utils.h"

static void render_job(void* data, void* user_data);
static bool render(zathura_t* zathura, zathura_page_t* page);
static gint render_thread_sort(gconstpointer a, gconstpointer b, gpointer data);

struct render_thread_s
{
  GThreadPool* pool; /**< Pool of threads */
  GStaticMutex mutex; /**< Render lock */
};

static void
render_job(void* data, void* user_data)
{
  zathura_page_t* page = data;
  zathura_t* zathura = user_data;
  if (page == NULL || zathura == NULL) {
    return;
  }

  girara_debug("rendring page %d ...", zathura_page_get_index(page));
  if (render(zathura, page) != true) {
    girara_error("Rendering failed (page %d)\n", zathura_page_get_index(page));
  }
}

render_thread_t*
render_init(zathura_t* zathura)
{
  render_thread_t* render_thread = g_malloc0(sizeof(render_thread_t));

  /* setup */
  render_thread->pool = g_thread_pool_new(render_job, zathura, 1, TRUE, NULL);
  if (render_thread->pool == NULL) {
    goto error_free;
  }

  g_thread_pool_set_sort_function(render_thread->pool, render_thread_sort, zathura);
  g_static_mutex_init(&render_thread->mutex);

  return render_thread;

error_free:

  render_free(render_thread);
  return NULL;
}

void
render_free(render_thread_t* render_thread)
{
  if (render_thread == NULL) {
    return;
  }

  if (render_thread->pool) {
    g_thread_pool_free(render_thread->pool, TRUE, TRUE);
  }

  g_static_mutex_free(&render_thread->mutex);
  g_free(render_thread);
}

bool
render_page(render_thread_t* render_thread, zathura_page_t* page)
{
  if (render_thread == NULL || page == NULL || render_thread->pool == NULL) {
    return false;
  }

  g_thread_pool_push(render_thread->pool, page, NULL);
  return true;
}

static bool
render(zathura_t* zathura, zathura_page_t* page)
{
  if (zathura == NULL || page == NULL) {
    return false;
  }

  /* create cairo surface */
  unsigned int page_width  = 0;
  unsigned int page_height = 0;
  page_calc_height_width(page, &page_height, &page_width, false);

  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, page_width, page_height);

  if (surface == NULL) {
    return false;
  }

  cairo_t* cairo = cairo_create(surface);

  if (cairo == NULL) {
    cairo_surface_destroy(surface);
    return false;
  }

  cairo_save(cairo);
  cairo_set_source_rgb(cairo, 1, 1, 1);
  cairo_rectangle(cairo, 0, 0, page_width, page_height);
  cairo_fill(cairo);
  cairo_restore(cairo);
  cairo_save(cairo);

  double scale = zathura_document_get_scale(zathura->document);
  if (fabs(scale - 1.0f) > FLT_EPSILON) {
    cairo_scale(cairo, scale, scale);
  }

  render_lock(zathura->sync.render_thread);
  if (zathura_page_render(page, cairo, false) != ZATHURA_ERROR_OK) {
    render_unlock(zathura->sync.render_thread);
    cairo_destroy(cairo);
    cairo_surface_destroy(surface);
    return false;
  }

  render_unlock(zathura->sync.render_thread);
  cairo_restore(cairo);
  cairo_destroy(cairo);

  const int rowstride  = cairo_image_surface_get_stride(surface);
  unsigned char* image = cairo_image_surface_get_data(surface);

  /* recolor */
  if (zathura->global.recolor == true) {
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

  /* update the widget */
  gdk_threads_enter();
  GtkWidget* widget = zathura_page_get_widget(zathura, page);
  zathura_page_widget_update_surface(ZATHURA_PAGE(widget), surface);
  gdk_threads_leave();

  return true;
}

void
render_all(zathura_t* zathura)
{
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  /* unmark all pages */
  unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(zathura->document, page_id);
    unsigned int page_height = 0, page_width = 0;
    page_calc_height_width(page, &page_height, &page_width, true);

    GtkWidget* widget = zathura_page_get_widget(zathura, page);
    gtk_widget_set_size_request(widget, page_width, page_height);
    gtk_widget_queue_resize(widget);
  }
}

static gint
render_thread_sort(gconstpointer a, gconstpointer b, gpointer data)
{
  if (a == NULL || b == NULL || data == NULL) {
    return 0;
  }

  zathura_page_t* page_a = (zathura_page_t*) a;
  zathura_page_t* page_b = (zathura_page_t*) b;
  zathura_t* zathura     = (zathura_t*) data;

  unsigned int page_a_index = zathura_page_get_index(page_a);
  unsigned int page_b_index = zathura_page_get_index(page_b);

  unsigned int last_view_a = 0;
  unsigned int last_view_b = 0;

  g_object_get(zathura->pages[page_a_index], "last-view", &last_view_a, NULL);
  g_object_get(zathura->pages[page_b_index], "last-view", &last_view_b, NULL);

  if (last_view_a > last_view_b) {
    return -1;
  } else if (last_view_b > last_view_a) {
    return 1;
  }

  return 0;
}

void
render_lock(render_thread_t* render_thread)
{
  if (render_thread == NULL) {
    return;
  }

  g_static_mutex_lock(&render_thread->mutex);
}

void
render_unlock(render_thread_t* render_thread)
{
  if (render_thread == NULL) {
    return;
  }

  g_static_mutex_unlock(&render_thread->mutex);
}
