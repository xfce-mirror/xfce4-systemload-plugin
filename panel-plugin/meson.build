plugin_sources = [
  'cpu.cc',
  'cpu.h',
  'memswap.cc',
  'memswap.h',
  'network.cc',
  'network.h',
  'plugin.c',
  'plugin.h',
  'settings.cc',
  'settings.h',
  'systemload.cc',
  'uptime.cc',
  'uptime.h',
  xfce_revision_h,
]

plugin_install_subdir = 'xfce4' / 'panel' / 'plugins'

plugin_lib = shared_module(
  'systemload',
  plugin_sources,
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-systemload-plugin'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libgtop,
    libm,
    libxfce4panel,
    libxfce4ui,
    libxfce4util,
    upower_glib,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / plugin_install_subdir,
)

i18n.merge_file(
  input: 'systemload.desktop.in',
  output: 'systemload.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / plugin_install_subdir,
)
