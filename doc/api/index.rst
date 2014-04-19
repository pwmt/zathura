API and Development
===================

This guide should give a short introduction in the way zathura's plugin
system works and how you can write your own plugin and let zathura use
it.

zathura's plugin system is quite simple. At startup zathura searches
through a specified directory for shared objects and tries to load them
as plugins. Each plugin has to register itself by a name, its version, a
special function as well as its supported mimetypes to zathura. After
the registration of the plugin zathura will automatically use it to open
files with the previous defined mimetypes. That's it.

.. toctree::
   :maxdepth: 2
   :hidden:

   plugin

