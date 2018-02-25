gnome = import('gnome')
zathura_resources = gnome.compile_resources(
  'resources',
  'zathura.gresource.xml',
  c_name: 'zathura_resources',
  dependencies: files('zathura.css_t', 'org.pwmt.zathura.xml')
)

install_data('org.pwmt.zathura.appdata.xml', install_dir: metainfodir)
install_data('org.pwmt.zathura.xml', install_dir: dbusinterfacesdir)

i18n = import('i18n')
desktop = i18n.merge_file('desktop',
  input: 'org.pwmt.zathura.desktop.in',
  output: 'org.pwmt.zathura.desktop',
  install: true,
  install_dir: desktopdir,
  po_dir: join_paths(meson.source_root(), 'po'),
  type: 'desktop'
)

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test('validate-desktop',
    desktop_file_validate,
    args: [desktop.full_path()]
  )
endif
