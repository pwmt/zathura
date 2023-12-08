/* SPDX-License-Identifier: Zlib */

#include <math.h>
#include <string.h>
#include <girara/datastructures.h>
#include <girara/utils.h>

#include "render.h"
#include "adjustment.h"
#include "zathura.h"
#include "document.h"
#include "page.h"
#include "page-widget.h"
#include "utils.h"

/* private data for ZathuraRenderer */
typedef struct private_s {
  GThreadPool* pool; /**< Pool of threads */
  GMutex mutex; /**< Render lock */
  volatile bool about_to_close; /**< Render thread is to be freed */

  /**
   * recolor information
   */
  struct {
    bool enabled;
    bool hue;
    bool reverse_video;

    GdkRGBA light;
    GdkRGBA dark;
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
} ZathuraRendererPrivate;

/* private data for ZathuraRenderRequest */
typedef struct request_private_s {
  ZathuraRenderer* renderer;
  zathura_page_t* page;
  gint64 last_view_time;
  girara_list_t* active_jobs;
  GMutex jobs_mutex;
  bool render_plain;
} ZathuraRenderRequestPrivate;

/* define the two types */
G_DEFINE_TYPE_WITH_CODE(ZathuraRenderer, zathura_renderer, G_TYPE_OBJECT, G_ADD_PRIVATE(ZathuraRenderer))
G_DEFINE_TYPE_WITH_CODE(ZathuraRenderRequest, zathura_render_request, G_TYPE_OBJECT, G_ADD_PRIVATE(ZathuraRenderRequest))

/* private methods for ZathuraRenderer  */
static void renderer_finalize(GObject* object);
/* private methods for ZathuraRenderRequest */
static void render_request_dispose(GObject* object);
static void render_request_finalize(GObject* object);

static void render_job(void* data, void* user_data);
static gint render_thread_sort(gconstpointer a, gconstpointer b, gpointer data);
static ssize_t page_cache_lru_invalidate(ZathuraRenderer* renderer);
static void page_cache_invalidate_all(ZathuraRenderer* renderer);
static bool page_cache_is_full(ZathuraRenderer* renderer, bool* result);

/* job descritption for render thread */
typedef struct render_job_s {
  ZathuraRenderRequest* request;
  volatile bool aborted;
} render_job_t;

/* init, new and free for ZathuraRenderer */

static void
zathura_renderer_class_init(ZathuraRendererClass* class)
{
  /* overwrite methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = renderer_finalize;
}

static void
zathura_renderer_init(ZathuraRenderer* renderer)
{
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  priv->pool = g_thread_pool_new(render_job, renderer, 1, TRUE, NULL);
  priv->about_to_close = false;
  g_thread_pool_set_sort_function(priv->pool, render_thread_sort, NULL);
  g_mutex_init(&priv->mutex);

  /* recolor */
  priv->recolor.enabled = false;
  priv->recolor.hue = true;
  priv->recolor.reverse_video = false;

  /* page cache */
  priv->page_cache.size = 0;
  priv->page_cache.cache = NULL;
  priv->page_cache.num_cached_pages = 0;

  zathura_renderer_set_recolor_colors_str(renderer, "#000000", "#FFFFFF");

  priv->requests = girara_list_new();
}

static bool
page_cache_init(ZathuraRenderer* renderer, size_t cache_size)
{
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);

  priv->page_cache.size  = cache_size;
  priv->page_cache.cache = g_try_malloc0(cache_size * sizeof(int));
  if (priv->page_cache.cache == NULL) {
    return false;
  }

  page_cache_invalidate_all(renderer);
  return true;
}

ZathuraRenderer*
zathura_renderer_new(size_t cache_size)
{
  g_return_val_if_fail(cache_size > 0, NULL);

  GObject* obj = g_object_new(ZATHURA_TYPE_RENDERER, NULL);
  ZathuraRenderer* ret = ZATHURA_RENDERER(obj);

  if (page_cache_init(ret, cache_size) == false) {
    g_object_unref(obj);
    return NULL;
  }

  return ret;
}

