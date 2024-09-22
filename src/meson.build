# Dependencies
deps = []
deps += dependency('gio-2.0', version : '>=2.72.4')
deps += dependency('glib-2.0', version : '>=2.72.4')
deps += dependency('gio-unix-2.0', version : '>=2.72.4')
deps += dependency('json-glib-1.0')
deps += dependency('libsoup-3.0')
deps += dependency('gstreamer-1.0')
deps += dependency('gstreamer-webrtc-1.0')
deps += dependency('gstreamer-sdp-1.0')
deps += dependency('gstreamer-pbutils-1.0')
deps += dependency('gtk4')
deps += dependency('libadwaita-1')

deps_writer = []
deps_writer += dependency('gio-2.0', version : '>=2.72.4')
deps_writer += dependency('glib-2.0', version : '>=2.72.4')
deps_writer += dependency('gio-unix-2.0', version : '>=2.72.4')
deps_writer += dependency('json-glib-1.0')
deps_writer += dependency('libsoup-3.0')
deps_writer += dependency('gstreamer-1.0')
deps_writer += dependency('gstreamer-webrtc-1.0')
deps_writer += dependency('gstreamer-sdp-1.0')
deps_writer += dependency('gstreamer-pbutils-1.0')

# Compiler flags
extra_cflags = ['-W', '-Wformat=2', '-Winline', '-ggdb',
                '-Wstrict-prototypes', '-Wmissing-prototypes',
                '-Wdisabled-optimization', '-Wfloat-equal', '-Wall',
                '-Werror', '-Ddebug=false', '-Dstrip=true']

sources = ([
  'main.c',
  'messages.c',
  'webrtc_client.c',
  'webrtc_session.c',
  'webrtc_gui.c',
  'sidebar.c',
])

sources_filewriter = ([
  'main_filewriter.c',
  'messages.c',
  'webrtc_client.c',
  'webrtc_session.c'
])

add_project_arguments('-DNO_FLAP=true', language : 'c')

config_include = include_directories('.')

executable('webrtc-player',
           sources : sources,
           dependencies : deps,
           c_args : extra_cflags
           )

executable('webrtc-writer',
           sources : sources_filewriter,
           dependencies : deps_writer,
           c_args : extra_cflags
           )

testable_lib = shared_library('webrtc-player-lib',
                              sources,
                              dependencies : deps,
                              install : false)
