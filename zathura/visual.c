/* SPDX-License-Identifier: Zlib */

#include "visual.h"

#include <limits.h>
#include <math.h>
#include <string.h>
#include <glib/gi18n.h>
#include <girara/datastructures.h>
#include <girara-gtk/dialog.h>
#include <girara-gtk/inputbar.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>

#include "adjustment.h"
#include "callbacks.h"
#include "document.h"
#include "document-widget.h"
#include "internal.h"
#include "macros.h"
#include "page.h"
#include "page-widget.h"
#include "plugin.h"
#include "utils.h"

/* a position in the document */
typedef struct visual_position_s {
  unsigned int page; /**< Page index */
  size_t index;      /**< Character on the page */
} visual_position_t;

/* texts of the pages used so far, loaded on first use */
typedef struct visual_texts_s {
  zathura_visual_text_t** pages; /**< Text of every page, NULL until the page is used */
  unsigned int n_pages;          /**< Number of pages */
} visual_texts_t;

/* pages on which a selection or caret is drawn */
typedef struct visual_drawn_s {
  bool drawn;         /**< Something is drawn on the pages first to last */
  unsigned int first; /**< First page */
  unsigned int last;  /**< Last page */
} visual_drawn_t;

/* a word that can be jumped to by typing its label */
typedef struct visual_label_s {
  visual_position_t position;                      /**< Start of the word, or the page of the image */
  bool image;                                      /**< The label is for an image instead of a word */
  size_t image_index;                              /**< Index of the image in zathura_page_images_get() */
  zathura_rectangle_t bounds;                      /**< Part of the image on the page */
  zathura_rectangle_t anchor;                      /**< Where the label of the image goes */
  char text[ZATHURA_PAGE_WIDGET_LABEL_LENGTH + 1]; /**< Label */
} visual_label_t;

struct zathura_visual_s {
  visual_position_t cursor;    /**< Caret */
  visual_position_t anchor;    /**< Other end of the selection */
  zathura_visual_kind_t kind;  /**< Kind of selection */
  double curswant;             /**< Horizontal position kept by up and down motions */
  girara_mode_t previous_mode; /**< Mode to return to */
  visual_texts_t texts;        /**< Texts of the pages used so far */
  visual_drawn_t drawn;        /**< Pages with a drawn selection or caret */
  bool yank_pending;           /**< y was pressed once without a selection */
  unsigned int yank_count;     /**< Count given to the first y */

  struct {
    gunichar c;   /**< Character of the last f, F, t or T */
    bool forward; /**< Direction of the last search */
    bool till;    /**< The last search was t or T */
  } find;

  struct {
    girara_list_t* list;        /**< Images of the page of the selected image */
    zathura_image_t* image;     /**< The selected image, in list */
    unsigned int page;          /**< Page of the selected image */
    zathura_rectangle_t bounds; /**< Part of the selected image on the page */
  } image;

  struct {
    girara_list_t* list;     /**< visual_label_t of the shown labels */
    unsigned int first_page; /**< First page with labels */
    unsigned int last_page;  /**< Last page with labels */
    GiraraDialog* dialog;    /**< Input of the label */
  } labels;
};

typedef struct zathura_visual_s zathura_visual_t;
typedef struct zathura_visual_mouse_s zathura_visual_mouse_t;
typedef struct zathura_visual_flash_s zathura_visual_flash_t;

/* a mouse selection that may span pages */
struct zathura_visual_mouse_s {
  bool dragging;            /**< The button is held after a press on a page */
  bool multi;               /**< The pointer left the page where the selection started */
  unsigned int start_page;  /**< Page where the button was pressed */
  double start_x;           /**< Point where the button was pressed, in page coordinates */
  double start_y;           /**< Point where the button was pressed, in page coordinates */
  visual_position_t anchor; /**< Start of the selection */
  visual_position_t cursor; /**< End of the selection */
  visual_texts_t texts;     /**< Texts of the pages used so far */
  visual_drawn_t drawn;     /**< Pages with a drawn selection */
  double view_x;            /**< Last pointer position, relative to the view */
  double view_y;            /**< Last pointer position, relative to the view */
  guint scroll_source;      /**< Source scrolling the view while the pointer is near its edge */
};

/* text that was just copied, drawn until a timeout */
struct zathura_visual_flash_s {
  visual_drawn_t drawn; /**< Pages with a drawn flash */
  guint source;         /**< Timeout that clears the flash */
};

/* the last selection, for gv */
struct zathura_visual_last_s {
  visual_position_t cursor;
  visual_position_t anchor;
  zathura_visual_kind_t kind;
};

/* Motions on the text of a page */

static bool is_newline(const zathura_visual_text_t* text, size_t pos) {
  return text->chars[pos] == '\n';
}

/* character classes as in vim: 0 is blank, 1 is punctuation, 2 is a word character; big words only distinguish
 * blank and non-blank */
static int char_class(const zathura_visual_text_t* text, size_t pos, bool big) {
  const gunichar c = text->chars[pos];
  if (g_unichar_isspace(c) == TRUE) {
    return 0;
  }
  if (big == true || g_unichar_isalnum(c) == TRUE || g_unichar_ismark(c) == TRUE || c == '_') {
    return 2;
  }
  return 1;
}

static size_t line_start(const zathura_visual_text_t* text, size_t pos) {
  while (pos > 0 && is_newline(text, pos - 1) == false) {
    --pos;
  }
  return pos;
}

/* last character of the line, which is the newline only for empty lines */
static size_t line_last(const zathura_visual_text_t* text, size_t pos) {
  if (is_newline(text, pos) == true) {
    return pos;
  }
  while (pos + 1 < text->n && is_newline(text, pos + 1) == false) {
    ++pos;
  }
  return pos;
}

static size_t line_first_nonblank(const zathura_visual_text_t* text, size_t pos) {
  const size_t start = line_start(text, pos);
  const size_t last  = line_last(text, pos);
  for (size_t idx = start; idx <= last; ++idx) {
    if (char_class(text, idx, true) != 0) {
      return idx;
    }
  }
  return last;
}

/* last character of the text that is not a newline */
static size_t text_last(const zathura_visual_text_t* text) {
  size_t pos = text->n - 1;
  while (pos > 0 && is_newline(text, pos) == true) {
    --pos;
  }
  return pos;
}

static double center_x(const zathura_visual_text_t* text, size_t pos) {
  return (text->rects[pos].x1 + text->rects[pos].x2) / 2;
}

/* the character of a line closest to a horizontal position */
static size_t line_closest(const zathura_visual_text_t* text, size_t start, double curswant) {
  const size_t last = line_last(text, start);
  size_t best       = start;
  double best_dist  = INFINITY;
  for (size_t idx = start; idx <= last; ++idx) {
    if (is_newline(text, idx) == true) {
      continue;
    }
    const double dist = fabs(center_x(text, idx) - curswant);
    if (dist < best_dist) {
      best      = idx;
      best_dist = dist;
    }
  }
  return best;
}

static size_t line_vertical(const zathura_visual_text_t* text, size_t pos, bool down, double curswant) {
  size_t target = 0;
  if (down == true) {
    const size_t last    = line_last(text, pos);
    const size_t newline = is_newline(text, last) == true ? last : last + 1;
    if (newline + 1 >= text->n) {
      return pos;
    }
    target = newline + 1;
  } else {
    const size_t start = line_start(text, pos);
    if (start == 0) {
      return pos;
    }
    target = line_start(text, start - 1);
  }

  return line_closest(text, target, curswant);
}

static size_t word_forward(const zathura_visual_text_t* text, size_t pos, bool big) {
  const int cls = char_class(text, pos, big);
  size_t idx    = pos;
  if (cls != 0) {
    while (idx < text->n && char_class(text, idx, big) == cls) {
      ++idx;
    }
  }
  while (idx < text->n && char_class(text, idx, big) == 0) {
    ++idx;
  }
  return idx < text->n ? idx : text_last(text);
}

static size_t word_end(const zathura_visual_text_t* text, size_t pos, bool big) {
  size_t idx = pos + 1;
  while (idx < text->n && char_class(text, idx, big) == 0) {
    ++idx;
  }
  if (idx >= text->n) {
    return text_last(text);
  }
  const int cls = char_class(text, idx, big);
  while (idx + 1 < text->n && char_class(text, idx + 1, big) == cls) {
    ++idx;
  }
  return idx;
}

static size_t word_backward(const zathura_visual_text_t* text, size_t pos, bool big) {
  if (pos == 0) {
    return 0;
  }
  size_t idx = pos - 1;
  while (idx > 0 && char_class(text, idx, big) == 0) {
    --idx;
  }
  const int cls = char_class(text, idx, big);
  while (idx > 0 && char_class(text, idx - 1, big) == cls) {
    --idx;
  }
  return idx;
}

size_t zathura_visual_motion(const zathura_visual_text_t* text, size_t pos, zathura_visual_motion_t motion,
                             double* curswant) {
  if (text == NULL || text->n == 0 || pos >= text->n) {
    return pos;
  }

  if (motion == ZATHURA_VISUAL_MOTION_UP || motion == ZATHURA_VISUAL_MOTION_DOWN) {
    double want = curswant != NULL && *curswant >= 0 ? *curswant : center_x(text, pos);
    if (curswant != NULL) {
      *curswant = want;
    }
    return line_vertical(text, pos, motion == ZATHURA_VISUAL_MOTION_DOWN, want);
  }

  /* every other motion picks a new horizontal position */
  if (curswant != NULL) {
    *curswant = -1;
  }

  switch (motion) {
  case ZATHURA_VISUAL_MOTION_LEFT:
    return pos > 0 && is_newline(text, pos - 1) == false ? pos - 1 : pos;
  case ZATHURA_VISUAL_MOTION_RIGHT:
    return pos + 1 < text->n && is_newline(text, pos) == false && is_newline(text, pos + 1) == false ? pos + 1 : pos;
  case ZATHURA_VISUAL_MOTION_WORD_FORWARD:
    return word_forward(text, pos, false);
  case ZATHURA_VISUAL_MOTION_WORD_BACKWARD:
    return word_backward(text, pos, false);
  case ZATHURA_VISUAL_MOTION_WORD_END:
    return word_end(text, pos, false);
  case ZATHURA_VISUAL_MOTION_BIGWORD_FORWARD:
    return word_forward(text, pos, true);
  case ZATHURA_VISUAL_MOTION_BIGWORD_BACKWARD:
    return word_backward(text, pos, true);
  case ZATHURA_VISUAL_MOTION_BIGWORD_END:
    return word_end(text, pos, true);
  case ZATHURA_VISUAL_MOTION_LINE_START:
    return line_start(text, pos);
  case ZATHURA_VISUAL_MOTION_LINE_FIRST_NONBLANK:
    return line_first_nonblank(text, pos);
  case ZATHURA_VISUAL_MOTION_LINE_END:
    return line_last(text, pos);
  case ZATHURA_VISUAL_MOTION_TOP:
    return line_first_nonblank(text, 0);
  case ZATHURA_VISUAL_MOTION_BOTTOM:
    return line_first_nonblank(text, text_last(text));
  default:
    return pos;
  }
}

void zathura_visual_range(const zathura_visual_text_t* text, zathura_visual_kind_t kind, size_t anchor, size_t cursor,
                          size_t* first, size_t* last) {
  size_t lo = MIN(anchor, cursor);
  size_t hi = MAX(anchor, cursor);
  if (kind == ZATHURA_VISUAL_CARET) {
    lo = hi = cursor;
  } else if (kind == ZATHURA_VISUAL_LINE && text != NULL && text->n != 0) {
    lo = line_start(text, lo);
    hi = line_last(text, hi);
  }
  *first = lo;
  *last  = hi;
}

void zathura_visual_lines(const zathura_visual_text_t* text, size_t pos, unsigned int count, size_t* first,
                          size_t* last) {
  *first = *last = pos;
  if (text == NULL || text->n == 0 || pos >= text->n) {
    return;
  }

  *first = line_start(text, pos);
  *last  = line_last(text, pos);
  for (unsigned int idx = 1; idx < MAX(count, 1); ++idx) {
    const size_t newline = is_newline(text, *last) == true ? *last : *last + 1;
    if (newline + 1 >= text->n) {
      break;
    }
    *last = line_last(text, newline + 1);
  }
}

size_t zathura_visual_find(const zathura_visual_text_t* text, size_t pos, gunichar c, bool forward, bool till,
                           bool repeat, unsigned int count) {
  if (text == NULL || text->n == 0 || pos >= text->n) {
    return pos;
  }

  /* a repeated till search would stop right away on a match next to the caret */
  const size_t skip  = (till == true && repeat == true) ? 1 : 0;
  unsigned int found = 0;

  if (forward == true) {
    for (size_t idx = pos + 1 + skip; idx < text->n && is_newline(text, idx) == false; ++idx) {
      if (text->chars[idx] == c && ++found == MAX(count, 1)) {
        return till == true ? idx - 1 : idx;
      }
    }
  } else if (pos >= 1 + skip) {
    for (size_t idx = pos - 1 - skip; is_newline(text, idx) == false; --idx) {
      if (text->chars[idx] == c && ++found == MAX(count, 1)) {
        return till == true ? idx + 1 : idx;
      }
      if (idx == 0) {
        break;
      }
    }
  }

  return pos;
}

