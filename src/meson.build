blutext_sources = [
  'main.c',
  'blutext-application.c',
  'blutext-window.c',
]

blutext_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
]

blueprints = custom_target('blueprints',
  input: files(
    'blutext-window.blp',
    'gtk/help-overlay.blp'
  ),
  output: '.',
  command: [find_program('blueprint-compiler'), 'batch-compile', '@OUTPUT@', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)

blutext_sources += gnome.compile_resources('blutext-resources',
  'blutext.gresource.xml',
  dependencies: blueprints,
  c_name: 'blutext'
)

executable('blutext', blutext_sources,
  dependencies: blutext_deps,
       install: true,
)
