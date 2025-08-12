[![License](https://img.shields.io/badge/License-BSD%202--Clause-blue.svg)](https://gitlab.xfce.org/panel-plugins/xfce4-systemload-plugin/-/blob/master/COPYING)
[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/panel-plugins/xfce4-systemload-plugin/-/blob/master/COPYING)

# xfce4-systemload-plugin

xfce4-systemload-plugin displays current CPU load, memory in use, network utilization, swap space, and system uptime in xfce4-panel.

----

### Homepage

[Xfce4-systemload-plugin documentation](https://docs.xfce.org/panel-plugins/xfce4-systemload-plugin/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/panel-plugins/xfce4-systemload-plugin/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Source Code Repository

[Xfce4-systemload-plugin source code](https://gitlab.xfce.org/panel-plugins/xfce4-systemload-plugin)

### Download a Release Tarball

[Xfce4-systemload-plugin archive](https://archive.xfce.org/src/panel-plugins/xfce4-systemload-plugin)
    or
[Xfce4-systemload-plugin tags](https://gitlab.xfce.org/panel-plugins/xfce4-systemload-plugin/-/tags)

### Installation

From source code repository: 

    % cd xfce4-systemload-plugin
    % meson setup build
    % meson compile -C build
    % meson install -C build

From release tarball:

    % tar xf xfce4-systemload-plugin-<version>.tar.xz
    % cd xfce4-systemload-plugin-<version>
    % meson setup build
    % meson compile -C build
    % meson install -C build

### Uninstallation

    % ninja uninstall -C build

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/panel-plugins/xfce4-systemload-plugin/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

