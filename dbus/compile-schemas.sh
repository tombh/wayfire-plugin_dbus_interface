#!/usr/bin/env bash

# Only compile schemas if DESTDIR isn't set
[ ! -z "$DESTDIR" ] && exit 0

exec glib-compile-schemas "$MESON_INSTALL_DESTDIR_PREFIX/$1"
