/* SPDX-License-Identifier: Zlib */

#include <string.h>

#include "visual.h"

/* lay out a text on a grid: every character is 10 wide and every line 20 high */
typedef struct {
  zathura_visual_text_t text;
  zathura_rectangle_t* rects;
} grid_text_t;

static grid_text_t* grid_text_new(const char* str) {
  grid_text_t* grid = g_try_malloc0(sizeof(grid_text_t));
  g_assert_nonnull(grid);
  glong n          = 0;
  grid->text.chars = g_utf8_to_ucs4_fast(str, -1, &n);
  grid->text.n     = n;
  grid->rects      = g_try_malloc0(MAX(n, 1) * sizeof(zathura_rectangle_t));
  g_assert_nonnull(grid->rects);

  unsigned int line = 0;
  unsigned int col  = 0;
  for (glong idx = 0; idx < n; ++idx) {
    grid->rects[idx] = (zathura_rectangle_t){col * 10.0, line * 20.0, col * 10.0 + 10, line * 20.0 + 20};
    if (grid->text.chars[idx] == '\n') {
      ++line;
      col = 0;
    } else {
      ++col;
    }
  }
  grid->text.rects = grid->rects;
  return grid;
}

static void grid_text_free(grid_text_t* grid) {
  g_free(grid->text.chars);
  g_free(grid->rects);
  g_free(grid);
}

static size_t move(const char* str, size_t pos, zathura_visual_motion_t motion) {
  grid_text_t* grid = grid_text_new(str);
  double curswant   = -1;
  const size_t res  = zathura_visual_motion(&grid->text, pos, motion, &curswant);
  grid_text_free(grid);
  return res;
}

static void test_left_right(void) {
  g_assert_cmpuint(move("ab\ncd", 0, ZATHURA_VISUAL_MOTION_RIGHT), ==, 1);
  /* h and l stay on the line */
  g_assert_cmpuint(move("ab\ncd", 1, ZATHURA_VISUAL_MOTION_RIGHT), ==, 1);
  g_assert_cmpuint(move("ab\ncd", 3, ZATHURA_VISUAL_MOTION_LEFT), ==, 3);
  g_assert_cmpuint(move("ab\ncd", 4, ZATHURA_VISUAL_MOTION_LEFT), ==, 3);
  g_assert_cmpuint(move("ab", 0, ZATHURA_VISUAL_MOTION_LEFT), ==, 0);
}

static void test_words(void) {
  g_assert_cmpuint(move("foo bar", 0, ZATHURA_VISUAL_MOTION_WORD_FORWARD), ==, 4);
  g_assert_cmpuint(move("foo.bar baz", 0, ZATHURA_VISUAL_MOTION_WORD_FORWARD), ==, 3);
  g_assert_cmpuint(move("foo.bar baz", 3, ZATHURA_VISUAL_MOTION_WORD_FORWARD), ==, 4);
  g_assert_cmpuint(move("foo.bar baz", 0, ZATHURA_VISUAL_MOTION_BIGWORD_FORWARD), ==, 8);
  /* words continue on the next line */
  g_assert_cmpuint(move("foo\nbar", 0, ZATHURA_VISUAL_MOTION_WORD_FORWARD), ==, 4);
  /* the last word stays on the last character */
  g_assert_cmpuint(move("foo bar\n", 4, ZATHURA_VISUAL_MOTION_WORD_FORWARD), ==, 6);

  g_assert_cmpuint(move("foo bar", 0, ZATHURA_VISUAL_MOTION_WORD_END), ==, 2);
  g_assert_cmpuint(move("foo bar", 2, ZATHURA_VISUAL_MOTION_WORD_END), ==, 6);
  g_assert_cmpuint(move("foo.bar baz", 0, ZATHURA_VISUAL_MOTION_BIGWORD_END), ==, 6);

  g_assert_cmpuint(move("foo bar", 6, ZATHURA_VISUAL_MOTION_WORD_BACKWARD), ==, 4);
  g_assert_cmpuint(move("foo bar", 4, ZATHURA_VISUAL_MOTION_WORD_BACKWARD), ==, 0);
  g_assert_cmpuint(move("foo.bar baz", 8, ZATHURA_VISUAL_MOTION_WORD_BACKWARD), ==, 4);
  g_assert_cmpuint(move("foo.bar baz", 8, ZATHURA_VISUAL_MOTION_BIGWORD_BACKWARD), ==, 0);
  g_assert_cmpuint(move("foo\nbar", 4, ZATHURA_VISUAL_MOTION_WORD_BACKWARD), ==, 0);
  g_assert_cmpuint(move("foo", 0, ZATHURA_VISUAL_MOTION_WORD_BACKWARD), ==, 0);
}

