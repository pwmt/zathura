/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_INPUT_HISTORY_H
#define GIRARA_INPUT_HISTORY_H

#include <glib-object.h>
#include "types.h"

struct girara_input_history_io_interface_s {
  GTypeInterface parent_iface;

  /* interface methods */

  /**
   * Write a line of input to the input history storage.
   *
   * @param io a GiraraInputHistoryIO object
   * @param input the input
   */
  void (*append)(GiraraInputHistoryIO* io, const char* input);

  /**
   * Read all items from the input history storage.
   *
   * @param io a GiraraInputHistoryIO object
   * @returns a list of inputs
   */
  girara_list_t* (*read)(GiraraInputHistoryIO* io);

  /* reserved for further methods */
  void (*reserved1)(void);
  void (*reserved2)(void);
  void (*reserved3)(void);
  void (*reserved4)(void);
};

#define GIRARA_TYPE_INPUT_HISTORY_IO (girara_input_history_io_get_type())
#define GIRARA_INPUT_HISTORY_IO(obj)                                                                                   \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GIRARA_TYPE_INPUT_HISTORY_IO, GiraraInputHistoryIO))
#define GIRARA_IS_INPUT_HISTORY_IO(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GIRARA_TYPE_INPUT_HISTORY_IO))
#define GIRARA_INPUT_HISTORY_IO_GET_INTERFACE(obj)                                                                     \
  (G_TYPE_INSTANCE_GET_INTERFACE((obj), GIRARA_TYPE_INPUT_HISTORY_IO, GiraraInputHistoryIOInterface))

GType girara_input_history_io_get_type(void) G_GNUC_CONST ;

void girara_input_history_io_append(GiraraInputHistoryIO* io, const char* input) ;

girara_list_t* girara_input_history_io_read(GiraraInputHistoryIO* io) ;

struct girara_input_history_s {
  GObject parent;
};

struct girara_input_history_class_s {
  GObjectClass parent_class;

  /* methods */

  /**
   * Append a new line of input. If the io property is set, the input will
   * be passed on to @ref girara_input_history_io_append.
   *
   * @param history an input history instance
   * @param input the input
   */
  void (*append)(GiraraInputHistory* history, const char* input);

  /**
   * Get a list of all the inputs stored.
   *
   * @param history an input history instance
   * @returns a list containing all inputs
   */
  girara_list_t* (*list)(GiraraInputHistory* history);

  /**
   * Get the "next" input from the history
   *
   * @param history an input history instance
   * @param current_input input used to find the "next" input
   * @returns "next" input
   */
  const char* (*next)(GiraraInputHistory* history, const char* current_input);

  /**
   * Get the "previous" input from the history
   *
   * @param history an input history instance
   * @param current_input input used to find the "next" input
   * @returns "previous" input
   */
  const char* (*previous)(GiraraInputHistory* history, const char* current_input);

  /**
   * Reset state of the input history, i.e reset any information used to
   * determine the next input. If the io property is set, the history will be
   * re-read with @ref girara_input_history_io_read.
   *
   * @param history an input history instance
   */
  void (*reset)(GiraraInputHistory* history);

  /* reserved for further methods */
  void (*reserved1)(void);
  void (*reserved2)(void);
  void (*reserved3)(void);
  void (*reserved4)(void);
};

#define GIRARA_TYPE_INPUT_HISTORY (girara_input_history_get_type())
#define GIRARA_INPUT_HISTORY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GIRARA_TYPE_INPUT_HISTORY, GiraraInputHistory))
#define GIRARA_INPUT_HISTORY_CLASS(obj)                                                                                \
  (G_TYPE_CHECK_CLASS_CAST((obj), GIRARA_TYPE_INPUT_HISTORY, GiraraInputHistoryClass))
#define GIRARA_IS_INPUT_HISTORY(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GIRARA_TYPE_INPUT_HISTORY))
#define GIRARA_IS_INPUT_HISTORY_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), GIRARA_TYPE_INPUT_HISTORY))
#define GIRARA_INPUT_HISTORY_GET_CLASS(obj)                                                                            \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GIRARA_TYPE_INPUT_HISTORY, GiraraInputHistoryClass))

/**
 * Returns the type of the input history.
 *
 * @return the type
 */
GType girara_input_history_get_type(void) G_GNUC_CONST ;

/**
 * Create new input history object.
 *
 * @param io a GiraraInputHistoryIO instance, may be NULL
 * @returns an input history object
 */
GiraraInputHistory* girara_input_history_new(GiraraInputHistoryIO* io) ;

/**
 * Append a new line of input.
 *
 * @param history an input history instance
 * @param input the input
 */
void girara_input_history_append(GiraraInputHistory* history, const char* input) ;

/**
 * Get the "next" input from the history
 *
 * @param history an input history instance
 * @param current_input input used to find the "next" input
 * @returns "next" input
 */
const char* girara_input_history_next(GiraraInputHistory* history, const char* current_input) ;

/**
 * Get the "previous" input from the history
 *
 * @param history an input history instance
 * @param current_input input used to find the "next" input
 * @returns "previous" input
 */
const char* girara_input_history_previous(GiraraInputHistory* history, const char* current_input) ;

/**
 * Reset state of the input history
 *
 * @param history an input history instance
 */
void girara_input_history_reset(GiraraInputHistory* history) ;

/**
 * Get a list of all the inputs stored.
 *
 * @param history an input history instance
 * @returns a list containing all inputs
 */
girara_list_t* girara_input_history_list(GiraraInputHistory* history) ;

#endif
