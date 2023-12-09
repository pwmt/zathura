*********
zathurarc
*********

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

set - Changing options
----------------------

In addition to the built-in ``:set`` command zathura offers more options to be
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

Especially for options with strings as values, note that escaping of special characters and white
spaces is necessary. In the above example, ``option4`` and ``option5`` are both set to ``hello
world``, but ``set option6 hello world`` would set ``option6`` only to ``hello``.

For colors, zathura supports HTML color codes and CSS3-style ``rgb(r,g,b)`` and ``rgba(r,g,b,a)``
values. If you want to use color codes for some options, make sure to quote them accordingly or
to escape the hash symbol.

::

    set default-fg "#CCBBCC"
    set default-fg \#CCBBCC


For ``rgba``, note that it parses the color components as integers between 0 and 255 and the alpha
component as float between 0 and 1.


include - Including another config file
---------------------------------------
This commands allows one to include other configuration files. If a relative
path is given, the path will be resolved relative to the configuration file that
is currently processed.

::

    include another-config

map - Mapping a shortcut
------------------------
It is possible to map or remap new key bindings to shortcut functions which
allows a high level of customization. The ``:map`` command can also be used in
the *zathurarc* file to make those changes permanent:

::

    map [mode] <binding> <shortcut function> <argument>

*Mode*
  The ``map`` command expects several arguments where only the ``binding`` as well as
  the ``shortcut-function`` argument is required. Since zathura uses several modes
  it is possible to map bindings only for a specific mode by passing the ``mode``
  argument which can take one of the following values:

  * normal (default)
  * fullscreen
  * presentation
  * index

  The brackets around the value are mandatory.

*Single key binding*
  The (possible) second argument defines the used key binding that should be
  mapped to the shortcut function and is structured like the following. On the one
  hand it is possible to just assign single letters, numbers or signs to it:

  ::

      map a shortcut_function optional_argument
      map b shortcut_function optional_argument
      map c shortcut_function optional_argument
      map 1 shortcut_function optional_argument
      map 2 shortcut_function optional_argument
      map 3 shortcut_function optional_argument
      map ! shortcut_function optional_argument
      map ? shortcut_function optional_argument

*Using modifiers*
  It is also possible to use modifiers like the Control or Alt button on the
  keyboard. It is possible to use the following modifiers:

  * A - Alt
  * C - Control
  * S - Shift

  If any of the modifiers should be used for a binding, it is required to define
  the ``binding`` with the following structure:

  ::

      map <A-a> shortcut_function
      map <C-a> shortcut_function

*Special keys*
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
    Super      Windows key
    Tab        Tab
    Print      Print key

  Of course it is possible to combine those special keys with a modifier. The
  usage of those keys should be explained by the following examples:

  ::

    map <Space> shortcut_function
    map <C-Space> shortcut_function

*Mouse buttons*
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

*Buffer commands*
  If a mapping does not match one of the previous definition but is still a valid
  mapping it will be mapped as a buffer command:

  ::

    map abc quit
    map test quit