/* same class on the same line */
static bool same_word(const zathura_visual_text_t* text, size_t pos, int cls, bool big) {
  return is_newline(text, pos) == false && char_class(text, pos, big) == cls;
}

void zathura_visual_word_object(const zathura_visual_text_t* text, size_t pos, bool big, bool around, size_t* first,
                                size_t* last) {
  *first = *last = pos;
  if (text == NULL || pos >= text->n || is_newline(text, pos) == true) {
    return;
  }

  const int cls = char_class(text, pos, big);
  size_t lo     = pos;
  size_t hi     = pos;
  while (lo > 0 && same_word(text, lo - 1, cls, big) == true) {
    --lo;
  }
  while (hi + 1 < text->n && same_word(text, hi + 1, cls, big) == true) {
    ++hi;
  }

  if (around == true) {
    if (cls == 0) {
      /* blanks and the word after them */
      if (hi + 1 < text->n && is_newline(text, hi + 1) == false) {
        const int next = char_class(text, ++hi, big);
        while (hi + 1 < text->n && same_word(text, hi + 1, next, big) == true) {
          ++hi;
        }
      }
    } else {
      /* the word and the blanks after it, or before it if there are none after it */
      size_t end = hi;
      while (end + 1 < text->n && same_word(text, end + 1, 0, big) == true) {
        ++end;
      }
      if (end != hi) {
        hi = end;
      } else {
        while (lo > 0 && same_word(text, lo - 1, 0, big) == true) {
          --lo;
        }
      }
    }
  }

  *first = lo;
  *last  = hi;
}

/* the last character an exclusive motion takes when it ends on pos: the one before it, or the end of the line before
 * when pos starts a line */
static size_t exclusive_end(const zathura_visual_text_t* text, size_t pos) {
  if (line_start(text, pos) == pos) {
    return line_last(text, line_start(text, pos - 1));
  }
  return pos - 1;
}

bool zathura_visual_operator_range(const zathura_visual_text_t* text, size_t origin, size_t target,
                                   zathura_visual_operator_range_t range, size_t* first, size_t* last) {
  *first = MIN(origin, target);
  *last  = MAX(origin, target);
  if (text == NULL || text->n == 0 || *last >= text->n) {
    return false;
  }

  switch (range) {
  case ZATHURA_VISUAL_OPERATOR_LINEWISE:
    *first = line_start(text, *first);
    *last  = line_last(text, *last);
    return true;
  case ZATHURA_VISUAL_OPERATOR_INCLUSIVE:
    return true;
  case ZATHURA_VISUAL_OPERATOR_EXCLUSIVE:
    if (*first == *last) {
      return false;
    }
    *last = exclusive_end(text, *last);
    return *last >= *first;
  }
  return false;
}

size_t zathura_visual_page_entry(const zathura_visual_text_t* text, zathura_visual_motion_t motion, double curswant) {
  if (text == NULL || text->n == 0) {
    return text != NULL ? text->n : 0;
  }

  size_t idx = 0;
  switch (motion) {
  case ZATHURA_VISUAL_MOTION_DOWN:
    return line_closest(text, 0, curswant);
  case ZATHURA_VISUAL_MOTION_UP:
    return line_closest(text, line_start(text, text_last(text)), curswant);
  case ZATHURA_VISUAL_MOTION_WORD_FORWARD:
  case ZATHURA_VISUAL_MOTION_BIGWORD_FORWARD:
  case ZATHURA_VISUAL_MOTION_WORD_END:
  case ZATHURA_VISUAL_MOTION_BIGWORD_END: {
    const bool big = motion == ZATHURA_VISUAL_MOTION_BIGWORD_FORWARD || motion == ZATHURA_VISUAL_MOTION_BIGWORD_END;
    while (idx < text->n && char_class(text, idx, big) == 0) {
      ++idx;
    }
    if (idx == text->n) {
      return text->n;
    }
    if (motion == ZATHURA_VISUAL_MOTION_WORD_END || motion == ZATHURA_VISUAL_MOTION_BIGWORD_END) {
      const int cls = char_class(text, idx, big);
      while (idx + 1 < text->n && char_class(text, idx + 1, big) == cls) {
        ++idx;
      }
    }
    return idx;
  }
  case ZATHURA_VISUAL_MOTION_WORD_BACKWARD:
  case ZATHURA_VISUAL_MOTION_BIGWORD_BACKWARD: {
    const bool big = motion == ZATHURA_VISUAL_MOTION_BIGWORD_BACKWARD;
    idx            = text->n - 1;
    while (idx > 0 && char_class(text, idx, big) == 0) {
      --idx;
    }
    if (char_class(text, idx, big) == 0) {
      return text->n;
    }
    const int cls = char_class(text, idx, big);
    while (idx > 0 && char_class(text, idx - 1, big) == cls) {
      --idx;
    }
    return idx;
  }
  default:
    return text->n;
  }
}

static bool has_size(const zathura_visual_text_t* text, size_t pos) {
  const zathura_rectangle_t* rect = &text->rects[pos];
  return is_newline(text, pos) == false && rect->x2 > rect->x1 && rect->y2 > rect->y1;
}

zathura_rectangle_t zathura_visual_caret_rectangle(const zathura_visual_text_t* text, size_t pos) {
  zathura_rectangle_t rect = text->rects[pos];
  if (has_size(text, pos) == true) {
    return rect;
  }

  /* the closest characters with a size before and after the caret */
  size_t prev = text->n;
  size_t next = text->n;
  for (size_t idx = pos; idx > 0; --idx) {
    if (has_size(text, idx - 1) == true) {
      prev = idx - 1;
      break;
    }
  }
  for (size_t idx = pos + 1; idx < text->n; ++idx) {
    if (has_size(text, idx) == true) {
      next = idx;
      break;
    }
  }

  if (prev == text->n && next == text->n) {
    rect.x2 = rect.x1 + 5;
    rect.y2 = rect.y1 + 10;
    return rect;
  }

  const zathura_rectangle_t* ref = &text->rects[prev != text->n ? prev : next];
  const double height            = ref->y2 - ref->y1;
  const double width             = height / 2;

  if (prev != text->n && next != text->n && text->rects[next].y1 - text->rects[prev].y2 >= height / 2) {
    /* an empty line between two lines: in the gap, at the start of the next line */
    const double middle = (text->rects[prev].y2 + text->rects[next].y1) / 2;
    rect.x1             = text->rects[line_start(text, next)].x1;
    rect.y1             = middle - height / 2;
  } else if (prev != text->n) {
    /* after the text before the caret */
    rect.x1 = ref->x2;
    rect.y1 = ref->y1;
  } else {
    /* before the text after the caret */
    rect.x1 = ref->x1 - width;
    rect.y1 = ref->y1;
  }
  rect.x2 = rect.x1 + width;
  rect.y2 = rect.y1 + height;
  return rect;
}

size_t zathura_visual_index_at(const zathura_visual_text_t* text, double x, double y) {
  if (text == NULL || text->n == 0) {
    return 0;
  }

  /* the closest line: first vertically, then horizontally */
  size_t best_line = text->n;
  double best_dy   = INFINITY;
  double best_dx   = INFINITY;
  for (size_t start = 0; start < text->n;) {
    const size_t last = line_last(text, start);
    double x1         = INFINITY;
    double y1         = INFINITY;
    double x2         = -INFINITY;
    double y2         = -INFINITY;
    for (size_t idx = start; idx <= last; ++idx) {
      if (has_size(text, idx) == true) {
        x1 = fmin(x1, text->rects[idx].x1);
        y1 = fmin(y1, text->rects[idx].y1);
        x2 = fmax(x2, text->rects[idx].x2);
        y2 = fmax(y2, text->rects[idx].y2);
      }
    }

    if (x1 <= x2) {
      const double dy = y < y1 ? y1 - y : (y > y2 ? y - y2 : 0);
      const double dx = x < x1 ? x1 - x : (x > x2 ? x - x2 : 0);
      if (dy < best_dy || (dy == best_dy && dx < best_dx)) {
        best_line = start;
        best_dy   = dy;
        best_dx   = dx;
      }
    }

    start = (is_newline(text, last) == true ? last : last + 1) + 1;
  }

  if (best_line == text->n) {
    return 0;
  }

  /* the character of the line under the point, or the closest one */
  const size_t last = line_last(text, best_line);
  size_t best       = best_line;
  double best_dist  = INFINITY;
  for (size_t idx = best_line; idx <= last; ++idx) {
    if (has_size(text, idx) == false) {
      continue;
    }
    const zathura_rectangle_t* rect = &text->rects[idx];
    if (x >= rect->x1 && x <= rect->x2) {
      return idx;
    }
    const double dist = x < rect->x1 ? rect->x1 - x : x - rect->x2;
    if (dist < best_dist) {
      best      = idx;
      best_dist = dist;
    }
  }
  return best;
}

/* Visual mode */

static bool visual_texts_init(zathura_t* zathura, visual_texts_t* texts) {
  texts->n_pages = zathura_document_get_number_of_pages(zathura_get_document(zathura));
  texts->pages   = g_try_malloc0(MAX(texts->n_pages, 1) * sizeof(zathura_visual_text_t*));
  return texts->pages != NULL;
}

static void visual_texts_clear(visual_texts_t* texts) {
  if (texts->pages == NULL) {
    return;
  }

  for (unsigned int idx = 0; idx < texts->n_pages; ++idx) {
    if (texts->pages[idx] != NULL) {
      g_free(texts->pages[idx]->chars);
      g_free(texts->pages[idx]);
    }
  }
  g_free(texts->pages);
  texts->pages   = NULL;
  texts->n_pages = 0;
}

/* text of a page, loaded on first use. NULL if it is not available. */
static const zathura_visual_text_t* visual_text(zathura_t* zathura, visual_texts_t* texts, unsigned int page_index) {
  if (texts->pages == NULL || page_index >= texts->n_pages) {
    return NULL;
  }
  if (texts->pages[page_index] != NULL) {
    return texts->pages[page_index];
  }

  zathura_page_t* page = zathura_document_get_page(zathura_get_document(zathura), page_index);
  if (page == NULL) {
    return NULL;
  }

  zathura_error_t error               = ZATHURA_ERROR_OK;
  const zathura_text_layout_t* layout = zathura_page_get_text_layout(page, &error);
  if (layout == NULL) {
    if (error == ZATHURA_ERROR_NOT_IMPLEMENTED) {
      girara_notify(zathura->ui.session, GIRARA_ERROR, _("Text selection is not supported for this document."));
    } else {
      girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to get the text of page %u."), page_index + 1);
    }
    return NULL;
  }

  glong n         = 0;
  gunichar* chars = g_utf8_to_ucs4_fast(layout->text, -1, &n);
  if (chars == NULL || (size_t)n != layout->n_rects) {
    g_free(chars);
    return NULL;
  }

  zathura_visual_text_t* text = g_try_malloc0(sizeof(zathura_visual_text_t));
  if (text == NULL) {
    g_free(chars);
    return NULL;
  }

  text->chars              = chars;
  text->rects              = layout->rects;
  text->n                  = n;
  texts->pages[page_index] = text;
  return text;
}

static const zathura_visual_text_t* visual_current_text(zathura_t* zathura) {
  return visual_text(zathura, &zathura->visual->texts, zathura->visual->cursor.page);
}

static int position_compare(const visual_position_t* a, const visual_position_t* b) {
  if (a->page != b->page) {
    return a->page < b->page ? -1 : 1;
  }
  if (a->index != b->index) {
    return a->index < b->index ? -1 : 1;
  }
  return 0;
}

/* position of the center of a rectangle of a page in the document, in the units of the document position */
static void visual_point_to_position(zathura_t* zathura, unsigned int page_index, zathura_rectangle_t rect,
                                     double* pos_x, double* pos_y) {
  zathura_page_t* page          = zathura_document_get_page(zathura_get_document(zathura), page_index);
  zathura_rectangle_t rectangle = zathura_page_widget_recalc_rectangle(
      zathura_document_widget_ensure_page(zathura->ui.document_widget, zathura_page_get_index(page)), rect);

  unsigned int cell_height = 0;
  unsigned int cell_width  = 0;
  zathura_document_widget_get_cell_size(zathura->ui.document_widget, page_index, &cell_height, &cell_width);

  unsigned int doc_height = 0;
  unsigned int doc_width  = 0;
  zathura_document_widget_get_document_size(zathura->ui.document_widget, &doc_height, &doc_width);

  /* center of the page, then the offset of the center of the rectangle from it */
  page_number_to_position(zathura->ui.document_widget, page_index, 0.5, 0.5, pos_x, pos_y);
  if (doc_height != 0) {
    *pos_y += ((rectangle.y1 + rectangle.y2) / 2 - (double)cell_height / 2) / (double)doc_height;
  }
  if (doc_width != 0) {
    *pos_x += ((rectangle.x1 + rectangle.x2) / 2 - (double)cell_width / 2) / (double)doc_width;
  }
}

