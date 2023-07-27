/* SPDX-License-Identifier: Zlib */

#include "print.h"
#include "document.h"
#include "render.h"
#include "page.h"

#include <girara/utils.h>
#include <girara/statusbar.h>
#include <girara/session.h>
#include <glib/gi18n.h>

static void
cb_print_end(GtkPrintOperation* UNUSED(print_operation), GtkPrintContext*
             UNUSED(context), zathura_t* zathura)
{
  if (zathura == NULL || zathura->ui.session == NULL ||
      zathura->document == NULL) {
    return;
  }

  char* file_path = get_formatted_filename(zathura, true);
  girara_statusbar_item_set_text(zathura->ui.session,
                                 zathura->ui.statusbar.file, file_path);
  g_free(file_path);
}

static bool
draw_page_cairo(cairo_t* cairo, zathura_t* zathura, zathura_page_t* page)
{
  /* Try to render the page without a temporary surface. This only works with
   * plugins that support rendering to any surface.  */
  zathura_renderer_lock(zathura->sync.render_thread);
  const int err = zathura_page_render(page, cairo, true);
  zathura_renderer_unlock(zathura->sync.render_thread);

  return err == ZATHURA_ERROR_OK;
}

static bool
draw_page_image(cairo_t* cairo, GtkPrintContext* context, zathura_t* zathura,
                zathura_page_t* page)
{
  /* Try to render the page on a temporary image surface. */
  const double width = gtk_print_context_get_width(context);
  const double height = gtk_print_context_get_height(context);

  const double scale_height = 5;
  const double scale_width  = 5;

  /* Render to a surface that is 5 times larger to workaround quality issues. */
  const double page_height = zathura_page_get_height(page) * scale_height;
  const double page_width  = zathura_page_get_width(page) * scale_width;
  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, page_width, page_height);
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    return false;
  }

  cairo_t* temp_cairo = cairo_create(surface);
  if (cairo_status(temp_cairo) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(surface);
    return false;
  }

  /* Draw a white background. */
  cairo_save(temp_cairo);
  cairo_set_source_rgb(temp_cairo, 1, 1, 1);
  cairo_rectangle(temp_cairo, 0, 0, page_width, page_height);
  cairo_fill(temp_cairo);
  cairo_restore(temp_cairo);

  /* Render the page to the temporary surface */
  zathura_renderer_lock(zathura->sync.render_thread);
  const int err = zathura_page_render(page, temp_cairo, true);
  zathura_renderer_unlock(zathura->sync.render_thread);
  if (err != ZATHURA_ERROR_OK) {
    cairo_destroy(temp_cairo);
    cairo_surface_destroy(surface);
    return false;
  }

  /* Rescale the page and keep the aspect ratio */
  const gdouble scale = MIN(width / page_width, height / page_height);
  cairo_scale(cairo, scale, scale);

  /* Blit temporary surface to original cairo object. */
  cairo_set_source_surface(cairo, surface, 0.0, 0.0);
  cairo_paint(cairo);
  cairo_destroy(temp_cairo);
  cairo_surface_destroy(surface);

  return true;
}

static void
cb_print_draw_page(GtkPrintOperation* print_operation, GtkPrintContext*
                   context, gint page_number, zathura_t* zathura)
{
  if (context == NULL || zathura == NULL || zathura->document == NULL ||
      zathura->ui.session == NULL || zathura->ui.statusbar.file == NULL) {
    gtk_print_operation_cancel(print_operation);
    return;
  }

  /* Update statusbar. */
  char* tmp = g_strdup_printf(_("Printing page %d ..."), page_number);
  girara_statusbar_item_set_text(zathura->ui.session,
                                 zathura->ui.statusbar.file, tmp);
  g_free(tmp);

  /* Get the page and cairo handle.  */
  zathura_page_t* page = zathura_document_get_page(zathura->document, page_number);
  cairo_t* cairo       = gtk_print_context_get_cairo_context(context);
  if (cairo_status(cairo) != CAIRO_STATUS_SUCCESS || page == NULL) {
    gtk_print_operation_cancel(print_operation);
    return;
  }

  girara_debug("printing page %d ...", page_number);
  if (draw_page_cairo(cairo, zathura, page) == true) {
    return;
  }

  girara_debug("printing page %d (fallback) ...", page_number);
  if (draw_page_image(cairo, context, zathura, page) == false) {
    gtk_print_operation_cancel(print_operation);
  }
}

static void
cb_print_request_page_setup(GtkPrintOperation* UNUSED(print_operation),
                            GtkPrintContext* UNUSED(context), gint page_number,
                            GtkPageSetup* setup, zathura_t* zathura)
{
  if (zathura == NULL || zathura->document == NULL) {
    return;
  }

  zathura_page_t* page = zathura_document_get_page(zathura->document, page_number);
  double width  = zathura_page_get_width(page);
  double height = zathura_page_get_height(page);

  if (width > height) {
    gtk_page_setup_set_orientation(setup, GTK_PAGE_ORIENTATION_LANDSCAPE);
  } else {
    gtk_page_setup_set_orientation(setup, GTK_PAGE_ORIENTATION_PORTRAIT);
  }
}

void
print(zathura_t* zathura)
{
  g_return_if_fail(zathura           != NULL);
  g_return_if_fail(zathura->document != NULL);

  GtkPrintOperation* print_operation = gtk_print_operation_new();

  /* print operation settings */
  gtk_print_operation_set_job_name(print_operation, zathura_document_get_path(zathura->document));
  gtk_print_operation_set_allow_async(print_operation, TRUE);
  gtk_print_operation_set_n_pages(print_operation, zathura_document_get_number_of_pages(zathura->document));
  gtk_print_operation_set_current_page(print_operation, zathura_document_get_current_page_number(zathura->document));
  gtk_print_operation_set_use_full_page(print_operation, TRUE);

  if (zathura->print.settings != NULL) {
    gtk_print_operation_set_print_settings(print_operation,
                                           zathura->print.settings);
  }

  if (zathura->print.page_setup != NULL) {
    gtk_print_operation_set_default_page_setup(print_operation,
                                               zathura->print.page_setup);
  }
  gtk_print_operation_set_embed_page_setup(print_operation, TRUE);

  /* print operation signals */
  g_signal_connect(print_operation, "draw-page",          G_CALLBACK(cb_print_draw_page),          zathura);
  g_signal_connect(print_operation, "end-print",          G_CALLBACK(cb_print_end),                zathura);
  g_signal_connect(print_operation, "request-page-setup", G_CALLBACK(cb_print_request_page_setup), zathura);

  /* print */
  GError* error = NULL;
  GtkPrintOperationResult result = gtk_print_operation_run(print_operation,
                                   GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                   GTK_WINDOW(zathura->ui.session->gtk.window), &error);

  if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Printing failed: %s"),
                  error->message);
    g_error_free(error);
  } else if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
    g_clear_object(&zathura->print.settings);
    g_clear_object(&zathura->print.page_setup);

    /* save previous settings */
    zathura->print.settings   = g_object_ref(gtk_print_operation_get_print_settings(print_operation));
    zathura->print.page_setup = g_object_ref(gtk_print_operation_get_default_page_setup(print_operation));
  }

  g_object_unref(print_operation);
}
