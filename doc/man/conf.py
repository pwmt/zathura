# -*- coding: utf-8 -*-
#
# SPDX-License-Identifier: Zlib

import os.path
import glob
import time

dirname = os.path.dirname(__file__)
files = glob.glob(os.path.join(dirname, '*.rst'))

maxdate = 0
for path in files:
    s = os.stat(path)
    maxdate = max(maxdate, s.st_mtime)

# -- General configuration ------------------------------------------------

source_suffix  = '.rst'
master_doc     = 'zathura.1'
templates_path = ['_templates']
today          = time.strftime('%Y-%m-%d', time.gmtime(maxdate))

# -- Project configuration ------------------------------------------------

project   = 'zathura'
copyright = '2009-2018, pwmt.org'
version   = '0.2.7'
release   = '0.2.7'

# -- Options for manual page output ---------------------------------------

man_pages = [
    ('zathura.1', 'zathura', 'a document viewer', ['pwmt.org'], 1),
    ('zathurarc.5', 'zathurarc', 'zathura configuration file', ['pwmt.org'], 5)
]