/* half of the viewport in the units of the document position */
static bool visual_viewport_half(zathura_t* zathura, double* half_w, double* half_h) {
  unsigned int doc_height = 0;
  unsigned int doc_width  = 0;
  zathura_document_widget_get_document_size(zathura->ui.document_widget, &doc_height, &doc_width);
  unsigned int view_height = 0;
  unsigned int view_width  = 0;
  zathura_document_widget_get_viewport_size(zathura->ui.document_widget, &view_height, &view_width);
  if (doc_height == 0 || doc_width == 0) {
    return false;
  }

  *half_w = view_width / 2.0 / doc_width;
  *half_h = view_height / 2.0 / doc_height;
  return true;
}

/* scroll so that the caret is visible, keeping a margin of a few lines */
static void visual_scroll_to_caret(zathura_t* zathura) {
  zathura_visual_t* visual          = zathura->visual;
  zathura_document_t* document      = zathura_get_document(zathura);
  const zathura_visual_text_t* text = visual_current_text(zathura);
  double half_w                     = 0;
  double half_h                     = 0;
  if (text == NULL || text->n == 0 || visual_viewport_half(zathura, &half_w, &half_h) == false) {
    return;
  }

  const zathura_rectangle_t rect = zathura_visual_caret_rectangle(text, visual->cursor.index);
  double caret_x                 = 0;
  double caret_y                 = 0;
  visual_point_to_position(zathura, visual->cursor.page, rect, &caret_x, &caret_y);

  unsigned int doc_height = 0;
  unsigned int doc_width  = 0;
  zathura_document_widget_get_document_size(zathura->ui.document_widget, &doc_height, &doc_width);
  zathura_page_t* page             = zathura_document_get_page(document, visual->cursor.page);
  const zathura_rectangle_t scaled = zathura_page_widget_recalc_rectangle(
      zathura_document_widget_ensure_page(zathura->ui.document_widget, zathura_page_get_index(page)), rect);
  const double margin = fmin(3 * fabs(scaled.y2 - scaled.y1) / doc_height, half_h / 2);
  const double old_x  = zathura_document_widget_get_position_x(zathura->ui.document_widget);
  const double old_y  = zathura_document_widget_get_position_y(zathura->ui.document_widget);
  double new_x        = old_x;
  double new_y        = old_y;

  if (caret_y - margin < old_y - half_h) {
    new_y = caret_y - margin + half_h;
  } else if (caret_y + margin > old_y + half_h) {
    new_y = caret_y + margin - half_h;
  }
  if (fabs(caret_x - old_x) > half_w) {
    new_x = caret_x;
  }

  if (new_x != old_x || new_y != old_y) {
    position_set(zathura, new_x, new_y);
  }
}

static girara_list_t* visual_selection_rectangles(const zathura_visual_text_t* text, size_t first, size_t last) {
  girara_list_t* list = girara_list_new_with_free(g_free);
  if (list == NULL) {
    return NULL;
  }

  /* one rectangle per line */
  zathura_rectangle_t* current = NULL;
  for (size_t idx = first; idx <= last && idx < text->n; ++idx) {
    if (is_newline(text, idx) == true) {
      current = NULL;
      continue;
    }

    const zathura_rectangle_t* rect = &text->rects[idx];
    if (current == NULL) {
      current = g_memdup2(rect, sizeof(zathura_rectangle_t));
      girara_list_append(list, current);
    } else {
      current->x1 = fmin(current->x1, rect->x1);
      current->y1 = fmin(current->y1, rect->y1);
      current->x2 = fmax(current->x2, rect->x2);
      current->y2 = fmax(current->y2, rect->y2);
    }
  }

  return list;
}

/* first and last selected character */
static void visual_selection(zathura_t* zathura, visual_position_t* first, visual_position_t* last) {
  zathura_visual_t* visual = zathura->visual;
  if (visual->kind == ZATHURA_VISUAL_CARET || visual->kind == ZATHURA_VISUAL_IMAGE) {
    *first = *last = visual->cursor;
    return;
  }

  const bool forward = position_compare(&visual->anchor, &visual->cursor) <= 0;
  *first             = forward ? visual->anchor : visual->cursor;
  *last              = forward ? visual->cursor : visual->anchor;

  if (visual->kind == ZATHURA_VISUAL_LINE) {
    const zathura_visual_text_t* first_text = visual_text(zathura, &visual->texts, first->page);
    const zathura_visual_text_t* last_text  = visual_text(zathura, &visual->texts, last->page);
    if (first_text != NULL && first_text->n != 0) {
      first->index = line_start(first_text, first->index);
    }
    if (last_text != NULL && last_text->n != 0) {
      last->index = line_last(last_text, last->index);
    }
  }
}

static void visual_clear_drawn(zathura_t* zathura, visual_drawn_t* drawn) {
  if (drawn->drawn == false) {
    return;
  }

  for (unsigned int page = drawn->first; page <= drawn->last; ++page) {
    ZathuraPageWidget* widget = zathura_page_get_widget_by_number(zathura, page);
    if (widget != NULL) {
      zathura_page_widget_set_visual(widget, NULL, NULL);
    }
  }
  drawn->drawn = false;
}

/* draw a selection from first to last, or only a caret, and clear the pages drawn before */
static void visual_draw(zathura_t* zathura, visual_texts_t* texts, visual_drawn_t* drawn, visual_position_t first,
                        visual_position_t last, bool selection, const visual_position_t* caret) {
  if (caret != NULL && selection == false) {
    first = last = *caret;
  }

  /* pages that are no longer part of the selection */
  if (drawn->drawn == true) {
    for (unsigned int page = drawn->first; page <= drawn->last; ++page) {
      ZathuraPageWidget* widget = zathura_page_get_widget_by_number(zathura, page);
      if (widget != NULL && (page < first.page || page > last.page)) {
        zathura_page_widget_set_visual(widget, NULL, NULL);
      }
    }
  }

  for (unsigned int page = first.page; page <= last.page; ++page) {
    ZathuraPageWidget* widget         = zathura_page_get_widget_by_number(zathura, page);
    const zathura_visual_text_t* text = visual_text(zathura, texts, page);
    if (widget == NULL) {
      continue;
    }
    if (text == NULL || text->n == 0) {
      zathura_page_widget_set_visual(widget, NULL, NULL);
      continue;
    }

    girara_list_t* rectangles = NULL;
    if (selection == true) {
      const size_t from = page == first.page ? first.index : 0;
      const size_t to   = page == last.page ? last.index : text->n - 1;
      rectangles        = visual_selection_rectangles(text, from, to);
    }

    const bool has_caret = caret != NULL && caret->page == page;
    zathura_rectangle_t caret_rect;
    if (has_caret == true) {
      caret_rect = zathura_visual_caret_rectangle(text, caret->index);
    }
    zathura_page_widget_set_visual(widget, rectangles, has_caret == true ? &caret_rect : NULL);
  }

  drawn->drawn = true;
  drawn->first = first.page;
  drawn->last  = last.page;
}

/* highlight the selected image, without a caret */
static void visual_draw_image(zathura_t* zathura) {
  zathura_visual_t* visual = zathura->visual;
  visual_clear_drawn(zathura, &visual->drawn);

  ZathuraPageWidget* widget = zathura_page_get_widget_by_number(zathura, visual->image.page);
  girara_list_t* list       = girara_list_new_with_free(g_free);
  zathura_rectangle_t* rect = g_try_malloc0(sizeof(zathura_rectangle_t));
  if (widget == NULL || list == NULL || rect == NULL) {
    girara_list_free(list);
    g_free(rect);
    return;
  }

  *rect = visual->image.bounds;
  girara_list_append(list, rect);
  zathura_page_widget_set_visual(widget, list, NULL);
  visual->drawn.drawn = true;
  visual->drawn.first = visual->image.page;
  visual->drawn.last  = visual->image.page;
}

static void visual_redraw(zathura_t* zathura) {
  zathura_visual_t* visual = zathura->visual;
  if (visual->kind == ZATHURA_VISUAL_IMAGE) {
    visual_draw_image(zathura);
    return;
  }

  visual_position_t first;
  visual_position_t last;
  visual_selection(zathura, &first, &last);
  visual_draw(zathura, &visual->texts, &visual->drawn, first, last, visual->kind != ZATHURA_VISUAL_CARET,
              &visual->cursor);
  visual_scroll_to_caret(zathura);
}

static void visual_notify_kind(zathura_t* zathura) {
  switch (zathura->visual->kind) {
  case ZATHURA_VISUAL_CARET:
    girara_notify(zathura->ui.session, GIRARA_INFO, _("-- CARET --"));
    break;
  case ZATHURA_VISUAL_CHAR:
    girara_notify(zathura->ui.session, GIRARA_INFO, _("-- VISUAL --"));
    break;
  case ZATHURA_VISUAL_LINE:
    girara_notify(zathura->ui.session, GIRARA_INFO, _("-- VISUAL LINE --"));
    break;
  case ZATHURA_VISUAL_IMAGE:
    girara_notify(zathura->ui.session, GIRARA_INFO, _("-- IMAGE --"));
    break;
  }
}

static void visual_image_clear(zathura_visual_t* visual) {
  girara_list_free(visual->image.list);
  visual->image.list  = NULL;
  visual->image.image = NULL;
}

/* anything but copying goes back to text, at the position of the image */
static void visual_leave_image(zathura_t* zathura) {
  zathura_visual_t* visual = zathura->visual;
  if (visual->kind != ZATHURA_VISUAL_IMAGE) {
    return;
  }

  visual_image_clear(visual);
  visual->kind   = ZATHURA_VISUAL_CARET;
  visual->anchor = visual->cursor;
  visual_notify_kind(zathura);
}

/* first character below the top of the viewport, or of the current search result if there is one on the page */
static size_t visual_start_position(zathura_t* zathura, unsigned int page_index, const zathura_visual_text_t* text) {
  ZathuraPageWidget* widget = zathura_page_get_widget_by_number(zathura, page_index);
  if (widget != NULL) {
    int current            = -1;
    girara_list_t* results = NULL;
    g_object_get(G_OBJECT(widget), "search-current", &current, "search-results", &results, NULL);
    if (results != NULL && current >= 0 && (size_t)current < girara_list_size(results)) {
      const zathura_rectangle_t* match = girara_list_nth(results, current);
      for (size_t idx = 0; idx < text->n; ++idx) {
        const double x = center_x(text, idx);
        const double y = (text->rects[idx].y1 + text->rects[idx].y2) / 2;
        if (x >= match->x1 && x <= match->x2 && y >= match->y1 && y <= match->y2) {
          return idx;
        }
      }
    }
  }

  double half_w = 0;
  double half_h = 0;
  visual_viewport_half(zathura, &half_w, &half_h);
  const double top = zathura_document_widget_get_position_y(zathura->ui.document_widget) - half_h;

  size_t fallback = text->n;
  for (size_t idx = 0; idx < text->n; ++idx) {
    if (char_class(text, idx, true) == 0) {
      continue;
    }
    if (fallback == text->n) {
      fallback = idx;
    }
    double x = 0;
    double y = 0;
    visual_point_to_position(zathura, page_index, text->rects[idx], &x, &y);
    if (y >= top) {
      return idx;
    }
  }
  return fallback != text->n ? fallback : 0;
}

static void visual_free(zathura_visual_t* visual) {
  visual_image_clear(visual);
  visual_texts_clear(&visual->texts);
  girara_list_free(visual->labels.list);
  g_free(visual);
}

static zathura_visual_t* visual_new(zathura_t* zathura) {
  zathura_visual_t* visual = g_try_malloc0(sizeof(zathura_visual_t));
  if (visual == NULL) {
    return NULL;
  }
  if (visual_texts_init(zathura, &visual->texts) == false) {
    visual_free(visual);
    return NULL;
  }

  visual->curswant = -1;
  return visual;
}

static void visual_activate(zathura_t* zathura, zathura_visual_t* visual) {
  visual->previous_mode = girara_mode_get(zathura->ui.session);
  zathura->visual       = visual;

  girara_mode_set(zathura->ui.session, zathura->modes.visual);
  visual_notify_kind(zathura);
  visual_redraw(zathura);
}

