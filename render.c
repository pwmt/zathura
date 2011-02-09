#include "render.h"
#include "zathura.h"

void* render_job(void* data);
bool render(zathura_page_t* page);

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

    printf("Rendered %d\n", page->number);

    render(page);
  }

  return NULL;
}

render_thread_t*
render_init(void)
{
  render_thread_t* render_thread = malloc(sizeof(render_thread_t));

  if (!render_thread) {
    goto error_ret;
  }

  /* init */
  render_thread->list   = NULL;
  render_thread->thread = NULL;
  render_thread->cond   = NULL;

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
render(zathura_page_t* page)
{
  g_static_mutex_lock(&(page->lock));
  GtkWidget* image = zathura_page_render(page);

  if (!image) {
    g_static_mutex_unlock(&(page->lock));
    printf("error: rendering failed\n");
    return false;
  }

  /* add new page */
  GList* list       = gtk_container_get_children(GTK_CONTAINER(Zathura.UI.page_view));
  GtkWidget* widget = (GtkWidget*) g_list_nth_data(list, page->number);
  g_list_free(list);

  if (!widget) {
    g_static_mutex_unlock(&(page->lock));
    printf("error: page container does not exist\n");
    g_object_unref(image);
    return false;
  }

  /* child packaging information */
  gboolean expand;
  gboolean fill;
  guint padding;
  GtkPackType pack_type;

  gtk_box_query_child_packing(GTK_BOX(Zathura.UI.page_view), widget, &expand, &fill, &padding, &pack_type);

  /* delete old widget */
  gtk_container_remove(GTK_CONTAINER(Zathura.UI.page_view), widget);

  /* add new widget */
  gtk_box_pack_start(GTK_BOX(Zathura.UI.page_view), image, TRUE,  TRUE, 0);

  /* set old packaging values */
  gtk_box_set_child_packing(GTK_BOX(Zathura.UI.page_view), image, expand, fill, padding, pack_type);

  /* reorder child */
  gtk_box_reorder_child(GTK_BOX(Zathura.UI.page_view), image, page->number);
  g_static_mutex_unlock(&(page->lock));

  return true;
}

void
render_all(void)
{
  if (Zathura.document == NULL) {
    return;
  }

  /* unmark all pages */
  for (unsigned int page_id = 0; page_id < Zathura.document->number_of_pages; page_id++) {
    Zathura.document->pages[page_id]->rendered = false;
  }

  /* redraw current page */
  GtkAdjustment* view_vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(Zathura.UI.session->gtk.view));
  cb_view_vadjustment_value_changed(view_vadjustment, NULL);
}
