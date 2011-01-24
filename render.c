#include "render.h"
#include "zathura.h"

void* search(void* data);
bool render(zathura_page_t* page);

void*
search(void* data)
{
  render_thread_t* render_thread = (render_thread_t*) data;

  while(true) {
    g_static_mutex_lock(&(render_thread->lock));

    if(girara_list_size(render_thread->list) <= 0) {
      g_static_mutex_unlock(&(render_thread->lock));
      g_thread_yield();
      continue;
    }

    zathura_page_t* page = (zathura_page_t*) girara_list_nth(render_thread->list, 0);
    girara_list_remove(render_thread->list, page);
    g_static_mutex_unlock(&(render_thread->lock));

    printf("Rendered %d\n", page->number);

    render(page);
  }

  return NULL;
}

render_thread_t*
render_init(void)
{
  render_thread_t* render_thread = malloc(sizeof(render_thread_t));

  if(!render_thread) {
    goto error_ret;
  }

  render_thread->list = girara_list_new();

  if(!render_thread->list) {
    goto error_free;
  }

  render_thread->thread = g_thread_create(search, render_thread, TRUE, NULL);

  if(!render_thread->thread) {
    goto error_free;
  }

  g_static_mutex_init(&(render_thread->lock));

  return render_thread;

error_free:

  free(render_thread);

error_ret:

  return NULL;
}

void
render_free(render_thread_t* render_thread)
{
  if(!render_thread) {
    return;
  }

  girara_list_free(render_thread->list);
  g_static_mutex_free(&(render_thread->lock));
}

bool
render_page(render_thread_t* render_thread, zathura_page_t* page)
{
  if(!render_thread || !page || !render_thread->list) {
    return false;
  }

  g_static_mutex_lock(&(render_thread->lock));
  if(!girara_list_contains(render_thread->list, page)) {
    girara_list_append(render_thread->list, page);
  }
  g_static_mutex_unlock(&(render_thread->lock));

  return true;
}

bool
render(zathura_page_t* page)
{
  g_static_mutex_lock(&(page->lock));
  GtkWidget* image = zathura_page_render(page);

  if(!image) {
    g_static_mutex_unlock(&(page->lock));
    printf("error: rendering failed\n");
    return false;
  }

  /* add new page */
  GList* list       = gtk_container_get_children(GTK_CONTAINER(Zathura.UI.page_view));
  GtkWidget* widget = (GtkWidget*) g_list_nth_data(list, page->number);
  g_list_free(list);

  if(!widget) {
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
