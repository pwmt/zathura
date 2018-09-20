/* See LICENSE file for license and copyright information */

#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stdlib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <girara/types.h>
#include "types.h"

typedef struct zathura_renderer_class_s ZathuraRendererClass;

struct zathura_renderer_s
{
  GObject parent;
};

struct zathura_renderer_class_s
{
  GObjectClass parent_class;
};

#define ZATHURA_TYPE_RENDERER \
  (zathura_renderer_get_type())
#define ZATHURA_RENDERER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_RENDERER, ZathuraRenderer))
#define ZATHURA_RENDERER_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_RENDERER, ZathuraRendererClass))
#define ZATHURA_IS_RENDERER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_RENDERER))
#define ZATHURA_IS_RENDERER_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_RENDERER))
#define ZATHURA_RENDERER_GET_CLASS \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_RENDERER, ZathuraRendererClass))

/**
 * Returns the type of the renderer.
 * @return the type
 */
GType zathura_renderer_get_type(void) G_GNUC_CONST;
/**
 * Create a renderer.
 * @return a renderer object
 */
ZathuraRenderer* zathura_renderer_new(size_t cache_size);

/**
 * Return whether recoloring is enabled.
 * @param renderer a renderer object
 * @returns true if recoloring is enabled, false otherwise
 */
bool zathura_renderer_recolor_enabled(ZathuraRenderer* renderer);
/**
 * Enable/disable recoloring.
 * @param renderer a renderer object
 * @param enable whether to enable or disable recoloring
 */
void zathura_renderer_enable_recolor(ZathuraRenderer* renderer, bool enable);
/**
 * Return whether hue should be preserved while recoloring.
 * @param renderer a renderer object
 * @returns true if hue should be preserved, false otherwise
 */
bool zathura_renderer_recolor_hue_enabled(ZathuraRenderer* renderer);
/**
 * Enable/disable preservation of hue while recoloring.
 * @param renderer a renderer object
 * @param enable whether to enable or disable hue preservation
 */
void zathura_renderer_enable_recolor_hue(ZathuraRenderer* renderer,
    bool enable);
/**
 * Return whether images should be recolored while recoloring.
 * @param renderer a renderer object
 * @returns true if images should be recolored, false otherwise
 */
bool zathura_renderer_recolor_reverse_video_enabled(ZathuraRenderer* renderer);
/**
 * Enable/disable recoloring of images while recoloring.
 * @param renderer a renderer object
 * @param enable or disable images recoloring
 */
void zathura_renderer_enable_recolor_reverse_video(ZathuraRenderer* renderer,
    bool enable);
/**
 * Set light and dark colors for recoloring.
 * @param renderer a renderer object
 * @param light light color
 * @param dark dark color
 */
void zathura_renderer_set_recolor_colors(ZathuraRenderer* renderer,
    const GdkRGBA* light, const GdkRGBA* dark);
/**
 * Set light and dark colors for recoloring.
 * @param renderer a renderer object
 * @param light light color
 * @param dark dark color
 */
void zathura_renderer_set_recolor_colors_str(ZathuraRenderer* renderer,
    const char* light, const char* dark);
/**
 * Get light and dark colors for recoloring.
 * @param renderer a renderer object
 * @param light light color
 * @param dark dark color
 */
void zathura_renderer_get_recolor_colors(ZathuraRenderer* renderer,
    GdkRGBA* light, GdkRGBA* dark);
/**
 * Stop rendering.
 * @param renderer a render object
 */
void zathura_renderer_stop(ZathuraRenderer* renderer);

/**
 * Lock the render thread. This is useful if you want to render on your own (e.g
 * for printing).
 *
 * @param renderer renderer object
 */
void zathura_renderer_lock(ZathuraRenderer* renderer);

/**
 * Unlock the render thread.
 *
 * @param renderer renderer object.
 */
void zathura_renderer_unlock(ZathuraRenderer* renderer);

/**
 * Add a page to the page cache.
 *
 * @param renderer renderer object.
 * @param page_index The index of the page to be cached.
 */
void zathura_renderer_page_cache_add(ZathuraRenderer* renderer,
    unsigned int page_index);


typedef struct zathura_render_request_class_s ZathuraRenderRequestClass;

struct zathura_render_request_s
{
  GObject parent;
};

struct zathura_render_request_class_s
{
  GObjectClass parent_class;
};

#define ZATHURA_TYPE_RENDER_REQUEST \
  (zathura_render_request_get_type())
#define ZATHURA_RENDER_REQUEST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_RENDER_REQUEST, \
                              ZathuraRenderRequest))
#define ZATHURA_RENDER_REQUEST_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_RENDER_REQUEST, \
                           ZathuraRenderRequestClass))
#define ZATHURA_IS_RENDER_REQUEST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_RENDER_REQUEST))
#define ZATHURA_IS_RENDER_REQUEST_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_RENDER_REQUEST))
#define ZATHURA_RENDER_REQUEST_GET_CLASS \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_RENDER_REQUEST, \
                             ZathuraRenderRequestClass))

/**
 * Returns the type of the render request.
 * @return the type
 */
GType zathura_page_render_info_get_type(void) G_GNUC_CONST;
/**
 * Create a render request object
 * @param renderer a renderer object
 * @param page the page to be displayed
 * @returns render request object
 */
ZathuraRenderRequest* zathura_render_request_new(ZathuraRenderer* renderer,
    zathura_page_t* page);

/**
 * Add a page to the render thread list that should be rendered.
 *
 * @param request request object of the page that should be renderer
 * @param last_view_time last view time of the page
 */
void zathura_render_request(ZathuraRenderRequest* request,
    gint64 last_view_time);

/**
 * Abort an existing render request.
 *
 * @param request request that should be aborted
 */
void zathura_render_request_abort(ZathuraRenderRequest* request);

/**
 * Update the time the page associated to the render request has been viewed the
 * last time.
 *
 * @param request request that should be updated
 */
void zathura_render_request_update_view_time(ZathuraRenderRequest* request);

/**
 * Set "plain" rendering mode, i.e. disabling scaling, recoloring, etc.
 * @param request request that should be updated
 * @param render_plain "plain" rendering setting
 */
void zathura_render_request_set_render_plain(ZathuraRenderRequest* request,
    bool render_plain);

/**
 * Get "plain" rendering mode, i.e. disabling scaling, recoloring, etc.
 * @param request request that should be updated
 * @returns "plain" rendering setting
 */
bool zathura_render_request_get_render_plain(ZathuraRenderRequest* request);

/**
 * This function is used to unmark all pages as not rendered. This should
 * be used if all pages should be rendered again (e.g.: the zoom level or the
 * colors have changed)
 *
 * @param zathura Zathura object
 */
void render_all(zathura_t* zathura);

#endif // RENDER_H