static void
renderer_finalize(GObject* object)
{
  ZathuraRenderer* renderer = ZATHURA_RENDERER(object);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);

  zathura_renderer_stop(renderer);
  if (priv->pool != NULL) {
    g_thread_pool_free(priv->pool, TRUE, TRUE);
  }
  g_mutex_clear(&(priv->mutex));

  g_free(priv->page_cache.cache);
  girara_list_free(priv->requests);
}

/* (un)register requests at the renderer */

static void
renderer_unregister_request(ZathuraRenderer* renderer,
    ZathuraRenderRequest* request)
{
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  girara_list_remove(priv->requests, request);
}

static void
renderer_register_request(ZathuraRenderer* renderer,
    ZathuraRenderRequest* request)
{
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
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
  /* overwrite methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->dispose      = render_request_dispose;
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
  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
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
  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
  /* we want to make sure that renderer lives long enough */
  priv->renderer = g_object_ref(renderer);
  priv->page = page;
  priv->active_jobs = girara_list_new();
  g_mutex_init(&priv->jobs_mutex);
  priv->render_plain = false;

  /* register the request with the renderer */
  renderer_register_request(renderer, request);

  return request;
}

static void
render_request_dispose(GObject* object)
{
  ZathuraRenderRequest* request = ZATHURA_RENDER_REQUEST(object);
  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);

  if (priv->renderer != NULL) {
    /* unregister the request */
    renderer_unregister_request(priv->renderer, request);
    /* release our private reference to the renderer */
    g_clear_object(&priv->renderer);
  }

  G_OBJECT_CLASS(zathura_render_request_parent_class)->dispose(object);
}

static void
render_request_finalize(GObject* object)
{
  ZathuraRenderRequest* request = ZATHURA_RENDER_REQUEST(object);
  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);

  if (girara_list_size(priv->active_jobs) != 0) {
    girara_error("This should not happen!");
  }
  girara_list_free(priv->active_jobs);
  g_mutex_clear(&priv->jobs_mutex);

  G_OBJECT_CLASS(zathura_render_request_parent_class)->finalize(object);
}

/* renderer methods */

bool
zathura_renderer_recolor_enabled(ZathuraRenderer* renderer)
{
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), false);

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  return priv->recolor.enabled;
}

void
zathura_renderer_enable_recolor(ZathuraRenderer* renderer, bool enable)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  priv->recolor.enabled = enable;
}

bool
zathura_renderer_recolor_hue_enabled(ZathuraRenderer* renderer)
{
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), false);

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  return priv->recolor.hue;
}

void
zathura_renderer_enable_recolor_hue(ZathuraRenderer* renderer, bool enable)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  priv->recolor.hue = enable;
}

bool
zathura_renderer_recolor_reverse_video_enabled(ZathuraRenderer* renderer)
{
  g_return_val_if_fail(ZATHURA_IS_RENDERER(renderer), false);

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  return priv->recolor.reverse_video;
}

void
zathura_renderer_enable_recolor_reverse_video(ZathuraRenderer* renderer, bool enable)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  priv->recolor.reverse_video = enable;
}
void
zathura_renderer_set_recolor_colors(ZathuraRenderer* renderer,
    const GdkRGBA* light, const GdkRGBA* dark)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  if (light != NULL) {
    memcpy(&priv->recolor.light, light, sizeof(GdkRGBA));
  }
  if (dark != NULL) {
    memcpy(&priv->recolor.dark, dark, sizeof(GdkRGBA));
  }
}

void
zathura_renderer_set_recolor_colors_str(ZathuraRenderer* renderer,
    const char* light, const char* dark)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  if (dark != NULL) {
    GdkRGBA color;
    if (parse_color(&color, dark) == true) {
      zathura_renderer_set_recolor_colors(renderer, NULL, &color);
    }
  }
  if (light != NULL) {
    GdkRGBA color;
    if (parse_color(&color, light) == true) {
      zathura_renderer_set_recolor_colors(renderer, &color, NULL);
    }
  }
}

