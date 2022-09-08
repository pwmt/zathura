gnome = import('gnome')
zathura_resources = gnome.compile_resources(
  'resources',
  'zathura.gresource.xml',
  c_name: 'zathura_resources',
  dependencies: files('zathura.css_t', 'org.pwmt.zathura.xml')
)

install_data('org.pwmt.zathura.xml', install_dir: dbusinterfacesdir)
install_data('org.pwmt.zathura.svg', install_dir: join_paths(datadir, 'icons', 'hicolor', 'scalable', 'apps'))

rsvg_convert = find_program('rsvg-convert', required: get_option('convert-icon'), native: true)
if rsvg_convert.found()
  foreach width : [16, 32, 64, 128, 256]
    subdir('icon-@0@'.format(width))
  endforeach
endif

i18n = import('i18n')
podir = join_paths(meson.project_source_root(), 'po')

desktop = i18n.merge_file(
  input: 'org.pwmt.zathura.desktop.in',
  output: 'org.pwmt.zathura.desktop',
  install: true,
  install_dir: desktopdir,
  po_dir: podir,
  type: 'desktop'
)

appdata = i18n.merge_file(
  input: 'org.pwmt.zathura.appdata.xml.in',
  output: 'org.pwmt.zathura.appdata.xml',
  install: true,
  install_dir: metainfodir,
  po_dir: podir,
)

desktop_file_validate = find_program('desktop-file-validate', required: get_option('tests'), native: true)
if desktop_file_validate.found()
  test('validate-desktop',
    desktop_file_validate,
    args: [desktop.full_path()]
  )
endif

appstream_util = find_program('appstream-util', required: get_option('tests'), native: true)
if appstream_util.found()
  test('validate-appdata',
    appstream_util,
    args: ['validate-relax', '--nonet', appdata.full_path()]
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
fish_completion = configure_file(
  input: 'fish-completion.in',
  output: 'zathura.fish',
  configuration: conf_data
)

bash_comp = dependency('bash-completion', required: false)
if bash_comp.found()
  bash_compdir = bash_comp.get_variable(pkgconfig: 'completionsdir')
else
  bash_compdir = join_paths(datadir, 'bash-completion', 'completions')
endif

fish_comp = dependency('fish', required: false)
if fish_comp.found()
  fish_compdir = fish_comp.get_variable(pkgconfig: 'completionsdir')
else
  fish_compdir = join_paths(datadir, 'fish', 'vendor_completions.d')
endif

install_data(bash_completion, install_dir: bash_compdir)
install_data(zsh_completion, install_dir: join_paths(datadir, 'zsh', 'site-functions'))
install_data(fish_completion, install_dir: fish_compdir)