*Shortcut functions*
  The following shortcut functions can be mapped:

  * ``abort``

    Switch back to normal mode.

  * ``adjust_window``

    Adjust page width. Possible arguments are ``best-fit`` and ``width``.

  * ``change_mode``

    Change current mode. Pass the desired mode as argument.

  * ``display_link``:

    Display link target.

  * ``exec``:

    Execute an external command. ``$FILE`` expands to the current document path,
    ``$PAGE`` to the current page number, and ``$DBUS`` to the bus name of the
    D-Bus interface.

  * ``focus_inputbar``

    Focus inputbar.

  * ``follow``

    Follow a link.

  * ``goto``

    Go to a certain page.

  * ``jumplist``

    Move forwards/backwards in the jumplist. Pass ``forward`` as argument to
    move to the next entry and ``backward`` to move to the previous one.

  * ``navigate``

    Navigate to the next/previous page.

  * ``navigate_index``

    Navigate through the index.

  * ``print``

    Show the print dialog.

  * ``quit``

    Quit zathura.

  * ``recolor``

    Recolor pages.

  * ``reload``

    Reload the document.

  * ``rotate``

    Rotate the page. Pass ``rotate-ccw`` as argument for counterclockwise rotation
    and ``rotate-cw`` for clockwise rotation.

  * ``scroll``

    Scroll.

  * ``search``

    Search next/previous item. Pass ``forward`` as argument to search for the next
    hit and ``backward`` to search for the previous hit.

  * ``set``

    Set an option.

  * ``snap_to_page``

    Snaps to the current page. Equivalent to ``goto <current_page>``

  * ``toggle_fullscreen``

    Toggle fullscreen.

  * ``toggle_index``

    Show or hide index.

  * ``toggle_inputbar``

    Show or hide inputbar.

  * ``toggle_page_mode``

    Toggle between one and multiple pages per row.

  * ``toggle_statusbar``

    Show or hide statusbar.

  * ``zoom``

    Zoom in or out.

  * ``mark_add``

    Set a quickmark.

  * ``mark_evaluate``

    Go to a quickmark.

  * ``feedkeys``

    Simulate key presses. Note that all keys will be interpreted as if pressing a
    key on the keyboard. To input uppercase letters, follow the same convention as
    for key bindings, i.e. for ``X``, use ``<S-X>``.


*Pass arguments*
  Some shortcut function require or have optional arguments which influence the
  behaviour of them. Those can be passed as the last argument:

  ::

    map <C-i> zoom in
    map <C-o> zoom out

  Possible arguments are:

  * best-fit
  * bottom
  * backward
  * collapse
  * collapse-all
  * default
  * down
  * expand
  * expand-all
  * forward
  * full-down
  * full-up
  * half-down
  * half-up
  * in
  * left
  * next
  * out
  * page-bottom
  * page-top
  * previous
  * right
  * rotate-ccw
  * rotate-cw
  * select
  * specific
  * toggle
  * top
  * up
  * width

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

This section describes settings concerning the behaviour of girara and
zathura. The settings described here can be changed with ``set``.

girara
------

*n-completion-items*
  Defines the maximum number of displayed completion entries.

  * Value type: Integer
  * Default value: 15

*completion-bg*
  Defines the background color that is used for command line completion
  entries

  * Value type: String
  * Default value: #232323

*completion-fg*
  Defines the foreground color that is used for command line completion
  entries

  * Value type: String
  * Default value: #DDDDDD

*completion-group-bg*
  Defines the background color that is used for command line completion
  group elements

  * Value type: String
  * Default value: #000000

*completion-group-fg*
  Defines the foreground color that is used for command line completion
  group elements

  * Value type: String
  * Default value: #DEDEDE

*completion-highlight-bg*
  Defines the background color that is used for the current command line
  completion element

  * Value type: String
  * Default value: #9FBC00

*completion-highlight-fg*
  Defines the foreground color that is used for the current command line
  completion element

  * Value type: String
  * Default value: #232323

*default-bg*
  Defines the default background color

  * Value type: String
  * Default value: #000000

*default-fg*
  Defines the default foreground color

  * Value type: String
  * Default value: #DDDDDD

*exec-command*
  Defines a command the should be prepended to any command run with exec.

  * Value type: String
  * Default value:

*font*
  Defines the font that will be used

  * Value type: String
  * Default value: monospace normal 9

*guioptions*
  Shows or hides GUI elements.
  If it contains 'c', the command line is displayed.
  If it contains 's', the statusbar is displayed.
  If it contains 'h', the horizontal scrollbar is displayed.
  If it contains 'v', the vertical scrollbar is displayed.

  * Value type: String
  * Default value: s

*inputbar-bg*
  Defines the background color for the inputbar

  * Value type: String
  * Default value: #131313

*inputbar-fg*
  Defines the foreground color for the inputbar

  * Value type: String
  * Default value: #9FBC00

*notification-bg*
  Defines the background color for a notification

  * Value type: String
  * Default value: #FFFFFF

*notification-fg*
  Defines the foreground color for a notification

  * Value type: String
  * Default value: #000000

*notification-error-bg*
  Defines the background color for an error notification

  * Value type: String
  * Default value: #FF1212

