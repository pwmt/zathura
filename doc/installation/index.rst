Installation
============

Dependencies
------------

The core of zathura depends on two external libraries,
`girara </projects/girara/>`_, our simplistic user interface library and
`GTK+ <http://www.gtk.org/>`_, a cross-platform widget toolkit.
Depending on which filetypes should be supported you are going to need
additional libraries to build those file type plugins.

Core dependencies
~~~~~~~~~~~~~~~~~

-  `girara </projects/girara/>`_, our simplistic user interface library
   (>= 0.1.8)
-  `GTK+ <http://www.gtk.org/>`_, a cross-platform widget toolkit (>=
   2.28)

Optional and build dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  `sqlite3 <https://www.sqlite.org/>`_, a SQL database engine
-  `intltool <https://launchpad.net/intltool>`_, utility scripts for
   internationalization
-  `check <http://check.sourceforge.net/>`_, a unit testing framework
   for C
-  libmagic from `file <http://www.darwinsys.com/file/>`_, a file type
   guesser
-  `docutils <http://docutils.sourceforge.net>`_, documentation
   utilities

Stable version
--------------

Since zathura packages are available in many distributions it is
recommended to install it from there with your prefered package manager.
Otherwise you can grab the latest version of the source code from our
website and build it by hand:

::

    $ tar xfv zathura-<version>.tar.gz
    $ cd zathura-<version>
    $ make
    $ make install

Known supported distributions
-----------------------------

-  `Arch
   Linux <http://www.archlinux.org/packages/community/x86_64/zathura>`_
-  `Debian <http://packages.debian.org/en/sid/zathura>`_
-  `Fedora <http://pkgs.org/fedora-rawhide/fedora-i386/zathura-0.0.8.5.fc17.i686.rpm.html>`_
-  `Gentoo <http://packages.gentoo.org/package/app-text/zathura>`_
-  `Ubuntu <http://packages.ubuntu.com/precise/zathura>`_
-  `OpenBSD <http://openports.se/textproc/zathura>`_

Developer version
-----------------

If you are interested in testing the very latest versions with all its
new features, that we are working on, type in the following commands. At
first you have to install the latest version of girara:

::

    $ git clone git://pwmt.org/girara.git
    $ cd girara
    $ git checkout --track -b develop origin/develop
    $ make
    $ make install

After the successful installation of the user interface library, grab
the latest version of zathura and install it:

::

    $ git clone git://pwmt.org/zathura.git
    $ cd zathura
    $ git checkout --track -b develop origin/develop
    $ make
    $ make install

For the installation of a file type plugin check the
`plugins <../plugins>`_ section.
