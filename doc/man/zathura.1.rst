Manpage
=======

Synopsis
--------

zathura [-c PATH] [-d PATH] [-p PATH] [-w PASSWORD] [-P NUMBER]
[--fork] [-l LEVEL] [-s] [-x CMD] [--synctex-forward INPUT] [--synctex-pid PID]
[-find STRING]
<files>

Description
-----------

**zathura** is a highly customizable and functional document viewer. It provides
a minimalistic and space saving interface as well as an easy usage that mainly
focuses on keyboard interaction.

**zathura** displays the given files. If a single hyphen-minus (-) is given as
file name, the content will be read from the standard input. If no files are
given, an empty **zathura** instance launches.

Options
-------

-c, --config-dir=path
  Path to the config directory

-d, --data-dir=path
  Path to the data directory

-p, --plugins-dir=path
  Path to the directory containing plugins

-w, --password=password
  The documents password. If multiple documents are opened at once, the
  password will be used for the first one and zathura will ask for the
  passwords of the remaining files if needed.

-P, --page=number
  Opens the document at the given page number. Pages are numbered starting
  with 1, and negative numbers indicate page numbers starting from the end
  of the document, -1 being the last page.

-f, --find=string
  Opens the document and searches for the given string.

-l, --log-level=level
  Set log level (debug, info, warning, error)

-x, --synctex-editor-command=command
  Set the SyncTeX editor command. Overrides the synctex-editor-command setting.

--synctex-forward=input
  Jump to the given position. The switch expects the same format as specified
  for synctex's view -i. If no instance is running for the specified document,
  a new instance will be launched (only if --synctex-pid is not specified).

--synctex-pid=pid
  Instead of looking for an instance having the correct file opened, try only
  the instance with the given PID. Note that if the given PID does not have the
  correct file open or does not exist, no new instance will be spanned.

--mode=mode
  Start in a non-default mode

--fork
  Fork into background

--version
  Display version string and exit

--help
  Display help and exit

Mouse and key bindings
----------------------

General

  J, PgDn
    Go to the next page
  K, PgUp
    Go to the previous page
  h, k, j, l
    Scroll to the left, down, up or right direction
  Left, Down, Up, Right
    Scroll to the left, down, up or right direction
  ^t, ^d, ^u, ^y
    Scroll a half page left, down, up or right
  t, ^f, ^b, space, <S-space>, y
    Scroll a full page left, down, up or right
  gg, G, nG
    Goto to the first, the last or to the nth page
  P
    Snaps to the current page
  H, L
    Goto top or bottom of the current page
  ^o, ^i
    Move backward and forward through the jump list
  ^j, ^k
    Bisect forward and backward between the last two jump points
  ^c, Escape
    Abort
  a, s
    Adjust window in best-fit or width mode
  /, ?
    Search for text
  n, N
    Search for the next or previous result
  o, O
    Open document
  f
    Follow links
  F
    Display link target
  c
    Copy link target into the clipboard
  \:
    Enter command
  r
    Rotate by 90 degrees
  Meta-r
    Rotate page by 90 degrees
  ^r
    Recolor (grayscale and invert colors)
  R
    Reload document
  Tab
    Show index and switch to **Index mode**
  v
    Show a caret on the current page and switch to **Visual mode**
  V
    Select the line under the caret and switch to **Visual mode**
  gv
    Switch to **Visual mode** with the last selection
  d
    Toggle dual page view
  D
    Cycle opening column in dual page view
  F5
    Switch to presentation mode
  F11
    Switch to fullscreen mode
  ^m
    Toggle inputbar
  ^n
    Toggle statusbar
  Escape
    Hide notification bar
  +, -, =
    Zoom in, out or to the original size
  zI, zO, z0
    Zoom in, out or to the original size
  n=
    Zoom to size n
  mX
    Set a quickmark to a letter or number X
    (may conflict with default action of X, e.g. 'O' or 'P')
  'X
    Goto quickmark saved at letter or number X
  q
    Quit
  w
    Toggle single-page mode


Fullscreen mode

  J, K
    Go to the next or previous page
  space, <S-space>, <BackSpace>
    Scroll a full page down or up
  gg, G, nG
    Goto to the first, the last or to the nth page
  ^c, Escape
    Abort
  F11
    Switch to normal mode
  +, -, =
    Zoom in, out or to the original size
  zI, zO, z0
    Zoom in, out or to the original size
  n=
    Zoom to size n
  q
    Quit

Presentation mode

  space, <S-space>, <BackSpace>
    Scroll a full page down or up
  ^c, Escape
    Abort
  F5
    Switch to normal mode
  q
    Quit

