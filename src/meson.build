
deps = []
deps += dependency('gio-2.0')
deps += dependency('gio-unix-2.0')
deps += dependency('glib-2.0')
deps += dependency('gtk4')
deps += dependency('libadwaita-1')
deps += dependency('yaml-0.1')

main_sources = files([
  'main.c',
  'editor_page.c',
  'dialog.c',
  'notes_page_list.c',
  'notes_tag_list.c',
  'notes_tag.c',
  'sidebar.c',
  'edit_tags.c',
  'utils.c',
])


executable('notes-editor',
  sources: main_sources,
  dependencies : deps
)

noteslib = shared_library('notes',
                        main_sources,
                        dependencies : deps,
                        install : false)

