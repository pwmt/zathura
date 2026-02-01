/* SPDX-License-Identifier: Zlib */

#include "input-history.h"
#include "internal.h"
#include "utils.h"

#include <girara/datastructures.h>

/**
 * Private data of the input history
 */
typedef struct ih_private_s {
  girara_list_t* history; /**< List of stored inputs */
  size_t current;
  size_t current_match;
  GiraraInputHistoryIO* io;
  char* command_line;
  bool reset; /**< Show history starting from the most recent command */
} GiraraInputHistoryPrivate;

G_DEFINE_TYPE_WITH_CODE(GiraraInputHistory, girara_input_history, G_TYPE_OBJECT, G_ADD_PRIVATE(GiraraInputHistory))

/* Methods */
static void ih_dispose(GObject* object);
static void ih_finalize(GObject* object);
static void ih_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void ih_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void ih_append(GiraraInputHistory* history, const char* input);
static girara_list_t* ih_list(GiraraInputHistory* history);
static const char* ih_next(GiraraInputHistory* history, const char* current_input);
static const char* ih_previous(GiraraInputHistory* history, const char* current_input);
static void ih_reset(GiraraInputHistory* history);

/* Properties */
enum {
  PROP_0,
  PROP_IO,
};

/* Class init */
static void girara_input_history_class_init(GiraraInputHistoryClass* class) {
  /* overwrite methods */
  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->dispose      = ih_dispose;
  object_class->finalize     = ih_finalize;
  object_class->set_property = ih_set_property;
  object_class->get_property = ih_get_property;

  class->append   = ih_append;
  class->list     = ih_list;
  class->next     = ih_next;
  class->previous = ih_previous;
  class->reset    = ih_reset;

  /* properties */
  g_object_class_install_property(
      object_class, PROP_IO,
      g_param_spec_object("io", "history reader/writer", "GiraraInputHistoryIO object used to read and write history",
                          girara_input_history_io_get_type(),
                          G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

/* Object init */
static void girara_input_history_init(GiraraInputHistory* history) {
  GiraraInputHistoryPrivate* priv = girara_input_history_get_instance_private(history);
  priv->history                   = girara_list_new_with_free((girara_free_function_t)g_free);
  priv->reset                     = true;
  priv->io                        = NULL;
}

/* GObject dispose */
static void ih_dispose(GObject* object) {
  GiraraInputHistory* ih          = GIRARA_INPUT_HISTORY(object);
  GiraraInputHistoryPrivate* priv = girara_input_history_get_instance_private(ih);

  g_clear_object(&priv->io);

  G_OBJECT_CLASS(girara_input_history_parent_class)->dispose(object);
}

/* GObject finalize */
static void ih_finalize(GObject* object) {
  GiraraInputHistory* ih          = GIRARA_INPUT_HISTORY(object);
  GiraraInputHistoryPrivate* priv = girara_input_history_get_instance_private(ih);
  girara_list_free(priv->history);
  g_free(priv->command_line);

  G_OBJECT_CLASS(girara_input_history_parent_class)->finalize(object);
}

/* GObject set_property */
static void ih_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
  GiraraInputHistory* ih          = GIRARA_INPUT_HISTORY(object);
  GiraraInputHistoryPrivate* priv = girara_input_history_get_instance_private(ih);

  switch (prop_id) {
  case PROP_IO: {
    g_clear_object(&priv->io);

    gpointer* tmp = g_value_dup_object(value);
    if (tmp != NULL) {
      priv->io = GIRARA_INPUT_HISTORY_IO(tmp);
    }
    girara_input_history_reset(GIRARA_INPUT_HISTORY(object));
    break;
  }
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

/* GObject get_property */
static void ih_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
  GiraraInputHistory* ih          = GIRARA_INPUT_HISTORY(object);
  GiraraInputHistoryPrivate* priv = girara_input_history_get_instance_private(ih);

  switch (prop_id) {
  case PROP_IO:
    g_value_set_object(value, priv->io);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

/* Object new */
GiraraInputHistory* girara_input_history_new(GiraraInputHistoryIO* io) {
  return GIRARA_INPUT_HISTORY(g_object_new(GIRARA_TYPE_INPUT_HISTORY, "io", io, NULL));
}

/* Method implementions */

static void ih_append(GiraraInputHistory* history, const char* input) {
  if (input == NULL) {
    return;
  }

  girara_list_t* list = girara_input_history_list(history);
  if (list == NULL) {
    return;
  }

  void* data = NULL;
  while ((data = girara_list_find(list, girara_list_cmpstr, data)) != NULL) {
    girara_list_remove(list, data);
  }

  girara_list_append(list, g_strdup(input));

  GiraraInputHistoryPrivate* priv = girara_input_history_get_instance_private(history);
  if (priv->io != NULL) {
    girara_input_history_io_append(priv->io, input);
  }

  /* begin from the last command when navigating through history */
  girara_input_history_reset(history);
}

static girara_list_t* ih_list(GiraraInputHistory* history) {
  GiraraInputHistoryPrivate* priv = girara_input_history_get_instance_private(history);
  return priv->history;
}

static const char* find_next(GiraraInputHistory* history, const char* current_input, bool next) {
  GiraraInputHistoryPrivate* priv = girara_input_history_get_instance_private(history);

  girara_list_t* list = girara_input_history_list(history);
  if (list == NULL) {
    return NULL;
  }

  size_t length = girara_list_size(list);
  if (length == 0) {
    return NULL;
  }

  if (priv->reset == true) {
    priv->current       = length;
    priv->current_match = priv->current;
  }

  /* Before moving into the history, save the current command-line. */
  if (priv->current_match == length) {
    g_free(priv->command_line);
    priv->command_line = g_strdup(current_input);
  }

  size_t i            = 0;
  const char* command = NULL;
  for (; i < length; ++i) {
    if (priv->reset == true || next == false) {
      if (priv->current < 1) {
        priv->reset   = false;
        priv->current = priv->current_match;
        return NULL;
      } else {
        --priv->current;
      }
    } else if (next == true) {
      if (priv->current + 1 >= length) {
        /* At the bottom of the history, return what the command-line was. */
        priv->current_match = length;
        priv->current       = priv->current_match;
        return priv->command_line;
      } else {
        ++priv->current;
      }
    }

    command = girara_list_nth(list, priv->current);
    if (command == NULL) {
      return NULL;
    }

    /* Only match history items starting with what was on the command-line. */
    if (g_str_has_prefix(command, priv->command_line)) {
      priv->reset         = false;
      priv->current_match = priv->current;
      break;
    }
  }

  if (i == length) {
    return NULL;
  }

  return command;
}

static const char* ih_next(GiraraInputHistory* history, const char* current_input) {
  return find_next(history, current_input, true);
}

static const char* ih_previous(GiraraInputHistory* history, const char* current_input) {
  return find_next(history, current_input, false);
}

static void ih_reset(GiraraInputHistory* history) {
  GiraraInputHistoryPrivate* priv = girara_input_history_get_instance_private(history);
  priv->reset                     = true;

  if (priv->io != NULL) {
    girara_list_t* list = girara_input_history_list(history);
    if (list == NULL) {
      return;
    }
    girara_list_clear(list);

    g_autoptr(girara_list_t) newlist = girara_input_history_io_read(priv->io);
    if (newlist != NULL) {
      girara_list_merge(list, newlist);
    }
  }
}

/* Wrapper functions for the members */

void girara_input_history_append(GiraraInputHistory* history, const char* input) {
  g_return_if_fail(GIRARA_IS_INPUT_HISTORY(history) == true);
  GIRARA_INPUT_HISTORY_GET_CLASS(history)->append(history, input);
}

girara_list_t* girara_input_history_list(GiraraInputHistory* history) {
  g_return_val_if_fail(GIRARA_IS_INPUT_HISTORY(history) == true, NULL);

  GiraraInputHistoryClass* klass = GIRARA_INPUT_HISTORY_GET_CLASS(history);
  g_return_val_if_fail(klass != NULL && klass->list != NULL, NULL);

  return klass->list(history);
}

const char* girara_input_history_next(GiraraInputHistory* history, const char* current_input) {
  g_return_val_if_fail(GIRARA_IS_INPUT_HISTORY(history) == true, NULL);
  return GIRARA_INPUT_HISTORY_GET_CLASS(history)->next(history, current_input);
}

const char* girara_input_history_previous(GiraraInputHistory* history, const char* current_input) {
  g_return_val_if_fail(GIRARA_IS_INPUT_HISTORY(history) == true, NULL);
  return GIRARA_INPUT_HISTORY_GET_CLASS(history)->previous(history, current_input);
}

void girara_input_history_reset(GiraraInputHistory* history) {
  g_return_if_fail(GIRARA_IS_INPUT_HISTORY(history) == true);

  GiraraInputHistoryClass* klass = GIRARA_INPUT_HISTORY_GET_CLASS(history);
  g_return_if_fail(klass != NULL && klass->reset != NULL);

  klass->reset(history);
}
