/* See LICENSE file for license and copyright information */

#include <math.h>
#include <girara/datastructures.h>
#include <girara/utils.h>
#include "glib-compat.h"

#include "render.h"
#include "adjustment.h"
#include "zathura.h"
#include "document.h"
#include "page.h"
#include "page-widget.h"
#include "utils.h"

/* define the two types */
G_DEFINE_TYPE(ZathuraRenderer, zathura_renderer, G_TYPE_OBJECT)
G_DEFINE_TYPE(ZathuraRenderRequest, zathura_render_request, G_TYPE_OBJECT)

/* private methods for ZathuraRenderer  */
static void renderer_finalize(GObject* object);
/* private methods for ZathuraRenderRequest */
static void render_request_finalize(GObject* object);

static void render_job(void* data, void* user_data);
static gint render_thread_sort(gconstpointer a, gconstpointer b, gpointer data);
static void color2double(const GdkColor* col, double* v);
static ssize_t page_cache_lru_invalidate(ZathuraRenderer* renderer);
static void page_cache_invalidate_all(ZathuraRenderer* renderer);
static bool page_cache_is_full(ZathuraRenderer* renderer, bool* result);


/* private data for ZathuraRenderer */
typedef struct private_s {
  GThreadPool* pool; /**< Pool of threads */
  mutex mutex; /**< Render lock */
  volatile bool about_to_close; /**< Render thread is to be freed */

  /**
   * recolor information
   */
  struct {
    bool enabled;
    bool hue;

    double light[3];
    GdkColor light_gdk;
    double dark[3];
    GdkColor dark_gdk;
  } recolor;

  /*
   * page cache
   */
  struct {
    int* cache;
    size_t size;
    size_t num_cached_pages;
  } page_cache;

  /* render requests */
  girara_list_t* requests;
} private_t;

/* private data for ZathuraRenderRequest */
typedef struct request_private_s {
  ZathuraRenderer* renderer;
  zathura_page_t* page;
  volatile bool requested;
  volatile bool aborted;
  gint64 last_view_time;
} request_private_t;

#define GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZATHURA_TYPE_RENDERER, private_t))
#define REQUEST_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZATHURA_TYPE_RENDER_REQUEST, \
                               request_private_t))

/* init, new and free for ZathuraRenderer */

static void
zathura_renderer_class_init(ZathuraRendererClass* class)
{
  /* add private members */
  g_type_class_add_private(class, sizeof(private_t));

  /* overwrite methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = renderer_finalize;
}

static void
zathura_renderer_init(ZathuraRenderer* renderer)
{
  private_t* priv = GET_PRIVATE(renderer);
  priv->pool = g_thread_pool_new(render_job, renderer, 1, TRUE, NULL);
  priv->about_to_close = false;
  g_thread_pool_set_sort_function(priv->pool, render_thread_sort, NULL);
  mutex_init(&priv->mutex);

  /* recolor */
  priv->recolor.enabled = false;
  priv->recolor.hue = true;

  /* page cache */
  priv->page_cache.size = 0;
  priv->page_cache.cache = NULL;
  priv->page_cache.num_cached_pages = 0;

  zathura_renderer_set_recolor_colors_str(renderer, "#000000", "#FFFFFF");

  priv->requests = girara_list_new();
}

static void
page_cache_init(ZathuraRenderer* renderer, size_t cache_size)
{
  private_t* priv = GET_PRIVATE(renderer);

  priv->page_cache.size = cache_size;
  priv->page_cache.cache = g_malloc(cache_size * sizeof(int));
  page_cache_invalidate_all(renderer);
}

ZathuraRenderer*
zathura_renderer_new(size_t cache_size)
{
  g_return_val_if_fail(cache_size > 0, NULL);

  GObject* obj = g_object_new(ZATHURA_TYPE_RENDERER, NULL);
  ZathuraRenderer* ret = ZATHURA_RENDERER(obj);
  page_cache_init(ret, cache_size);

  return ret;
}

static void
renderer_finalize(GObject* object)
{
  ZathuraRenderer* renderer = ZATHURA_RENDERER(object);
  private_t* priv = GET_PRIVATE(renderer);

  zathura_renderer_stop(renderer);
  if (priv->pool) {
    g_thread_pool_free(priv->pool, TRUE, TRUE);
  }
  mutex_free(&(priv->mutex));

  free(priv->page_cache.cache);
  girara_list_free(priv->requests);
}

