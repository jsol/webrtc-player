tests = [
  { 'name': 'parse-messages'},
  { 'name': 'create-messages'},
]

foreach test: tests
  test_name = '@0@-test'.format(test['name'])
  testexe = executable(test_name,  test_name + '.c',
                        include_directories : '../src',
                       dependencies : deps,
                       link_with : testable_lib)

  test(test_name, testexe,
                     env: [
                       'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
                       'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
                     ],
                     protocol: 'tap',)

endforeach
