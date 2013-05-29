/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2012 Red Hat, Inc.

   Red Hat Authors:
   Hans de Goede <hdegoede@redhat.com>

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
#ifndef __SPICE_DESKTOP_INTEGRATION_H__
#define __SPICE_DESKTOP_INTEGRATION_H__

#include "spice-client.h"

G_BEGIN_DECLS

#define SPICE_TYPE_DESKTOP_INTEGRATION            (spice_desktop_integration_get_type ())
#define SPICE_DESKTOP_INTEGRATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPICE_TYPE_DESKTOP_INTEGRATION, SpiceDesktopIntegration))
#define SPICE_DESKTOP_INTEGRATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SPICE_TYPE_DESKTOP_INTEGRATION, SpiceDesktopIntegrationClass))
#define SPICE_IS_DESKTOP_INTEGRATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPICE_TYPE_DESKTOP_INTEGRATION))
#define SPICE_IS_DESKTOP_INTEGRATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SPICE_TYPE_DESKTOP_INTEGRATION))
#define SPICE_DESKTOP_INTEGRATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SPICE_TYPE_DESKTOP_INTEGRATION, SpiceDesktopIntegrationClass))

typedef struct _SpiceDesktopIntegration SpiceDesktopIntegration;
typedef struct _SpiceDesktopIntegrationClass SpiceDesktopIntegrationClass;
typedef struct _SpiceDesktopIntegrationPrivate SpiceDesktopIntegrationPrivate;

/*
 * SpiceDesktopIntegration offers helper-functions to do desktop environment
 * and/or platform specific tasks like disabling automount, disabling the
 * screen-saver, etc. SpiceDesktopIntegration is for internal spice-gtk usage
 * only!
 */
struct _SpiceDesktopIntegration
{
    GObject parent;

    SpiceDesktopIntegrationPrivate *priv;
};

struct _SpiceDesktopIntegrationClass
{
    GObjectClass parent_class;
};

GType spice_desktop_integration_get_type(void);
SpiceDesktopIntegration *spice_desktop_integration_get(SpiceSession *session);
void spice_desktop_integration_inhibit_automount(SpiceDesktopIntegration *self);
void spice_desktop_integration_uninhibit_automount(SpiceDesktopIntegration *self);

G_END_DECLS

#endif /* __SPICE_DESKTOP_INTEGRATION_H__ */