/* (un)register requests at the renderer */

static void
renderer_unregister_request(ZathuraRenderer* renderer,
    ZathuraRenderRequest* request)
{
  private_t* priv = GET_PRIVATE(renderer);
  girara_list_remove(priv->requests, request);
}

static void
renderer_register_request(ZathuraRenderer* renderer,
    ZathuraRenderRequest* request)
{
  private_t* priv = GET_PRIVATE(renderer);
  if (girara_list_contains(priv->requests, request) == false) {
    girara_list_append(priv->requests, request);
  }
}

/* init, new and free for ZathuraRenderRequest */

enum {
  REQUEST_COMPLETED,
  REQUEST_CACHE_ADDED,
  REQUEST_CACHE_INVALIDATED,
  REQUEST_LAST_SIGNAL
};

static guint request_signals[REQUEST_LAST_SIGNAL] = { 0 };

static void
zathura_render_request_class_init(ZathuraRenderRequestClass* class)
{
  /* add private members */
  g_type_class_add_private(class, sizeof(request_private_t));

  /* overwrite methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = render_request_finalize;

  request_signals[REQUEST_COMPLETED] = g_signal_new("completed",
      ZATHURA_TYPE_RENDER_REQUEST,
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      1,
      G_TYPE_POINTER);

  request_signals[REQUEST_CACHE_ADDED] = g_signal_new("cache-added",
      ZATHURA_TYPE_RENDER_REQUEST,
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      0);

  request_signals[REQUEST_CACHE_INVALIDATED] = g_signal_new("cache-invalidated",
      ZATHURA_TYPE_RENDER_REQUEST,
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      g_cclosure_marshal_generic,
      G_TYPE_NONE,
      0);
}

static void
zathura_render_request_init(ZathuraRenderRequest* request)
{
  request_private_t* priv = REQUEST_GET_PRIVATE(request);
  priv->renderer = NULL;
  priv->page = NULL;
}

ZathuraRenderRequest*
zathura_render_request_new(ZathuraRenderer* renderer, zathura_page_t* page)
{
  g_return_val_if_fail(renderer != NULL && page != NULL, NULL);

  GObject* obj = g_object_new(ZATHURA_TYPE_RENDER_REQUEST, NULL);
  if (obj == NULL) {
    return NULL;
  }

  ZathuraRenderRequest* request = ZATHURA_RENDER_REQUEST(obj);
  request_private_t* priv = REQUEST_GET_PRIVATE(request);
  /* we want to make sure that renderer lives long enough */
  priv->renderer = g_object_ref(renderer);
  priv->page = page;
  priv->aborted = false;
  priv->requested = false;

  /* register the request with the renderer */
  renderer_register_request(renderer, request);

  return request;
}

static void
render_request_finalize(GObject* object)
{
  ZathuraRenderRequest* request = ZATHURA_RENDER_REQUEST(object);
  request_private_t* priv = REQUEST_GET_PRIVATE(request);

  if (priv->renderer) {
    /* unregister the request */
    renderer_unregister_request(priv->renderer, request);
    /* release our private reference to the renderer */
    g_object_unref(priv->renderer);
  }
}

/* renderer methods */

bool
zathura_renderer_recolor_enabled(ZathuraRenderer* renderer)
{
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), false);

  return GET_PRIVATE(renderer)->recolor.enabled;
}

void
zathura_renderer_enable_recolor(ZathuraRenderer* renderer, bool enable)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  GET_PRIVATE(renderer)->recolor.enabled = enable;
}

bool
zathura_renderer_recolor_hue_enabled(ZathuraRenderer* renderer)
{
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), false);

  return GET_PRIVATE(renderer)->recolor.hue;
}

void
zathura_renderer_enable_recolor_hue(ZathuraRenderer* renderer, bool enable)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  GET_PRIVATE(renderer)->recolor.hue = enable;
}