static void test_lines(void) {
  g_assert_cmpuint(move("  ab cd\nx", 5, ZATHURA_VISUAL_MOTION_LINE_START), ==, 0);
  g_assert_cmpuint(move("  ab cd\nx", 5, ZATHURA_VISUAL_MOTION_LINE_FIRST_NONBLANK), ==, 2);
  g_assert_cmpuint(move("  ab cd\nx", 2, ZATHURA_VISUAL_MOTION_LINE_END), ==, 6);
  g_assert_cmpuint(move("ab\ncd\n", 1, ZATHURA_VISUAL_MOTION_TOP), ==, 0);
  g_assert_cmpuint(move("ab\n  cd\n", 0, ZATHURA_VISUAL_MOTION_BOTTOM), ==, 5);
}

static void test_up_down(void) {
  grid_text_t* grid = grid_text_new("abcdef\nab\nabcdef");
  double curswant   = -1;

  /* the short line has no character under column 4, keep the column for the next line */
  size_t pos = zathura_visual_motion(&grid->text, 4, ZATHURA_VISUAL_MOTION_DOWN, &curswant);
  g_assert_cmpuint(pos, ==, 8);
  pos = zathura_visual_motion(&grid->text, pos, ZATHURA_VISUAL_MOTION_DOWN, &curswant);
  g_assert_cmpuint(pos, ==, 14);
  pos = zathura_visual_motion(&grid->text, pos, ZATHURA_VISUAL_MOTION_DOWN, &curswant);
  g_assert_cmpuint(pos, ==, 14);
  pos = zathura_visual_motion(&grid->text, pos, ZATHURA_VISUAL_MOTION_UP, &curswant);
  pos = zathura_visual_motion(&grid->text, pos, ZATHURA_VISUAL_MOTION_UP, &curswant);
  g_assert_cmpuint(pos, ==, 4);
  pos = zathura_visual_motion(&grid->text, pos, ZATHURA_VISUAL_MOTION_UP, &curswant);
  g_assert_cmpuint(pos, ==, 4);

  /* horizontal motions forget the column */
  pos = zathura_visual_motion(&grid->text, pos, ZATHURA_VISUAL_MOTION_LEFT, &curswant);
  g_assert_cmpfloat(curswant, <, 0);
  grid_text_free(grid);

  /* empty lines are a stop of their own */
  g_assert_cmpuint(move("ab\n\ncd", 0, ZATHURA_VISUAL_MOTION_DOWN), ==, 3);
  g_assert_cmpuint(move("ab\n\ncd", 3, ZATHURA_VISUAL_MOTION_DOWN), ==, 4);
  g_assert_cmpuint(move("ab\n", 0, ZATHURA_VISUAL_MOTION_DOWN), ==, 0);
}

static void test_range(void) {
  grid_text_t* grid = grid_text_new("abc\ndef\nghi");
  size_t first      = 0;
  size_t last       = 0;

  zathura_visual_range(&grid->text, ZATHURA_VISUAL_CHAR, 5, 1, &first, &last);
  g_assert_cmpuint(first, ==, 1);
  g_assert_cmpuint(last, ==, 5);

  zathura_visual_range(&grid->text, ZATHURA_VISUAL_LINE, 5, 1, &first, &last);
  g_assert_cmpuint(first, ==, 0);
  g_assert_cmpuint(last, ==, 6);

  zathura_visual_range(&grid->text, ZATHURA_VISUAL_CARET, 5, 1, &first, &last);
  g_assert_cmpuint(first, ==, 1);
  g_assert_cmpuint(last, ==, 1);
  grid_text_free(grid);
}

