===========
 zathurarc
===========

--------------------------
zathura configuration file
--------------------------

:Author: Sebastian Ramacher <s.ramacher@gmx.at>
:Date: 19.8.2010
:Manual section: 5

SYNOPOSIS
=========

/etc/zathurarc, ~/.config/zathura/zathurarc

DESCRIPTION
===========

The zathurarc contains various options controlling the behavior of zathura. One
can use the ``set`` and ``map`` commands:

* ``set`` [id] [value]
* ``map`` [key] [function] [argument] [mode]

They behave the same as the ``set`` and ``map`` commands in zathura. Any line
not starting with ``set`` or ``map`` will be ignored.

set
---

[id] and the corresponding [value] can be one of

* adjust_open [bestfit|width|noadjust] - adjust mode.

  - bestfit: adjust to best fit
  - width: adjust to width
  - noadjust: don't adjust

* browser,
  uri_command [string] - command to open external URIs.
  
  The string has to include a ``%s`` which will be replaced with the URI.

* completion_bgcolor,
  completion_fgcolor,
  completion_g_bgcolor,
  completion_g_fgcolor,
  completion_hl_bgcolor,
  completion_hl_fgcolor,
  default_bgcolor,
  default_fgcolor,
  inputbar_bgcolor,
  inputbar_fgcolor,
  notification_e_bgcolor,
  notification_e_fgcolor,
  notification_w_bgcolor,
  notification_w_fgcolor,
  recolor_darkcolor,
  recolor_lightcolor,
  statusbar_bgcolor,
  statusbar_fgcolor,
  search_highlight,
  select_text [color] -
  colors settings.
  
  The color can be given as hex triplet (#rrggbb) or any color
  string understood by GTK+ (e.g. red, green, blue, black, ...).

* default_text [string] - text displayed in the statusbar if no file is opened.

* font [string] - the used font.

* height,
  width [int] - default height and width of the zathura window.

* labels [bool] - allow label mode.

* list_printer_command [string] - command to list all available printers.

* n_completion_items [int] - number of completion items to display.

* offset - page offset.

* print_command [string] - command to print the file.
  
  The string has to include
  ``%s`` four times. The first occurence of ``%s`` will be replaced with the
  printer, the second with additional options given on the command line, the
  third with the pages to print and the fourth with the filename.

* recolor [bool] - invert the image.

* save_position, save_zoom_level [bool] - save current page and zoom level in
  bookmarks file.

* scroll_step [float] - scroll step.

* scroll_wrap [bool] - wrap scrolling at the end and beginning of the document.

* scrollbars,
  show_statusbar,
  show_inputbar [bool] -
  show statusbar, inputbar and scrollbars.

* smooth_scrolling [float] - smooth scrolling.

* transparency [float] - transparency of rectangles.

* zoom_max,
  zoom_min,
  zoom_step [float] - maximal and minimal zoom level and zoom step.

map
---

[key] can be a single character, ``<C-?>`` for ``Ctrl + ?`` like shortcuts,
where ``?`` stands for some key (e.g. ``<C-q>``). Also it can be ``<S-?>`` for
uppercase shortcuts or one of

``<BackSpace>, <CapsLock>, <Down>, <Esc>, <F[1-12]>, <Left>, <PageDown>,
<PageUp>, <Return>, <Right>, <Space>, <Super>, <Tab>, <Up>``.

[function] and the corresponding [argument] can be one of

* abort - clear command line and buffer.
* adjust_window
* change_buffer [delete_last]: change buffer.

  - delete_last: delete last character

* change_mode [mode] - change mode.

  For the possible modes see the list of modes below.

* focus_inputbar - focus the inputbar.
* follow - follow a URI.
* navigate [next|previous|left|right] - navigate the document.
* navigate_index [up|down|expand|collapse|select] - nagivate the index.
* quit - quit zathura.
* recolor - toogle recolor setting.
* reload - reload the file.
* rotate - rotate by 90 degrees clockwise.
* scroll [up|down|half_up|half_down|full_up|full_down|left|right] - scroll.
* search [string] - search for the specified string.
* switch_goto_mode - toogle goto mode.
* toggle_fullscreen - toogle fullscreen mode.
* toggle_index - toogle index mode.
* toggle_inputbar - toogle inputbar display setting.
* toogle_statusbar - toogle statusbar display setting.
* zoom [in|out|float] - zoom in, out or to a specific zooming level.

[mode] can be one of

* all
* fullscreen
* index
* normal
* insert
* visual

If [mode] is omitted, all will be used.

EXAMPLE
=======

::

  # zathurarc

  # colors
  set statusbar_bgcolor #00FF00
  set statusbar_fgcolor red

  # settings
  set height 1024
  set width 768
  set adjust_open width

  # key bindings
  map <PageUp> navigate previous
  map <PageDown> navigate next

  map + zoom in
  map - zoom out

  map <C-q> quit
  

SEE ALSO
========

zathura(1)