*notification-error-fg*
  Defines the foreground color for an error notification

  * Value type: String
  * Default value: #FFFFFF

*notification-warning-bg*
  Defines the background color for a warning notification

  * Value type: String
  * Default value: #FFF712

*notification-warning-fg*
  Defines the foreground color for a warning notification

  * Value type: String
  * Default value: #FFFFFF

*statusbar-bg*
  Defines the background color of the statusbar

  * Value type: String
  * Default value: #000000

*statusbar-fg*
  Defines the foreground color of the statusbar

  * Value type: String
  * Default value: #FFFFFF

*statusbar-h-padding*
  Defines the horizontal padding of the statusbar and notificationbar

  * Value type: Integer
  * Default value: 8

*statusbar-v-padding*
  Defines the vertical padding of the statusbar and notificationbar

  * Value type: Integer
  * Default value: 2

*window-icon*
  Defines the path for a icon to be used as window icon.

  * Value type: String
  * Default value:

*window-height*
  Defines the window height on startup

  * Value type: Integer
  * Default value: 600

*window-width*
  Defines the window width on startup

  * Value type: Integer
  * Default value: 800

zathura
-------

  This section describes settings concerning the behaviour of zathura.

*abort-clear-search*
  Defines if the search results should be cleared on abort.

  * Value type: Boolean
  * Default value: true

*adjust-open*
  Defines which auto adjustment mode should be used if a document is loaded.
  Possible options are "best-fit" and "width".

  * Value type: String
  * Default value: best-fit

*advance-pages-per-row*
  Defines if the number of pages per row should be honored when advancing a page.

  * Value type: Boolean
  * Default value: false

*continuous-hist-save*
  Tells zathura whether to save document history at each page change or only when
  closing a document.

  * Value type: Boolean
  * Default value: false

*database*
  Defines the database backend to use for bookmarks and input history. Possible
  values are "plain", "sqlite" (if built with sqlite support) and "null". If
  "null" is used, bookmarks and input history will not be stored.

  * Value type: String
  * Default value: plain

*dbus-raise-window*
  Defines whether zathura's window should be raised when receiving certain
  commands via D-Bus.

  * Value type: Boolean
  * Default value: true

*dbus-service*
  En/Disables the D-Bus service. If the services is disabled, SyncTeX forward
  synchronization is not available.

  * Value type: Boolean
  * Default value: true

*double-click-follow*
  Defines whether double or single click on a link should trigger follow.

  * Value type: Boolean
  * Default value: true

*filemonitor*
  Defines the file monitor backend used to check for changes in files. Possible
  values are "glib", "signal" (if signal handling is supported), and "noop". The
  "noop" file monitor does not trigger reloads.

  * Value type: String
  * Default value: glib

*first-page-column*
  Defines the column in which the first page will be displayed.
  This setting is stored separately for every value of pages-per-row according to
  the following pattern <1 page per row>:[<2 pages per row>[: ...]]. The last
  value in the list will be used for all other number of pages per row if not set
  explicitly.

  Per default, the first column is set to 2 for double-page layout, i.e. the value
  is set to 1:2. A value of 1:1:3 would put the first page in dual-page layout in
  the first column, and for layouts with more columns the first page would be put
  in the 3rd column.

  * Value type: String
  * Default value: 1:2

*highlight-active-color*
  Defines the color that is used to show the current selected highlighted element
  (e.g: current search result)

  * Value type: String
  * Default value: #00BC00

*highlight-color*
  Defines the color that is used for highlighting parts of the document (e.g.:
  show search results)

  * Value type: String
  * Default value: #9FBC00

*highlight-fg*
  Defines the color that is used for text when highlighting parts of the
  document (e.g.: number for links).

  * Value type: String
  * Default value: #9FBC00

*highlight-transparency*
  Defines the opacity of a highlighted element

  * Value type: Float
  * Default value: 0.5

*highlighter-modifier*
  Defines the modifier that needs to be pressed together with the left mouse button
  to draw the highlighter. Possible values are "shift", "ctrl" and "alt".

  * Value type: String
  * Default value: shift

*incremental-search*
  En/Disables incremental search (search while typing).

  * Value type: Boolean
  * Default value: true