static void test_lines_count(void) {
  grid_text_t* grid = grid_text_new("abc\ndef\n\nghi\n");
  size_t first      = 0;
  size_t last       = 0;

  /* yy from the middle of a line */
  zathura_visual_lines(&grid->text, 5, 1, &first, &last);
  g_assert_cmpuint(first, ==, 4);
  g_assert_cmpuint(last, ==, 6);

  /* 2yy and 3yy, across an empty line */
  zathura_visual_lines(&grid->text, 1, 2, &first, &last);
  g_assert_cmpuint(first, ==, 0);
  g_assert_cmpuint(last, ==, 6);
  zathura_visual_lines(&grid->text, 1, 4, &first, &last);
  g_assert_cmpuint(first, ==, 0);
  g_assert_cmpuint(last, ==, 11);

  /* counts past the end stop at the last line */
  zathura_visual_lines(&grid->text, 9, 5, &first, &last);
  g_assert_cmpuint(first, ==, 9);
  g_assert_cmpuint(last, ==, 11);
  grid_text_free(grid);
}

static void test_find(void) {
  grid_text_t* grid = grid_text_new("a,b,c,d\nx,y");

  g_assert_cmpuint(zathura_visual_find(&grid->text, 0, ',', true, false, false, 1), ==, 1);
  g_assert_cmpuint(zathura_visual_find(&grid->text, 0, ',', true, false, false, 2), ==, 3);
  g_assert_cmpuint(zathura_visual_find(&grid->text, 0, 'c', true, true, false, 1), ==, 3);
  /* a repeated t skips the match next to the caret */
  g_assert_cmpuint(zathura_visual_find(&grid->text, 3, 'c', true, true, false, 1), ==, 3);
  g_assert_cmpuint(zathura_visual_find(&grid->text, 2, ',', true, true, true, 1), ==, 4);
  /* backward, and never past the line */
  g_assert_cmpuint(zathura_visual_find(&grid->text, 6, 'b', false, false, false, 1), ==, 2);
  g_assert_cmpuint(zathura_visual_find(&grid->text, 6, 'b', false, true, false, 1), ==, 3);
  g_assert_cmpuint(zathura_visual_find(&grid->text, 0, 'x', true, false, false, 1), ==, 0);
  g_assert_cmpuint(zathura_visual_find(&grid->text, 9, 'd', false, false, false, 1), ==, 9);
  g_assert_cmpuint(zathura_visual_find(&grid->text, 0, 'a', false, false, false, 1), ==, 0);
  grid_text_free(grid);
}

static void word_object(const char* str, size_t pos, bool big, bool around, size_t first, size_t last) {
  grid_text_t* grid = grid_text_new(str);
  size_t f          = 0;
  size_t l          = 0;
  zathura_visual_word_object(&grid->text, pos, big, around, &f, &l);
  g_assert_cmpuint(f, ==, first);
  g_assert_cmpuint(l, ==, last);
  grid_text_free(grid);
}

static void test_word_object(void) {
  word_object("foo bar.baz qux", 5, false, false, 4, 6);
  word_object("foo bar.baz qux", 5, true, false, 4, 10);
  word_object("foo bar.baz qux", 5, false, true, 3, 6);
  word_object("foo bar.baz qux", 9, false, true, 8, 11);
  /* the last word takes the blanks before it */
  word_object("foo bar", 5, false, true, 3, 6);
  /* on blanks, aw takes the next word */
  word_object("foo   bar", 4, false, false, 3, 5);
  word_object("foo   bar", 4, false, true, 3, 8);
  /* words end at the line */
  word_object("foo\nbar", 1, false, true, 0, 2);
}

