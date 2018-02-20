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

  tests = executable('tests', test_sources,
    dependencies: build_dependencies + test_dependencies,
    include_directories: include_directories,
    c_args: defines + flags
  )

  test('tests', tests)
endif