*index-active-bg*
  Define the background color of the selected element in index mode.

  * Value type: String
  * Default value: #9FBC00

*index-active-fg*
  Defines the foreground color of the selected element in index mode.

  * Value type: String
  * Default value: #232323

*index-bg*
  Define the background color of the index mode.

  * Value type: String
  * Default value: #232323

*index-fg*
  Defines the foreground color of the index mode.

  * Value type: String
  * Default value: #DDDDDD

*link-hadjust*
  En/Disables aligning to the left internal link targets, for example from the
  index.

  * Value type: Boolean
  * Default value: true

*link-zoom*
  En/Disables the ability of changing zoom when following links.

  * Value type: Boolean
  * Default value: true

*page-cache-size*
  Defines the maximum number of pages that could be kept in the page cache. When
  the cache is full and a new page that isn't cached becomes visible, the least
  recently viewed page in the cache will be evicted to make room for the new one.
  Large values for this variable are NOT recommended, because this will lead to
  consuming a significant portion of the system memory.

  * Value type: Integer
  * Default value: 15

*page-padding*
  The page padding defines the gap in pixels between each rendered page.

  * Value type: Integer
  * Default value: 1

*page-right-to-left*
  Defines whether pages in multi-column view should start from the right side.

  * Value type: Boolean
  * Default value: false

*page-thumbnail-size*
  Defines the maximum size in pixels of the thumbnail that could be kept in the
  thumbnail cache per page. The thumbnail is scaled for a quick preview during
  zooming before the page is rendered. When the page is rendered, the result is
  saved as the thumbnail only if the size is no more than this value. A larger
  value increases quality but introduces longer delay in zooming and uses more
  system memory.

  * Value type: Integer
  * Default value: 4194304 (4M)

*pages-per-row*
  Defines the number of pages that are rendered next to each other in a row.

  * Value type: Integer
  * Default value: 1

*recolor*
  En/Disables recoloring

  * Value type: Boolean
  * Default value: false

*recolor-darkcolor*
  Defines the color value that is used to represent dark colors in recoloring mode

  * Value type: String
  * Default value: #FFFFFF

*recolor-keephue*
  En/Disables keeping original hue when recoloring

  * Value type: Boolean
  * Default value: false

*recolor-lightcolor*
  Defines the color value that is used to represent light colors in recoloring mode

  * Value type: String
  * Default value: #000000

*recolor-reverse-video*
  Defines if original image colors should be kept while recoloring.

  * Value type: Boolean
  * Default value: false

*render-loading*
  Defines if the "Loading..." text should be displayed if a page is rendered.

  * Value type: Boolean
  * Default value: true

*render-loading-bg*
  Defines the background color that is used for the "Loading..." text.

  * Value type: String
  * Default value: #FFFFFF

*render-loading-fg*
  Defines the foreground color that is used for the "Loading..." text.

  * Value type: String
  * Default value: #000000

*sandbox*
  Defines the sandbox mode to use for the seccomp syscall filter. Possible
  values are "none", "normal" and "strict". If "none" is used, the sandbox
  will be disabled. The use of "normal" will provide minimal protection and
  allow normal use of zathura with support for all features. The "strict" mode
  is a read only sandbox that is intended for viewing documents only.

  * Value type: String
  * Default value: normal

  Some features are disabled when using strict sandbox mode:

  * saving/writing files
  * use of input methods like ibus
  * printing
  * bookmarks and history

  The strict sandbox mode is still experimental with some libc implementations.
  Currently supported and tested libc implementations: glibc

  No feature regressions are expected when using normal sandbox mode.

  When running under WSL, the default is "none" since seccomp is not supported in
  that environment.

*scroll-full-overlap*
  Defines the proportion of the current viewing area that should be
  visible after scrolling a full page.

  * Value type: Float
  * Default value: 0

*scroll-hstep*
  Defines the horizontal step size of scrolling by calling the scroll command once

  * Value type: Float
  * Default value: -1

*scroll-step*
  Defines the step size of scrolling by calling the scroll command once

  * Value type: Float
  * Default value: 40

