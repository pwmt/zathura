/* See LICENSE file for license and copyright information */

#include "annotations.h"

#include <stdlib.h>
#include <string.h>

struct zathura_annotation_s {
  zathura_annotation_type_t type; /**< Type of the annotation */
  zathura_annotation_flag_t flags; /**< Flags of the annotation */
  char* name; /**< Name of the annotation */
  char* content; /**< Content of the annotation */
  time_t modification_date; /**< Modification date */
  zathura_page_t* page; /**< Zathura page */
  zathura_rectangle_t position; /**< Position of the annotation */
  void* user_data; /**< Custom data */

  union {
    struct {
      zathura_annotation_text_icon_t icon; /**< The icon of the text annotation */
      zathura_annotation_text_state_t state; /**< The state of the text annotation */
      bool opened; /**< Open status of the text annotation */
    } text;
  } data;

  struct {
    char* label; /**< Label */
    char* subject; /**< Subject of the annotation */
    zathura_annotation_popup_t* popup; /**< Optional popup */
    time_t creation_date; /**< Creation date */
  } markup;
};

struct zathura_annotation_popup_s {
  double opacity; /**< The opacity of the popup */
  zathura_rectangle_t position; /**< Position of the annotation */
  bool opened; /**< true if popup is opened */
};

static char* __strdup(const char* string);

zathura_annotation_t*
zathura_annotation_new(zathura_annotation_type_t type)
{
  zathura_annotation_t* annotation = calloc(1, sizeof(zathura_annotation_t));
  if (annotation == NULL) {
    return NULL;
  }

  switch (type) {
    case ZATHURA_ANNOTATION_TEXT:
      break;
    default:
      free(annotation);
      return NULL;
  }

  annotation->type                      = type;
  annotation->modification_date         = (time_t) -1;
  annotation->markup.creation_date = (time_t) -1;
  annotation->data.text.icon            = ZATHURA_ANNOTATION_TEXT_ICON_UNKNOWN;
  annotation->data.text.state           = ZATHURA_ANNOTATION_TEXT_STATE_UNKNOWN;

  return annotation;
}

void
zathura_annotation_free(zathura_annotation_t* annotation)
{
  if (annotation == NULL) {
    return;
  }

  if (annotation->name) {
    free(annotation->name);
  }

  if (annotation->content) {
    free(annotation->content);
  }

  if (annotation->markup.label != NULL) {
    free(annotation->markup.label);
  }

  if (annotation->markup.subject != NULL) {
    free(annotation->markup.subject);
  }

  if (annotation->markup.popup != NULL) {
    zathura_annotation_popup_free(annotation->markup.popup);
  }

  free(annotation);
}

void*
zathura_annotation_get_data(zathura_annotation_t* annotation)
{
  if (annotation == NULL) {
    return NULL;
  }

  return annotation->user_data;
}

void
zathura_annotation_set_data(zathura_annotation_t* annotation, void* data)
{
  if (annotation == NULL) {
    return;
  }

  annotation->user_data = data;
}

zathura_annotation_type_t
zathura_annotation_get_type(zathura_annotation_t* annotation)
{
  if (annotation == NULL) {
    return ZATHURA_ANNOTATION_UNKNOWN;
  }

  return annotation->type;
}

bool
zathura_annotation_is_markup_annotation(zathura_annotation_t* annotation)
{
  if (annotation == NULL) {
    return false;
  }

  switch (annotation->type) {
    case ZATHURA_ANNOTATION_TEXT:
    case ZATHURA_ANNOTATION_FREE_TEXT:
    case ZATHURA_ANNOTATION_LINE:
    case ZATHURA_ANNOTATION_STAMP:
    case ZATHURA_ANNOTATION_POLYGON:
    case ZATHURA_ANNOTATION_CARET:
    case ZATHURA_ANNOTATION_INK:
    case ZATHURA_ANNOTATION_FILE_ATTACHMENT:
    case ZATHURA_ANNOTATION_SOUND:
      return true;
    default:
      return false;
  }
}

zathura_annotation_flag_t
zathura_annotation_get_flags(zathura_annotation_t* annotation)
{
  if (annotation == NULL) {
    return ZATHURA_ANNOTATION_FLAG_UNKNOWN;
  }

  return annotation->flags;
}

void
zathura_annotation_set_flags(zathura_annotation_t* annotation, zathura_annotation_flag_t flags)
{
  if (annotation == NULL) {
    return;
  }

  annotation->flags = flags;
}

