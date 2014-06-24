Example - A minimalistic PDF plugin
===================================

In this section we are going to develop a simplified version of the
`zathura-pdf-poppler <../zathura-pdf-poppler>`_ plugin. For the sake of
simplicity we are not discussing the build process of the plugin because
we would recommend you to adapt our Makefiles from existing plugins. In
addition we avoid most of the error handling that should be implemented.

Prerequisites
~~~~~~~~~~~~~

In order to use the following described functions and macros you have to
include the *plugin-api.h* header file:

::

    #include <zathura/plugin-api.h>

This automatically loads other header files for the
*zathura\_document\_t*, *zathura\_page\_t* as well as all the other
types that are necessary automatically.

Register the plugin
~~~~~~~~~~~~~~~~~~~

As previously described each plugin has to register itself to zathura so
that it can be used properly. Therefore we have introduced a macro
called *ZATHURA\_PLUGIN\_REGISTER* which expects several parameters:

-  Plugin name *The name of the plugin*
-  Major version *The plugins major version*
-  Minor version *The plugins minor version*
-  Revision *The plugins revision*
-  Open function *The open function*
-  Mimetypes *A character array of supported mime types*

In our case we are going to register our plugin "my plugin" with its
version 1.0.1, the register function *register\_functions* and the list
of supported mimetypes.

::

    ZATHURA_PLUGIN_REGISTER(
      "plugin-tutorial",
      0, 1, 0,
      register_functions,
      ZATHURA_PLUGIN_MIMETYPES({
        "application/pdf"
      })
    )

This macro will automatically generate among others a function called
*plugin\_register* which is used to register the plugin to zathura when
it has been loaded.

Register the plugin functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In our macro we have defined that the function *register\_functions* is
used to install our functions which will implement a certain
functionality in the struct:

::

    void
    register_functions(zathura_plugin_functions_t* functions)
    {
      functions->document_open     = plugin_document_open;
      functions->document_free     = plugin_document_free;
      functions->page_init         = plugin_page_init;
      functions->page_clear        = plugin_page_clear;
      functions->page_render_cairo = plugin_page_render_cairo;
    }

We are now going to give a short overview about the used functions in
the above code snippet. For a complete documentation you should checkout
the documentation of `zathura\_document\_functions\_t <../../doxygen>`_.
A document instance consists out of a *zathura\_document\_t* document
object that contains information about the document itself and a defined
number of *zathura\_page\_t* page objects. There are several functions
defined for those two types and they have to be implemented by the
plugin. For our simple plugin which will only be capable of rendering a
page we will need one function that is capable of opening the PDF
document and setting up all necessary objects for further usage and one
function which will clean up all the allocated objects afterwards. In
addition we need two of those functions for page objects as well and one
function that will actually implement the rendering process.

Open and closing a document
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first thing we have to do when opening a document is to initialize
all necessary objects and values that we are going to need for the
future use of the plugin. Therefore we have to implement our
*pdf\_document\_open* function:

::

    zathura_error_t
    plugin_document_open(zathura_document_t* document)
    {
      /* get path and password */
      const char* path     = zathura_document_get_path(document);
      const char* password = zathura_document_get_password(document);

      /* create document data */
      char* uri = g_filename_to_uri(path, NULL, NULL);
      PopplerDocument* poppler_document = poppler_document_new_from_file(uri, password, NULL);
      g_free(uri);

      if (poppler_document == NULL) {
        return ZATHURA_ERROR_UNKNOWN;
      }

      /* save poppler document for further usage */
      zathura_document_set_data(document, poppler_document);

      /* get number of pages */
      unsigned int number_of_pages = poppler_document_get_n_pages(poppler_document);
      zathura_document_set_number_of_pages(document, number_of_pages);

      return ZATHURA_ERROR_OK;
    }

To open the document we retrieve the *path* and the optional *password*
of the document to create an instance of *PopplerDocument* which
represents a document in the poppler library. If this fails for any
reason (e.g.: the path does not exist, the user provided the incorrect
password) we tell zathura that this function failed for an unknown
reason. If we are lucky we continue and save the created
*poppler\_document* object in the custom data field of the document so
that we can access it later on. After that we determine the number of
pages that the document contains so that zathura can initialize every
single page.

Since we have allocated the *poppler\_document* object we have to make
sure that its resources will be freed when it is no longer needed. This
happens in our *pdf\_document\_free* function:

::

    zathura_error_t
    plugin_document_free(zathura_document_t* document, PopplerDocument* poppler_document)
    {
      g_object_unref(poppler_document);

      return ZATHURA_ERROR_OK;
    }

Page initialization
~~~~~~~~~~~~~~~~~~~

Each page has to be initialized so that zathura knows about its
dimension. In addition this stage is used to store additional data in
the page that will be used for further use with it. Therefore we are
implementing *pdf\_page\_init* which will save the width and the height
of the page in the given structure:

::

    zathura_error_t
    plugin_page_init(zathura_page_t* page)
    {
      unsigned int page_index           = zathura_page_get_index(page);
      zathura_document_t* document      = zathura_page_get_document(page);
      PopplerDocument* poppler_document = zathura_document_get_data(document);

      /* create poppler page */
      PopplerPage* poppler_page = poppler_document_get_page(poppler_document, page_index);
      zathura_page_set_data(page, poppler_page);

      /* get page dimensions */
      double width, height;
      poppler_page_get_size(poppler_page, &width, &height);

      zathura_page_set_width(page,  width);
      zathura_page_set_height(page, height);

      return ZATHURA_ERROR_OK;
    }

And we have to make sure that all requested resources are freed in the
end:

::

    zathura_error_t
    plugin_page_clear(zathura_page_t* page, PopplerPage* poppler_page)
    {
      g_object_unref(poppler_page);

      return ZATHURA_ERROR_OK;
    }

Render a page
~~~~~~~~~~~~~

After we have setup the document and the page objects we are ready to
implement the render function which finally will be able to draw our
page on a widget so that it can be viewed with zathura. This function
has two additional parameters to the already known *zathura\_page\_t*
object: One of them is a *cairo\_t* object which will be used to render
the page, the other one is a flag called *printing* which determines if
the rendered page should be rendered for the print process of zathura.
For instance if this flag is set to true you should not render any
rectangles around links in the document because they are totally
worthless on paper:

::

    zathura_error_t
    pdf_page_render_cairo(zathura_page_t* page, cairo_t* cairo, bool printing)
    {
      if (printing == false) {
        poppler_page_render(poppler_page, cairo);
      } else {
        poppler_page_render_for_printing(poppler_page, cairo);
      }

      return ZATHURA_ERROR_OK;
    }

In this case the *pdf\_page\_render\_cairo* function is very simplistic
since all the work is done by the *poppler* library. In your case you
might have to do some magic here to draw the page to the cairo object.
Make sure to check out the source code of our plugins.

Installation of the plugin
~~~~~~~~~~~~~~~~~~~~~~~~~~

As we suggested earlier the easiest way to build and install the plugin
is to duplicate the *Makefile* (as long with its *common.mk* and
*config.mk* files of one of our plugins. It already contains all
necessary targets for building, installing and debugging the plugin.

Otherwise you could build the above plugin with the following command:

::

    $ gcc -std=c99 -shared -fPIC -pedantic -Wall `pkg-config --cflags --libs poppler-glib zathura` \
      -o pdf.so pdf.c

After that you have to copy the *pdf.so* file into the directory where
zathura looks for plugins (this is by default: */usr/lib/zathura*).
