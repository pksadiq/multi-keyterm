project(
  'multi-keyterm', 'c',
  version: '0.1.0',
  meson_version: '>= 0.50.0',
  license: 'GPL3+',
  default_options: [
    'c_std=gnu11',
    'warning_level=1',
    'buildtype=debugoptimized'
  ])

pkg_name      = meson.project_name()
pkg_id        = 'org.sadiqpk.multi-keyterm'
pkg_version   = meson.project_version()
pkg_is_debug  = get_option('buildtype').startswith('debug')

pkg_prefix    = get_option('prefix')
pkg_srcdir    = meson.source_root()
pkg_builddir  = meson.build_root()
pkg_bindir    = join_paths(pkg_prefix, get_option('bindir'))
pkg_libdir    = join_paths(pkg_prefix, get_option('libdir'))
pkg_localedir = join_paths(pkg_prefix, get_option('localedir'))
pkg_datadir   = join_paths(pkg_prefix, get_option('datadir'))
pkg_schemadir = join_paths(pkg_datadir, 'glib-2.0', 'schemas')
pkg_po_dir    = join_paths(pkg_srcdir, 'po')
top_inc       = include_directories('.')
src_inc       = include_directories('src')

gnome = import('gnome')
i18n  = import('i18n')
cc    = meson.get_compiler('c')


conf = configuration_data()
conf.set('NDEBUG', (not pkg_is_debug))
conf.set('G_DISABLE_ASSERT', (not pkg_is_debug))
conf.set('G_DISABLE_CAST_CHECKS', (not pkg_is_debug))
conf.set_quoted('GETTEXT_PACKAGE', pkg_name)
conf.set_quoted('PACKAGE_NAME', pkg_name)
conf.set_quoted('PACKAGE_VERSION', pkg_version)
conf.set_quoted('PACKAGE_ID', pkg_id)
conf.set_quoted('PACKAGE_LOCALE_DIR', pkg_localedir)

configure_file(
  output: 'config.h',
  configuration: conf,
)

run_conf = configuration_data()
run_conf.set('BUILD_DIR', meson.current_build_dir())
configure_file(
  input: 'run.in',
  output: 'run',
  configuration: run_conf,
)

common_flags = [
  '-DHAVE_CONFIG_H',
  '-D_FORTIFY_SOURCE=2',
  '-DG_LOG_USE_STRUCTURED',
]

add_global_arguments(common_flags, language: 'c')
c_link_args = cc.get_supported_arguments(
  ['-fasynchronous-unwind-tables',
   '-fno-strict-aliasing',
   '-Wall',
   '-Wcast-align',
   '-Wdate-time',
   '-Werror=format-security',
   '-Werror=implicit-function-declaration',
   '-Werror=pointer-to-int-cast',
   '-Werror=redundant-decls',
   '-Werror=return-type',
   '-Werror=vla',
   '-Werror=write-strings',
   '-Wfloat-equal',
   '-Wformat=2',
   '-Wimplicit-fallthrough',
   '-Wlogical-op',
   '-Wmissing-format-attribute',
   '-Wmissing-include-dirs',
   '-Wmissing-prototypes',
   '-Wnested-externs',
   '-Wold-style-declaration',
   '-Wold-style-definition',
   '-Wpointer-arith',
   '-Wshadow',
   '-Wswitch-default',
   '-Wswitch-enum',
   '-Wtrigraphs',
   '-Wunused',
   '-pipe',
  ])

if cc.get_id() == 'msvc'
  warning('Your compiler doesn\'t support C99/C11 features.' +
          ' Please use MinGW or native GNU/Linux')
endif

pkg_dep = [
  dependency('glib-2.0', version: '>= 2.44.0'),
  dependency('gtk4', version: '>= 4.10'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('vte-2.91-gtk4', version: '>= 0.70.0'),
  dependency('libudev'),
  dependency('libinput'),
  dependency('xkbcommon'),
]

subdir('src')
subdir('data')
subdir('docs')

if get_option('tests')
  subdir('tests')
endif

if get_option('bash_completion')
  subdir('completion')
endif

meson.add_install_script('build-aux/meson_post_install.py')

system = host_machine.system()
if system == 'linux'
  system = 'GNU/Linux'
endif

output = '\n        ' + pkg_name + ' ' + pkg_version + '\n'
output += '        ==============================\n\n'
output += '        prefix:          ' + pkg_prefix + '\n'
output += '        compiler:        ' + cc.get_id() + ' ' + cc.version()+ '\n'
output += '        compiler args:   ' + ' '.join(c_link_args) + '\n'
output += '        build type:      ' + get_option('buildtype') + '\n'
output += '        host system:     ' + system + '\n'
output += '        tests:           ' + get_option('tests').to_string() + '\n'
output += '        manpage:         ' + get_option('man').to_string() + '\n'
output += '        bash-completion: ' + get_option('bash_completion').to_string() + '\n'
message(output)