char*
zathura_annotation_get_content(zathura_annotation_t* annotation)
{
  if (annotation == NULL) {
    return NULL;
  }

  return annotation->content;
}

void
zathura_annotation_set_content(zathura_annotation_t* annotation, char* content)
{
  if (annotation == NULL) {
    return;
  }

  if (content != NULL) {
    annotation->content = __strdup(content);
  } else {
    annotation->content = NULL;
  }
}

char*
zathura_annotation_get_name(zathura_annotation_t* annotation)
{
  if (annotation == NULL) {
    return NULL;
  }

  return annotation->name;
}

void
zathura_annotation_set_name(zathura_annotation_t* annotation, const char* name)
{
  if (annotation == NULL) {
    return;
  }

  if (name != NULL) {
    annotation->name = __strdup(name);
  } else {
    annotation->name = NULL;
  }
}

time_t
zathura_annotation_get_modification_date(zathura_annotation_t* annotation)
{
  if (annotation == NULL) {
    return (time_t) -1;
  }

  return annotation->modification_date;
}

void
zathura_annotation_set_modification_date(zathura_annotation_t* annotation, time_t modification_date)
{
  if (annotation == NULL) {
    return;
  }

  annotation->modification_date = modification_date;
}

zathura_page_t*
zathura_annotation_get_page(zathura_annotation_t* annotation)
{
  if (annotation == NULL) {
    return NULL;
  }

  return annotation->page;
}

void
zathura_annotation_set_page(zathura_annotation_t* annotation, zathura_page_t* page)
{
  if (annotation == NULL) {
    return;
  }

  annotation->page = page;
}

zathura_rectangle_t
zathura_annotation_get_position(zathura_annotation_t* annotation)
{
  zathura_rectangle_t rectangle = { 0, 0, 0, 0 };

  if (annotation != NULL) {
    rectangle.x1 = annotation->position.x1;
    rectangle.x2 = annotation->position.x2;
    rectangle.y1 = annotation->position.y1;
    rectangle.y2 = annotation->position.y2;
  }

  return rectangle;
}

void
zathura_annotation_set_position(zathura_annotation_t* annotation,
    zathura_rectangle_t position)
{
  if (annotation == NULL) {
    return;
  }

  annotation->position.x1 = position.x1;
  annotation->position.x2 = position.x2;
  annotation->position.y1 = position.y1;
  annotation->position.y2 = position.y2;
}

char*
zathura_annotation_markup_get_label(zathura_annotation_t* annotation)
{
  if (annotation == NULL || zathura_annotation_is_markup_annotation(annotation) == false) {
    return NULL;
  }

  return annotation->markup.label;
}

void
zathura_annotation_markup_set_label(zathura_annotation_t* annotation, const char* label)
{
  if (annotation == NULL || zathura_annotation_is_markup_annotation(annotation) == false) {
    return;
  }

  if (annotation->markup.label != NULL) {
    free(annotation->markup.label);
  }

  if (label != NULL) {
    annotation->markup.label = __strdup(label);
  } else {
    annotation->markup.label = NULL;
  }
}

char*
zathura_annotation_markup_get_subject(zathura_annotation_t* annotation)
{
  if (annotation == NULL || zathura_annotation_is_markup_annotation(annotation) == false) {
    return NULL;
  }

  return annotation->markup.subject;
}

void
zathura_annotation_markup_set_subject(zathura_annotation_t* annotation, const char* subject)
{
  if (annotation == NULL || zathura_annotation_is_markup_annotation(annotation) == false) {
    return;
  }
  if (annotation->markup.subject != NULL) {
    free(annotation->markup.subject);
  }

  if (subject != NULL) {
    annotation->markup.subject = __strdup(subject);
  } else {
    annotation->markup.subject = NULL;
  }
}

zathura_annotation_popup_t*
zathura_annotation_markup_get_popup(zathura_annotation_t* annotation)
{
  if (annotation == NULL || zathura_annotation_is_markup_annotation(annotation) == false) {
    return NULL;
  }

  return annotation->markup.popup;
}

void
zathura_annotation_markup_set_popup(zathura_annotation_t* annotation, zathura_annotation_popup_t* popup)
{
  if (annotation == NULL || zathura_annotation_is_markup_annotation(annotation) == false || popup == NULL) {
    return;
  }

  annotation->markup.popup = popup;
}

time_t
zathura_annotation_markup_get_creation_date(zathura_annotation_t* annotation)
{
  if (annotation == NULL || zathura_annotation_is_markup_annotation(annotation) == false) {
    return (time_t) -1;
  }

  return annotation->markup.creation_date;
}