void
zathura_renderer_get_recolor_colors(ZathuraRenderer* renderer,
    GdkRGBA* light, GdkRGBA* dark)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  if (light != NULL) {
    memcpy(light, &priv->recolor.light, sizeof(GdkRGBA));
  }
  if (dark != NULL) {
    memcpy(dark, &priv->recolor.dark, sizeof(GdkRGBA));
  }
}

void
zathura_renderer_lock(ZathuraRenderer* renderer)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  g_mutex_lock(&priv->mutex);
}

void
zathura_renderer_unlock(ZathuraRenderer* renderer)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  g_mutex_unlock(&priv->mutex);
}

void
zathura_renderer_stop(ZathuraRenderer* renderer)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  priv->about_to_close = true;
}


/* ZathuraRenderRequest methods */

void zathura_render_request(ZathuraRenderRequest* request, gint64 last_view_time) {
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);
  g_mutex_lock(&request_priv->jobs_mutex);

  bool unfinished_jobs = false;
  /* check if there are any active jobs left */
  for (size_t idx = 0; idx != girara_list_size(request_priv->active_jobs); ++idx) {
    render_job_t* job = girara_list_nth(request_priv->active_jobs, idx);
    if (job->aborted == false) {
      unfinished_jobs = true;
      break;
    }
  }

  /* only add a new job if there are no active ones left */
  if (unfinished_jobs == false) {
    request_priv->last_view_time = last_view_time;

    render_job_t* job = g_try_malloc0(sizeof(render_job_t));
    if (job == NULL) {
      g_mutex_unlock(&request_priv->jobs_mutex);
      return;
    }

    job->request = g_object_ref(request);
    job->aborted = false;
    girara_list_append(request_priv->active_jobs, job);

    ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(request_priv->renderer);
    g_thread_pool_push(priv->pool, job, NULL);
  }

  g_mutex_unlock(&request_priv->jobs_mutex);
}

void zathura_render_request_abort(ZathuraRenderRequest* request) {
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);
  g_mutex_lock(&request_priv->jobs_mutex);
  for (size_t idx = 0; idx != girara_list_size(request_priv->active_jobs); ++idx) {
    render_job_t* job = girara_list_nth(request_priv->active_jobs, idx);
    job->aborted      = true;
  }
  g_mutex_unlock(&request_priv->jobs_mutex);
}

void
zathura_render_request_update_view_time(ZathuraRenderRequest* request)
{
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);
  request_priv->last_view_time = g_get_real_time();
}

/* render job */

static void
remove_job_and_free(render_job_t* job)
{
  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(job->request);

  g_mutex_lock(&request_priv->jobs_mutex);
  girara_list_remove(request_priv->active_jobs, job);
  g_mutex_unlock(&request_priv->jobs_mutex);

  g_object_unref(job->request);
  g_free(job);
}

typedef struct emit_completed_signal_s
{
  render_job_t* job;
  cairo_surface_t* surface;
} emit_completed_signal_t;

static gboolean
emit_completed_signal(void* data)
{
  emit_completed_signal_t* ecs = data;
  render_job_t* job = ecs->job;
  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(job->request);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(request_priv->renderer);

  if (priv->about_to_close == false && job->aborted == false) {
    /* emit the signal */
    girara_debug("Emitting signal for page %d",
        zathura_page_get_index(request_priv->page) + 1);
    g_signal_emit(job->request, request_signals[REQUEST_COMPLETED], 0, ecs->surface);
  } else {
    girara_debug("Rendering of page %d aborted",
        zathura_page_get_index(request_priv->page) + 1);
  }
  /* mark the request as done */
  remove_job_and_free(job);

  /* clean up the data */
  cairo_surface_destroy(ecs->surface);
  g_free(ecs);

  return FALSE;
}

/* Returns the maximum possible saturation for given h and l.
   Assumes that l is in the interval l1, l2 and corrects the value to
   force u=0 on l1 and l2 */
