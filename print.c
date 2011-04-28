#include "print.h"
#include "document.h"

void
print(zathura_t* zathura)
{
  g_return_if_fail(zathura           != NULL);
  g_return_if_fail(zathura->document != NULL);

  GtkPrintOperation* print_operation = gtk_print_operation_new();

  /* print operation settings */
  if (zathura->print.settings != NULL) {
    gtk_print_operation_set_print_settings(print_operation, zathura->print.settings);
  }

  if (zathura->print.page_setup != NULL) {
    gtk_print_operation_set_default_page_setup(print_operation, zathura->print.page_setup);
  }
  
  gtk_print_operation_set_show_progress(print_operation, TRUE);
  gtk_print_operation_set_n_pages(print_operation, zathura->document->number_of_pages);
  gtk_print_operation_set_current_page(print_operation, zathura->document->current_page_number);

  /* print operation signals */
  g_signal_connect(print_operation, "draw-page", G_CALLBACK(cb_print_draw_page), zathura);

  /* print */
  GtkPrintOperationResult result = gtk_print_operation_run(print_operation,
      GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, NULL, NULL);

  if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
    /* save previous settings */
    zathura->print.settings   = gtk_print_operation_get_print_settings(print_operation);
    zathura->print.page_setup = gtk_print_operation_get_default_page_setup(print_operation);
  } else if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
    girara_error("Error occured while printing progress");
  }

  g_object_unref(print_operation);
}

void
cb_print_draw_page(GtkPrintOperation* print_operation, GtkPrintContext* context, gint page_number, zathura_t* zathura)
{
  cairo_t* cairo = gtk_print_context_get_cairo_context(context);

  /*zathura_page_t* page = zathura->document->pages[page_number];*/
  /*g_static_mutex_lock(&(page->lock));*/
  /*zathura_image_buffer_t* image_buffer = zathura_page_render(page);*/
  /*g_static_mutex_unlock(&(page->lock));*/

  /*for (unsigned int y = 0; y < image_buffer->height; y++) {*/
    /*unsigned char* src = image_buffer->data + y * image_buffer->rowstride;*/
    /*for (unsigned int x = 0; x < image_buffer->width; x++) {*/
      /*cairo_set_source_rgb(cairo, src[0], src[1], src[2]);*/
      /*cairo_rectangle(cairo, x, y, 1, 1);*/
      /*cairo_fill(cairo);*/
    /*}*/
  /*}*/
}
