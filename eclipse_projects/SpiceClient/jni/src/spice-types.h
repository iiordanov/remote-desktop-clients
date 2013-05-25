/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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
#ifndef __SPICE_CLIENT_TYPES_H__
#define __SPICE_CLIENT_TYPES_H__

G_BEGIN_DECLS

/* SpiceSession */
typedef struct _SpiceSession SpiceSession;
typedef struct _SpiceSessionClass SpiceSessionClass;
typedef struct _SpiceSessionPrivate SpiceSessionPrivate;

/* SpiceChannel */
typedef struct _SpiceChannel SpiceChannel;
typedef struct _SpiceChannelClass SpiceChannelClass;
typedef struct _SpiceChannelPrivate SpiceChannelPrivate;

G_END_DECLS

#endif /* __SPICE_CLIENT_TYPES_H__ */
