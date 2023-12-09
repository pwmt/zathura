project('zathura', 'c',
  version: '0.5.4',
  meson_version: '>=0.61',
  default_options: ['c_std=c17', 'warning_level=3'],
)

version = meson.project_version()
version_array = version.split('.')

# Rules for plugin API and ABI (non-exhaustive):
# * zathura_plugin_function_t: If functions are addedd or removed or their
#   signature changes, bump both ABI and API.
# * zathura_plugin_definition_t: If the struct changes in an ABI-incompatible
#   way, bump the ABI.
plugin_api_version = '5'
plugin_abi_version = '6'

conf_data = configuration_data()
conf_data.set('ZVMAJOR', version_array[0])
conf_data.set('ZVMINOR', version_array[1])
conf_data.set('ZVREV', version_array[2])
conf_data.set('ZVAPI', plugin_api_version)
conf_data.set('ZVABI', plugin_abi_version)
conf_data.set('version', version)

cc = meson.get_compiler('c')

prefix = get_option('prefix')
localedir = get_option('localedir')
datadir = get_option('datadir')
metainfodir = join_paths(datadir, 'metainfo')
desktopdir = join_paths(datadir, 'applications')
dbusinterfacesdir = join_paths(datadir, 'dbus-1', 'interfaces')
plugindir = join_paths(get_option('libdir'), 'zathura')

# required dependencies
libm = cc.find_library('m', required: false)
girara = dependency('girara-gtk3', version: '>=0.4.1', fallback: ['girara', 'girara_dependency'])
glib = dependency('glib-2.0', version: '>=2.50')
gio = dependency('gio-unix-2.0', required: host_machine.system() != 'windows')
gthread = dependency('gthread-2.0', version: '>=2.50')
gmodule = dependency('gmodule-no-export-2.0', version: '>=2.50')
gtk3 = dependency('gtk+-3.0', version: '>=3.22')
json_glib = dependency('json-glib-1.0')
cairo = dependency('cairo')
magic = cc.find_library('magic', required: true)

build_dependencies = [libm, girara, glib, gio, gthread, gmodule, gtk3, cairo, magic, json_glib]

if host_machine.system() == 'darwin'
    gtk_mac_integration = dependency('gtk-mac-integration-gtk3')
    build_dependencies += gtk_mac_integration
endif


# defines
defines = [
  '-DGETTEXT_PACKAGE="zathura"',
  '-DLOCALEDIR="@0@"'.format(join_paths(prefix, localedir)),
  '-DZATHURA_PLUGINDIR="@0@"'.format(join_paths(prefix, plugindir)),
  '-D_DEFAULT_SOURCE',
]

if host_machine.system() == 'darwin'
  defines += '-DGTKOSXAPPLICATION'
endif

# compile flags
flags = [
  '-Werror=implicit-function-declaration',
  '-Werror=vla',
  '-fvisibility=hidden'
]
flags = cc.get_supported_arguments(flags)

# optional dependencies
additional_sources = []
sqlite = dependency('sqlite3', version: '>=3.6.23', required: get_option('sqlite'))
synctex = dependency('synctex', version: '>=1.19', required: get_option('synctex'))
seccomp = dependency('libseccomp', required: get_option('seccomp'))

if sqlite.found()
  build_dependencies += sqlite
  defines += '-DWITH_SQLITE'
  additional_sources += files('zathura/database-sqlite.c')
endif

if synctex.found()
  build_dependencies += synctex
  defines += '-DWITH_SYNCTEX'
  if synctex.version() < '2.0.0' and synctex.version() >= '1.19.0'
    warning('You are using a synctex version pre-SONAME bump, but post-ABI-break. Please make sure to always run zathura using the correct synctex version.')
  endif
endif

if seccomp.found()
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
  'zathura/file-monitor-noop.c',
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

# header files to install
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
  win_subsystem: 'windows'
)
install_headers(headers, subdir: 'zathura')

# pkg-config file
pkg = import('pkgconfig')
pkg.generate(
  name: 'zathura',
  description: 'document viewer - plugin API',
  url: 'https://pwmt.org/projects/zathura',
  version: version,
  requires: ['girara-gtk3', 'cairo'],
  variables: [
    'apiversion=@0@'.format(plugin_api_version),
    'abiversion=@0@'.format(plugin_abi_version),
    'plugindir=${libdir}/zathura'
  ]
)

zathura_dependency = declare_dependency(link_with: zathura, include_directories: include_directories)

subdir('doc')
subdir('tests')
