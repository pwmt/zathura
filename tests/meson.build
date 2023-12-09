check = dependency('check', required: get_option('tests'), version: '>=0.11')
if check.found()
  test_dependencies = [
    declare_dependency(link_with: libzathura),
    check
  ]

  include_directories += [ include_directories('../zathura') ]

  document = executable('test_document', ['test_document.c', 'tests.c'],
    dependencies: build_dependencies + test_dependencies,
    include_directories: include_directories,
    c_args: defines + flags
  )
  test('document', document,
    timeout: 60*60
  )

  types = executable('test_types', ['test_types.c', 'tests.c'],
    dependencies: build_dependencies + test_dependencies,
    include_directories: include_directories,
    c_args: defines + flags
  )
  test('types', types,
    timeout: 60*60
  )

  utils = executable('test_utils', ['test_utils.c', 'tests.c'],
    dependencies: build_dependencies + test_dependencies,
    include_directories: include_directories,
    c_args: defines + flags
  )
  test('utils', utils,
    timeout: 60*60
  )

  xvfb = find_program('xvfb-run', required: get_option('tests'))
  if xvfb.found()
    session = executable('test_session', ['test_session.c', 'tests.c'],
      dependencies: build_dependencies + test_dependencies,
      include_directories: include_directories,
      c_args: defines + flags
    )
    test('session', xvfb,
      args: ['-a', '-s', '-screen 0 1400x900x24 -ac +extension GLX +render -noreset', session],
      timeout: 60*60
    )

    if seccomp.found()
      sandbox = executable('test_sandbox', ['test_sandbox.c', 'tests.c'],
        dependencies: build_dependencies + test_dependencies,
        include_directories: include_directories,
        c_args: defines + flags
      )
      test('sandbox', xvfb,
        args: ['-a', '-s', '-screen 0 1400x900x24 -ac +extension GLX +render -noreset', sandbox],
        timeout: 60*60
      )
    endif
  endif
endif
