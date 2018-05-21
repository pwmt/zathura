check = dependency('check', required: false)
if check.found()
  test_dependencies = [
    declare_dependency(link_with: libzathura),
    check
  ]

  include_directories += [ include_directories('../zathura') ]

  test_sources = [
    'tests.c',
    'test_document.c',
    'test_session.c',
    'test_utils.c'
  ]

  document = executable('test_document', ['test_document.c', 'tests.c'],
    dependencies: build_dependencies + test_dependencies,
    include_directories: include_directories,
    c_args: defines + flags
  )
  test('document', document,
    timeout: 60*60
  )

  session = executable('test_session', ['test_session.c', 'tests.c'],
    dependencies: build_dependencies + test_dependencies,
    include_directories: include_directories,
    c_args: defines + flags
  )
  test('session', session,
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
endif