bool zathura_visual_enter(zathura_t* zathura, zathura_visual_kind_t kind) {
  g_return_val_if_fail(zathura != NULL, false);

  if (zathura_has_document(zathura) == false) {
    return false;
  }

  if (zathura->visual != NULL) {
    zathura_visual_toggle(zathura, kind);
    return true;
  }

  zathura_document_t* document      = zathura_get_document(zathura);
  const unsigned int n_pages        = zathura_document_get_number_of_pages(document);
  const unsigned int current        = zathura_document_widget_get_current_page_number(zathura->ui.document_widget);
  zathura_visual_t* visual          = visual_new(zathura);
  const zathura_visual_text_t* text = NULL;
  if (visual == NULL) {
    return false;
  }

  /* start on the current page, or the next one with text */
  unsigned int page_index = current;
  for (; page_index < n_pages; ++page_index) {
    text = visual_text(zathura, &visual->texts, page_index);
    if (text == NULL || text->n != 0) {
      break;
    }
  }
  if (text == NULL || text->n == 0) {
    if (text != NULL) {
      girara_notify(zathura->ui.session, GIRARA_ERROR, _("No text found from page %u on."), current + 1);
    }
    visual_free(visual);
    return false;
  }

  visual->cursor.page  = page_index;
  visual->cursor.index = visual_start_position(zathura, page_index, text);
  visual->anchor       = visual->cursor;
  visual->kind         = kind;
  visual_activate(zathura, visual);
  return true;
}

static void visual_labels_close(zathura_t* zathura);

void zathura_visual_exit(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL) {
    return;
  }

  visual_labels_close(zathura);
  visual_clear_drawn(zathura, &visual->drawn);

  /* remember the selection of text for gv */
  if (visual->kind == ZATHURA_VISUAL_CHAR || visual->kind == ZATHURA_VISUAL_LINE) {
    if (zathura->visual_last == NULL) {
      zathura->visual_last = g_try_malloc0(sizeof(struct zathura_visual_last_s));
    }
    if (zathura->visual_last != NULL) {
      zathura->visual_last->cursor = visual->cursor;
      zathura->visual_last->anchor = visual->anchor;
      zathura->visual_last->kind   = visual->kind;
    }
  }

  const girara_mode_t mode = visual->previous_mode;
  visual_free(visual);
  zathura->visual = NULL;

  if (girara_mode_get(zathura->ui.session) == zathura->modes.visual) {
    girara_mode_set(zathura->ui.session, mode != zathura->modes.visual ? mode : zathura->modes.normal);
  }
}

static void visual_forget(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);

  g_free(zathura->visual_last);
  zathura->visual_last = NULL;
}

bool zathura_visual_reselect(zathura_t* zathura) {
  g_return_val_if_fail(zathura != NULL, false);

  if (zathura_has_document(zathura) == false || zathura->visual != NULL) {
    return false;
  }

  const struct zathura_visual_last_s* last = zathura->visual_last;
  if (last == NULL) {
    girara_notify(zathura->ui.session, GIRARA_WARNING, _("There is no previous selection."));
    return false;
  }

  zathura_visual_t* visual = visual_new(zathura);
  if (visual == NULL) {
    return false;
  }

  const zathura_visual_text_t* cursor_text = visual_text(zathura, &visual->texts, last->cursor.page);
  const zathura_visual_text_t* anchor_text = visual_text(zathura, &visual->texts, last->anchor.page);
  if (cursor_text == NULL || anchor_text == NULL || last->cursor.index >= cursor_text->n ||
      last->anchor.index >= anchor_text->n) {
    visual_free(visual);
    return false;
  }

  visual->cursor = last->cursor;
  visual->anchor = last->anchor;
  visual->kind   = last->kind;
  visual_activate(zathura, visual);
  return true;
}

void zathura_visual_toggle(zathura_t* zathura, zathura_visual_kind_t kind) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL) {
    return;
  }
  visual->yank_pending = false;
  visual_leave_image(zathura);

  if (visual->kind == kind) {
    visual->kind = ZATHURA_VISUAL_CARET;
  } else {
    /* a selection starts at the caret */
    if (visual->kind == ZATHURA_VISUAL_CARET) {
      visual->anchor = visual->cursor;
    }
    visual->kind = kind;
  }

  visual_notify_kind(zathura);
  visual_redraw(zathura);
}

/* continue a motion on the next or previous page with a position for it */
static bool visual_cross_page(zathura_t* zathura, zathura_visual_motion_t motion) {
  zathura_visual_t* visual = zathura->visual;

  bool forward = false;
  switch (motion) {
  case ZATHURA_VISUAL_MOTION_DOWN:
  case ZATHURA_VISUAL_MOTION_WORD_FORWARD:
  case ZATHURA_VISUAL_MOTION_WORD_END:
  case ZATHURA_VISUAL_MOTION_BIGWORD_FORWARD:
  case ZATHURA_VISUAL_MOTION_BIGWORD_END:
    forward = true;
    break;
  case ZATHURA_VISUAL_MOTION_UP:
  case ZATHURA_VISUAL_MOTION_WORD_BACKWARD:
  case ZATHURA_VISUAL_MOTION_BIGWORD_BACKWARD:
    forward = false;
    break;
  default:
    return false;
  }

  const unsigned int n_pages = zathura_document_get_number_of_pages(zathura_get_document(zathura));
  const unsigned int start   = visual->cursor.page;
  for (unsigned int page = forward ? start + 1 : start - 1; page < n_pages; page = forward ? page + 1 : page - 1) {
    const zathura_visual_text_t* text = visual_text(zathura, &visual->texts, page);
    if (text == NULL) {
      return false;
    }

    const size_t pos = zathura_visual_page_entry(text, motion, visual->curswant);
    if (pos < text->n) {
      visual->cursor.page  = page;
      visual->cursor.index = pos;
      return true;
    }
  }

  return false;
}

/* move to the first or last line with text of the document */
static bool visual_document_edge(zathura_t* zathura, bool top) {
  zathura_visual_t* visual   = zathura->visual;
  const unsigned int n_pages = zathura_document_get_number_of_pages(zathura_get_document(zathura));

  for (unsigned int idx = 0; idx < n_pages; ++idx) {
    const unsigned int page           = top ? idx : n_pages - 1 - idx;
    const zathura_visual_text_t* text = visual_text(zathura, &visual->texts, page);
    if (text == NULL) {
      return false;
    }
    if (text->n == 0) {
      continue;
    }

    visual->cursor.page  = page;
    visual->cursor.index = zathura_visual_motion(
        text, 0, top ? ZATHURA_VISUAL_MOTION_TOP : ZATHURA_VISUAL_MOTION_BOTTOM, &visual->curswant);
    return true;
  }

  return false;
}

static void visual_apply_motion(zathura_t* zathura, zathura_visual_motion_t motion, unsigned int count) {
  zathura_visual_t* visual = zathura->visual;

  for (unsigned int idx = 0; idx < MAX(count, 1); ++idx) {
    const zathura_visual_text_t* text = visual_current_text(zathura);
    if (text == NULL) {
      break;
    }

    const size_t pos = zathura_visual_motion(text, visual->cursor.index, motion, &visual->curswant);
    if (pos != visual->cursor.index) {
      visual->cursor.index = pos;
    } else if (visual_cross_page(zathura, motion) == false) {
      break;
    }
  }
}

static void visual_yank_motion(zathura_t* zathura, visual_position_t origin, zathura_visual_motion_t motion);

void zathura_visual_move(zathura_t* zathura, zathura_visual_motion_t motion, unsigned int count) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL) {
    return;
  }

  /* after y, the motion tells what to copy; counts multiply, like 2y3w */
  const bool yank                = visual->yank_pending;
  const visual_position_t origin = visual->cursor;
  if (yank == true) {
    count = MAX(visual->yank_count, 1) * MAX(count, 1);
  }
  visual->yank_pending = false;
  visual_leave_image(zathura);

  if (motion == ZATHURA_VISUAL_MOTION_TOP || motion == ZATHURA_VISUAL_MOTION_BOTTOM) {
    visual_document_edge(zathura, motion == ZATHURA_VISUAL_MOTION_TOP);
  } else {
    /* 3$ goes to the end of the third line */
    if (motion == ZATHURA_VISUAL_MOTION_LINE_END && count > 1) {
      visual_apply_motion(zathura, ZATHURA_VISUAL_MOTION_DOWN, count - 1);
      count = 1;
    }
    visual_apply_motion(zathura, motion, count);
  }

  if (yank == true) {
    visual_yank_motion(zathura, origin, motion);
  }

  if (visual->kind == ZATHURA_VISUAL_CARET) {
    visual->anchor = visual->cursor;
  }
  visual_redraw(zathura);
}

void zathura_visual_swap(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL || visual->kind == ZATHURA_VISUAL_CARET) {
    return;
  }
  if (visual->kind == ZATHURA_VISUAL_IMAGE) {
    visual_leave_image(zathura);
    visual_redraw(zathura);
    return;
  }

  visual->yank_pending        = false;
  const visual_position_t tmp = visual->cursor;
  visual->cursor              = visual->anchor;
  visual->anchor              = tmp;
  visual->curswant            = -1;
  visual_redraw(zathura);
}

/* text from first to last, with a newline between pages */
static char* visual_selected_text(zathura_t* zathura, visual_position_t first, visual_position_t last) {
  zathura_document_t* document = zathura_get_document(zathura);
  GString* selected            = g_string_new(NULL);

  for (unsigned int page_index = first.page; page_index <= last.page; ++page_index) {
    zathura_page_t* page                = zathura_document_get_page(document, page_index);
    const zathura_text_layout_t* layout = page != NULL ? zathura_page_get_text_layout(page, NULL) : NULL;
    if (layout == NULL || layout->n_rects == 0) {
      continue;
    }

    const size_t from = page_index == first.page ? first.index : 0;
    const size_t to   = page_index == last.page ? last.index : layout->n_rects - 1;
    if (from > to || to >= layout->n_rects) {
      continue;
    }

    if (selected->len != 0 && selected->str[selected->len - 1] != '\n') {
      g_string_append_c(selected, '\n');
    }
    const char* start = g_utf8_offset_to_pointer(layout->text, from);
    const char* end   = g_utf8_offset_to_pointer(layout->text, to + 1);
    g_string_append_len(selected, start, end - start);
  }

  while (selected->len != 0 && selected->str[selected->len - 1] == '\n') {
    g_string_truncate(selected, selected->len - 1);
  }
  return g_string_free(selected, FALSE);
}

static void visual_copy(zathura_t* zathura, visual_position_t first, visual_position_t last) {
  g_autofree char* selected = visual_selected_text(zathura, first, last);
  if (*selected != '\0') {
    copy_text_to_selection(zathura, selected);
  }
}

static void visual_flash_clear(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_flash_t* flash = zathura->visual_flash;
  if (flash == NULL) {
    return;
  }

  g_clear_handle_id(&flash->source, g_source_remove);
  if (flash->drawn.drawn == true) {
    for (unsigned int page = flash->drawn.first; page <= flash->drawn.last; ++page) {
      ZathuraPageWidget* widget = zathura_page_get_widget_by_number(zathura, page);
      if (widget != NULL) {
        zathura_page_widget_set_flash(widget, NULL);
      }
    }
  }

  g_free(flash);
  zathura->visual_flash = NULL;
}

static gboolean visual_flash_timeout(gpointer data) {
  zathura_t* zathura            = data;
  zathura->visual_flash->source = 0;
  visual_flash_clear(zathura);
  return G_SOURCE_REMOVE;
}

/* briefly highlight copied text, like the yank highlight of LazyVim */
/* replace the flash with a new one, or return NULL when the flash is turned off */
static zathura_visual_flash_t* visual_flash_new(zathura_t* zathura, int* duration) {
  visual_flash_clear(zathura);

  *duration = 150;
  girara_setting_get(zathura->ui.session, "visual-flash-duration", duration);
  if (*duration <= 0) {
    return NULL;
  }
  return g_try_malloc0(sizeof(zathura_visual_flash_t));
}

static void visual_flash_start(zathura_t* zathura, zathura_visual_flash_t* flash, unsigned int first_page,
                               unsigned int last_page, int duration) {
  flash->drawn.drawn    = true;
  flash->drawn.first    = first_page;
  flash->drawn.last     = last_page;
  flash->source         = g_timeout_add(duration, visual_flash_timeout, zathura);
  zathura->visual_flash = flash;
}

static void visual_flash(zathura_t* zathura, visual_texts_t* texts, visual_position_t first, visual_position_t last) {
  int duration                  = 0;
  zathura_visual_flash_t* flash = visual_flash_new(zathura, &duration);
  if (flash == NULL) {
    return;
  }

  for (unsigned int page = first.page; page <= last.page; ++page) {
    ZathuraPageWidget* widget         = zathura_page_get_widget_by_number(zathura, page);
    const zathura_visual_text_t* text = visual_text(zathura, texts, page);
    if (widget == NULL || text == NULL || text->n == 0) {
      continue;
    }

    const size_t from = page == first.page ? first.index : 0;
    const size_t to   = page == last.page ? last.index : text->n - 1;
    zathura_page_widget_set_flash(widget, visual_selection_rectangles(text, from, to));
  }

  visual_flash_start(zathura, flash, first.page, last.page, duration);
}

