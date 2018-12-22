custom_target('org.pwmt.zathura_256.png',
  input: '../org.pwmt.zathura.svg',
  output: 'org.pwmt.zathura.png',
  command: [
    rsvg_convert,
    '-w', '256', '-h', '256',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(datadir, 'icons', 'hicolor', '256x256', 'apps')
)
