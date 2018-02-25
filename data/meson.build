gnome = import('gnome')
zathura_resources = gnome.compile_resources(
  'resources',
  'zathura.gresource.xml',
  c_name: 'zathura_resources',
  dependencies: files('zathura.css_t', 'org.pwmt.zathura.xml')
)

install_data('org.pwmt.zathura.xml', install_dir: dbusinterfacesdir)
install_data('org.pwmt.zathura.png', install_dir: join_paths(datadir, 'icons', 'hicolor', '128x128'))

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
