sphinx = find_program('sphinx-build', required: false)
if sphinx.found()
  custom_target('man pages',
    command: [
      sphinx,
      '-b', 'man',
      '-D', 'version=' + version,
      '-D', 'release=' + version,
      join_paths(meson.current_source_dir(), 'man'),
      meson.current_build_dir()],
    output: ['zathura.1', 'zathurarc.5'],
    input: [
      'man/conf.py',
      'man/_bindings.txt',
      'man/_commands.txt',
      'man/_options.txt',
      'man/_synopsis.txt',
      'man/zathurarc.5.rst',
      'man/_bugs.txt',
      'man/_configuration.txt',
      'man/_description.txt',
      'man/_synctex.txt',
      'man/zathura.1.rst'
    ],
    build_by_default: true,
    install: true,
    install_dir: [
      join_paths(get_option('mandir'), 'man1'),
      join_paths(get_option('mandir'), 'man5')
    ]
  )
endif