void
zathura_renderer_set_recolor_colors(ZathuraRenderer* renderer,
    const GdkColor* light, const GdkColor* dark)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  private_t* priv = GET_PRIVATE(renderer);
  if (light != NULL) {
    priv->recolor.light_gdk.red = light->red;
    priv->recolor.light_gdk.blue = light->blue;
    priv->recolor.light_gdk.green = light->green;
    color2double(light, priv->recolor.light);
  }
  if (dark != NULL) {
    priv->recolor.dark_gdk.red = dark->red;
    priv->recolor.dark_gdk.blue = dark->blue;
    priv->recolor.dark_gdk.green = dark->green;
    color2double(dark, priv->recolor.dark);
  }
}

void
zathura_renderer_set_recolor_colors_str(ZathuraRenderer* renderer,
    const char* light, const char* dark)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  if (dark != NULL) {
    GdkColor color;
    gdk_color_parse(dark, &color);
    zathura_renderer_set_recolor_colors(renderer, NULL, &color);
  }
  if (light != NULL) {
    GdkColor color;
    gdk_color_parse(light, &color);
    zathura_renderer_set_recolor_colors(renderer, &color, NULL);
  }
}

void
zathura_renderer_get_recolor_colors(ZathuraRenderer* renderer,
    GdkColor* light, GdkColor* dark)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  private_t* priv = GET_PRIVATE(renderer);
  if (light != NULL) {
    light->red = priv->recolor.light_gdk.red;
    light->blue = priv->recolor.light_gdk.blue;
    light->green = priv->recolor.light_gdk.green;
  }
  if (dark != NULL) {
    dark->red = priv->recolor.dark_gdk.red;
    dark->blue = priv->recolor.dark_gdk.blue;
    dark->green = priv->recolor.dark_gdk.green;
  }
}

void
zathura_renderer_lock(ZathuraRenderer* renderer)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  private_t* priv = GET_PRIVATE(renderer);
  mutex_lock(&priv->mutex);
}

void
zathura_renderer_unlock(ZathuraRenderer* renderer)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  private_t* priv = GET_PRIVATE(renderer);
  mutex_unlock(&priv->mutex);
}

void
zathura_renderer_stop(ZathuraRenderer* renderer)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));
  GET_PRIVATE(renderer)->about_to_close = true;
}


/* ZathuraRenderRequest methods */

void
zathura_render_request(ZathuraRenderRequest* request, gint64 last_view_time)
{
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  request_private_t* request_priv = REQUEST_GET_PRIVATE(request);
  if (request_priv->requested == false) {
    request_priv->requested = true;
    request_priv->aborted = false;
    request_priv->last_view_time = last_view_time;

    private_t* priv = GET_PRIVATE(request_priv->renderer);
    g_thread_pool_push(priv->pool, request, NULL);
  }
}

void
zathura_render_request_abort(ZathuraRenderRequest* request)
{
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  request_private_t* request_priv = REQUEST_GET_PRIVATE(request);
  if (request_priv->requested == true) {
    request_priv->aborted = true;
  }
}

void
zathura_render_request_update_view_time(ZathuraRenderRequest* request)
{
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  request_private_t* request_priv = REQUEST_GET_PRIVATE(request);
  request_priv->last_view_time = g_get_real_time();
}

/* render job */

typedef struct emit_completed_signal_s
{
  ZathuraRenderer* renderer;
  ZathuraRenderRequest* request;
  cairo_surface_t* surface;
} emit_completed_signal_t;

static gboolean
emit_completed_signal(void* data)
{
  emit_completed_signal_t* ecs = data;
  private_t* priv = GET_PRIVATE(ecs->renderer);
  request_private_t* request_priv = REQUEST_GET_PRIVATE(ecs->request);

  if (priv->about_to_close == false && request_priv->aborted == false) {
    /* emit the signal */
    girara_debug("Emitting signal for page %d",
        zathura_page_get_index(request_priv->page) + 1);
    g_signal_emit(ecs->request, request_signals[REQUEST_COMPLETED], 0, ecs->surface);
  } else {
    girara_debug("Rendering of page %d aborted",
        zathura_page_get_index(request_priv->page) + 1);
  }
  /* mark the request as done */
  request_priv->requested = false;

  /* clean up the data */
  cairo_surface_destroy(ecs->surface);
  g_object_unref(ecs->renderer);
  g_object_unref(ecs->request);
  g_free(ecs);

  return FALSE;
}

