project('zathura', 'c',
  version: '0.3.9',
  meson_version: '>=0.45',
  default_options: 'c_std=c11',
)

version = meson.project_version()
version_array = version.split('.')

# Rules for so_major and so_minor:
# Before a release perform the following checks against the last release:
# * If a function has been removed or the paramaters of a function have changed
#   bump SOMAJOR and set SOMINOR to 0.
# * If any of the exported datastructures have changed in a incompatible way
#   bump SOMAJOR and set SOMINOR to 0.
# * If a function has been added bump SOMINOR.
plugin_api_version = '3'
plugin_abi_version = '4'

conf_data = configuration_data()
conf_data.set('ZVMAJOR', version_array[0])
conf_data.set('ZVMINOR', version_array[1])
conf_data.set('ZVREV', version_array[2])
conf_data.set('ZVAPI', plugin_api_version)
conf_data.set('ZVABI', plugin_abi_version)
conf_data.set('version', version)

cc = meson.get_compiler('c', required: false)

prefix = get_option('prefix')
localedir = get_option('localedir')
datadir = get_option('datadir')
metainfodir = join_paths(datadir, 'metainfo')
desktopdir = join_paths(datadir, 'applications')
dbusinterfacesdir = join_paths(datadir, 'dbus-1', 'interfaces')
plugindir = join_paths(get_option('libdir'), 'zathura')

# required dependencies
libm = cc.find_library('libm', required: false)
girara = dependency('girara-gtk3', version: '>=0.2.9')
glib = dependency('glib-2.0', version: '>=2.50')
gio = dependency('gio-unix-2.0', required: host_machine.system() != 'windows')
gthread = dependency('gthread-2.0', version: '>=2.50')
gmodule = dependency('gmodule-no-export-2.0', version: '>=2.50')
gtk3 = dependency('gtk+-3.0', version: '>=3.22')
cairo = dependency('cairo')

build_dependencies = [libm, girara, glib, gio, gthread, gmodule, gtk3, cairo]

# defines
defines = [
  '-DGETTEXT_PACKAGE="zathura"',
  '-DLOCALEDIR="@0@"'.format(join_paths(prefix, localedir)),
  '-DZATHURA_PLUGINDIR="@0@"'.format(join_paths(prefix, plugindir)),
  '-D_DEFAULT_SOURCE',
]

# compile flags
flags = [
  '-Wall',
  '-Wextra',
  '-pedantic',
  '-Werror=implicit-function-declaration',
  '-Werror=vla',
  '-fvisibility=hidden'
]
flags = cc.get_supported_arguments(flags)

# optional dependencies
additional_sources = []
sqlite = dependency('sqlite3', version: '>=3.5.9', required: false)
synctex = dependency('synctex', required: false)
magic = cc.find_library('magic', required: false)
seccomp = dependency('libseccomp', required: false)

if get_option('enable-sqlite') and sqlite.found()
  build_dependencies += sqlite
  defines += '-DWITH_SQLITE'
  additional_sources += files('zathura/database-sqlite.c')
endif

if get_option('enable-synctex') and synctex.found()
  build_dependencies += synctex
  defines += '-DWITH_SYNCTEX'
endif

if get_option('enable-magic') and magic.found()
  build_dependencies += magic
  defines += '-DWITH_MAGIC'
endif

if get_option('enable-seccomp') and seccomp.found()
  build_dependencies += seccomp
  defines += '-DWITH_SECCOMP'
  additional_sources += files('zathura/seccomp-filters.c')
endif

# generate version header file
version_header = configure_file(
  input: 'zathura/version.h.in',
  output: 'zathura-version.h',
  configuration: conf_data
)
include_directories = [
  include_directories('.')
]

subdir('data')
subdir('po')

# source files
sources = files(
  'zathura/adjustment.c',
  'zathura/bookmarks.c',
  'zathura/callbacks.c',
  'zathura/checked-integer-arithmetic.c',
  'zathura/commands.c',
  'zathura/completion.c',
  'zathura/config.c',
  'zathura/content-type.c',
  'zathura/database.c',
  'zathura/database-plain.c',
  'zathura/dbus-interface.c',
  'zathura/document.c',
  'zathura/file-monitor.c',
  'zathura/file-monitor-glib.c',
  'zathura/file-monitor-signal.c',
  'zathura/jumplist.c',
  'zathura/links.c',
  'zathura/marks.c',
  'zathura/page.c',
  'zathura/page-widget.c',
  'zathura/plugin.c',
  'zathura/print.c',
  'zathura/render.c',
  'zathura/shortcuts.c',
  'zathura/synctex.c',
  'zathura/types.c',
  'zathura/utils.c',
  'zathura/zathura.c',
)
sources += zathura_resources
sources += additional_sources

# header fiels to install
headers = files(
  'zathura/document.h',
  'zathura/links.h',
  'zathura/macros.h',
  'zathura/page.h',
  'zathura/plugin-api.h',
  'zathura/types.h',
)
headers += version_header

# zathura helper library
libzathura = static_library('zathura',
  sources,
  dependencies: build_dependencies,
  include_directories: include_directories,
  c_args: defines + flags
)

# zathura executable
zathura = executable(
  'zathura',
  files('zathura/main.c'),
  dependencies: build_dependencies + [declare_dependency(link_with: libzathura)],
  install: true,
  include_directories: include_directories,
  c_args: defines + flags,
  export_dynamic: true,
  gui_app: true
)
install_headers(headers, subdir: 'zathura')

# pkg-config file
pkg = import('pkgconfig')
pkg.generate(
  name: 'zathura',
  description: 'document viewer - plugin API',
  url: 'https://pwmt.org/projects/zathura',
  version: version,
  requires_private: ['girara-gtk3', 'cairo'],
  variables: [
    'apiversion=@0@'.format(plugin_api_version),
    'abiversion=@0@'.format(plugin_abi_version),
    'plugindir=${libdir}/zathura'
  ]
)

subdir('doc')
subdir('tests')