static void test_page_entry(void) {
  grid_text_t* grid = grid_text_new("  abc def\nghi jkl  \n");

  g_assert_cmpuint(zathura_visual_page_entry(&grid->text, ZATHURA_VISUAL_MOTION_WORD_FORWARD, -1), ==, 2);
  g_assert_cmpuint(zathura_visual_page_entry(&grid->text, ZATHURA_VISUAL_MOTION_WORD_END, -1), ==, 4);
  g_assert_cmpuint(zathura_visual_page_entry(&grid->text, ZATHURA_VISUAL_MOTION_WORD_BACKWARD, -1), ==, 14);
  g_assert_cmpuint(zathura_visual_page_entry(&grid->text, ZATHURA_VISUAL_MOTION_DOWN, 45), ==, 4);
  g_assert_cmpuint(zathura_visual_page_entry(&grid->text, ZATHURA_VISUAL_MOTION_UP, 45), ==, 14);
  g_assert_cmpuint(zathura_visual_page_entry(&grid->text, ZATHURA_VISUAL_MOTION_LEFT, -1), ==, grid->text.n);
  grid_text_free(grid);

  /* pages without words have no entry */
  grid = grid_text_new("   \n");
  g_assert_cmpuint(zathura_visual_page_entry(&grid->text, ZATHURA_VISUAL_MOTION_WORD_FORWARD, -1), ==, grid->text.n);
  g_assert_cmpuint(zathura_visual_page_entry(&grid->text, ZATHURA_VISUAL_MOTION_WORD_BACKWARD, -1), ==, grid->text.n);
  grid_text_free(grid);
}

static void test_caret_rectangle(void) {
  grid_text_t* grid = grid_text_new("ab\n\ncd");
  /* newlines have no size, like in poppler */
  for (size_t idx = 0; idx < grid->text.n; ++idx) {
    if (grid->text.chars[idx] == '\n') {
      grid->rects[idx].x2 = grid->rects[idx].x1;
      grid->rects[idx].y2 = grid->rects[idx].y1;
    }
  }

  /* characters with a size keep it */
  zathura_rectangle_t rect = zathura_visual_caret_rectangle(&grid->text, 0);
  g_assert_cmpfloat(rect.x2 - rect.x1, ==, 10);

  /* the empty line gets a caret in the gap between the lines, as high as the text */
  rect = zathura_visual_caret_rectangle(&grid->text, 3);
  g_assert_cmpfloat(rect.y2 - rect.y1, ==, 20);
  g_assert_cmpfloat(rect.x2 - rect.x1, >, 0);
  g_assert_cmpfloat(rect.y1, >=, 20);
  g_assert_cmpfloat(rect.y2, <=, 60);
  g_assert_cmpfloat(rect.x1, ==, 0);
  grid_text_free(grid);
}

static void test_index_at(void) {
  grid_text_t* grid = grid_text_new("abc\ndefgh\n\nij");
  for (size_t idx = 0; idx < grid->text.n; ++idx) {
    if (grid->text.chars[idx] == '\n') {
      grid->rects[idx].x2 = grid->rects[idx].x1;
      grid->rects[idx].y2 = grid->rects[idx].y1;
    }
  }

  /* on a character */
  g_assert_cmpuint(zathura_visual_index_at(&grid->text, 15, 5), ==, 1);
  g_assert_cmpuint(zathura_visual_index_at(&grid->text, 25, 30), ==, 6);
  /* left and right of a line */
  g_assert_cmpuint(zathura_visual_index_at(&grid->text, -50, 30), ==, 4);
  g_assert_cmpuint(zathura_visual_index_at(&grid->text, 500, 30), ==, 8);
  /* above and below the text */
  g_assert_cmpuint(zathura_visual_index_at(&grid->text, 15, -100), ==, 1);
  g_assert_cmpuint(zathura_visual_index_at(&grid->text, 15, 500), ==, 12);
  /* the empty line has no size, so the closest line is taken */
  g_assert_cmpuint(zathura_visual_index_at(&grid->text, 5, 45), ==, 4);
  grid_text_free(grid);
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/visual/left_right", test_left_right);
  g_test_add_func("/visual/words", test_words);
  g_test_add_func("/visual/lines", test_lines);
  g_test_add_func("/visual/up_down", test_up_down);
  g_test_add_func("/visual/range", test_range);
  g_test_add_func("/visual/lines_count", test_lines_count);
  g_test_add_func("/visual/find", test_find);
  g_test_add_func("/visual/word_object", test_word_object);
  g_test_add_func("/visual/page_entry", test_page_entry);
  g_test_add_func("/visual/caret_rectangle", test_caret_rectangle);
  g_test_add_func("/visual/index_at", test_index_at);
  return g_test_run();
}