static void
color2double(const GdkColor* col, double* v)
{
  v[0] = (double) col->red / 65535.;
  v[1] = (double) col->green / 65535.;
  v[2] = (double) col->blue / 65535.;
}

/* Returns the maximum possible saturation for given h and l.
   Assumes that l is in the interval l1, l2 and corrects the value to
   force u=0 on l1 and l2 */
static double
colorumax(const double* h, double l, double l1, double l2)
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

      if (uu < u) {
        u = uu;
      }
      if (vv < v) {
        v = vv;
      }
    } else if (h[k] < 0) {
      uu = fabs(l/h[k]);
      vv = fabs(lv/h[k]);

      if (uu < u) {
        u = uu;
      }
      if (vv < v) {
        v = vv;
      }
    }
  }

  /* rescale v according to the length of the interval [l1, l2] */
  v = fabs(l2 - l1) * v;

  /* forces the returned value to be 0 on l1 and l2, trying not to distort colors too much */
  return fmin(u, v);
}

static void
recolor(private_t* priv, unsigned int page_width, unsigned int page_height,
    cairo_surface_t* surface)
{
  /* uses a representation of a rgb color as follows:
     - a lightness scalar (between 0,1), which is a weighted average of r, g, b,
     - a hue vector, which indicates a radian direction from the grey axis,
       inside the equal lightness plane.
     - a saturation scalar between 0,1. It is 0 when grey, 1 when the color is
       in the boundary of the rgb cube.
  */

  const int rowstride  = cairo_image_surface_get_stride(surface);
  unsigned char* image = cairo_image_surface_get_data(surface);

  /* RGB weights for computing lightness. Must sum to one */
  static const double a[] = {0.30, 0.59, 0.11};

#define rgb1 priv->recolor.dark
#define rgb2 priv->recolor.light
  const double l1 = (a[0]*rgb1[0] + a[1]*rgb1[1] + a[2]*rgb1[2]);
  const double l2 = (a[0]*rgb2[0] + a[1]*rgb2[1] + a[2]*rgb2[2]);

  const double rgb_diff[] = {
    rgb2[0] - rgb1[0],
    rgb2[1] - rgb1[1],
    rgb2[2] - rgb1[2]
  };

  for (unsigned int y = 0; y < page_height; y++) {
    unsigned char* data = image + y * rowstride;

    for (unsigned int x = 0; x < page_width; x++, data += 4) {
      /* Careful. data color components blue, green, red. */
      const double rgb[3] = {
        (double) data[2] / 256.,
        (double) data[1] / 256.,
        (double) data[0] / 256.
      };

      /* compute h, s, l data   */
      double l = a[0]*rgb[0] + a[1]*rgb[1] + a[2]*rgb[2];

      if (priv->recolor.hue == true) {
        /* adjusting lightness keeping hue of current color. white and black
         * go to grays of same ligtness as light and dark colors. */
        const double h[3] = {
          rgb[0] - l,
          rgb[1] - l,
          rgb[2] - l
        };

        /* u is the maximum possible saturation for given h and l. s is a
         * rescaled saturation between 0 and 1 */
        double u = colorumax(h, l, 0, 1);
        double s = 0;
        if (u != 0) {
          s = 1/u;
        }

        /* Interpolates lightness between light and dark colors. white goes to
         * light, and black goes to dark. */
        l = l * (l2 - l1) + l1;
        u = colorumax(h, l, l1, l2);

        data[2] = (unsigned char)round(255.*(l + s*u * h[0]));
        data[1] = (unsigned char)round(255.*(l + s*u * h[1]));
        data[0] = (unsigned char)round(255.*(l + s*u * h[2]));
      } else {
        /* linear interpolation between dark and light with color ligtness as
         * a parameter */
        data[2] = (unsigned char)round(255.*(l * rgb_diff[0] + rgb1[0]));
        data[1] = (unsigned char)round(255.*(l * rgb_diff[1] + rgb1[1]));
        data[0] = (unsigned char)round(255.*(l * rgb_diff[2] + rgb1[2]));
      }
    }
  }

#undef rgb1
#undef rgb2
}

