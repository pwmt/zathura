/* See LICENSE file for license and copyright information */

#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stdlib.h>
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
GType zathura_renderer_get_type(void);
/**
 * Create a page view widget.
 * @param zathura the zathura instance
 * @param page the page to be displayed
 * @return a page view widget
 */
ZathuraRenderer* zathura_renderer_new(zathura_t* zathura);

bool zathura_renderer_recolor_enabled(ZathuraRenderer* renderer);
void zathura_renderer_enable_recolor(ZathuraRenderer* renderer, bool enable);
bool zathura_renderer_recolor_hue_enabled(ZathuraRenderer* renderer);
void zathura_renderer_enable_recolor_hue(ZathuraRenderer* renderer,
    bool enable);
void zathura_renderer_set_recolor_colors(ZathuraRenderer* renderer,
    GdkColor* light, GdkColor* dark);

void zathura_renderer_stop(ZathuraRenderer* renderer);

/**
 * Lock the render thread. This is useful if you want to render on your own (e.g
 * for printing).
 *
 * @param render_thread The render thread object.
 */
void zathura_renderer_lock(ZathuraRenderer* renderer);

/**
 * Unlock the render thread.
 *
 * @param render_thread The render thread object.
 */
void zathura_renderer_unlock(ZathuraRenderer* renderer);



typedef struct zathura_render_request_s ZathuraRenderRequest;
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
 * Returns the type of the renderer.
 * @return the type
 */
GType zathura_page_render_info_get_type(void);
/**
 * Create a page view widget.
 * @param zathura the zathura instance
 * @param page the page to be displayed
 * @return a page view widget
 */
ZathuraRenderRequest* zathura_render_request_new(ZathuraRenderer* renderer,
    zathura_page_t* page);

/**
 * This function is used to add a page to the render thread list
 * that should be rendered.
 *
 * @param request request object of the page that should be renderer
 */
void zathura_render_request(ZathuraRenderRequest* request);

/**
 * Abort an existing render request.
 *
 * @param reqeust request that should be aborted
 */
void zathura_render_request_abort(ZathuraRenderRequest* request);


/**
 * This function is used to unmark all pages as not rendered. This should
 * be used if all pages should be rendered again (e.g.: the zoom level or the
 * colors have changed)
 *
 * @param zathura Zathura object
 */
void render_all(zathura_t* zathura);

#endif // RENDER_H