void
zathura_annotation_markup_set_creation_date(zathura_annotation_t* annotation, time_t creation_date)
{
  if (annotation == NULL || zathura_annotation_is_markup_annotation(annotation) == false) {
    return;
  }

  annotation->markup.creation_date = creation_date;
}

zathura_annotation_text_icon_t
zathura_annotation_text_get_icon(zathura_annotation_t* annotation)
{
  if (annotation == NULL || annotation->type != ZATHURA_ANNOTATION_TEXT) {
    return ZATHURA_ANNOTATION_TEXT_ICON_UNKNOWN;
  }

  return annotation->data.text.icon;
}

void
zathura_annotation_text_set_icon(zathura_annotation_t* annotation, zathura_annotation_text_icon_t icon)
{
  if (annotation == NULL || annotation->type != ZATHURA_ANNOTATION_TEXT) {
    return;
  }

  annotation->data.text.icon = icon;
}

zathura_annotation_text_state_t
zathura_annotation_text_get_state(zathura_annotation_t* annotation)
{
  if (annotation == NULL || annotation->type != ZATHURA_ANNOTATION_TEXT) {
    return ZATHURA_ANNOTATION_TEXT_STATE_UNKNOWN;
  }

  return annotation->data.text.state;
}

void
zathura_annotation_text_set_state(zathura_annotation_t* annotation, zathura_annotation_text_state_t state)
{
  if (annotation == NULL || annotation->type != ZATHURA_ANNOTATION_TEXT) {
    return;
  }

  annotation->data.text.state = state;
}

bool
zathura_annotation_text_get_open_status(zathura_annotation_t* annotation)
{
  if (annotation == NULL || annotation->type != ZATHURA_ANNOTATION_TEXT) {
    return false;
  }

  return annotation->data.text.opened;
}

void
zathura_annotation_text_set_open_status(zathura_annotation_t* annotation, bool opened)
{
  if (annotation == NULL || annotation->type != ZATHURA_ANNOTATION_TEXT) {
    return;
  }

  annotation->data.text.opened = opened;
}

zathura_annotation_popup_t*
zathura_annotation_popup_new()
{
  zathura_annotation_popup_t* popup = calloc(1, sizeof(zathura_annotation_popup_t));
  if (popup == NULL) {
    return NULL;
  }

  popup->opacity = 1.0;

  return popup;
}

void
zathura_annotation_popup_free(zathura_annotation_popup_t* popup)
{
  if (popup == NULL) {
    return;
  }

  free(popup);
}

zathura_rectangle_t
zathura_annotation_popup_get_position(zathura_annotation_popup_t* popup)
{
  zathura_rectangle_t rectangle = { 0, 0, 0, 0 };

  if (popup != NULL) {
    rectangle.x1 = popup->position.x1;
    rectangle.x2 = popup->position.x2;
    rectangle.y1 = popup->position.y1;
    rectangle.y2 = popup->position.y2;
  }

  return rectangle;
}

void
zathura_annotation_popup_set_position(zathura_annotation_popup_t* popup, zathura_rectangle_t position)
{
  if (popup == NULL) {
    return;
  }

  popup->position.x1 = position.x1;
  popup->position.x2 = position.x2;
  popup->position.y1 = position.y1;
  popup->position.y2 = position.y2;
}

double
zathura_annotation_popup_get_opacity(zathura_annotation_popup_t* popup)
{
  if (popup == NULL) {
    return 0.0;
  }

  return popup->opacity;
}

void
zathura_annotation_popup_set_opacity(zathura_annotation_popup_t* popup, double opacity)
{
  if (popup == NULL) {
    return;
  }

  if (opacity < 0) {
    popup->opacity = 0;
  } else if (opacity > 1) {
    popup->opacity = 1;
  } else {
    popup->opacity = opacity;
  }
}

bool
zathura_annotation_popup_get_open_status(zathura_annotation_popup_t* popup)
{
  if (popup == NULL) {
    return false;
  }

  return popup->opened;
}

void
zathura_annotation_popup_set_open_status(zathura_annotation_popup_t* popup, bool opened)
{
  if (popup == NULL) {
    return;
  }

  popup->opened = opened;
}

static char*
__strdup(const char* string)
{
  if (string == NULL) {
    return NULL;
  }

  size_t length = strlen(string) + 1;

  char* new_string = malloc(sizeof(char) * length);
  if (new_string == NULL) {
    return NULL;
  }

  return (char*) memcpy(new_string, string, length);
}