/* copy the selected image, like the copy entry of the image menu */
static void visual_copy_image(zathura_t* zathura) {
  zathura_visual_t* visual = zathura->visual;
  zathura_page_t* page     = zathura_document_get_page(zathura_get_document(zathura), visual->image.page);
  cairo_surface_t* surface = page != NULL ? zathura_page_image_get_cairo(page, visual->image.image, NULL) : NULL;
  if (surface == NULL) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to copy the image."));
    return;
  }

  GdkTexture* texture = texture_from_surface(surface);
  cairo_surface_destroy(surface);
  if (texture == NULL) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to copy the image."));
    return;
  }
  copy_image_to_selection(zathura, texture);
  g_object_unref(texture);

  int duration                  = 0;
  zathura_visual_flash_t* flash = visual_flash_new(zathura, &duration);
  ZathuraPageWidget* widget     = zathura_page_get_widget_by_number(zathura, visual->image.page);
  girara_list_t* rects          = girara_list_new_with_free(g_free);
  zathura_rectangle_t* rect     = g_try_malloc0(sizeof(zathura_rectangle_t));
  if (flash == NULL || widget == NULL || rects == NULL || rect == NULL) {
    g_free(flash);
    girara_list_free(rects);
    g_free(rect);
    return;
  }

  *rect = visual->image.bounds;
  girara_list_append(rects, rect);
  zathura_page_widget_set_flash(widget, rects);
  visual_flash_start(zathura, flash, visual->image.page, visual->image.page, duration);
}

/* the text y takes from the caret having moved from origin; false if there is none */
static bool visual_operator_range(zathura_t* zathura, visual_position_t origin, zathura_visual_operator_range_t range,
                                  visual_position_t* first, visual_position_t* last) {
  zathura_visual_t* visual = zathura->visual;
  const bool forward       = position_compare(&origin, &visual->cursor) <= 0;
  *first                   = forward ? origin : visual->cursor;
  *last                    = forward ? visual->cursor : origin;

  const zathura_visual_text_t* first_text = visual_text(zathura, &visual->texts, first->page);
  const zathura_visual_text_t* last_text  = visual_text(zathura, &visual->texts, last->page);
  if (first_text == NULL || last_text == NULL || first_text->n == 0 || last_text->n == 0) {
    return false;
  }
  if (first->page == last->page) {
    return zathura_visual_operator_range(first_text, first->index, last->index, range, &first->index, &last->index);
  }

  switch (range) {
  case ZATHURA_VISUAL_OPERATOR_LINEWISE:
    first->index = line_start(first_text, first->index);
    last->index  = line_last(last_text, last->index);
    return true;
  case ZATHURA_VISUAL_OPERATOR_INCLUSIVE:
    return true;
  case ZATHURA_VISUAL_OPERATOR_EXCLUSIVE:
    if (last->index != 0) {
      last->index = exclusive_end(last_text, last->index);
      return true;
    }
    break;
  }

  /* an exclusive motion that ends at the start of a page takes the text before up to its end */
  for (unsigned int page = last->page; page-- > first->page;) {
    const zathura_visual_text_t* text = visual_text(zathura, &visual->texts, page);
    if (text == NULL) {
      return false;
    }
    if (text->n == 0) {
      continue;
    }
    last->page  = page;
    last->index = text_last(text);
    return page != first->page || last->index >= first->index;
  }
  return false;
}

/* copy the text, highlight it for a moment and put the caret at its start, like y in vim */
static void visual_yank_range(zathura_t* zathura, visual_position_t origin, zathura_visual_operator_range_t range) {
  zathura_visual_t* visual = zathura->visual;
  visual_position_t first  = origin;
  visual_position_t last   = origin;
  if (visual_operator_range(zathura, origin, range, &first, &last) == false) {
    visual->cursor = origin;
    return;
  }

  visual_copy(zathura, first, last);
  visual_flash(zathura, &visual->texts, first, last);

  /* the caret goes to where the text starts; after yk that is where k went, so the column is kept */
  if (position_compare(&origin, &visual->cursor) < 0) {
    visual->cursor = origin;
  }
  if (range != ZATHURA_VISUAL_OPERATOR_LINEWISE) {
    visual->curswant = -1;
  }
  visual->anchor = visual->cursor;
}

/* yw and yW stop at the end of the line as in vim, without the blanks before the word on the next line, and take
 * the last word of the page, after which w stops on the last character instead of the start of a word */
static zathura_visual_operator_range_t visual_word_range(zathura_t* zathura, visual_position_t origin, bool big) {
  zathura_visual_t* visual          = zathura->visual;
  const zathura_visual_text_t* text = visual_current_text(zathura);
  const size_t pos                  = visual->cursor.index;
  if (text == NULL || text->n == 0 || position_compare(&origin, &visual->cursor) >= 0) {
    return ZATHURA_VISUAL_OPERATOR_EXCLUSIVE;
  }

  const int cls         = char_class(text, pos, big);
  const bool word_start = cls != 0 && (pos == 0 || char_class(text, pos - 1, big) != cls);
  if (pos == text_last(text) && word_start == false) {
    return ZATHURA_VISUAL_OPERATOR_INCLUSIVE;
  }

  const size_t start = line_start(text, pos);
  if (start != pos && line_first_nonblank(text, pos) == pos &&
      (origin.page != visual->cursor.page || start > origin.index)) {
    visual->cursor.index = start;
  }
  return ZATHURA_VISUAL_OPERATOR_EXCLUSIVE;
}

static void visual_yank_motion(zathura_t* zathura, visual_position_t origin, zathura_visual_motion_t motion) {
  zathura_visual_t* visual = zathura->visual;
  const bool moved         = position_compare(&origin, &visual->cursor) != 0;

  zathura_visual_operator_range_t range = ZATHURA_VISUAL_OPERATOR_EXCLUSIVE;
  switch (motion) {
  case ZATHURA_VISUAL_MOTION_UP:
  case ZATHURA_VISUAL_MOTION_DOWN:
    if (moved == false) {
      /* yj on the last line copies nothing, as in vim */
      return;
    }
    range = ZATHURA_VISUAL_OPERATOR_LINEWISE;
    break;
  case ZATHURA_VISUAL_MOTION_TOP:
  case ZATHURA_VISUAL_MOTION_BOTTOM:
    range = ZATHURA_VISUAL_OPERATOR_LINEWISE;
    break;
  case ZATHURA_VISUAL_MOTION_WORD_END:
  case ZATHURA_VISUAL_MOTION_BIGWORD_END:
  case ZATHURA_VISUAL_MOTION_LINE_END:
    range = ZATHURA_VISUAL_OPERATOR_INCLUSIVE;
    break;
  case ZATHURA_VISUAL_MOTION_WORD_FORWARD:
  case ZATHURA_VISUAL_MOTION_BIGWORD_FORWARD:
    /* at the end of the text, yw still takes the character under the caret */
    range = moved == true ? visual_word_range(zathura, origin, motion == ZATHURA_VISUAL_MOTION_BIGWORD_FORWARD)
                          : ZATHURA_VISUAL_OPERATOR_INCLUSIVE;
    break;
  case ZATHURA_VISUAL_MOTION_RIGHT:
    /* yl at the end of the line takes its last character */
    range = moved == true ? ZATHURA_VISUAL_OPERATOR_EXCLUSIVE : ZATHURA_VISUAL_OPERATOR_INCLUSIVE;
    break;
  default:
    break;
  }

  visual_yank_range(zathura, origin, range);
}

/* copy the selection, or its whole lines, and leave visual mode */
static void visual_yank_selection(zathura_t* zathura, bool lines) {
  zathura_visual_t* visual = zathura->visual;
  if (visual->kind == ZATHURA_VISUAL_IMAGE) {
    visual_copy_image(zathura);
    zathura_visual_exit(zathura);
    return;
  }

  visual_position_t first = visual->cursor;
  visual_position_t last  = visual->cursor;
  visual_selection(zathura, &first, &last);
  if (lines == true) {
    const zathura_visual_text_t* first_text = visual_text(zathura, &visual->texts, first.page);
    const zathura_visual_text_t* last_text  = visual_text(zathura, &visual->texts, last.page);
    if (first_text != NULL && first_text->n != 0) {
      first.index = line_start(first_text, first.index);
    }
    if (last_text != NULL && last_text->n != 0) {
      last.index = line_last(last_text, last.index);
    }
  }

  visual_copy(zathura, first, last);
  /* the flash keeps the selection visible for a moment after visual mode ends */
  visual_flash(zathura, &visual->texts, first, last);
  zathura_visual_exit(zathura);
}

void zathura_visual_yank(zathura_t* zathura, unsigned int count) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL) {
    return;
  }

  if (visual->kind != ZATHURA_VISUAL_CARET) {
    visual_yank_selection(zathura, false);
    return;
  }

  /* without a selection, y waits for a motion or text object, and yy copies whole lines and keeps the caret */
  if (visual->yank_pending == false) {
    visual->yank_pending = true;
    visual->yank_count   = count;
    return;
  }

  visual->yank_pending              = false;
  const zathura_visual_text_t* text = visual_current_text(zathura);
  if (text != NULL) {
    visual_position_t first = visual->cursor;
    visual_position_t last  = visual->cursor;
    zathura_visual_lines(text, visual->cursor.index, MAX(visual->yank_count, 1) * MAX(count, 1), &first.index,
                         &last.index);
    visual_copy(zathura, first, last);
    visual_flash(zathura, &visual->texts, first, last);
  }
}

void zathura_visual_yank_line_end(zathura_t* zathura, unsigned int count) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL) {
    return;
  }

  if (visual->kind != ZATHURA_VISUAL_CARET) {
    visual_yank_selection(zathura, true);
    return;
  }

  /* Y is y$ */
  visual->yank_pending = true;
  visual->yank_count   = count;
  zathura_visual_move(zathura, ZATHURA_VISUAL_MOTION_LINE_END, 1);
}

bool zathura_visual_cancel(zathura_t* zathura) {
  g_return_val_if_fail(zathura != NULL, false);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL || visual->yank_pending == false) {
    return false;
  }
  visual->yank_pending = false;
  return true;
}

void zathura_visual_find_char(zathura_t* zathura, gunichar c, bool forward, bool till, unsigned int count) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL) {
    return;
  }
  const bool yank                = visual->yank_pending;
  const visual_position_t origin = visual->cursor;
  if (yank == true) {
    count = MAX(visual->yank_count, 1) * MAX(count, 1);
  }
  visual->yank_pending = false;
  visual_leave_image(zathura);

  const bool repeat = c == 0;
  if (repeat == true) {
    if (visual->find.c == 0) {
      return;
    }
    c       = visual->find.c;
    till    = visual->find.till;
    forward = forward == visual->find.forward;
  } else {
    visual->find.c       = c;
    visual->find.forward = forward;
    visual->find.till    = till;
  }

  const zathura_visual_text_t* text = visual_current_text(zathura);
  if (text == NULL) {
    return;
  }

  visual->cursor.index = zathura_visual_find(text, visual->cursor.index, c, forward, till, repeat, count);
  visual->curswant     = -1;
  /* yf and yt take the character they end on, yF and yT do not; nothing is copied if the character is not found */
  if (yank == true && visual->cursor.index != origin.index) {
    visual_yank_range(zathura, origin,
                      forward == true ? ZATHURA_VISUAL_OPERATOR_INCLUSIVE : ZATHURA_VISUAL_OPERATOR_EXCLUSIVE);
  }
  if (visual->kind == ZATHURA_VISUAL_CARET) {
    visual->anchor = visual->cursor;
  }
  visual_redraw(zathura);
}

void zathura_visual_select_word(zathura_t* zathura, bool big, bool around) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL) {
    return;
  }
  const bool yank      = visual->yank_pending;
  visual->yank_pending = false;
  visual_leave_image(zathura);

  const zathura_visual_text_t* text = visual_current_text(zathura);
  if (text == NULL) {
    return;
  }

  size_t first = 0;
  size_t last  = 0;
  zathura_visual_word_object(text, visual->cursor.index, big, around, &first, &last);

  /* yiw copies the word instead of selecting it */
  if (yank == true) {
    const visual_position_t start = {visual->cursor.page, first};
    visual->cursor.index          = last;
    visual_yank_range(zathura, start, ZATHURA_VISUAL_OPERATOR_INCLUSIVE);
    visual_redraw(zathura);
    return;
  }

  const bool notify    = visual->kind != ZATHURA_VISUAL_CHAR;
  visual->kind         = ZATHURA_VISUAL_CHAR;
  visual->anchor.page  = visual->cursor.page;
  visual->anchor.index = first;
  visual->cursor.index = last;
  visual->curswant     = -1;
  if (notify == true) {
    visual_notify_kind(zathura);
  }
  visual_redraw(zathura);
}

/* Labels */

/* home row first, like flash.nvim */
static const char label_alphabet[] = "asdfghjklqwertyuiopzxcvbnm";

typedef struct visual_label_candidate_s {
  visual_label_t label; /**< The label, without its text */
  double distance;      /**< Distance from the caret */
} visual_label_candidate_t;

/* images smaller than this, in page coordinates, are decorations and get no label */
static const double image_min_size = 16;

/* height of a label on screen, to keep the labels of images apart */
static const double label_height = 18;

