
/* Generated data (by glib-mkenums) */

#include <glib-object.h>
#include "spice-widget-enums.h"


#include "spice-widget.h"
static const GEnumValue _spice_display_key_event_values[] = {
  { SPICE_DISPLAY_KEY_EVENT_PRESS, "SPICE_DISPLAY_KEY_EVENT_PRESS", "press" },
  { SPICE_DISPLAY_KEY_EVENT_RELEASE, "SPICE_DISPLAY_KEY_EVENT_RELEASE", "release" },
  { SPICE_DISPLAY_KEY_EVENT_CLICK, "SPICE_DISPLAY_KEY_EVENT_CLICK", "click" },
  { 0, NULL, NULL }
};

GType
spice_display_key_event_get_type (void)
{
  static GType type = 0;
  static volatile gsize type_volatile = 0;

  if (g_once_init_enter(&type_volatile)) {
    type = g_enum_register_static ("SpiceDisplayKeyEvent", _spice_display_key_event_values);
    g_once_init_leave(&type_volatile, type);
  }

  return type;
}


/* Generated data ends here */

