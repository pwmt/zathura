Configuration options
=====================

General settings
----------------

.. describe:: abort-clear-search

  Defines if the search results should be cleared on abort.

   :type: Boolean
   :default: True

.. describe:: adjust-open

  Defines which auto adjustment mode should be used if a document is
  loaded. Possible options are "best-fit" and "width".

   :type: String
   :default: best-fit

.. describe:: advance-ds-per-row

  Defines if the number of pages per row should be honored when advancing
  a page.

   :type: Boolean
   :default: true

.. describe:: database

  Defines the used database backend. Possible options are 'plain' and
  'sqlite'

   :type: String
   :default: plain

.. describe:: highlight-color

  Defines the color that is used for highlighting parts of the document
  (e.g.: show search results)

   :type: String
   :default: #9FBC00

.. describe:: highlight-fg

  Defines the color that is for text when highlighting parts of the document
  (e.g.: numbers for links)

   :type: String
   :default: #FFFFFF

.. describe:: highlight-active-color

  Defines the color that is used to show the current selected highlighted
  element (e.g: current search result)

   :type: String
   :default: #00BC00

.. describe:: highlight-transparency

  Defines the opacity of a highlighted element

   :type: Float
   :default: 0.5

.. describe:: page-padding

  The page padding defines the gap in pixels between each rendered page.

   :type: Integer
   :default: 1

.. describe:: page-store-threshold

  Pages that are not visible get unloaded after some time. Every page that
  has not been visible for page-store-treshold seconds will be unloaded.

   :type: Integer
   :default: 30

.. describe:: page-store-interval

  Defines the amount of seconds between the check to unload invisible
  pages.

   :type: Integer
   :default: 30

.. describe:: pages-per-row

  Defines the number of pages that are rendered next to each other in a
  row.

   :type: Integer
   :default: 1

.. describe:: recolor

  En/Disables recoloring

   :type: Boolean
   :default: false

.. describe:: recolor-darkcolor

  Defines the color value that is used to represent dark colors in
  recoloring mode

   :type: String
   :default: #FFFFFF

.. describe:: recolor-lightcolor

  Defines the color value that is used to represent light colors in
  recoloring mode

   :type: String
   :default: #000000

.. describe:: render-loading

  Defines if the "Loading..." text should be displayed if a page is
  rendered.

   :type: Boolean
   :default: true

.. describe:: scroll-step

  Defines the step size of scrolling by calling the scroll command once

   :type: Float
   :default: 40

.. describe:: signature-error-color

   Defines the background color when displaying additional information for signatures with errors.

   :type: String
   :default: rgba(92%,11%,14%,0.9)

.. describe:: signature-success-color

   Defines the background color when displaying additional information for valid signatures.

   :type: String
   :default: rgba(18%,80%,33%,0.9)

.. describe:: signature-warning-color

   Defines the background color when displaying additional information for signatures with warnings.

   :type: String
   :default: rgba(100%,84%,0%,0.9)

.. describe:: scroll-wrap

  Defines if the last/first page should be wrapped

   :type: Boolean
   :default: false

.. describe:: smooth-reload

   Defines if flickering will be removed when a file is reloaded on change.
   This option might increase memory usage.

   :type: Boolean
   :default: true

.. describe:: show-signature-information

   Defines whether additional information on signatures embedded in documents should be displayed.

   :type: Boolean
   :default: false

.. describe:: zoom-max

  Defines the maximum percentage that the zoom level can be

   :type: Integer
   :default: 1000

.. describe:: zoom-min

  Defines the minimum percentage that the zoom level can be

   :type: Integer
   :default: 10

.. describe:: zoom-step

  Defines the amount of percent that is zoomed in or out on each comand.

   :type: Integer
   :default: 10

Girara settings
---------------

Most of the options affecting the appearance of zathura are derived from
the options that are offered by our user interface library called girara
and can be found in its `documentation </projects/girara/options>`_.
Those values can also be set via the *zathurarc* file.
