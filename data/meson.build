gnome = import('gnome')
zathura_resources = gnome.compile_resources(
  'resources',
  'zathura.gresource.xml',
  c_name: 'zathura_resources',
  dependencies: files('zathura.css_t', 'org.pwmt.zathura.xml')
)

install_data('zathura.appdata.xml', install_dir: metainfodir)
install_data('org.pwmt.zathura.xml', install_dir: dbusinterfacesdir)