static bool
render(ZathuraRenderRequest* request, ZathuraRenderer* renderer)
{
  private_t* priv = GET_PRIVATE(renderer);
  request_private_t* request_priv = REQUEST_GET_PRIVATE(request);
  zathura_page_t* page = request_priv->page;

  /* create cairo surface */
  unsigned int page_width  = 0;
  unsigned int page_height = 0;

  zathura_document_t* document = zathura_page_get_document(page);
  const double height = zathura_page_get_height(page);
  const double width = zathura_page_get_width(page);

  const double real_scale = page_calc_height_width(document, height, width,
                                                   &page_height, &page_width, false);


  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
      page_width, page_height);
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

  if (fabs(real_scale - 1.0f) > FLT_EPSILON) {
    cairo_scale(cairo, real_scale, real_scale);
  }

  zathura_renderer_lock(renderer);
  const int err = zathura_page_render(page, cairo, false);
  zathura_renderer_unlock(renderer);
  cairo_restore(cairo);
  cairo_destroy(cairo);
  if (err != ZATHURA_ERROR_OK) {
    cairo_surface_destroy(surface);
    return false;
  }

  /* before recoloring, check if we've been aborted */
  if (priv->about_to_close == true || request_priv->aborted == true) {
    girara_debug("Rendering of page %d aborted",
        zathura_page_get_index(request_priv->page) + 1);
    request_priv->requested = false;
    cairo_surface_destroy(surface);
    return true;
  }

  /* recolor */
  if (priv->recolor.enabled == true) {
    recolor(priv, page_width, page_height, surface);
  }

  emit_completed_signal_t* ecs = g_malloc(sizeof(ecs));
  ecs->renderer = g_object_ref(renderer);
  ecs->request = g_object_ref(request);
  ecs->surface = cairo_surface_reference(surface);

  /* emit signal from the main context, i.e. the main thread */
  g_main_context_invoke(NULL, emit_completed_signal, ecs);

  cairo_surface_destroy(surface);

  return true;
}

static void
render_job(void* data, void* user_data)
{
  ZathuraRenderRequest* request = data;
  ZathuraRenderer* renderer = user_data;
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  private_t* priv = GET_PRIVATE(renderer);
  request_private_t* request_priv = REQUEST_GET_PRIVATE(request);
  if (priv->about_to_close == true || request_priv->aborted == true) {
    /* back out early */
    request_priv->requested = false;
    return;
  }

  girara_debug("Rendering page %d ...",
      zathura_page_get_index(request_priv->page) + 1);
  if (render(request, renderer) != true) {
    girara_error("Rendering failed (page %d)\n",
        zathura_page_get_index(request_priv->page) + 1);
    request_priv->requested = false;
  }
}


void
render_all(zathura_t* zathura)
{
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  /* unmark all pages */
  const unsigned int number_of_pages = zathura_document_get_number_of_pages(zathura->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; page_id++) {
    zathura_page_t* page = zathura_document_get_page(zathura->document,
        page_id);
    unsigned int page_height = 0, page_width = 0;
    double height = zathura_page_get_height(page);
    double width = zathura_page_get_width(page);
    page_calc_height_width(zathura->document, height, width, &page_height, &page_width, true);

    GtkWidget* widget = zathura_page_get_widget(zathura, page);
    gtk_widget_set_size_request(widget, page_width, page_height);
    gtk_widget_queue_resize(widget);
  }
}

static gint
render_thread_sort(gconstpointer a, gconstpointer b, gpointer UNUSED(data))
{
  if (a == NULL || b == NULL) {
    return 0;
  }

  ZathuraRenderRequest* request_a = (ZathuraRenderRequest*) a;
  ZathuraRenderRequest* request_b = (ZathuraRenderRequest*) b;
  request_private_t* priv_a = REQUEST_GET_PRIVATE(request_a);
  request_private_t* priv_b = REQUEST_GET_PRIVATE(request_b);
  if (priv_a->aborted == priv_b->aborted) {
    return priv_a->last_view_time < priv_b->last_view_time ? -1 :
        (priv_a->last_view_time > priv_b->last_view_time ? 1 : 0);
  }

  /* sort aborted entries earlier so that the are thrown out of the queue */
  return priv_a->aborted ? 1 : -1;
}


