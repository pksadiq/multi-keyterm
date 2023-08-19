src = []

revision_tag = vcs_tag(
  input: 'version.h.in',
  output: 'version.h',
)
pkg_dep += declare_dependency(sources: revision_tag)

resources = gnome.compile_resources(
  'resources',
  'resources/keyterm.gresource.xml',
  source_dir: 'resources',
  c_name: '_keyterm',
)

libsrc = [
  'mkt-terminal.c',
  'mkt-controller.c',
  'mkt-keyboard.c',
  'mkt-log.c',
  'mkt-utils.c',
  'mkt-settings.c',
  'mkt-preferences-window.c',
  'mkt-window.c',
]

libkeyterm = both_libraries(
  'keyterm', libsrc,
  install: false,
  include_directories: top_inc,
  dependencies: pkg_dep,
)

ui_files = files(
  'resources/gtk/help-overlay.ui',
  'resources/ui/mkt-preferences-window.ui',
  'resources/ui/mkt-window.ui',
  'resources/ui/mkt-terminal.ui',
)

gtk_builder_tool = find_program('gtk-builder-tool', required: false)
if gtk_builder_tool.found()
  # Separate multiple libraries by ‘:’ eg.: 'LD_PRELOAD=@0@:libhandy-1.so'...
  preload_env = 'LD_PRELOAD=@0@'.format(libkeyterm.get_shared_lib().full_path())
  foreach file: ui_files
    test('Validate @0@'.format(file), gtk_builder_tool,
         env: [preload_env],
         args: ['validate', file])
  endforeach
endif

src += [
  'main.c',
  'mkt-application.c',
  revision_tag,
  resources,
]

executable(
  'multi-keyterm',
  src,
  install: true,
  gui_app: true,
  link_args: c_link_args,
  link_with: libkeyterm.get_static_lib(),
  include_directories: top_inc,
  dependencies: pkg_dep,
)
