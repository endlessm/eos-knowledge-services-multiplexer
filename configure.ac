# Process this file with autoconf to produce configure

# Copyright 2018 Endless Mobile, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
# All rights reserved.

# ------------
# configure.ac
# ------------
# Please keep this file well-commented. Autotools involve a lot of magical
# incantations, and it is easy to mess things up if you don't know what you
# are doing.

# Initialization
# --------------
# Initialize Autoconf: package name, version, bug report address, tarball name,
# website
AC_INIT([Endless OS Knowledge Services Multiplexer], [0],
    [], [eos-knowledge-services-multiplexer], [http://endlessm.com])
# Initialize Automake: enable all warnings and do not insist on GNU standards
# no-portability suppresses warnings about syntax specific to GNU make
AM_INIT_AUTOMAKE([-Wall -Wno-portability foreign 1.11 tar-ustar subdir-objects])
# Avoid spewing garbage over the terminal ('make V=1' to see the garbage)
AM_SILENT_RULES([yes])
# Keep Autotools macros local to this source tree
AC_CONFIG_MACRO_DIR([m4])

AC_CACHE_SAVE

# Required build tools
# --------------------
# Make sure we can create directory hierarchies
AC_PROG_MKDIR_P
# Needed for paths substitutions
AC_PROG_SED
# C compiler
AC_PROG_CC
AC_PROG_CC_C99
# Library configuration tool
PKG_PROG_PKG_CONFIG

# Newer arches will only build EknServices 3. Figure out what to do
# based on the architecture. flatpak-builder sets FLATPAK_ARCH.
# Otherwise, use flatpak --default-arch for local testing.
AC_ARG_VAR([FLATPAK_ARCH], [Flatpak architecture])
if test -z "$FLATPAK_ARCH"; then
  FLATPAK_ARCH=$(flatpak --default-arch)
fi
case "$FLATPAK_ARCH" in
  x86_64|arm)
    COMPAT_ARCH=y
    AC_DEFINE([COMPAT_ARCH], [1], [Arch providing EknServices 1 and 2])
    ;;
  *)
    COMPAT_ARCH=n
    ;;
esac
AM_CONDITIONAL([COMPAT_ARCH], [test "$COMPAT_ARCH" = y])

AC_SUBST([FLATPAK_ARCH])

# Only need glib and gio for the multi-services binary
PKG_CHECK_MODULES([MULTI_SERVICES], [
  gio-2.0
  glib-2.0
  gobject-2.0
])

AC_CACHE_SAVE

# Output
# ------
# List files here that the configure script should output
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
  Makefile
])

# Do the output
AC_OUTPUT
