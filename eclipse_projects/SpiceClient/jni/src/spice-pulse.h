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
#ifndef __SPICE_CLIENT_PULSE_H__
#define __SPICE_CLIENT_PULSE_H__

#include "spice-client.h"
#include "spice-audio.h"

G_BEGIN_DECLS

#define SPICE_TYPE_PULSE            (spice_pulse_get_type())
#define SPICE_PULSE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SPICE_TYPE_PULSE, SpicePulse))
#define SPICE_PULSE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SPICE_TYPE_PULSE, SpicePulseClass))
#define SPICE_IS_PULSE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SPICE_TYPE_PULSE))
#define SPICE_IS_PULSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SPICE_TYPE_PULSE))
#define SPICE_PULSE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), SPICE_TYPE_PULSE, SpicePulseClass))


typedef struct _SpicePulse SpicePulse;
typedef struct _SpicePulseClass SpicePulseClass;
typedef struct _SpicePulsePrivate SpicePulsePrivate;

struct _SpicePulse {
    SpiceAudio parent;
    SpicePulsePrivate *priv;
    /* Do not add fields to this struct */
};

struct _SpicePulseClass {
    SpiceAudioClass parent_class;
    /* Do not add fields to this struct */
};

GType           spice_pulse_get_type(void);

SpicePulse *spice_pulse_new(SpiceSession *session,
                            GMainContext *context,
                            const char *name);

G_END_DECLS

#endif /* __SPICE_CLIENT_PULSE_H__ */
