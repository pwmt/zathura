#include "render.h"
#include "zathura.h"

bool page_render(zathura_page_t* page)
{
  GtkWidget* image = zathura_page_render(page);
  if(!image) {
    printf("error: rendering failed\n");
    return false;
  }

  /* add new page */
  GList* list       = gtk_container_get_children(GTK_CONTAINER(Zathura.UI.page_view));
  GtkWidget* widget = (GtkWidget*) g_list_nth_data(list, page->number);
  g_list_free(list);

  if(!widget) {
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

  return true;
}
