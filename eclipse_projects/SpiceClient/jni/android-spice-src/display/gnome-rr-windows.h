/* gnome-rr-windows.h
 *
 * Copyright 2011, Red Hat, Inc.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Soren Sandmann <sandmann@redhat.com>
 *          Marc-André Lureau <marcandre.lureau@redhat.com>
 */

#ifndef GNOME_RR_WINDOWS_H
#define GNOME_RR_WINDOWS_H

#include <glib.h>
#include "gnome-rr.h"

#define GNOME_TYPE_RR_WINDOWS_SCREEN                  (gnome_rr_windows_screen_get_type())
#define GNOME_RR_WINDOWS_SCREEN(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_RR_WINDOWS_SCREEN, GnomeRRWindowsScreen))
#define GNOME_IS_RR_WINDOWS_SCREEN(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_TYPE_RR_WINDOWS_SCREEN))
#define GNOME_RR_WINDOWS_SCREEN_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_TYPE_RR_WINDOWS_SCREEN, GnomeRRWindowsScreenClass))
#define GNOME_IS_RR_WINDOWS_SCREEN_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_RR_WINDOWS_SCREEN))
#define GNOME_RR_WINDOWS_SCREEN_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GNOME_TYPE_RR_WINDOWS_SCREEN, GnomeRRWindowsScreenClass))

typedef struct GnomeRRWindowsScreenPrivate GnomeRRWindowsScreenPrivate;

typedef struct {
	GnomeRRScreen parent;

	GnomeRRWindowsScreenPrivate* priv;
} GnomeRRWindowsScreen;

typedef struct {
	GnomeRRScreenClass parent_class;

        void (* changed) (void);
} GnomeRRWindowsScreenClass;

GType gnome_rr_windows_screen_get_type (void);

#endif  /* GNOME_RR_WINDOWS_H */