static int candidate_compare(const void* a, const void* b) {
  const double da = ((const visual_label_candidate_t*)a)->distance;
  const double db = ((const visual_label_candidate_t*)b)->distance;
  return (da > db) - (da < db);
}

static void visual_labels_clear(zathura_t* zathura) {
  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL || visual->labels.list == NULL) {
    return;
  }

  for (unsigned int page = visual->labels.first_page; page <= visual->labels.last_page; ++page) {
    ZathuraPageWidget* widget = zathura_page_get_widget_by_number(zathura, page);
    if (widget != NULL) {
      zathura_page_widget_set_labels(widget, NULL);
    }
  }

  girara_list_free(visual->labels.list);
  visual->labels.list = NULL;
}

static gboolean visual_labels_hide_idle(gpointer data) {
  GtkWidget* dialog = data;
  gtk_widget_set_visible(dialog, FALSE);
  g_object_unref(dialog);
  return G_SOURCE_REMOVE;
}

static void visual_labels_close(zathura_t* zathura) {
  zathura_visual_t* visual = zathura->visual;
  if (visual != NULL && visual->labels.dialog != NULL) {
    gtk_widget_set_visible(GTK_WIDGET(visual->labels.dialog), FALSE);
  }
  visual_labels_clear(zathura);
}

static void cb_visual_labels_hide(GtkWidget* UNUSED(dialog), zathura_t* zathura) {
  if (zathura->visual != NULL) {
    zathura->visual->labels.dialog = NULL;
    /* closing the labels without choosing one cancels ys */
    zathura->visual->yank_pending = false;
  }
  visual_labels_clear(zathura);
}

/* select the image of a label; the caret goes to the text closest to it, for when the image is left */
static void visual_select_image(zathura_t* zathura, const visual_label_t* label) {
  zathura_visual_t* visual = zathura->visual;
  zathura_page_t* page     = zathura_document_get_page(zathura_get_document(zathura), label->position.page);
  girara_list_t* images    = page != NULL ? zathura_page_images_get(page, NULL) : NULL;
  zathura_image_t* image   = images != NULL ? girara_list_nth(images, label->image_index) : NULL;
  if (image == NULL) {
    girara_list_free(images);
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to select the image."));
    return;
  }

  visual_image_clear(visual);
  visual->image.list   = images;
  visual->image.image  = image;
  visual->image.page   = label->position.page;
  visual->image.bounds = label->bounds;

  const zathura_visual_text_t* text = visual_text(zathura, &visual->texts, label->position.page);
  if (text != NULL && text->n != 0) {
    visual->cursor.page  = label->position.page;
    visual->cursor.index = zathura_visual_index_at(text, label->bounds.x1, label->bounds.y1);
  }
  visual->anchor   = visual->cursor;
  visual->curswant = -1;
  visual->kind     = ZATHURA_VISUAL_IMAGE;
  visual_notify_kind(zathura);
}

static void cb_visual_labels_changed(GtkEditable* entry, zathura_t* zathura) {
  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL || visual->labels.list == NULL || visual->labels.dialog == NULL) {
    return;
  }

  const char* input = gtk_editable_get_text(entry);
  bool prefix       = false;
  for (size_t idx = 0; idx != girara_list_size(visual->labels.list); ++idx) {
    const visual_label_t* label = girara_list_nth(visual->labels.list, idx);
    if (g_strcmp0(label->text, input) == 0) {
      const bool yank      = visual->yank_pending;
      visual->yank_pending = false;
      if (label->image == true) {
        visual_select_image(zathura, label);
      } else {
        visual_leave_image(zathura);
        const visual_position_t origin = visual->cursor;
        visual->cursor                 = label->position;
        visual->curswant               = -1;
        /* ys copies up to the word, like y with a search in vim */
        if (yank == true && position_compare(&origin, &visual->cursor) != 0) {
          visual_yank_range(zathura, origin, ZATHURA_VISUAL_OPERATOR_EXCLUSIVE);
        }
        if (visual->kind == ZATHURA_VISUAL_CARET) {
          visual->anchor = visual->cursor;
        }
      }
      prefix = false;
      visual_labels_clear(zathura);
      visual_redraw(zathura);
      break;
    }
    if (g_str_has_prefix(label->text, input) == TRUE) {
      prefix = true;
    }
  }

  /* close the input once a label is chosen or nothing can match anymore; not from within its own signal */
  if (prefix == false) {
    g_idle_add(visual_labels_hide_idle, g_object_ref(visual->labels.dialog));
  }
}

/* the top left corner of the visible part of an image, found on a grid over the image; false if none is visible */
static bool visual_image_anchor(zathura_t* zathura, unsigned int page_index, zathura_rectangle_t bounds, double view_x,
                                double view_y, double half_w, double half_h, zathura_rectangle_t* anchor, double* x,
                                double* y) {
  static const int steps = 8;
  for (int row = 0; row < steps; ++row) {
    for (int column = 0; column < steps; ++column) {
      const double px                 = bounds.x1 + (bounds.x2 - bounds.x1) * column / steps;
      const double py                 = bounds.y1 + (bounds.y2 - bounds.y1) * row / steps;
      const zathura_rectangle_t point = {px, py, px, py};
      visual_point_to_position(zathura, page_index, point, x, y);
      if (fabs(*x - view_x) <= half_w && fabs(*y - view_y) <= half_h) {
        *anchor = point;
        return true;
      }
    }
  }
  return false;
}

/* the start of every visible word, with its distance from the caret */
static girara_list_t* visual_label_candidates(zathura_t* zathura) {
  zathura_visual_t* visual     = zathura->visual;
  zathura_document_t* document = zathura_get_document(zathura);
  double half_w                = 0;
  double half_h                = 0;
  if (visual_viewport_half(zathura, &half_w, &half_h) == false) {
    return NULL;
  }
  const double view_x = zathura_document_widget_get_position_x(zathura->ui.document_widget);
  const double view_y = zathura_document_widget_get_position_y(zathura->ui.document_widget);

  const zathura_visual_text_t* cursor_text = visual_current_text(zathura);
  double caret_x                           = view_x;
  double caret_y                           = view_y;
  if (cursor_text != NULL && cursor_text->n != 0) {
    visual_point_to_position(zathura, visual->cursor.page, cursor_text->rects[visual->cursor.index], &caret_x,
                             &caret_y);
  }

  girara_list_t* candidates = girara_list_new_with_free(g_free);
  if (candidates == NULL) {
    return NULL;
  }

  const unsigned int n_pages = zathura_document_get_number_of_pages(document);
  for (unsigned int page_index = 0; page_index < n_pages; ++page_index) {
    zathura_page_t* page = zathura_document_get_page(document, page_index);
    if (page == NULL || zathura_document_widget_page_is_visible(zathura->ui.document_widget, page_index) == false) {
      continue;
    }

    const zathura_visual_text_t* text = visual_text(zathura, &visual->texts, page_index);
    if (text == NULL) {
      continue;
    }

    for (size_t idx = 0; idx < text->n; ++idx) {
      const int cls = char_class(text, idx, false);
      if (cls != 2 || (idx > 0 && char_class(text, idx - 1, false) == cls)) {
        continue;
      }

      double x = 0;
      double y = 0;
      visual_point_to_position(zathura, page_index, text->rects[idx], &x, &y);
      if (fabs(x - view_x) > half_w || fabs(y - view_y) > half_h) {
        continue;
      }

      visual_label_candidate_t* candidate = g_try_malloc0(sizeof(visual_label_candidate_t));
      if (candidate == NULL) {
        continue;
      }
      candidate->label.position.page  = page_index;
      candidate->label.position.index = idx;
      candidate->distance =
          (x - caret_x) * (x - caret_x) / (half_w * half_w) + (y - caret_y) * (y - caret_y) / (half_h * half_h);
      girara_list_append(candidates, candidate);
    }

    /* images with a visible part; images often reach beyond the page, so only their part on the page counts */
    const double page_width  = zathura_page_get_width(page);
    const double page_height = zathura_page_get_height(page);
    const double label_step  = label_height / zathura_document_widget_get_scale(zathura->ui.document_widget);
    girara_list_t* images    = zathura_page_images_get(page, NULL);
    girara_list_t* anchors   = girara_list_new_with_free(g_free);
    for (size_t idx = 0; images != NULL && anchors != NULL && idx != girara_list_size(images); ++idx) {
      const zathura_image_t* image = girara_list_nth(images, idx);

      const zathura_rectangle_t r = {
          .x1 = fmax(image->position.x1, 0),
          .y1 = fmax(image->position.y1, 0),
          .x2 = fmin(image->position.x2, page_width),
          .y2 = fmin(image->position.y2, page_height),
      };
      if (r.x2 - r.x1 < image_min_size || r.y2 - r.y1 < image_min_size) {
        continue;
      }

      double x                   = 0;
      double y                   = 0;
      zathura_rectangle_t anchor = {0};
      if (visual_image_anchor(zathura, page_index, r, view_x, view_y, half_w, half_h, &anchor, &x, &y) == false) {
        continue;
      }

      /* images starting at the same place, e.g. a background and a header, get their labels below each other */
      for (size_t used = 0; used != girara_list_size(anchors); ++used) {
        const zathura_rectangle_t* other = girara_list_nth(anchors, used);
        const double below               = other->y1 + label_step;
        /* only a move down restarts the scan: with rounding, below can equal the anchor and still collide */
        if (fabs(other->x1 - anchor.x1) < label_step && fabs(other->y1 - anchor.y1) < label_step && below > anchor.y1) {
          anchor.y1 = anchor.y2 = below;
          used                  = (size_t)-1;
        }
      }
      if (anchor.y1 > r.y2) {
        continue;
      }

      zathura_rectangle_t* used_anchor    = g_try_malloc0(sizeof(zathura_rectangle_t));
      visual_label_candidate_t* candidate = g_try_malloc0(sizeof(visual_label_candidate_t));
      if (candidate == NULL || used_anchor == NULL) {
        g_free(candidate);
        g_free(used_anchor);
        continue;
      }
      *used_anchor = anchor;
      girara_list_append(anchors, used_anchor);
      candidate->label.position.page = page_index;
      candidate->label.image         = true;
      candidate->label.image_index   = idx;
      candidate->label.bounds        = r;
      candidate->label.anchor        = anchor;
      candidate->distance =
          (x - caret_x) * (x - caret_x) / (half_w * half_w) + (y - caret_y) * (y - caret_y) / (half_h * half_h);
      girara_list_append(candidates, candidate);
    }
    girara_list_free(anchors);
    girara_list_free(images);
  }

  return candidates;
}

/* hand the labels on a page to its widget */
static void visual_labels_draw(zathura_t* zathura, unsigned int page) {
  zathura_visual_t* visual          = zathura->visual;
  ZathuraPageWidget* widget         = zathura_page_get_widget_by_number(zathura, page);
  const zathura_visual_text_t* text = visual_text(zathura, &visual->texts, page);
  if (widget == NULL || text == NULL) {
    return;
  }

  girara_list_t* drawn = girara_list_new_with_free(g_free);
  if (drawn == NULL) {
    return;
  }

  for (size_t idx = 0; idx != girara_list_size(visual->labels.list); ++idx) {
    const visual_label_t* label = girara_list_nth(visual->labels.list, idx);
    if (label->position.page != page) {
      continue;
    }

    zathura_page_widget_label_t* page_label = g_try_malloc0(sizeof(zathura_page_widget_label_t));
    if (page_label == NULL) {
      continue;
    }
    if (label->image == true) {
      page_label->position = label->anchor;
      page_label->bounds   = label->bounds;
      page_label->outline  = true;
    } else {
      page_label->position = text->rects[label->position.index];
    }
    g_strlcpy(page_label->text, label->text, sizeof(page_label->text));
    girara_list_append(drawn, page_label);
  }

  zathura_page_widget_set_labels(widget, drawn);
}