static double
colorumax(const double h[3], double l, double l1, double l2)
{
  if (fabs(h[0]) <= DBL_EPSILON && fabs(h[1]) <= DBL_EPSILON && fabs(h[2]) <= DBL_EPSILON) {
    return 0;
  }

  const double lv = (l - l1) / (l2 - l1);    /* Remap l to the whole interval [0,1] */
  double u = DBL_MAX;
  double v = DBL_MAX;
  for (unsigned int k = 0; k < 3; ++k) {
    if (h[k] > DBL_EPSILON) {
      u = fmin(fabs((1-l)/h[k]), u);
      v = fmin(fabs((1-lv)/h[k]), v);
    } else if (h[k] < -DBL_EPSILON) {
      u = fmin(fabs(l/h[k]), u);
      v = fmin(fabs(lv/h[k]), v);
    }
  }

  /* rescale v according to the length of the interval [l1, l2] */
  v = fabs(l2 - l1) * v;

  /* forces the returned value to be 0 on l1 and l2, trying not to distort colors too much */
  return fmin(u, v);
}

static void
recolor(ZathuraRendererPrivate* priv, zathura_page_t* page, unsigned int page_width, 
        unsigned int page_height, cairo_surface_t* surface)
{
  /* uses a representation of a rgb color as follows:
     - a lightness scalar (between 0,1), which is a weighted average of r, g, b,
     - a hue vector, which indicates a radian direction from the grey axis,
       inside the equal lightness plane.
     - a saturation scalar between 0,1. It is 0 when grey, 1 when the color is
       in the boundary of the rgb cube.
  */

  /* TODO: split handling of image handling off
   * Ideally we would create a mask surface for the location of the images and
   * we would blit the recolored and unmodified surfaces together to get the
   * same effect.
   */

  cairo_surface_flush(surface);

  const int rowstride  = cairo_image_surface_get_stride(surface);
  unsigned char* image = cairo_image_surface_get_data(surface);

  /* RGB weights for computing lightness. Must sum to one */
  static const double a[] = {0.30, 0.59, 0.11};

  const GdkRGBA rgb1 = priv->recolor.dark;
  const GdkRGBA rgb2 = priv->recolor.light;

  const double l1 = a[0]*rgb1.red + a[1]*rgb1.green + a[2]*rgb1.blue;
  const double l2 = a[0]*rgb2.red + a[1]*rgb2.green + a[2]*rgb2.blue;
  const double negalph1 = 1. - rgb1.alpha;
  const double negalph2 = 1. - rgb2.alpha;

  const double rgb_diff[] = {
    rgb2.red - rgb1.red,
    rgb2.green - rgb1.green,
    rgb2.blue - rgb1.blue
  };

  const double h1[3] = {
    rgb1.red*rgb1.alpha - l1,
    rgb1.green*rgb1.alpha - l1,
    rgb1.blue*rgb1.alpha - l1,
  };

  const double h2[3] = {
    rgb2.red*rgb2.alpha - l2,
    rgb2.green*rgb2.alpha - l2,
    rgb2.blue*rgb2.alpha - l2,
  };

  /* Decide if we can use the older, faster formulas */
  const bool fast_formula = (!priv->recolor.hue || (
        fabs(rgb1.red - rgb1.blue) < DBL_EPSILON && fabs(rgb1.red - rgb1.green) < DBL_EPSILON &&
        fabs(rgb2.red - rgb2.blue) < DBL_EPSILON && fabs(rgb2.red - rgb2.green) < DBL_EPSILON
      )) && (rgb1.alpha >= 1. - DBL_EPSILON && rgb2.alpha >= 1. - DBL_EPSILON);

  girara_list_t* images     = NULL;
  girara_list_t* rectangles = NULL;
  bool found_images         = false;

  /* If in reverse video mode retrieve images */
  if (priv->recolor.reverse_video == true) {
    images = zathura_page_images_get(page, NULL);
    found_images = (images != NULL);

    rectangles = girara_list_new();
    if (rectangles == NULL) {
      found_images = false;
      girara_warning("Failed to retrieve images.");
    }

    if (found_images == true) {
      /* Get images bounding boxes */
      for (size_t idx = 0; idx != girara_list_size(images); ++idx) {
        zathura_image_t* image_it = girara_list_nth(images, idx);
        zathura_rectangle_t* rect = g_try_malloc(sizeof(zathura_rectangle_t));
        if (rect == NULL) {
          break;
        }
        *rect = recalc_rectangle(page, image_it->position);
        girara_list_append(rectangles, rect);
      }
    }
  }

  for (unsigned int y = 0; y < page_height; y++) {
    unsigned char* data = image + y * rowstride;

    for (unsigned int x = 0; x < page_width; x++, data += 4) {
      /* Check if the pixel belongs to an image when in reverse video mode*/
      if (priv->recolor.reverse_video == true && found_images == true) {
        bool inside_image = false;
        for (size_t idx = 0; idx != girara_list_size(rectangles); ++idx) {
          zathura_rectangle_t* rect_it = girara_list_nth(rectangles, idx);
          if (rect_it->x1 <= x && rect_it->x2 >= x && rect_it->y1 <= y && rect_it->y2 >= y) {
            inside_image = true;
            break;
          }
        }
        /* If it's inside and image don't recolor */
        if (inside_image == true) {
          /* It is not guaranteed that the pixel is already opaque. */
          data[3] = 255;
          continue;
        }
      }

      /* Careful. data color components blue, green, red. */
      const double rgb[3] = {
        data[2] / 255.,
        data[1] / 255.,
        data[0] / 255.
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
        const double u = colorumax(h, l, 0, 1);
        const double s = fabs(u) > DBL_EPSILON ? 1.0 / u : 0.0;

        /* Interpolates lightness between light and dark colors. white goes to
         * light, and black goes to dark. */
        l = l * (l2 - l1) + l1;

        const double su = s * colorumax(h, l, l1, l2);

        if (fast_formula) {
          data[3] = 255;
          data[2] = (unsigned char)round(255.*(l + su * h[0]));
          data[1] = (unsigned char)round(255.*(l + su * h[1]));
          data[0] = (unsigned char)round(255.*(l + su * h[2]));
        } else {
          /* Mix lightcolor, darkcolor and the original color, according to the
           * minimal and maximal channel of the original color */
          const double tr1 = (1. - fmax(fmax(rgb[0], rgb[1]), rgb[2]));
          const double tr2 = fmin(fmin(rgb[0], rgb[1]), rgb[2]);
          data[3] = (unsigned char)round(255.*(1. - tr1*negalph1 - tr2*negalph2));
          data[2] = (unsigned char)round(255.*fmin(1, fmax(0, tr1*h1[0] + tr2*h2[0] + (l + su * h[0]))));
          data[1] = (unsigned char)round(255.*fmin(1, fmax(0, tr1*h1[1] + tr2*h2[1] + (l + su * h[1]))));
          data[0] = (unsigned char)round(255.*fmin(1, fmax(0, tr1*h1[2] + tr2*h2[2] + (l + su * h[2]))));
        }
      } else {
        /* linear interpolation between dark and light with color ligtness as
         * a parameter */
        if (fast_formula) {
          data[3] = 255;
          data[2] = (unsigned char)round(255.*(l * rgb_diff[0] + rgb1.red));
          data[1] = (unsigned char)round(255.*(l * rgb_diff[1] + rgb1.green));
          data[0] = (unsigned char)round(255.*(l * rgb_diff[2] + rgb1.blue));
        } else {
          const double f1 = 1. - (1. - fmax(fmax(rgb[0], rgb[1]), rgb[2]))*negalph1;
          const double f2 = fmin(fmin(rgb[0], rgb[1]), rgb[2])*negalph2;
          data[3] = (unsigned char)round(255.*(f1 - f2));
          data[2] = (unsigned char)round(255.*(l * rgb_diff[0] - f2*rgb2.red + f1*rgb1.red));
          data[1] = (unsigned char)round(255.*(l * rgb_diff[1] - f2*rgb2.green + f1*rgb1.green));
          data[0] = (unsigned char)round(255.*(l * rgb_diff[2] - f2*rgb2.blue + f1*rgb1.blue));
        }
      }
    }
  }

  if (images != NULL) {
    girara_list_free(images);
  }
  if (rectangles != NULL) {
    girara_list_free(rectangles);
  }

  cairo_surface_mark_dirty(surface);
}

