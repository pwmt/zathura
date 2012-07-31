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
  bool about_to_close; /**< Render thread is to be freed */
};

static void
render_job(void* data, void* user_data)
{
  zathura_page_t* page = data;
  zathura_t* zathura = user_data;
  if (page == NULL || zathura == NULL) {
    return;
  }

  girara_debug("rendering page %d ...", zathura_page_get_index(page));
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

  render_thread->about_to_close = false;
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

  render_thread->about_to_close = true;
  if (render_thread->pool) {
    g_thread_pool_free(render_thread->pool, TRUE, TRUE);
  }

  g_static_mutex_free(&render_thread->mutex);
  g_free(render_thread);
}

bool
render_page(render_thread_t* render_thread, zathura_page_t* page)
{
  if (render_thread == NULL || page == NULL || render_thread->pool == NULL || render_thread->about_to_close == true) {
    return false;
  }

  g_thread_pool_push(render_thread->pool, page, NULL);
  return true;
}

void
color2double(GdkColor* col, double* v)
{
  v[0] = (double) col->red / 65535.;
  v[1] = (double) col->green / 65535.;
  v[2] = (double) col->blue / 65535.;
}

/* Returns the maximum possible saturation for given h and l.
   Assumes that l is in the interval l1, l2 and corrects the value to
   force u=0 on l1 and l2 */
double
colorumax(double* h, double l, double l1, double l2)
{
  double u, uu, v, vv, lv;
  if (h[0] == 0 && h[1] == 0 && h[2] == 0) {
    return 0;
  }

  lv = (l - l1)/(l2 - l1);    /* Remap l to the whole interval 0,1 */
  
  u = v = 1000000;
  for (int k = 0; k < 3; k++) {
    if (h[k] > 0) {
      uu = fabs((1-l)/h[k]);
      vv = fabs((1-lv)/h[k]);
      
      if (uu < u)    u = uu;
      if (vv < v) v = vv;

    } else if (h[k] < 0) {
      uu = fabs(l/h[k]);
      vv = fabs(lv/h[k]);

      if (uu < u)    u = uu;
      if (vv < v) v = vv;      
    }
  }

  /* rescale v according to the length of the interval [l1, l2] */
  v = fabs(l2 - l1) * v; 

  /* forces the returned value to be 0 on l1 and l2, trying not to distort colors too much */
  return fmin(u, v);
}


static bool
render(zathura_t* zathura, zathura_page_t* page)
{
  if (zathura == NULL || page == NULL || zathura->sync.render_thread->about_to_close == true) {
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
  /* uses a representation of a rgb color as follows:
     - a lightness scalar (between 0,1), which is a weighted average of r, g, b,
     - a hue vector, which indicates a radian direction from the grey axis, inside the equal lightness plane.
     - a saturation scalar between 0,1. It is 0 when grey, 1 when the color is in the boundary of the rgb cube.
  */
   
  if (zathura->global.recolor == true) {

    /* RGB weights for computing lightness. Must sum to one */
    double a[] = {0.30, 0.59, 0.11};

    double l1, l2, l, s, u, t;
    double h[3];   
    double rgb1[3], rgb2[3], rgb[3];
    
    color2double(&zathura->ui.colors.recolor_dark_color, rgb1);
    color2double(&zathura->ui.colors.recolor_light_color, rgb2);

    l1 = (a[0]*rgb1[0] + a[1]*rgb1[1] + a[2]*rgb1[2]);
    l2 = (a[0]*rgb2[0] + a[1]*rgb2[1] + a[2]*rgb2[2]);
       
    for (unsigned int y = 0; y < page_height; y++) {
      unsigned char* data = image + y * rowstride;

      for (unsigned int x = 0; x < page_width; x++) {	
	/* Careful. data color components blue, green, red. */
	rgb[0] = (double) data[2] / 256.;
	rgb[1] = (double) data[1] / 256.;
	rgb[2] = (double) data[0] / 256.;
	
	/* compute h, s, l data   */
	l = a[0]*rgb[0] + a[1]*rgb[1] + a[2]*rgb[2];

	h[0] = rgb[0] - l;
	h[1] = rgb[1] - l;
	h[2] = rgb[2] - l;

	/* u is the maximum possible saturation for given h and l. s is a rescaled saturation between 0 and 1 */
	u = colorumax(h, l, 0, 1);
	if (u == 0)   s = 0;
	else          s = 1/u;

	/* Interpolates lightness between light and dark colors. white goes to light, and black goes to dark. */
        t = l;
	l = t * (l2 - l1) + l1;	

	if (zathura->global.recolor_keep_hue == true) {
	  /* adjusting lightness keeping hue of current color. white and black go to grays of same ligtness
	     as light and dark colors. */  
	  u = colorumax(h, l, l1, l2);	  
	  data[2] = (unsigned char)round(255.*(l + s*u * h[0]));
	  data[1] = (unsigned char)round(255.*(l + s*u * h[1]));
	  data[0] = (unsigned char)round(255.*(l + s*u * h[2]));
	} else {
	  /* Linear interpolation between dark and light with color ligtness as a parameter */
	  data[2] = (unsigned char)round(255.*(t * (rgb2[0] - rgb1[0]) + rgb1[0]));
	  data[1] = (unsigned char)round(255.*(t * (rgb2[1] - rgb1[1]) + rgb1[1]));
	  data[0] = (unsigned char)round(255.*(t * (rgb2[2] - rgb1[2]) + rgb1[2]));
	}
	
	data += 4;
      }
    }
  }

  if (zathura->sync.render_thread->about_to_close == false) {
    /* update the widget */
    gdk_threads_enter();
    GtkWidget* widget = zathura_page_get_widget(zathura, page);
    zathura_page_widget_update_surface(ZATHURA_PAGE(widget), surface);
    gdk_threads_leave();
  } else {
    cairo_surface_destroy(surface);
  }

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

  if (last_view_a < last_view_b) {
    return -1;
  } else if (last_view_a > last_view_b) {
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
