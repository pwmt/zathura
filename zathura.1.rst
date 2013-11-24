=======
zathura
=======

-----------------
a document viewer
-----------------

:Author: pwmt.org
:Date: VERSION
:Manual section: 1

SYNOPOSIS
=========
| zathura [OPTION]...
| zathura [OPTION]... FILE [FILE ...]

DESCRIPTION
===========
zathura is a highly customizable and functional document viewer. It provides a
minimalistic and space saving interface as well as an easy usage that mainly
focuses on keyboard interaction.

OPTIONS
=======

-e [xid], --reparent [xid]
  Reparents to window specified by xid

-c [path], --config-dir [path]
  Path to the config directory

-d [path], --data-dir [path]
  Path to the data directory

-p [path], --plugins-dir [path]
  Path to the directory containing plugins

-w [password], --password [password]
  The documents password. If multiple documents are opened at once, the password
  will be used for the first one and zathura will ask for the passwords of the
  remaining files if needed.

-P [number], --page [number]
  Open the document at the given page number. Pages are numbered starting with
  1, and negative numbers indicate page numbers starting from the end of the
  document, -1 being the last page.

--fork
  Fork into the background

-l [level], --debug [level]
  Set log debug level (debug, info, warning, error)

-s, --synctex
  Enable synctex support

-x [cmd], --synctex-editor-command [cmd]
  Set the synctex editor command

MOUSE AND KEY BINDINGS
======================

J, K
  Go to the next or previous page
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
\:
  Enter command
r
  Rotate by 90 degrees
^r
  Recolor
R
  Reload document
Tab
  Show index and switch to **Index mode**
d
  Toggle dual page view
F5
  Switch to fullscreen mode
^m
  Toggle inputbar
^n
  Toggle statusbar
+, -, =
  Zoom in, out or to the original size
zI, zO, z0
  Zoom in, out or to the original size
n=
  Zoom to size n
mX
  Set a quickmark to a letter or number X
'X
  Goto quickmark saved at letter or number X
q
  Quit

Fullscreen mode
---------------

J, K
  Go to the next or previous page
space, <S-space>, <BackSpace>
  Scroll a full page down or up
gg, G, nG
  Goto to the first, the last or to the nth page
^c, Escape
  Abort
F5
  Switch to normal mode
+, -, =
  Zoom in, out or to the original size
zI, zO, z0
  Zoom in, out or to the original size
n=
  Zoom to size n
q
  Quit

Index mode
----------

k, j
  Move to upper or lower entry
l
  Expand entry
L
  Expand all entries
h
  Collapse entry
H
  Collapse all entries
space, Return
  Select and open entry

Mouse bindings
--------------
Scroll
  Scroll up or down
^Scroll
  Zoom in or out
Hold Button2
  Pan the document
Button1
  Follow link

COMMANDS
========
bmark
  Save a bookmark
bdelete
  Delete a bookmark
blist
  List bookmarks
close
  Close document
exec
  Execute an external command
info
  Show document information
help
  Show help page
open, o
  Open a document
offset
  Set page offset
print
  Print document
write, write!
  Save document (and force overwriting)
export
  Export attachments

CONFIGURATION
=============
The default appearance and behaviour of zathura can be overwritten by modifying
the *zathurarc* file (default path: ~/.config/zathura/zathurarc). For a detailed
description please consult zathurarc(5).

KNOWN BUGS
==========
If GDK_NATIVE_WINDOWS is enabled you will experience problems with large
documents. In this case zathura might crash or pages cannot be rendered
properly. Disabling GDK_NATIVE_WINDOWS fixes this issue.

SEE ALSO
========

zathurarc(5)
