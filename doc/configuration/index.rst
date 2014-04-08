Configuration
=============

.. toctree::
   :maxdepth: 1

   set
   map
   options

The customization of zathura is be managed via a configuration file
called *zathurarc*. By default zathura will evaluate the following
files:

-  */etc/zathurarc*
-  *$XDG\_CONFIG\_HOME/zathura/zathurarc* (default:
   ~/.config/zathura/zathurarc)

The *zathurarc* file is a simple plain text file that can be populated
with various commands to change the behaviour and the look of zathura
which we are going to describe in the following subsections. Each line
(besides empty lines and comments (which start with a prepended *#*) is
evaluated on its own, so it is not possible to write multiple commands
in one single line.

