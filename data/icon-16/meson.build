custom_target('org.pwmt.zathura_16.png',
  input: '../org.pwmt.zathura.svg',
  output: 'org.pwmt.zathura.png',
  command: [
    rsvg_convert,
    '-w', '16', '-h', '16',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(datadir, 'icons', 'hicolor', '16x16', 'apps')
)