/* cache functions */

static bool
page_cache_is_cached(ZathuraRenderer* renderer, unsigned int page_index)
{
  g_return_val_if_fail(renderer != NULL, false);
  private_t* priv = GET_PRIVATE(renderer);

  if (priv->page_cache.num_cached_pages != 0) {
    for (size_t i = 0; i < priv->page_cache.size; ++i) {
      if (priv->page_cache.cache[i] >= 0 &&
          page_index == (unsigned int)priv->page_cache.cache[i]) {
        girara_debug("Page %d is a cache hit", page_index + 1);
        return true;
      }
    }
  }

  girara_debug("Page %d is a cache miss", page_index + 1);
  return false;
}

static int
find_request_by_page_index(const void* req, const void* data)
{
  const ZathuraRenderRequest* request = req;
  const unsigned int page_index = *((const int*)data);

  request_private_t* priv = REQUEST_GET_PRIVATE(request);
  if (zathura_page_get_index(priv->page) == page_index) {
    return 0;
  }
  return 1;
}

static ssize_t
page_cache_lru_invalidate(ZathuraRenderer* renderer)
{
  g_return_val_if_fail(renderer != NULL, -1);
  private_t* priv = GET_PRIVATE(renderer);
  g_return_val_if_fail(priv->page_cache.size != 0, -1);

  ssize_t lru_index = 0;
  gint64 lru_view_time = G_MAXINT64;
  ZathuraRenderRequest* request = NULL;
  for (size_t i = 0; i < priv->page_cache.size; ++i) {
    ZathuraRenderRequest* tmp_request = girara_list_find(priv->requests,
        find_request_by_page_index, &priv->page_cache.cache[i]);
    g_return_val_if_fail(tmp_request != NULL, -1);
    request_private_t* request_priv = REQUEST_GET_PRIVATE(tmp_request);

    if (request_priv->last_view_time < lru_view_time) {
      lru_view_time = request_priv->last_view_time;
      lru_index = i;
      request = tmp_request;
    }
  }

  request_private_t* request_priv = REQUEST_GET_PRIVATE(request);

  /* emit the signal */
  g_signal_emit(request, request_signals[REQUEST_CACHE_INVALIDATED], 0);
  girara_debug("Invalidated page %d at cache index %zd",
      zathura_page_get_index(request_priv->page) + 1, lru_index);
  priv->page_cache.cache[lru_index] = -1;
  --priv->page_cache.num_cached_pages;

  return lru_index;
}

static bool
page_cache_is_full(ZathuraRenderer* renderer, bool* result)
{
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer) && result != NULL, false);

  private_t* priv = GET_PRIVATE(renderer);
  *result = priv->page_cache.num_cached_pages == priv->page_cache.size;

  return true;
}

static void
page_cache_invalidate_all(ZathuraRenderer* renderer)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  private_t* priv = GET_PRIVATE(renderer);
  for (size_t i = 0; i < priv->page_cache.size; ++i) {
    priv->page_cache.cache[i] = -1;
  }
  priv->page_cache.num_cached_pages = 0;
}

void
zathura_renderer_page_cache_add(ZathuraRenderer* renderer,
    unsigned int page_index)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));
  if (page_cache_is_cached(renderer, page_index) == true) {
    return;
  }

  private_t* priv = GET_PRIVATE(renderer);
  bool full = false;
  if (page_cache_is_full(renderer, &full) == false) {
    return;
  } else if (full == true) {
    const ssize_t idx = page_cache_lru_invalidate(renderer);
    if (idx == -1) {
      return;
    }

    priv->page_cache.cache[idx] = page_index;
    ++priv->page_cache.num_cached_pages;
    girara_debug("Page %d is cached at cache index %zd", page_index + 1, idx);
  } else {
    priv->page_cache.cache[priv->page_cache.num_cached_pages++] = page_index;
    girara_debug("Page %d is cached at cache index %zu", page_index + 1,
        priv->page_cache.num_cached_pages - 1);
  }

  ZathuraRenderRequest* request = girara_list_find(priv->requests,
        find_request_by_page_index, &page_index);
  g_return_if_fail(request != NULL);
  g_signal_emit(request, request_signals[REQUEST_CACHE_ADDED], 0);
}
