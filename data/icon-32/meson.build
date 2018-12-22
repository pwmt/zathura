custom_target('org.pwmt.zathura_32.png',
  input: '../org.pwmt.zathura.svg',
  output: 'org.pwmt.zathura.png',
  command: [
    rsvg_convert,
    '-w', '32', '-h', '32',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(datadir, 'icons', 'hicolor', '32x32', 'apps')
)
