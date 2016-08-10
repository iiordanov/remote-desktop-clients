/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
  Copyright (C) 2014 Red Hat, Inc.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __SPICE_VERSION_H__
#define __SPICE_VERSION_H__

/**
 * SECTION:spice-version
 * @short_description: Spice-Gtk version checking
 *
 * Spice-Gtk provides macros to check the version of the library
 * at compile-time
 */

/**
 * SPICE_GTK_MAJOR_VERSION:
 *
 * Spice-Gtk major version component (e.g. 1 if version is 1.2.3)
 * Since: 0.24
 */
#define SPICE_GTK_MAJOR_VERSION              (0)

/**
 * SPICE_GTK_MINOR_VERSION:
 *
 * Spice-Gtk minor version component (e.g. 2 if version is 1.2.3)
 * Since: 0.24
 */
#define SPICE_GTK_MINOR_VERSION              (29)

/**
 * SPICE_GTK_MICRO_VERSION:
 *
 * Spice-Gtk micro version component (e.g. 3 if version is 1.2.3)
 * Since: 0.24
 */
#define SPICE_GTK_MICRO_VERSION              (0)

/**
 * SPICE_GTK_CHECK_VERSION:
 * @major: required major version
 * @minor: required minor version
 * @micro: required micro version
 *
 * Compile-time version checking. Evaluates to %TRUE if the version
 * of Spice-Gtk is greater than the required one.
 * Since: 0.24
 */
#define SPICE_GTK_CHECK_VERSION(major, minor, micro)                    \
        (SPICE_GTK_MAJOR_VERSION > (major) ||                           \
         (SPICE_GTK_MAJOR_VERSION == (major) && SPICE_GTK_MINOR_VERSION > (minor)) || \
         (SPICE_GTK_MAJOR_VERSION == (major) && SPICE_GTK_MINOR_VERSION == (minor) && \
          SPICE_GTK_MICRO_VERSION >= (micro)))


#endif /* __SPICE_VERSION_H__ */
