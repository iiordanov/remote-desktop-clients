/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
  Copyright (C) 2012 Red Hat, Inc.

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
#ifndef __SPICE_URI_PRIV_H__
#define __SPICE_URI_PRIV_H__

#include "spice-uri.h"

G_BEGIN_DECLS

SpiceURI* spice_uri_new(void);
gboolean spice_uri_parse(SpiceURI* self, const gchar* uri, GError** error);

G_END_DECLS

#endif /* __SPICE_URI_PRIV_H__ */