Index mode

  k, j
    Move to upper or lower entry
  Up, Down
    Move to upper or lower entry
  ^u, ^d
    Scroll and move up or down by half a screen
  l, Right, zo
    Expand entry
  zO
    Expand entry recursively
  L, zR
    Expand all entries
  h, Left, zc
    Collapse entry
  zC
    Collapse entry recursively
  H, zM
    Collapse all entries
  space, Return, ^j
    Select and open entry
  gg, G
    Goto to the first or last entry
  Tab, Escape, ^[, ^c
    Hide index and switch to normal mode

Visual mode

  Visual mode selects text with the keyboard. It starts with a caret at the top
  of the visible part of the current page, or at the current search result on
  it. j, k, w, b, e and their big word variants continue on the next or
  previous page, so selections can span pages. Motions take a count, e.g. 3w
  or 10j. Text selection needs a plugin that provides the text layout of a
  page.

  Images can be selected too, through their labels (see s). A selected image
  is highlighted instead of the caret; y copies it, and v, V or any motion goes
  back to the text next to it.

  h, l, Left, Right
    Move the caret one character left or right on the line
  j, k, Down, Up
    Move the caret one line down or up
  w, b, e
    Move to the start of the next word, the start of the previous word or the
    end of the word
  W, B, E
    Like w, b and e, but words are separated by blanks only
  0, Home, ^, $, End
    Move to the start of the line, its first non-blank character or its end
  gg, G
    Move to the first or the last line of the document
  fX, FX
    Move to the next or previous character X on the line
  tX, TX
    Move to just before the next or just after the previous character X on the
    line
  ;, ,
    Repeat the last f, F, t or T in the same or the opposite direction
  s
    Show labels on the visible words and images and move the caret to the word,
    or select the image, whose label is typed. An image is outlined and its
    label sits in the top left corner of its visible part on the page; images
    smaller than 16 points get none.
  iw, aw, iW, aW
    Select the word under the caret, with the blanks around it for aw and aW
  v
    Start or stop a characterwise selection
  V
    Start or stop a linewise selection
  o
    Move the caret to the other end of the selection
  y
    Copy the selection, or the selected image, to the clipboard set by
    *selection-clipboard* and switch back. The copied text or image stays
    highlighted for a moment, see *visual-flash-duration*.
  yy
    Without a selection, copy the line of the caret and stay in visual mode.
    With a count, copy that many lines, e.g. 3yy. The copied lines are
    highlighted for a moment.
  \:
    Open the command line
  Escape, ^[, ^c
    Clear the selection and switch back


Mouse bindings

  Scroll
    Scroll up or down
  ^Scroll
    Zoom in or out
  Drag Button2 (middle button drag)
    Pan the document
  Button1 (left click)
    Follow link
  Drag Button1
    Select text, also across pages. Dragging near the top or bottom of the
    window scrolls. A selection across pages needs the same plugin support as
    visual mode and stays highlighted until the next click or Escape.
  Drag S-Button1
    Highlight region
  Button3 (right click)
    Open popup menu to copy/save image
    (activates for images recognized by ``export`` command)


Commands
---------

bmark
  Save a bookmark.

bdelete
  Delete a bookmark.

blist
  List bookmarks.

bjump
  Jump to given bookmark.

jumplist
  Show recent jumps in jumplist (by default last 5). Optional argument
  specifies number of entries to show. Negative value "-N" shows all
  except the first "N" entries.

mark
  Set a quickmark.

delmarks
  Delete a quickmark. Abbreviation: ``delm``.

close
  Close document.

quit
  Quit zathura. Abbreviation: ``q``.

exec
  Execute an external command. ``$FILE`` expands to the current document path,
  ``$PAGE`` to the current page number, and ``$DBUS`` to the bus name of the
  D-Bus interface. Alias: ``!`` (space is still needed after).

info
  Show document information.

open
  Open a document. Abbreviation: ``o``.

offset
  Set page offset.

print
  Print document.

write(!)
  Save document (and force overwriting). Alias: ``save(!)``.

export
  Export attachments. First argument specifies the attachment identifier
  (use completion with ``Tab``), second argument gives the target filename
  (relative to current working directory).

dump
  Write values, descriptions, etc. of all current settings to a file.

source
  Source a configuration file. It is possible to change the config directory
  by passing an argument.

hlsearch
  Highlight current search results.

nohlsearch
  Remove highlights of current search results. Abbreviation: ``nohl``.

version
  Show version information.

Configuration
-------------

The default appearance and behaviour of zathura can be overwritten by modifying
the *zathurarc* file (default path: ~/.config/zathura/zathurarc). For a detailed
description please consult zathurarc(5).

SyncTeX support
---------------

Both SyncTeX forward and backwards synchronization are supported by zathura, To
enable SyncTeX forward synchronization, please look at the *--synctex-forward*
and *--synctex-editor* options. zathura will also emit a signal via the D-Bus
interface. To support synctex backwards synchronization, zathura provides a
D-Bus interface that can be called by the editor. For convince zathura also
knows how to parse the output of the *synctex view* command. It is enough to
pass the arguments to *synctex view*'s *-i* option to zathura via
*--synctex-forward* and zathura will pass the information to the correct
instance.

For gvim forward and backwards synchronization support can be set up as follows:
First add the following to the vim configuration:

::

    function! Synctex()
      execute "silent !zathura --synctex-forward " . line('.') . ":" . col('.') . ":" . bufname('%') . " " . g:syncpdf
      redraw!
    endfunction
    map <C-enter> :call Synctex()<cr>

Then launch *zathura* with

::

    zathura -x "gvim --servername vim -c \"let g:syncpdf='$1'\" --remote +%{line} %{input}" $file

Some editors support zathura as viewer out of the box:

* LaTeXTools for SublimeText
  (https://latextools.readthedocs.io/en/latest/available-viewers/#zathura)
* LaTeX for Atom (https://atom.io/packages/latex)

Environment variables
---------------------

ZATHURA_PLUGINS_PATH
  Path to the directory containing plugins. This directory is only considered if
  no other directory was specified using --plugins-dir.

Sandbox
-------

The **zathura-sandbox** binary runs zathura with a seccomp and/or landlock
based sandbox enabled. Some features are disabled when using sandbox mode:

* saving/writing files
* use of input methods like ibus
* printing
* bookmarks and history
* D-Bus integration
* SyncTeX support
* accessibility support

The sandbox mode is still experimental with some libc implementations. The
current supported and tested libc implementations is glibc.

Known bugs
----------

If GDK_NATIVE_WINDOWS is enabled you will experience problems with large
documents. In this case zathura might crash or pages cannot be rendered
properly. Disabling GDK_NATIVE_WINDOWS fixes this issue. The same issue may
appear, if overlay-scrollbar is enabled in GTK_MODULES.

See Also
--------
`zathurarc(5)`