void zathura_visual_jump(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL || visual->labels.dialog != NULL) {
    return;
  }
  /* after y, the label tells up to where to copy */
  const bool yank      = visual->yank_pending;
  visual->yank_pending = false;

  girara_list_t* candidates = visual_label_candidates(zathura);
  if (candidates == NULL) {
    return;
  }
  if (girara_list_size(candidates) == 0) {
    girara_list_free(candidates);
    girara_notify(zathura->ui.session, GIRARA_WARNING, _("No visible words."));
    return;
  }

  /* the closest words get labels, two letters each when there are more words than letters */
  girara_list_sort(candidates, candidate_compare);
  const size_t alphabet_size = sizeof(label_alphabet) - 1;
  const size_t n_labels      = MIN(girara_list_size(candidates), alphabet_size * alphabet_size);
  const bool two_letters     = n_labels > alphabet_size;

  visual_labels_clear(zathura);
  visual->labels.list = girara_list_new_with_free(g_free);
  if (visual->labels.list == NULL) {
    girara_list_free(candidates);
    return;
  }
  visual->labels.first_page = UINT_MAX;
  visual->labels.last_page  = 0;

  for (size_t idx = 0; idx < n_labels; ++idx) {
    const visual_label_candidate_t* candidate = girara_list_nth(candidates, idx);
    visual_label_t* label                     = g_try_malloc0(sizeof(visual_label_t));
    if (label == NULL) {
      continue;
    }

    *label = candidate->label;
    if (two_letters == true) {
      label->text[0] = label_alphabet[idx / alphabet_size];
      label->text[1] = label_alphabet[idx % alphabet_size];
    } else {
      label->text[0] = label_alphabet[idx];
    }
    girara_list_append(visual->labels.list, label);
    visual->labels.first_page = MIN(visual->labels.first_page, label->position.page);
    visual->labels.last_page  = MAX(visual->labels.last_page, label->position.page);
  }
  girara_list_free(candidates);

  if (girara_list_size(visual->labels.list) == 0) {
    visual_labels_clear(zathura);
    return;
  }
  for (unsigned int page = visual->labels.first_page; page <= visual->labels.last_page; ++page) {
    visual_labels_draw(zathura, page);
  }

  /* ask for a label */
  GiraraDialog* dialog = girara_dialog(zathura->ui.session, _("Jump to label:"), FALSE);
  if (dialog == NULL) {
    visual_labels_clear(zathura);
    return;
  }
  visual->labels.dialog = dialog;
  visual->yank_pending  = yank;
  /* like the link hints: showing the prompt must not zoom and re-render the pages to fit the smaller view */
  zathura_inputbar_save_adjust_mode(zathura);
  g_signal_connect(dialog, "hide", G_CALLBACK(cb_hide_inputbar), zathura);
  g_signal_connect(dialog, "hide", G_CALLBACK(cb_visual_labels_hide), zathura);
  g_signal_connect(girara_inputbar_get_entry(GIRARA_INPUTBAR(dialog)), "changed", G_CALLBACK(cb_visual_labels_changed),
                   zathura);
}

/* Mouse selection over several pages */

/* distance from the edge of the view in which dragging scrolls */
static const double mouse_scroll_margin = 30;

/* the page widget under a point of the view, or the closest one */
static ZathuraPageWidget* mouse_page_at(zathura_t* zathura, double view_x, double view_y, unsigned int* page_index,
                                        double* x, double* y) {
  zathura_document_t* document = zathura_get_document(zathura);
  const unsigned int n_pages   = zathura_document_get_number_of_pages(document);
  ZathuraPageWidget* best      = NULL;
  double best_dist             = INFINITY;

  for (unsigned int idx = 0; idx < n_pages; ++idx) {
    zathura_page_t* page = zathura_document_get_page(document, idx);
    if (page == NULL || (zathura_document_widget_page_is_visible(zathura->ui.document_widget, idx) == false &&
                         idx != zathura->visual_mouse->start_page)) {
      continue;
    }
    ZathuraPageWidget* widget = zathura_page_get_widget(zathura, page);
    graphene_rect_t bounds;
    if (widget == NULL || gtk_widget_compute_bounds(GTK_WIDGET(widget), zathura->ui.view, &bounds) == false) {
      continue;
    }

    const double left   = bounds.origin.x;
    const double top    = bounds.origin.y;
    const double right  = left + bounds.size.width;
    const double bottom = top + bounds.size.height;
    const double dx     = view_x < left ? left - view_x : (view_x > right ? view_x - right : 0);
    const double dy     = view_y < top ? top - view_y : (view_y > bottom ? view_y - bottom : 0);
    const double dist   = dx * dx + dy * dy;
    if (dist < best_dist) {
      best        = widget;
      best_dist   = dist;
      *page_index = idx;
      *x          = CLAMP(view_x, left, right) - left;
      *y          = CLAMP(view_y, top, bottom) - top;
    }
  }

  return best;
}

static void mouse_order(zathura_visual_mouse_t* mouse, visual_position_t* first, visual_position_t* last) {
  const bool forward = position_compare(&mouse->anchor, &mouse->cursor) <= 0;
  *first             = forward ? mouse->anchor : mouse->cursor;
  *last              = forward ? mouse->cursor : mouse->anchor;
}

static void mouse_update(zathura_t* zathura) {
  zathura_visual_mouse_t* mouse = zathura->visual_mouse;

  unsigned int page_index   = 0;
  double x                  = 0;
  double y                  = 0;
  ZathuraPageWidget* widget = mouse_page_at(zathura, mouse->view_x, mouse->view_y, &page_index, &x, &y);
  if (widget == NULL) {
    return;
  }

  /* on the page where the selection started, the page selects like it always did */
  if (page_index == mouse->start_page) {
    if (mouse->multi == true) {
      visual_clear_drawn(zathura, &mouse->drawn);
      mouse->multi = false;
    }
    zathura_page_widget_update_mouse_selection(widget, x, y);
    return;
  }

  if (mouse->multi == false) {
    /* the keyboard selection uses the same drawing */
    zathura_visual_exit(zathura);

    const zathura_visual_text_t* text = visual_text(zathura, &mouse->texts, mouse->start_page);
    mouse->anchor.page                = mouse->start_page;
    mouse->anchor.index               = zathura_visual_index_at(text, mouse->start_x, mouse->start_y);
    mouse->multi                      = true;

    ZathuraPageWidget* start_widget = zathura_page_get_widget_by_number(zathura, mouse->start_page);
    if (start_widget != NULL) {
      zathura_page_widget_clear_selection(start_widget);
    }
  }

  double page_x = 0;
  double page_y = 0;
  zathura_page_widget_point_to_page(widget, x, y, &page_x, &page_y);
  const zathura_visual_text_t* text = visual_text(zathura, &mouse->texts, page_index);
  mouse->cursor.page                = page_index;
  mouse->cursor.index               = zathura_visual_index_at(text, page_x, page_y);

  visual_position_t first;
  visual_position_t last;
  mouse_order(mouse, &first, &last);
  visual_draw(zathura, &mouse->texts, &mouse->drawn, first, last, true, NULL);
}

static void mouse_stop_scrolling(zathura_visual_mouse_t* mouse) {
  g_clear_handle_id(&mouse->scroll_source, g_source_remove);
}

/* scroll while the pointer is near the top or bottom of the view, so that dragging reaches pages off screen */
static gboolean mouse_scroll(gpointer data) {
  zathura_t* zathura            = data;
  zathura_visual_mouse_t* mouse = zathura->visual_mouse;

  const double height = gtk_widget_get_height(zathura->ui.view);
  double delta        = 0;
  if (mouse->view_y < mouse_scroll_margin) {
    delta = mouse->view_y - mouse_scroll_margin;
  } else if (mouse->view_y > height - mouse_scroll_margin) {
    delta = mouse->view_y - (height - mouse_scroll_margin);
  }

  if (mouse->dragging == false || delta == 0) {
    mouse->scroll_source = 0;
    return G_SOURCE_REMOVE;
  }

  GtkAdjustment* adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(zathura->ui.view));
  gtk_adjustment_set_value(adjustment, gtk_adjustment_get_value(adjustment) + CLAMP(delta, -60, 60) / 2);
  mouse_update(zathura);
  return G_SOURCE_CONTINUE;
}

void zathura_visual_mouse_press(zathura_t* zathura, unsigned int page_index, double x, double y) {
  g_return_if_fail(zathura != NULL);

  if (zathura->visual_mouse == NULL) {
    zathura->visual_mouse = g_try_malloc0(sizeof(zathura_visual_mouse_t));
    if (zathura->visual_mouse == NULL) {
      return;
    }
    if (visual_texts_init(zathura, &zathura->visual_mouse->texts) == false) {
      g_free(zathura->visual_mouse);
      zathura->visual_mouse = NULL;
      return;
    }
  }

  zathura_visual_mouse_t* mouse = zathura->visual_mouse;
  visual_clear_drawn(zathura, &mouse->drawn);
  mouse_stop_scrolling(mouse);

  /* without text layouts, e.g. for images, the pages select on their own as before */
  const zathura_plugin_t* plugin              = zathura_document_get_plugin(zathura_get_document(zathura));
  const zathura_plugin_functions_t* functions = zathura_plugin_get_functions(plugin);
  if (functions == NULL || functions->page_get_text_layout == NULL) {
    mouse->dragging = false;
    return;
  }

  mouse->dragging   = true;
  mouse->multi      = false;
  mouse->start_page = page_index;
  mouse->start_x    = x;
  mouse->start_y    = y;
}

bool zathura_visual_mouse_motion(zathura_t* zathura, ZathuraPageWidget* widget, double x, double y) {
  g_return_val_if_fail(zathura != NULL && widget != NULL, false);

  zathura_visual_mouse_t* mouse = zathura->visual_mouse;
  if (mouse == NULL || mouse->dragging == false || zathura_has_document(zathura) == false) {
    return false;
  }

  graphene_point_t point = GRAPHENE_POINT_INIT(x, y);
  graphene_point_t view_point;
  if (gtk_widget_compute_point(GTK_WIDGET(widget), zathura->ui.view, &point, &view_point) == false) {
    return false;
  }

  mouse->view_x = view_point.x;
  mouse->view_y = view_point.y;
  mouse_update(zathura);

  const double height = gtk_widget_get_height(zathura->ui.view);
  if (mouse->scroll_source == 0 &&
      (mouse->view_y < mouse_scroll_margin || mouse->view_y > height - mouse_scroll_margin)) {
    mouse->scroll_source = g_timeout_add(30, mouse_scroll, zathura);
  }
  return true;
}

void zathura_visual_mouse_cancel(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_mouse_t* mouse = zathura->visual_mouse;
  if (mouse != NULL) {
    mouse->dragging = false;
    mouse->multi    = false;
    mouse_stop_scrolling(mouse);
  }
}

bool zathura_visual_mouse_release(zathura_t* zathura) {
  g_return_val_if_fail(zathura != NULL, false);

  zathura_visual_mouse_t* mouse = zathura->visual_mouse;
  if (mouse == NULL || mouse->dragging == false) {
    return false;
  }

  mouse->dragging = false;
  mouse_stop_scrolling(mouse);
  if (mouse->multi == false) {
    return false;
  }
  mouse->multi = false;

  /* copy like a selection on one page, which only happens in normal and fullscreen mode */
  const girara_mode_t mode = girara_mode_get(zathura->ui.session);
  if (mode == zathura->modes.normal || mode == zathura->modes.fullscreen) {
    visual_position_t first;
    visual_position_t last;
    mouse_order(mouse, &first, &last);
    visual_copy(zathura, first, last);
  }
  return true;
}

void zathura_visual_mouse_clear(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_mouse_t* mouse = zathura->visual_mouse;
  if (mouse == NULL) {
    return;
  }

  visual_clear_drawn(zathura, &mouse->drawn);
  mouse_stop_scrolling(mouse);
  visual_texts_clear(&mouse->texts);
  g_free(mouse);
  zathura->visual_mouse = NULL;
}

void zathura_visual_reset(zathura_t* zathura) {
  g_return_if_fail(zathura != NULL);

  zathura_visual_exit(zathura);
  visual_forget(zathura);
  zathura_visual_mouse_clear(zathura);
  visual_flash_clear(zathura);
}

/* Annotations */

/* default colors of the annotation types */
static const double annotation_color_highlight[3] = {1.0, 0.92, 0.0};
static const double annotation_color_underline[3] = {0.0, 0.6, 0.0};
static const double annotation_color_squiggly[3]  = {0.9, 0.1, 0.1};
static const double annotation_color_strikeout[3] = {0.9, 0.1, 0.1};
static const double annotation_color_text[3]      = {1.0, 0.92, 0.0};

static const double* annotation_default_color(zathura_annotation_type_t type) {
  switch (type) {
  case ZATHURA_ANNOTATION_HIGHLIGHT:
    return annotation_color_highlight;
  case ZATHURA_ANNOTATION_UNDERLINE:
    return annotation_color_underline;
  case ZATHURA_ANNOTATION_SQUIGGLY:
    return annotation_color_squiggly;
  case ZATHURA_ANNOTATION_STRIKEOUT:
    return annotation_color_strikeout;
  default:
    return annotation_color_text;
  }
}

static bool rect_is_empty(const zathura_rectangle_t* rect) {
  return rect->x2 <= rect->x1 || rect->y2 <= rect->y1;
}

static zathura_rectangle_t rect_normalize(zathura_rectangle_t rect) {
  return (zathura_rectangle_t){
      .x1 = MIN(rect.x1, rect.x2),
      .y1 = MIN(rect.y1, rect.y2),
      .x2 = MAX(rect.x1, rect.x2),
      .y2 = MAX(rect.y1, rect.y2),
  };
}

static bool rects_intersect(const zathura_rectangle_t* a, const zathura_rectangle_t* b) {
  return a->x1 < b->x2 && b->x1 < a->x2 && a->y1 < b->y2 && b->y1 < a->y2;
}

