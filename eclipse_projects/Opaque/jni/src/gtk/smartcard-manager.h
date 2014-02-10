/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011 Red Hat, Inc.

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
#ifndef __SPICE_SMARTCARD_MANAGER_H__
#define __SPICE_SMARTCARD_MANAGER_H__

G_BEGIN_DECLS

#include "spice-types.h"
#include "spice-util.h"

#define SPICE_TYPE_SMARTCARD_MANAGER            (spice_smartcard_manager_get_type ())
#define SPICE_SMARTCARD_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPICE_TYPE_SMARTCARD_MANAGER, SpiceSmartcardManager))
#define SPICE_SMARTCARD_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SPICE_TYPE_SMARTCARD_MANAGER, SpiceSmartcardManagerClass))
#define SPICE_IS_SMARTCARD_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPICE_TYPE_SMARTCARD_MANAGER))
#define SPICE_IS_SMARTCARD_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SPICE_TYPE_SMARTCARD_MANAGER))
#define SPICE_SMARTCARD_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SPICE_TYPE_SMARTCARD_MANAGER, SpiceSmartcardManagerClass))

#define SPICE_TYPE_SMARTCARD_READER (spice_smartcard_reader_get_type())

typedef struct _SpiceSmartcardManager SpiceSmartcardManager;
typedef struct _SpiceSmartcardManagerClass SpiceSmartcardManagerClass;
typedef struct _SpiceSmartcardManagerPrivate SpiceSmartcardManagerPrivate;
typedef struct _SpiceSmartcardReader SpiceSmartcardReader;

struct _SpiceSmartcardManager
{
    GObject parent;

    /*< private >*/
    SpiceSmartcardManagerPrivate *priv;
    /* Do not add fields to this struct */
};

struct _SpiceSmartcardManagerClass
{
    GObjectClass parent_class;
    /*< public >*/
    /* signals */
    void (*reader_added)(SpiceSmartcardManager *manager, SpiceSmartcardReader *reader);
    void (*reader_removed)(SpiceSmartcardManager *manager, SpiceSmartcardReader *reader);
    void (*card_inserted)(SpiceSmartcardManager *manager, SpiceSmartcardReader *reader);
    void (*card_removed)(SpiceSmartcardManager *manager, SpiceSmartcardReader *reader );

    /*< private >*/
    /*
     * If adding fields to this struct, remove corresponding
     * amount of padding to avoid changing overall struct size
     */
    gchar _spice_reserved[SPICE_RESERVED_PADDING];
};

GType spice_smartcard_manager_get_type(void);
GType spice_smartcard_reader_get_type(void);

SpiceSmartcardManager *spice_smartcard_manager_get(void);
gboolean spice_smartcard_manager_insert_card(SpiceSmartcardManager *manager);
gboolean spice_smartcard_manager_remove_card(SpiceSmartcardManager *manager);
gboolean spice_smartcard_reader_is_software(SpiceSmartcardReader *reader);
gboolean spice_smartcard_reader_insert_card(SpiceSmartcardReader *reader);
gboolean spice_smartcard_reader_remove_card(SpiceSmartcardReader *reader);
GList *spice_smartcard_manager_get_readers(SpiceSmartcardManager *manager);

G_END_DECLS

#endif /* __SPICE_SMARTCARD_MANAGER_H__ */

