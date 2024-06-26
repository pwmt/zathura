if get_option('tests').allowed()
  test_dependencies = [
    libzathura_dep,
  ]

  include_directories += [ include_directories('../zathura') ]

  document = executable('test_document', files('test_document.c'),
    dependencies: build_dependencies + test_dependencies,
    include_directories: include_directories,
    c_args: defines + flags
  )
  test('document', document,
    timeout: 60*60,
    protocol: 'tap',
    env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
    ]
  )

  types = executable('test_types', files('test_types.c'),
    dependencies: build_dependencies + test_dependencies,
    include_directories: include_directories,
    c_args: defines + flags
  )
  test('types', types,
    timeout: 60*60,
    protocol: 'tap',
    env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
    ]
  )

  utils = executable('test_utils', files('test_utils.c'),
    dependencies: build_dependencies + test_dependencies,
    include_directories: include_directories,
    c_args: defines + flags
  )
  test('utils', utils,
    timeout: 60*60,
    protocol: 'tap',
    env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
    ]
  )

  xvfb = find_program('xvfb-run', required: get_option('tests'))
  if xvfb.found()
    xvfb_args = ['-s', '-screen 0 1400x900x24 -ac +extension GLX +render -noreset']
    xvfb_h_output = run_command(xvfb, '-h', capture: true, check: false)
    if xvfb_h_output.stdout().contains('--auto-display')
      # because Arch and Fedora
      xvfb_args += ['-d']
    else
      xvfb_args += ['-a']
    endif

    session = executable('test_session', files('test_session.c'),
      dependencies: build_dependencies + test_dependencies,
      include_directories: include_directories,
      c_args: defines + flags
    )
    test('session', xvfb,
      args: xvfb_args + [session],
      timeout: 60*60,
      protocol: 'tap',
      env: [
        'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
        'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
        'NO_AT_BRIDGE=1',
        'MESA_LOG=null',
        'LIBGL_DEBUG=quiet'
      ]
    )

    # need the equivalent of xvfb for wayland
    if seccomp.found() or landlock
      sandbox = executable('test_sandbox', files('test_sandbox.c'),
        dependencies: build_dependencies + [libzathura_sandbox_dep] + sandbox_dependencies,
        include_directories: include_directories,
        c_args: defines + sandbox_defines + flags
      )
      test('sandbox', xvfb,
        args: xvfb_args + [sandbox],
        timeout: 60*60,
        protocol: 'tap',
        env: [
          'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
          'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
          'NO_AT_BRIDGE=1',
          'MESA_LOG=null',
          'LIBGL_DEBUG=quiet'
        ]
      )
    endif
  endif
endif