/* join rectangles of characters that follow each other on a line to one rectangle per line */
static GArray* visual_merge_lines(const zathura_rectangle_t* rects, size_t n) {
  GArray* merged = g_array_new(FALSE, FALSE, sizeof(zathura_rectangle_t));

  bool have                = false;
  zathura_rectangle_t line = {0};
  for (size_t i = 0; i < n; ++i) {
    const zathura_rectangle_t rect = rect_normalize(rects[i]);
    if (rect_is_empty(&rect) == true) {
      continue;
    }

    const double center = (rect.y1 + rect.y2) / 2;
    if (have == true && center > line.y1 && center < line.y2 && rect.x1 - line.x2 < (line.y2 - line.y1)) {
      line.x1 = MIN(line.x1, rect.x1);
      line.y1 = MIN(line.y1, rect.y1);
      line.x2 = MAX(line.x2, rect.x2);
      line.y2 = MAX(line.y2, rect.y2);
    } else {
      if (have == true) {
        g_array_append_val(merged, line);
      }
      line = rect;
      have = true;
    }
  }
  if (have == true) {
    g_array_append_val(merged, line);
  }
  return merged;
}

/* The rectangles of the characters from to to on the layout, with the characters of a line joined to one rectangle.
 * With lines false, the rectangles are those of the single characters instead. Newlines and characters without area
 * are skipped. */
static GArray* visual_selection_rects(const zathura_text_layout_t* layout, size_t from, size_t to, bool lines) {
  GArray* rects = g_array_new(FALSE, FALSE, sizeof(zathura_rectangle_t));
  if (layout == NULL || from > to || to >= layout->n_rects) {
    return rects;
  }

  const char* text = g_utf8_offset_to_pointer(layout->text, from);
  for (size_t i = from; i <= to; ++i, text = g_utf8_next_char(text)) {
    const gunichar c = g_utf8_get_char(text);
    if (g_unichar_isspace(c) == TRUE && c != ' ') {
      continue;
    }
    const zathura_rectangle_t rect = rect_normalize(layout->rects[i]);
    if (rect_is_empty(&rect) == false) {
      g_array_append_val(rects, rect);
    }
  }

  if (lines == true) {
    GArray* merged = visual_merge_lines((const zathura_rectangle_t*)(void*)rects->data, rects->len);
    g_array_free(rects, TRUE);
    return merged;
  }
  return rects;
}

/* the selected characters of a page; false if the page has no text in the selection */
static bool visual_page_range(const zathura_text_layout_t* layout, unsigned int page_index, visual_position_t first,
                              visual_position_t last, size_t* from, size_t* to) {
  if (layout == NULL || layout->n_rects == 0 || page_index < first.page || page_index > last.page) {
    return false;
  }

  *from = page_index == first.page ? first.index : 0;
  *to   = page_index == last.page ? last.index : layout->n_rects - 1;
  return *from <= *to && *to < layout->n_rects;
}

static void visual_annotations_changed(zathura_t* zathura) {
  if (zathura->ui.document_widget != NULL) {
    zathura_document_widget_render_all(zathura->ui.document_widget);
  }
}

/* add an annotation covering the rectangles to a page; the rectangles are lines of text, or the first one is where a
 * note goes */
static bool visual_add_annotation(zathura_page_t* page, GArray* rects, zathura_annotation_type_t type,
                                  const double* color, const char* contents) {
  if (rects->len == 0) {
    return false;
  }

  const double* use_color         = color != NULL ? color : annotation_default_color(type);
  zathura_rectangle_t* data       = (zathura_rectangle_t*)(void*)rects->data;
  zathura_annotation_t annotation = {
      .type      = type,
      .has_color = true,
      .color     = {use_color[0], use_color[1], use_color[2]},
      .contents  = (char*)contents,
  };

  if (type == ZATHURA_ANNOTATION_TEXT) {
    /* the note sits at the top left of the first rectangle */
    annotation.rect = (zathura_rectangle_t){data[0].x1, data[0].y1, data[0].x1 + 20, data[0].y1 + 20};
  } else {
    annotation.quads   = data;
    annotation.n_quads = rects->len;
    annotation.rect    = data[0];
    for (guint i = 1; i < rects->len; ++i) {
      annotation.rect.x1 = MIN(annotation.rect.x1, data[i].x1);
      annotation.rect.y1 = MIN(annotation.rect.y1, data[i].y1);
      annotation.rect.x2 = MAX(annotation.rect.x2, data[i].x2);
      annotation.rect.y2 = MAX(annotation.rect.y2, data[i].y2);
    }
  }

  return zathura_page_annotation_add(page, &annotation) == ZATHURA_ERROR_OK;
}

static void visual_annotation_result(zathura_t* zathura, unsigned int added, bool failed) {
  if (failed == true || added == 0) {
    girara_notify(zathura->ui.session, GIRARA_ERROR, _("Failed to add the annotation."));
  }
  if (added != 0) {
    visual_annotations_changed(zathura);
  }
}

/* annotate the text from first to last; returns the number of annotations added */
static unsigned int visual_annotate_range(zathura_t* zathura, visual_position_t first, visual_position_t last,
                                          zathura_annotation_type_t type, const double* color, const char* contents) {
  zathura_document_t* document = zathura_get_document(zathura);
  unsigned int added           = 0;
  bool failed                  = false;

  for (unsigned int page_index = first.page; page_index <= last.page; ++page_index) {
    zathura_page_t* page                = zathura_document_get_page(document, page_index);
    const zathura_text_layout_t* layout = page != NULL ? zathura_page_get_text_layout(page, NULL) : NULL;
    size_t from                         = 0;
    size_t to                           = 0;
    if (visual_page_range(layout, page_index, first, last, &from, &to) == false) {
      continue;
    }

    g_autoptr(GArray) rects = visual_selection_rects(layout, from, to, type != ZATHURA_ANNOTATION_TEXT);
    if (rects->len == 0) {
      continue;
    }

    if (visual_add_annotation(page, rects, type, color, contents) == true) {
      ++added;
    } else {
      failed = true;
    }

    if (type == ZATHURA_ANNOTATION_TEXT) {
      break;
    }
  }

  visual_annotation_result(zathura, added, failed);
  return added;
}

unsigned int zathura_visual_annotate(zathura_t* zathura, zathura_annotation_type_t type, const double* color,
                                     const char* contents) {
  g_return_val_if_fail(zathura != NULL, 0);

  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL) {
    return 0;
  }
  if (type == ZATHURA_ANNOTATION_OTHER) {
    return 0;
  }
  if (visual->kind == ZATHURA_VISUAL_IMAGE ||
      (visual->kind == ZATHURA_VISUAL_CARET && type != ZATHURA_ANNOTATION_TEXT)) {
    girara_notify(zathura->ui.session, GIRARA_WARNING, _("Select text to annotate first."));
    return 0;
  }

  visual_position_t first = visual->cursor;
  visual_position_t last  = visual->cursor;
  visual_selection(zathura, &first, &last);

  const unsigned int added = visual_annotate_range(zathura, first, last, type, color, contents);
  zathura_visual_exit(zathura);
  return added;
}

unsigned int zathura_visual_annotate_mouse(zathura_t* zathura, zathura_annotation_type_t type, const double* color,
                                           const char* contents) {
  g_return_val_if_fail(zathura != NULL, 0);
  if (type == ZATHURA_ANNOTATION_OTHER) {
    return 0;
  }

  /* a selection over several pages */
  zathura_visual_mouse_t* mouse = zathura->visual_mouse;
  if (mouse != NULL && mouse->dragging == false && mouse->drawn.drawn == true) {
    visual_position_t first;
    visual_position_t last;
    mouse_order(mouse, &first, &last);
    const unsigned int added = visual_annotate_range(zathura, first, last, type, color, contents);
    zathura_visual_mouse_clear(zathura);
    return added;
  }

  /* a selection on one page */
  zathura_document_t* document = zathura_get_document(zathura);
  const unsigned int n_pages   = zathura_document_get_number_of_pages(document);
  for (unsigned int page_index = 0; page_index < n_pages; ++page_index) {
    ZathuraPageWidget* widget = zathura_page_get_widget_by_number(zathura, page_index);
    girara_list_t* selection  = widget != NULL ? zathura_page_widget_get_selection(widget) : NULL;
    if (selection == NULL || girara_list_size(selection) == 0) {
      continue;
    }

    GArray* raw = g_array_new(FALSE, FALSE, sizeof(zathura_rectangle_t));
    for (size_t i = 0; i < girara_list_size(selection); ++i) {
      g_array_append_val(raw, *(zathura_rectangle_t*)girara_list_nth(selection, i));
    }
    g_autoptr(GArray) rects = type == ZATHURA_ANNOTATION_TEXT
                                  ? raw
                                  : visual_merge_lines((const zathura_rectangle_t*)(void*)raw->data, raw->len);
    if (rects != raw) {
      g_array_free(raw, TRUE);
    }

    const bool ok =
        visual_add_annotation(zathura_document_get_page(document, page_index), rects, type, color, contents);
    visual_annotation_result(zathura, ok == true ? 1 : 0, ok == false);
    zathura_page_widget_clear_selection(widget);
    return ok == true ? 1 : 0;
  }

  girara_notify(zathura->ui.session, GIRARA_WARNING, _("Select text with the mouse to annotate first."));
  return 0;
}

typedef enum visual_annotation_action_e {
  VISUAL_ANNOTATION_DELETE,
  VISUAL_ANNOTATION_COMMENT,
} visual_annotation_action_t;

static int compare_uint_descending(const void* a, const void* b) {
  const unsigned int x = *(const unsigned int*)a;
  const unsigned int y = *(const unsigned int*)b;
  return (x < y) - (x > y);
}

/* apply the action to the annotations overlapping the selection or the caret */
static unsigned int visual_annotation_apply(zathura_t* zathura, visual_annotation_action_t action,
                                            const char* contents) {
  zathura_visual_t* visual = zathura->visual;
  if (visual == NULL) {
    return 0;
  }
  if (visual->kind == ZATHURA_VISUAL_IMAGE) {
    return 0;
  }

  visual_position_t first = visual->cursor;
  visual_position_t last  = visual->cursor;
  visual_selection(zathura, &first, &last);

  zathura_document_t* document = zathura_get_document(zathura);
  unsigned int changed         = 0;

  for (unsigned int page_index = first.page; page_index <= last.page; ++page_index) {
    zathura_page_t* page                = zathura_document_get_page(document, page_index);
    const zathura_text_layout_t* layout = page != NULL ? zathura_page_get_text_layout(page, NULL) : NULL;
    size_t from                         = 0;
    size_t to                           = 0;
    if (visual_page_range(layout, page_index, first, last, &from, &to) == false) {
      continue;
    }

    g_autoptr(GArray) selected           = visual_selection_rects(layout, from, to, false);
    g_autoptr(girara_list_t) annotations = zathura_page_annotations_get(page, NULL);
    if (annotations == NULL || selected->len == 0) {
      continue;
    }

    g_autoptr(GArray) hits = g_array_new(FALSE, FALSE, sizeof(unsigned int));
    for (size_t n = 0; n < girara_list_size(annotations); ++n) {
      const zathura_annotation_t* annotation = girara_list_nth(annotations, n);
      bool hit                               = false;
      for (guint i = 0; i < selected->len && hit == false; ++i) {
        const zathura_rectangle_t* rect = &g_array_index(selected, zathura_rectangle_t, i);
        if (annotation->n_quads == 0) {
          hit = rects_intersect(rect, &annotation->rect);
        }
        for (size_t q = 0; q < annotation->n_quads && hit == false; ++q) {
          hit = rects_intersect(rect, &annotation->quads[q]);
        }
      }
      if (hit == true) {
        g_array_append_val(hits, annotation->index);
      }
    }

    if (action == VISUAL_ANNOTATION_DELETE) {
      /* later annotations shift down when one is deleted, so go from the back */
      g_array_sort(hits, compare_uint_descending);
    }
    for (guint i = 0; i < hits->len; ++i) {
      const unsigned int index    = g_array_index(hits, unsigned int, i);
      const zathura_error_t error = action == VISUAL_ANNOTATION_DELETE
                                        ? zathura_page_annotation_delete(page, index)
                                        : zathura_page_annotation_set_contents(page, index, contents);
      if (error == ZATHURA_ERROR_OK) {
        ++changed;
      }
    }
  }

  return changed;
}

unsigned int zathura_visual_annotation_delete(zathura_t* zathura) {
  g_return_val_if_fail(zathura != NULL, 0);

  const unsigned int deleted = visual_annotation_apply(zathura, VISUAL_ANNOTATION_DELETE, NULL);
  if (deleted != 0) {
    visual_annotations_changed(zathura);
  }
  zathura_visual_exit(zathura);
  return deleted;
}

unsigned int zathura_visual_annotation_comment(zathura_t* zathura, const char* contents) {
  g_return_val_if_fail(zathura != NULL && contents != NULL, 0);

  const unsigned int changed = visual_annotation_apply(zathura, VISUAL_ANNOTATION_COMMENT, contents);
  if (changed != 0) {
    visual_annotations_changed(zathura);
  }
  zathura_visual_exit(zathura);
  return changed;
}
