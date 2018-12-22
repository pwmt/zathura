custom_target('org.pwmt.zathura_64.png',
  input: '../org.pwmt.zathura.svg',
  output: 'org.pwmt.zathura.png',
  command: [
    rsvg_convert,
    '-w', '64', '-h', '64',
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(datadir, 'icons', 'hicolor', '64x64', 'apps')
)
