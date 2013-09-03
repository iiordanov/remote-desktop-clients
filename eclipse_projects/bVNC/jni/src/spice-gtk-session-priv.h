/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010-2011 Red Hat, Inc.

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
#ifndef __SPICE_CLIENT_GTK_SESSION_PRIV_H__
#define __SPICE_CLIENT_GTK_SESSION_PRIV_H__

#include "spice-gtk-session.h"

G_BEGIN_DECLS

void spice_gtk_session_request_auto_usbredir(SpiceGtkSession *self,
                                             gboolean state);
gboolean spice_gtk_session_get_read_only(SpiceGtkSession *self);

G_END_DECLS

#endif /* __SPICE_CLIENT_GTK_SESSION_PRIV_H__ */