*scroll-page-aware*
  Defines if scrolling by half or full pages stops at page boundaries.

  * Value type: Boolean
  * Default value: false

*scroll-wrap*
  Defines if the last/first page should be wrapped

  * Value type: Boolean
  * Default value: false

*search-hadjust*
  En/Disables horizontally centered search results.

  * Value type: Boolean
  * Default value: true

*selection-clipboard*
  Defines the X clipboard into which mouse-selected data will be written.  When it
  is "clipboard", selected data will be written to the CLIPBOARD clipboard, and
  can be pasted using the Ctrl+v key combination.  When it is "primary", selected
  data will be written to the PRIMARY clipboard, and can be pasted using the
  middle mouse button, or the Shift-Insert key combination.

  * Value type: String
  * Default value: primary

*selection-notification*
  Defines if a notification should be displayed after selecting text.

  * Value type: Boolean
  * Default value: true

*signature-error-color*
  Defines the background color when displaying additional information for
  signatures with errors.

  * Value type: String
  * Default value: rgba(92%,11%,14%,0.9)

*signature-success-color*
  Defines the background color when displaying additional information for valid
  signatures.

  * Value type: String
  * Default value: rgba(18%,80%,33%,0.9)

*signature-warning-color*
  Defines the background color when displaying additional information for
  signatures with warnings.

  * Value type: String
  * Default value: rgba(100%,84%,0%,0.9)

*show-directories*
  Defines if the directories should be displayed in completion.

  * Value type: Boolean
  * Default value: true

*show-hidden*
  Defines if hidden files and directories should be displayed in completion.

  * Value type: Boolean
  * Default value: false

*show-recent*
  Defines the number of recent files that should be displayed in completion.
  If the value is negative, no upper bounds are applied. If the value is 0, no
  recent files are shown.

  * Value type: Integer
  * Default value: 10

*show-signature-information*
  Defines whether additional information on signatures embedded in documents
  should be displayed.

  * value type: Boolean
  * Default value false

*smooth-reload*
  Defines if flickering will be removed when a file is reloaded on change. This
  option might increase memory usage.

  * Value type: Boolean
  * Default value: true

*statusbar-basename*
  Use basename of the file in the statusbar.

  * Value type: Boolean
  * Default value: false

*statusbar-home-tilde*
  Display a short version of the file path, which replaces $HOME with ~, in the statusbar.

  * Value type: Boolean
  * Default value: false

*statusbar-page-percent*
  Display (current page / total pages) as a percent in the statusbar.

  * Value type: Boolean
  * Default value: false

*synctex*
  En/Disables SyncTeX backward synchronization support.

  * Value type: Boolean
  * Default value: true

*synctex-edit-modifier*
  Defines the modifier that needs to be pressed together with the left mouse button
  to trigger the SyncTeX backward synchronization. Possible values are "shift",
  "ctrl" and "alt".

  * Value type: String
  * Default value: ctrl

*synctex-editor-command*
  Defines the command executed for SyncTeX backward synchronization.

  * Value type: String
  * Default value:

*vertical-center*
  Center the screen at the vertical midpoint of the page by default.

  * Value type: Boolean
  * Default value: false

*window-icon-document*
  Defines whether the window document should be updated based on the first page of
  a dcument.

  * Value type: Boolean
  * Default value: false

*window-title-basename*
  Use basename of the file in the window title.

  * Value type: Boolean
  * Default value: false

*window-title-home-tilde*
  Display a short version of the file path, which replaces $HOME with ~, in the window title.

  * Value type: Boolean
  * Default value: false

*window-title-page*
  Display the page number in the window title.

  * Value type: Boolean
  * Default value: false

*zoom-center*
  En/Disables horizontally centered zooming.

  * Value type: Boolean
  * Default value: false

*zoom-max*
  Defines the maximum percentage that the zoom level can be.

  * Value type: Integer
  * Default value: 1000

*zoom-min*
  Defines the minimum percentage that the zoom level can be.

  * Value type: Integer
  * Default value: 10

*zoom-step*
  Defines the amount of percent that is zoomed in or out on each command.

  * Value type: Integer
  * Default value: 10

SEE ALSO
========

zathura(1)
