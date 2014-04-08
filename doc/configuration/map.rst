map - Mapping a shortcut
========================

It is possible to map or remap new key bindings to shortcut functions
which allows a high level of customization. The *:map* command can also
be used in the *zathurarc* file to make those changes permanent:

::

    map [mode] <binding> <shortcut function> <argument>

Mode
----

The *map* command expects several arguments where only the *binding* as
well as the *shortcut-function* argument is required. Since zathura uses
several modes it is possible to map bindings only for a specific mode by
passing the *mode* argument which can take one of the following values:

-  normal (default)
-  visual
-  insert
-  fullscreen
-  index

The brackets around the value are mandatory.

Single key binding
~~~~~~~~~~~~~~~~~~

The (possible) second argument defines the used key binding that should
be mapped to the shortcut function and is structured like the following.
On the one hand it is possible to just assign single letters, numbers or
signs to it:

::

    map a shortcut_function
    map b shortcut_function
    map c shortcut_function
    map 1 shortcut_function
    map 2 shortcut_function
    map 3 shortcut_function
    map ! shortcut_function
    map ? shortcut_function

Using modifiers
---------------

It is also possible to use modifiers like the *Control* or *Alt* button
on the keyboard. It is possible to use the following modifiers:

-  A - *Alt*
-  C - *Control*
-  S - *Shift*

Now it is required to define the *binding* with the following structure:

::

    map <A-a> shortcut_function
    map <C-a> shortcut_function

Special keys
------------

zathura allows it also to assign keys like the space bar or the tab
button which also have to be written in between angle brackets. The
following special keys are currently available:

+--------------+--------------------+
| Identifier   | Description        |
+==============+====================+
| BackSpace    | *Back space*       |
+--------------+--------------------+
| CapsLock     | *Caps lock*        |
+--------------+--------------------+
| Esc          | *Escape*           |
+--------------+--------------------+
| Down         | *Arrow down*       |
+--------------+--------------------+
| Up           | *Arrow up*         |
+--------------+--------------------+
| Left         | *Arrow left*       |
+--------------+--------------------+
| Right        | *Arrow right*      |
+--------------+--------------------+
| F1           | *F1*               |
+--------------+--------------------+
| F2           | *F2*               |
+--------------+--------------------+
| F3           | *F3*               |
+--------------+--------------------+
| F4           | *F4*               |
+--------------+--------------------+
| F5           | *F5*               |
+--------------+--------------------+
| F6           | *F6*               |
+--------------+--------------------+
| F7           | *F7*               |
+--------------+--------------------+
| F8           | *F8*               |
+--------------+--------------------+
| F9           | *F9*               |
+--------------+--------------------+
| F10          | *F10*              |
+--------------+--------------------+
| F11          | *F11*              |
+--------------+--------------------+
| F12          | *F12*              |
+--------------+--------------------+
| PageDown     | *Page Down*        |
+--------------+--------------------+
| PageUp       | *Page Up*          |
+--------------+--------------------+
| Return       | *Return*           |
+--------------+--------------------+
| Space        | *Space*            |
+--------------+--------------------+
| Super        | *Windows button*   |
+--------------+--------------------+
| Tab          | *Tab*              |
+--------------+--------------------+

Of course it is possible to combine those special keys with a modifier.
The usage of those keys should be explained by the following examples:

::

    map <Space> shortcut_function
    map <C-Space> shortcut_function

Mouse buttons
-------------

It is also possible to map mouse buttons to shortcuts by using the
following special keys:

+--------------+--------------------+
| Identifier   | Description        |
+==============+====================+
| Button1      | *Mouse button 1*   |
+--------------+--------------------+
| Button2      | *Mouse button 2*   |
+--------------+--------------------+
| Button3      | *Mouse button 3*   |
+--------------+--------------------+
| Button4      | *Mouse button 4*   |
+--------------+--------------------+
| Button5      | *Mouse button 5*   |
+--------------+--------------------+

They can also be combined with modifiers:

::

    map <Button1> shortcut_function
    map <C-Button1> shortcut_function

Buffer commands
---------------

If a mapping does not match one of the previous definition but is still
a valid mapping it will be mapped as a buffer command:

::

    map abc quit
    map test quit

Shortcut functions
------------------

The following shortcut functions can be mapped:

+----------------------+----------------------------------------+
| Function             | Description                            |
+======================+========================================+
| abort                | *Switch back to normal mode*           |
+----------------------+----------------------------------------+
| adjust\_window       | *Adjust page width*                    |
+----------------------+----------------------------------------+
| change\_mode         | *Change current mode*                  |
+----------------------+----------------------------------------+
| follow               | *Follow a link*                        |
+----------------------+----------------------------------------+
| focus\_inputbar      | *Focus inputbar*                       |
+----------------------+----------------------------------------+
| goto                 | *Go to a certain page*                 |
+----------------------+----------------------------------------+
| index\_navigate      | *Navigate through the index*           |
+----------------------+----------------------------------------+
| navigate             | *Navigate to the next/previous page*   |
+----------------------+----------------------------------------+
| quit                 | *Quit zathura*                         |
+----------------------+----------------------------------------+
| recolor              | *Recolor the pages*                    |
+----------------------+----------------------------------------+
| reload               | *Reload the document*                  |
+----------------------+----------------------------------------+
| rotate               | *Rotate the page*                      |
+----------------------+----------------------------------------+
| scroll               | *Scroll*                               |
+----------------------+----------------------------------------+
| search               | *Search next/previous item*            |
+----------------------+----------------------------------------+
| set                  | *Set an option*                        |
+----------------------+----------------------------------------+
| toggle\_fullscreen   | *Toggle fullscreen*                    |
+----------------------+----------------------------------------+
| toggle\_index        | *Show or hide index*                   |
+----------------------+----------------------------------------+
| toggle\_inputbar     | *Show or hide inputbar*                |
+----------------------+----------------------------------------+
| toggle\_statusbar    | *Show or hide statusbar*               |
+----------------------+----------------------------------------+
| zoom                 | *Zoom in or out*                       |
+----------------------+----------------------------------------+

Pass arguments
--------------

Some shortcut function require or have optional arguments which
influence the behaviour of them. Those can be passed as the last
argument:

::

    map <C-i> zoom in
    map <C-o> zoom out

Possible arguments are:

-  bottom
-  default
-  down
-  full-down
-  full-up
-  half-down
-  half-up
-  in
-  left
-  next
-  out
-  previous
-  right
-  specific
-  top
-  up
-  best-fit
-  width
-  rotate-cw
-  rotate-ccw

unmap - Removing a shortcut
~~~~~~~~~~~~~~~~~~~~~~~~~~~

In addition to mapping or remaping custom key bindings it is possible to
remove existing ones by using the *:unmap* command. The command is used
in the following way (the explanation of the parameters is described in
the *map* section of this document

::

    unmap [mode] <binding>