static bool
invoke_completed_signal(render_job_t* job, cairo_surface_t* surface)
{
  emit_completed_signal_t* ecs = g_try_malloc0(sizeof(emit_completed_signal_t));
  if (ecs == NULL) {
    return false;
  }

  ecs->job     = job;
  ecs->surface = cairo_surface_reference(surface);

  /* emit signal from the main context, i.e. the main thread */
  g_main_context_invoke(NULL, emit_completed_signal, ecs);
  return true;
}

static bool
render_to_cairo_surface(cairo_surface_t* surface, zathura_page_t* page, ZathuraRenderer* renderer, double real_scale)
{
  cairo_t* cairo = cairo_create(surface);
  if (cairo_status(cairo) != CAIRO_STATUS_SUCCESS) {
    return false;
  }

  cairo_save(cairo);
  cairo_set_source_rgb(cairo, 1, 1, 1);
  cairo_paint(cairo);
  cairo_restore(cairo);

  /* apply scale (used by e.g. Poppler as pixels per point) */
  if (fabs(real_scale - 1.0f) > FLT_EPSILON) {
    cairo_scale(cairo, real_scale, real_scale);
  }

  zathura_renderer_lock(renderer);
  const int err = zathura_page_render(page, cairo, false);
  zathura_renderer_unlock(renderer);
  cairo_destroy(cairo);

  return err == ZATHURA_ERROR_OK;
}

