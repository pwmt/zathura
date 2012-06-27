=========
zathurarc
=========

--------------------------
zathura configuration file
--------------------------

:Author: pwmt.org
:Date: VERSION
:Manual section: 5

SYNOPSIS
========

/etc/zathurarc, $XDG_CONFIG_HOME/zathura/zathurarc

DESCRIPTION
===========

The *zathurarc* file is a simple plain text file that can be populated with
various commands to change the behaviour and the look of zathura which we are
going to describe in the following subsections. Each line (besides empty lines
and comments (which start with a prepended #) is evaluated on its own, so it
is not possible to write multiple commands in one single line.

COMMANDS
========

set - Chaning options
---------------------

In addition to the build-in ``:set`` command zathura offers more options to be
changed and makes those changes permanent. To overwrite an option you just have
to add a line structured like the following

::

    set <option> <new value>

The ``option`` field has to be replaced with the name of the option that should be
changed and the ``new value`` field has to be replaced with the new value the
option should get. The type of the value can be one of the following:

* INT - An integer number
* FLOAT - A floating point number
* STRING - A character string
* BOOL - A boolean value ("true" for true, "false" for false)

In addition we advice you to check the options to get a more detailed view of
the options that can be changed and which values they should be set to.

The following example should give some deeper insight of how the ``set`` command
can be used

::

    set option1 5
    set option2 2.0
    set option3 hello
    set option4 hello\ world
    set option5 "hello world"

map - Mapping a shortcut
------------------------
It is possible to map or remap new key bindings to shortcut functions which
allows a high level of customization. The ``:map`` command can also be used in
the *zathurarc* file to make those changes permanent:

::

    map [mode] <binding> <shortcut function> <argument>

Mode
^^^^
The ``map`` command expects several arguments where only the ``binding`` as well as
the ``shortcut-function`` argument is required. Since zathura uses several modes
it is possible to map bindings only for a specific mode by passing the ``mode``
argument which can take one of the following values:

* normal (default)
* visual
* insert
* index

The brackets around the value are mandatory.

Single key binding
^^^^^^^^^^^^^^^^^^
The (possible) second argument defines the used key binding that should be
mapped to the shortcut function and is structured like the following. On the one
hand it is possible to just assign single letters, numbers or signs to it:

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
^^^^^^^^^^^^^^^
It is also possible to use modifiers like the Control or Alt button on the
keyboard. It is possible to use the following modifiers:

* A - Alt
* C - Control
* S - Shift

Now it is required to define the ``binding`` with the following structure:

::

    map <A-a> shortcut_function
    map <C-a> shortcut_function

Special keys
^^^^^^^^^^^^
zathura allows it also to assign keys like the space bar or the tab button which
also have to be written in between angle brackets. The following special keys
are currently available:

::

    Identifier Description

    BackSpace  Back space
    CapsLock   Caps lock
    Esc        Escape
    Down       Arrow down
    Up         Arrow up
    Left       Arrow left
    Right      Arrow right
    F1         F1
    F2         F2
    F3         F3
    F4         F4
    F5         F5
    F6         F6
    F7         F7
    F8         F8
    F9         F9
    F10        F10
    F11        F11
    F12        F12
    PageDown   Page Down
    PageUp     Page Up
    Return     Return
    Space      Space
    Super      Windows button
    Tab        Tab

Of course it is possible to combine those special keys with a modifier. The
usage of those keys should be explained by the following examples:

::

    map <Space> shortcut_function
    map <C-Space> shortcut_function

Mouse buttons
^^^^^^^^^^^^^
It is also possible to map mouse buttons to shortcuts by using the following
special keys:

::

    Identifier Description

    Button1    Mouse button 1
    Button2    Mouse button 2
    Button3    Mouse button 3
    Button4    Mouse button 4
    Button5    Mouse button 5

They can also be combined with modifiers:

::

    map <Button1> shortcut_function
    map <C-Button1> shortcut_function

Buffer commands
^^^^^^^^^^^^^^^
If a mapping does not match one of the previous definition but is still a valid
mapping it will be mapped as a buffer command:

::

    map abc quit
    map test quit

Shortcut functions
^^^^^^^^^^^^^^^^^^
The following shortcut functions can be mapped:


::

    Function          Description

    abort             Switch back to normal mode
    adjust_window     Adjust page width
    change_mode       Change current mode
    follow            Follow a link
    focus_inputbar    Focus inputbar
    goto              Go to a certain page
    index_navigate    Navigate through the index
    navigate          Navigate to the next/previous page
    quit              Quit zathura
    recolor           Recolor the pages
    reload            Reload the document
    rotate            Rotate the page
    scroll            Scroll
    search            Search next/previous item
    set               Set an option
    toggle_fullscreen Toggle fullscreen
    toggle_index      Show or hide index
    toggle_inputbar   Show or hide inputbar
    toggle_statusbar  Show or hide statusbar
    zoom              Zoom in or out

Pass arguments
^^^^^^^^^^^^^^
Some shortcut function require or have optional arguments which influence the
behaviour of them. Those can be passed as the last argument:

    map <C-i> zoom in
    map <C-o> zoom out

Possible arguments are:

* bottom
* default
* down
* full-down
* full-up
* half-down
* half-up
* in
* left
* next
* out
* previous
* right
* specific
* top
* up
* best-fit
* width
* rotate-cw
* rotate-ccw

unmap - Removing a shortcut
---------------------------
In addition to mapping or remaping custom key bindings it is possible to remove
existing ones by using the ``:unmap`` command. The command is used in the
following way (the explanation of the parameters is described in the ``map``
section of this document

::

    unmap [mode] <binding>


OPTIONS
=======

girara
------
This section describes settings concerning the behaviour of libgirara and
zathura. The settings described here can be changed with ``set``.

n-completion-items
^^^^^^^^^^^^^^^^^^
Defines the maximum number of displayed completion entries.

* Value type: Integer
* Default value: 15

completion-bg
^^^^^^^^^^^^^
Defines the background color that is used for command line completion
entries

* Value type: String
* Default value: #232323

completion-fg
^^^^^^^^^^^^^
Defines the foreground color that is used for command line completion
entries

* Value type: String
* Default value: #DDDDDD

completion-group-bg
^^^^^^^^^^^^^^^^^^^
Defines the background color that is used for command line completion
group elements

* Value type: String
* Default value: #000000

completion-group-fg
^^^^^^^^^^^^^^^^^^^
Defines the foreground color that is used for command line completion
group elements

* Value type: String
* Default value: #DEDEDE

completion-highlight-bg
^^^^^^^^^^^^^^^^^^^^^^^
Defines the background color that is used for the current command line
completion element

* Value type: String
* Default value: #9FBC00

completion-highlight-fg
^^^^^^^^^^^^^^^^^^^^^^^
Defines the foreground color that is used for the current command line
completion element

* Value type: String
* Default value: #232323

default-fg
^^^^^^^^^^
Defines the default foreground color

* Value type: String
* Default value: #DDDDDD

default-bg
^^^^^^^^^^
Defines the default background color

* Value type: String
* Default value: #000000

exec-command
^^^^^^^^^^^^
Defines a command the should be prepanded to any command run with exec.

* Value type: String
* Default value:

font
^^^^
Defines the font that will be used

* Value type: String
* Default value: monospace normal 9

inputbar-bg
^^^^^^^^^^^
Defines the background color for the inputbar

* Value type: String
* Default value: #131313

inputbar-fg
^^^^^^^^^^^
Defines the foreground color for the inputbar

* Value type: String
* Default value: #9FBC00

notification-error-bg
^^^^^^^^^^^^^^^^^^^^^
Defines the background color for an error notification

* Value type: String
* Default value: #FFFFFF

notification-error-fg
^^^^^^^^^^^^^^^^^^^^^
Defines the foreground color for an error notification

* Value type: String
* Default value: #FF1212

notification-warning-bg
^^^^^^^^^^^^^^^^^^^^^^^
Defines the background color for a warning notification

* Value type: String
* Default value: #FFFFFF

notification-warning-fg
^^^^^^^^^^^^^^^^^^^^^^^
Defines the foreground color for a warning notification

* Value type: String
* Default value: #FFF712

tabbar-fg
^^^^^^^^^
Defines the foreground color for a tab

* Value type: String
* Default value: #FFFFFF

tabbar-bg
^^^^^^^^^
Defines the background color for a tab

* Value type: String
* Default value: #000000

tabbar-focus-fg
^^^^^^^^^^^^^^^
Defines the foreground color for the focused tab

* Value type: String
* Default value: #9FBC00

tabbar-focus-bg
^^^^^^^^^^^^^^^
Defines the background color for the focused tab

* Value type: String
* Default value: #000000

show-scrollbars
^^^^^^^^^^^^^^^
Defines if scrollbars should be shown or not

* Value type: Boolean
* Default value: false

statusbar-bg
^^^^^^^^^^^^
Defines the background color of the statusbar

* Value type: String
* Default value: #000000

statusbar-fg
^^^^^^^^^^^^
Defines the foreground color of the statusbar

* Value type: String
* Default value: #FFFFFF

window-height
^^^^^^^^^^^^^
Defines the window height on startup

* Value type: Integer
* Default value: 600

window-width
^^^^^^^^^^^^
Defines the window width on startup

* Value type: Integer
* Default value: 800

zathura
-------

This section describes settings concerning the behaviour of zathura.

abort-clear-search
^^^^^^^^^^^^^^^^^^
Defines if the search results should be cleared on abort.

* Value type: Boolean
* Default value: true

adjust-open
^^^^^^^^^^^
Defines which auto adjustment mode should be used if a document is loaded.
Possible options are "best-fit" and "width".

* Value type: String
* Default value: best-fit

advance-pages-per-row
^^^^^^^^^^^^^^^^^^^^^
Defines if the number of pages per row should be honored when advancing a page.

* Value type: Boolean
* Default value: false

highlight-color
^^^^^^^^^^^^^^^
Defines the color that is used for highlighting parts of the document (e.g.:
show search results)

* Value type: String
* Default value: #9FBC00

highlight-active-color
^^^^^^^^^^^^^^^^^^^^^^
Defines the color that is used to show the current selected highlighted element
(e.g: current search result)

* Value type: String
* Default value: #00BC00

highlight-transparency
^^^^^^^^^^^^^^^^^^^^^^
Defines the opacity of a highlighted element

* Value type: Float
* Default value: 0.5

page-padding
^^^^^^^^^^^^
The page padding defines the gap in pixels between each rendered page.

* Value type: Integer
* Default value: 1

page-store-threshold
^^^^^^^^^^^^^^^^^^^^
Pages that are not visible get unloaded after some time. Every page that has not
been visible for page-store-treshold seconds will be unloaded.

* Value type: Integer
* Default value: 30

page-store-interval
^^^^^^^^^^^^^^^^^^^
Defines the amount of seconds between the check to unload invisible pages.

* Value type: Integer
* Default value: 30

pages-per-row
^^^^^^^^^^^^^
Defines the number of pages that are rendered next to each other in a row.

* Value type: Integer
* Default value: 1

first-page-column
^^^^^^^^^^^^^^^^^
Defines the column in which the first page will be displayed.

* Value type: Integer
* Default value: 1

recolor
^^^^^^^
En/Disables recoloring

* Value type: Boolean
* Default value: false

recolor-darkcolor
^^^^^^^^^^^^^^^^^
Defines the color value that is used to represent dark colors in recoloring mode

* Value type: String
* Default value: #FFFFFF

recolor-lightcolor
^^^^^^^^^^^^^^^^^^
Defines the color value that is used to represent light colors in recoloring mode

* Value type: String
* Default value: #000000

render-loading
^^^^^^^^^^^^^^
Defines if the "Loading..." text should be displayed if a page is rendered.

* Value type: Boolean
* Default value: true

scroll-hstep
^^^^^^^^^^^^
Defines the horizontal step size of scrolling by calling the scroll command once

* Value-type: Float
* Default value: -1

scroll-step
^^^^^^^^^^^
Defines the step size of scrolling by calling the scroll command once

* Value type: Float
* Default value: 40

scroll-wrap
^^^^^^^^^^^
Defines if the last/first page should be wrapped

* Value type: Boolean
* Default value: false

search-hadjust
^^^^^^^^^^^^^^
En/Disables horizontally centered search results

* Value-type: Boolean
* Default value: true

zoom-center
^^^^^^^^^^^
En/Disables horizontally centered zooming

* Value-type: Bool
* Default value: False

zoom-max
^^^^^^^^
Defines the maximum percentage that the zoom level can be

* Value type: Integer
* Default value: 1000

zoom-min
^^^^^^^^
Defines the minimum percentage that the zoom level can be

* Value type: Integer
* Default value: 10

zoom-step
^^^^^^^^^
Defines the amount of percent that is zoomed in or out on each command.

* Value type: Integer
* Default value: 10

SEE ALSO
========

zathura(1)
