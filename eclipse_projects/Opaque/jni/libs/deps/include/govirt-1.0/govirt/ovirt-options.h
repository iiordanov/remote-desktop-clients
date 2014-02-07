/*
   Copyright (C) 2010, 2013 Red Hat, Inc.

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
#ifndef OVIRT_OPTION_H
#define OVIRT_OPTION_H

#include <glib.h>

#include "ovirt-proxy.h"

G_BEGIN_DECLS

GOptionGroup* ovirt_get_option_group(void);
void ovirt_set_proxy_options(OvirtProxy *proxy);

G_END_DECLS

#endif /* OVIRT_OPTION_H */