static bool
render(render_job_t* job, ZathuraRenderRequest* request, ZathuraRenderer* renderer)
{
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);
  zathura_page_t* page = request_priv->page;

  /* create cairo surface */
  unsigned int page_width  = 0;
  unsigned int page_height = 0;

  /* page size in points */
  zathura_document_t* document = zathura_page_get_document(page);
  const double height = zathura_page_get_height(page);
  const double width = zathura_page_get_width(page);

  zathura_device_factors_t device_factors = { 0 };
  double real_scale = 1;
  if (request_priv->render_plain == false) {
    /* page size in user pixels based on document zoom: if PPI information is
     * correct, 100% zoom will result in 72 documents points per inch of screen
     * (i.e. document size on screen matching the physical paper size). */
    real_scale = page_calc_height_width(document, height, width,
                                        &page_height, &page_width, false);

    device_factors = zathura_document_get_device_factors(document);
    page_width *= device_factors.x;
    page_height *= device_factors.y;
  } else {
    page_width = width;
    page_height = height;
  }

  cairo_format_t format;
  if (priv->recolor.enabled) {
    format = CAIRO_FORMAT_ARGB32;
  }
  else {
    format = CAIRO_FORMAT_RGB24;
  }
  cairo_surface_t* surface = cairo_image_surface_create(format,
      page_width, page_height);
  if (request_priv->render_plain == false) {
    cairo_surface_set_device_scale(surface, device_factors.x, device_factors.y);
  }

  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(surface);
    return false;
  }

  /* actually render to the surface */
  if (!render_to_cairo_surface(surface, page, renderer, real_scale)) {
    cairo_surface_destroy(surface);
    return false;
  }

  /* before recoloring, check if we've been aborted */
  if (priv->about_to_close == true || job->aborted == true) {
    girara_debug("Rendering of page %d aborted",
                 zathura_page_get_index(request_priv->page) + 1);
    remove_job_and_free(job);
    cairo_surface_destroy(surface);
    return true;
  }

  /* recolor */
  if (request_priv->render_plain == false && priv->recolor.enabled == true) {
    recolor(priv, page, page_width, page_height, surface);
  }

  if (!invoke_completed_signal(job, surface)) {
    cairo_surface_destroy(surface);
    return false;
  }

  cairo_surface_destroy(surface);

  return true;
}

