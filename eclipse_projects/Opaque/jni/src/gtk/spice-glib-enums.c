


#include <glib-object.h>
#include "spice-glib-enums.h"


#include "spice-session.h"

#include "spice-channel.h"

#include "channel-inputs.h"
static const GEnumValue _spice_channel_event_values[] = {
  { SPICE_CHANNEL_NONE, "SPICE_CHANNEL_NONE", "none" },
  { SPICE_CHANNEL_OPENED, "SPICE_CHANNEL_OPENED", "opened" },
  { SPICE_CHANNEL_SWITCHING, "SPICE_CHANNEL_SWITCHING", "switching" },
  { SPICE_CHANNEL_CLOSED, "SPICE_CHANNEL_CLOSED", "closed" },
  { SPICE_CHANNEL_ERROR_CONNECT, "SPICE_CHANNEL_ERROR_CONNECT", "error-connect" },
  { SPICE_CHANNEL_ERROR_TLS, "SPICE_CHANNEL_ERROR_TLS", "error-tls" },
  { SPICE_CHANNEL_ERROR_LINK, "SPICE_CHANNEL_ERROR_LINK", "error-link" },
  { SPICE_CHANNEL_ERROR_AUTH, "SPICE_CHANNEL_ERROR_AUTH", "error-auth" },
  { SPICE_CHANNEL_ERROR_IO, "SPICE_CHANNEL_ERROR_IO", "error-io" },
  { 0, NULL, NULL }
};

GType
spice_channel_event_get_type (void)
{
  static GType type = 0;
  static volatile gsize type_volatile = 0;

  if (g_once_init_enter(&type_volatile)) {
    type = g_enum_register_static ("SpiceChannelEvent", _spice_channel_event_values);
    g_once_init_leave(&type_volatile, type);
  }

  return type;
}


#include "spice-session.h"

#include "spice-channel.h"

#include "channel-inputs.h"
static const GFlagsValue _spice_inputs_lock_values[] = {
  { SPICE_INPUTS_SCROLL_LOCK, "SPICE_INPUTS_SCROLL_LOCK", "scroll-lock" },
  { SPICE_INPUTS_NUM_LOCK, "SPICE_INPUTS_NUM_LOCK", "num-lock" },
  { SPICE_INPUTS_CAPS_LOCK, "SPICE_INPUTS_CAPS_LOCK", "caps-lock" },
  { 0, NULL, NULL }
};

GType
spice_inputs_lock_get_type (void)
{
  static GType type = 0;
  static volatile gsize type_volatile = 0;

  if (g_once_init_enter(&type_volatile)) {
    type = g_flags_register_static ("SpiceInputsLock", _spice_inputs_lock_values);
    g_once_init_leave(&type_volatile, type);
  }

  return type;
}


#include "spice-session.h"

#include "spice-channel.h"

#include "channel-inputs.h"
static const GFlagsValue _spice_session_verify_values[] = {
  { SPICE_SESSION_VERIFY_PUBKEY, "SPICE_SESSION_VERIFY_PUBKEY", "pubkey" },
  { SPICE_SESSION_VERIFY_HOSTNAME, "SPICE_SESSION_VERIFY_HOSTNAME", "hostname" },
  { SPICE_SESSION_VERIFY_SUBJECT, "SPICE_SESSION_VERIFY_SUBJECT", "subject" },
  { 0, NULL, NULL }
};

GType
spice_session_verify_get_type (void)
{
  static GType type = 0;
  static volatile gsize type_volatile = 0;

  if (g_once_init_enter(&type_volatile)) {
    type = g_flags_register_static ("SpiceSessionVerify", _spice_session_verify_values);
    g_once_init_leave(&type_volatile, type);
  }

  return type;
}

static const GEnumValue _spice_session_migration_values[] = {
  { SPICE_SESSION_MIGRATION_NONE, "SPICE_SESSION_MIGRATION_NONE", "none" },
  { SPICE_SESSION_MIGRATION_SWITCHING, "SPICE_SESSION_MIGRATION_SWITCHING", "switching" },
  { SPICE_SESSION_MIGRATION_MIGRATING, "SPICE_SESSION_MIGRATION_MIGRATING", "migrating" },
  { 0, NULL, NULL }
};

GType
spice_session_migration_get_type (void)
{
  static GType type = 0;
  static volatile gsize type_volatile = 0;

  if (g_once_init_enter(&type_volatile)) {
    type = g_enum_register_static ("SpiceSessionMigration", _spice_session_migration_values);
    g_once_init_leave(&type_volatile, type);
  }

  return type;
}




