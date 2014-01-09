


#ifndef SPICE_GLIB_ENUMS_H
#define SPICE_GLIB_ENUMS_H

G_BEGIN_DECLS

#define SPICE_TYPE_CHANNEL_EVENT spice_channel_event_get_type()
GType spice_channel_event_get_type (void);
#define SPICE_TYPE_INPUTS_LOCK spice_inputs_lock_get_type()
GType spice_inputs_lock_get_type (void);
#define SPICE_TYPE_SESSION_VERIFY spice_session_verify_get_type()
GType spice_session_verify_get_type (void);
#define SPICE_TYPE_SESSION_MIGRATION spice_session_migration_get_type()
GType spice_session_migration_get_type (void);
G_END_DECLS

#endif /* SPICE_CHANNEL_ENUMS_H */



