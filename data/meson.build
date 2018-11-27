gnome = import('gnome')
zathura_resources = gnome.compile_resources(
  'resources',
  'zathura.gresource.xml',
  c_name: 'zathura_resources',
  dependencies: files('zathura.css_t', 'org.pwmt.zathura.xml')
)

install_data('org.pwmt.zathura.xml', install_dir: dbusinterfacesdir)
install_data('org.pwmt.zathura.svg', install_dir: join_paths(datadir, 'icons', 'scalable', 'apps'))

rsvg_convert = find_program('rsvg-convert', required: false, native: true)
if rsvg_convert.found()
  foreach width : [16, 32, 64, 128, 256]
    subdir('icon-@0@'.format(width))
  endforeach
endif

i18n = import('i18n')
podir = join_paths(meson.source_root(), 'po')

desktop = i18n.merge_file('desktop',
  input: 'org.pwmt.zathura.desktop.in',
  output: 'org.pwmt.zathura.desktop',
  install: true,
  install_dir: desktopdir,
  po_dir: podir,
  type: 'desktop'
)

appdata = i18n.merge_file('appdata',
  input: 'org.pwmt.zathura.appdata.xml.in',
  output: 'org.pwmt.zathura.appdata.xml',
  install: true,
  install_dir: metainfodir,
  po_dir: podir,
)

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test('validate-desktop',
    desktop_file_validate,
    args: [desktop.full_path()]
  )
endif

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test('validate-appdata',
    appstream_util,
    args: ['validate-relax', appdata.full_path()]
  )
endif

conf_data = configuration_data()
conf_data.set('PLUGINDIR', join_paths(prefix, plugindir))
bash_completion = configure_file(
  input: 'bash-completion.in',
  output: 'zathura',
  configuration: conf_data
)
zsh_completion = configure_file(
  input: 'zsh-completion.in',
  output: '_zathura',
  configuration: conf_data
)

install_data(bash_completion, install_dir: join_paths(datadir, 'bash-completion', 'completions'))
install_data(zsh_completion, install_dir: join_paths(datadir, 'zsh', 'vendor-completions'))
