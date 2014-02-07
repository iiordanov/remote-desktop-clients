/*
 * GTK VNC Widget
 *
 * Copyright (C) 2006  Anthony Liguori <anthony@codemonkey.ws>
 * Copyright (C) 2009-2010 Daniel P. Berrange <dan@berrange.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef VNC_DISPLAY_KEYMAP_H
#define VNC_DISPLAY_KEYMAP_H

#include <glib.h>

const guint16 const *vnc_display_keymap_gdk2xtkbd_table(GdkWindow *window,
                                                        size_t *maplen);
guint16 vnc_display_keymap_gdk2xtkbd(const guint16 *keycode_map,
                                     size_t keycode_maplen,
                                     guint16 keycode);
void vnc_display_keyval_set_entries(void);
void vnc_display_keyval_free_entries(void);
guint vnc_display_keyval_from_keycode(guint keycode, guint keyval);

#endif /* VNC_DISPLAY_KEYMAP_H */
