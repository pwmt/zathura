custom_target('org.pwmt.zathura_128.png',
  input: '../org.pwmt.zathura.svg',
  output: 'org.pwmt.zathura.png',
  command: [
    rsvg_convert,
    '-w', '128', '-h', '128',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(datadir, 'icons', 'hicolor', '128x128', 'apps')
)