static void
render_job(void* data, void* user_data)
{
  render_job_t* job = data;
  ZathuraRenderRequest* request = job->request;
  ZathuraRenderer* renderer = user_data;
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  if (priv->about_to_close == true || job->aborted == true) {
    /* back out early */
    remove_job_and_free(job);
    return;
  }

  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);
  girara_debug("Rendering page %d ...",
      zathura_page_get_index(request_priv->page) + 1);
  if (render(job, request, renderer) != true) {
    girara_error("Rendering failed (page %d)\n",
        zathura_page_get_index(request_priv->page) + 1);
    remove_job_and_free(job);
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
  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    zathura_page_t* page = zathura_document_get_page(zathura->document,
        page_id);
    unsigned int page_height = 0, page_width = 0;
    const double height = zathura_page_get_height(page);
    const double width = zathura_page_get_width(page);
    page_calc_height_width(zathura->document, height, width, &page_height, &page_width, true);

    girara_debug("Queuing resize for page %u to %u x %u (%0.2f x %0.2f).", page_id, page_width, page_height, width, height);
    GtkWidget* widget = zathura_page_get_widget(zathura, page);
    if (widget != NULL) {
      gtk_widget_set_size_request(widget, page_width, page_height);
      gtk_widget_queue_resize(widget);
    }
  }
}

static gint
render_thread_sort(gconstpointer a, gconstpointer b, gpointer UNUSED(data))
{
  if (a == NULL || b == NULL) {
    return 0;
  }

  const render_job_t* job_a = a;
  const render_job_t* job_b = b;
  if (job_a->aborted == job_b->aborted) {
    ZathuraRenderRequestPrivate* priv_a = zathura_render_request_get_instance_private(job_a->request);
    ZathuraRenderRequestPrivate* priv_b = zathura_render_request_get_instance_private(job_b->request);

    return priv_a->last_view_time < priv_b->last_view_time ? -1 :
        (priv_a->last_view_time > priv_b->last_view_time ? 1 : 0);
  }

  /* sort aborted entries earlier so that they are thrown out of the queue */
  return job_a->aborted ? 1 : -1;
}

/* cache functions */

static bool
page_cache_is_cached(ZathuraRenderer* renderer, unsigned int page_index)
{
  g_return_val_if_fail(renderer != NULL, false);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);

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
  ZathuraRenderRequest* request = (void*) req;
  const unsigned int page_index = *((const int*)data);

  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
  if (zathura_page_get_index(priv->page) == page_index) {
    return 0;
  }
  return 1;
}

static ssize_t
page_cache_lru_invalidate(ZathuraRenderer* renderer)
{
  g_return_val_if_fail(renderer != NULL, -1);
  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  g_return_val_if_fail(priv->page_cache.size != 0, -1);

  ssize_t lru_index = 0;
  gint64 lru_view_time = G_MAXINT64;
  ZathuraRenderRequest* request = NULL;
  for (size_t i = 0; i < priv->page_cache.size; ++i) {
    ZathuraRenderRequest* tmp_request = girara_list_find(priv->requests,
        find_request_by_page_index, &priv->page_cache.cache[i]);
    g_return_val_if_fail(tmp_request != NULL, -1);
    ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(tmp_request);

    if (request_priv->last_view_time < lru_view_time) {
      lru_view_time = request_priv->last_view_time;
      lru_index = i;
      request = tmp_request;
    }
  }

  ZathuraRenderRequestPrivate* request_priv = zathura_render_request_get_instance_private(request);

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

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
  *result = priv->page_cache.num_cached_pages == priv->page_cache.size;

  return true;
}

static void
page_cache_invalidate_all(ZathuraRenderer* renderer)
{
  g_return_if_fail(ZATHURA_IS_RENDERER(renderer));

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
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

  ZathuraRendererPrivate* priv = zathura_renderer_get_instance_private(renderer);
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

void zathura_render_request_set_render_plain(ZathuraRenderRequest* request,
    bool render_plain)
{
  g_return_if_fail(ZATHURA_IS_RENDER_REQUEST(request));

  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
  priv->render_plain =render_plain;
}

bool
zathura_render_request_get_render_plain(ZathuraRenderRequest* request)
{
  g_return_val_if_fail(ZATHURA_IS_RENDER_REQUEST(request), false);

  ZathuraRenderRequestPrivate* priv = zathura_render_request_get_instance_private(request);
  return priv->render_plain;
}

