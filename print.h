/* See LICENSE file for license and copyright information */

#ifndef PRINT_H
#define PRINT_H

#include "zathura.h"

/**
 * Opens a print dialog to print the current file
 *
 * @param zathura
 */
void print(zathura_t* zathura);

/**
 * Callback that is executed for every page that should be printed
 *
 * @param print_operation Print operation object
 * @param context Print context
 * @param page_number Current page number
 * @param zathura Zathura object
 */
void cb_print_draw_page(GtkPrintOperation* print_operation, GtkPrintContext*
    context, gint page_number, zathura_t* zathura);

/**
 * Emitted after all pages have been rendered
 *
 * @param print_operation Print operation
 * @param context Print context
 * @param zathura Zathura object
 */
void cb_print_end(GtkPrintOperation* print_operation, GtkPrintContext* context,
    zathura_t* zathura);

#endif // PRINT_H
