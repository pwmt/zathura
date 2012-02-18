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
| zathura [OPTION]... FILE
| zathura [OPTION]... FILE PASSWORD

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

--fork
  Fork into the background

MOUSE AND KEY BINDINGS
======================

J, K
  Go to the next or previous page
h, k, j, l
  Scroll to the left, down, up or right direction
Left, Down, Up, Right
  Scroll to the left, down, up or right direction
^d, ^u
  Scroll a half page down or up
^f, ^b, space, <S-space>
  Scroll a full page down or up
gg, G, nG
  Goto to the first, the last or to the nth page
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
\:
  Enter command
r
  Rotate by 90 degrees
^i
  Recolor
R
  Reload document
Tab
  Show index and switch to **Index mode**
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

Index mode
----------

k, j
  Move to upper or lower entry
l
  Expand entry
h
  Collapse entry
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
info
  Show document information
help
  Show help page
open, o
  Open a document
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
description please visit http://pwmt.org/projects/zathura/configuration
