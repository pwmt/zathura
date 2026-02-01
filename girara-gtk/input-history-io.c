/* SPDX-License-Identifier: Zlib */

#include "input-history.h"
#include <girara/macros.h>

G_DEFINE_INTERFACE(GiraraInputHistoryIO, girara_input_history_io, G_TYPE_OBJECT)

static void girara_input_history_io_default_init(GiraraInputHistoryIOInterface* GIRARA_UNUSED(iface)) {}

void girara_input_history_io_append(GiraraInputHistoryIO* io, const char* input) {
  g_return_if_fail(GIRARA_IS_INPUT_HISTORY_IO(io) == true);
  GIRARA_INPUT_HISTORY_IO_GET_INTERFACE(io)->append(io, input);
}

girara_list_t* girara_input_history_io_read(GiraraInputHistoryIO* io) {
  g_return_val_if_fail(GIRARA_IS_INPUT_HISTORY_IO(io) == true, NULL);
  return GIRARA_INPUT_HISTORY_IO_GET_INTERFACE(io)->read(io);
}
