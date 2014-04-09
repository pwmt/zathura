Plugin system
=============

zathura's plugin system is quite simple. At startup zathura searches
through a specified directory for shared objects and tries to load them
as plugins. Each plugin has to register itself by a name, its version, a
special function as well as its supported mimetypes to zathura. After
the registration of the plugin zathura will automatically use it to open
files with the previous defined mimetypes. That's it.

Each plugin has to implement a basic set of functionality so that it can
be used in a meaningful way with zathura. For instance it would not make
any sense if the plugin was not able to render any page at all. On the
contrary the export of images out of the document might not be
considered as that important.

We have predefined a certain set of functionality that a plugin can have
and that can be used by zathura if it has been implemented by the
plugin. When a plugin is loaded, zathura calls a certain function that
the plugin **must implemented** in order to work correctly. This
function gets a data structure which has to be filled with function
pointers by the plugin, which are then used by the main application.

.. toctree::

   plugin-development
