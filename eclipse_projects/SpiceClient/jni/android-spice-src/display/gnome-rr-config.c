/* gnome-rr-config.c
 * -*- c-basic-offset: 4 -*-
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * Copyright 2010 Giovanni Campagna
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <config.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#endif

#include "gnome-rr-config.h"

#include "edid.h"
#include "gnome-rr-private.h"

#define CONFIG_INTENDED_BASENAME "monitors.xml"
#define CONFIG_BACKUP_BASENAME "monitors.xml.backup"

/* In version 0 of the config file format, we had several <configuration>
 * toplevel elements and no explicit version number.  So, the filed looked
 * like
 *
 *   <configuration>
 *     ...
 *   </configuration>
 *   <configuration>
 *     ...
 *   </configuration>
 *
 * Since version 1 of the config file, the file has a toplevel <monitors>
 * element to group all the configurations.  That element has a "version"
 * attribute which is an integer. So, the file looks like this:
 *
 *   <monitors version="1">
 *     <configuration>
 *       ...
 *     </configuration>
 *     <configuration>
 *       ...
 *     </configuration>
 *   </monitors>
 */

/* A helper wrapper around the GMarkup parser stuff */
static gboolean parse_file_gmarkup (const gchar *file,
				    const GMarkupParser *parser,
				    gpointer data,
				    GError **err);

typedef struct CrtcAssignment CrtcAssignment;

static gboolean         crtc_assignment_apply (CrtcAssignment   *assign,
					       guint32           timestamp,
					       GError          **error);
static CrtcAssignment  *crtc_assignment_new   (GnomeRRScreen      *screen,
					       GnomeRROutputInfo **outputs,
					       GError            **error);
static void             crtc_assignment_free  (CrtcAssignment   *assign);

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_LAST
};

G_DEFINE_TYPE (GnomeRRConfig, gnome_rr_config, G_TYPE_OBJECT)

typedef struct Parser Parser;

/* Parser for monitor configurations */
struct Parser
{
    int			config_file_version;
    GnomeRROutputInfo *	output;
    GnomeRRConfig *	configuration;
    GPtrArray *		outputs;
    GPtrArray *		configurations;
    GQueue *		stack;
};

static int
parse_int (const char *text)
{
    return strtol (text, NULL, 0);
}

static guint
parse_uint (const char *text)
{
    return strtoul (text, NULL, 0);
}

static gboolean
stack_is (Parser *parser,
	  const char *s1,
	  ...)
{
    GList *stack = NULL;
    const char *s;
    GList *l1, *l2;
    va_list args;

    stack = g_list_prepend (stack, (gpointer)s1);

    va_start (args, s1);

    s = va_arg (args, const char *);
    while (s)
    {
	stack = g_list_prepend (stack, (gpointer)s);
	s = va_arg (args, const char *);
    }

    l1 = stack;
    l2 = parser->stack->head;

    while (l1 && l2)
    {
	if (strcmp (l1->data, l2->data) != 0)
	{
	    g_list_free (stack);
	    return FALSE;
	}

	l1 = l1->next;
	l2 = l2->next;
    }

    g_list_free (stack);

    return (!l1 && !l2);
}

static void
handle_start_element (GMarkupParseContext *context,
		      const gchar         *name,
		      const gchar        **attr_names,
		      const gchar        **attr_values,
		      gpointer             user_data,
		      GError             **err)
{
    Parser *parser = user_data;

    if (strcmp (name, "output") == 0)
    {
	int i;
	g_return_if_fail (parser->output == NULL);

	parser->output = g_object_new (GNOME_TYPE_RR_OUTPUT_INFO, NULL);
	parser->output->priv->rotation = 0;

	for (i = 0; attr_names[i] != NULL; ++i)
	{
	    if (strcmp (attr_names[i], "name") == 0)
	    {
		parser->output->priv->name = g_strdup (attr_values[i]);
		break;
	    }
	}

	if (!parser->output->priv->name)
	{
	    /* This really shouldn't happen, but it's better to make
	     * something up than to crash later.
	     */
	    g_warning ("Malformed monitor configuration file");

	    parser->output->priv->name = g_strdup ("default");
	}
	parser->output->priv->connected = FALSE;
	parser->output->priv->on = FALSE;
	parser->output->priv->primary = FALSE;
    }
    else if (strcmp (name, "configuration") == 0)
    {
	g_return_if_fail (parser->configuration == NULL);

	parser->configuration = g_object_new (GNOME_TYPE_RR_CONFIG, NULL);
	parser->configuration->priv->clone = FALSE;
	parser->configuration->priv->outputs = NULL;
    }
    else if (strcmp (name, "monitors") == 0)
    {
	int i;

	for (i = 0; attr_names[i] != NULL; i++)
	{
	    if (strcmp (attr_names[i], "version") == 0)
	    {
		parser->config_file_version = parse_int (attr_values[i]);
		break;
	    }
	}
    }

    g_queue_push_tail (parser->stack, g_strdup (name));
}

static void
handle_end_element (GMarkupParseContext *context,
		    const gchar         *name,
		    gpointer             user_data,
		    GError             **err)
{
    Parser *parser = user_data;

    if (strcmp (name, "output") == 0)
    {
	/* If no rotation properties were set, just use GNOME_RR_ROTATION_0 */
	if (parser->output->priv->rotation == 0)
	    parser->output->priv->rotation = GNOME_RR_ROTATION_0;

	g_ptr_array_add (parser->outputs, parser->output);

	parser->output = NULL;
    }
    else if (strcmp (name, "configuration") == 0)
    {
	g_ptr_array_add (parser->outputs, NULL);
	parser->configuration->priv->outputs =
	    (GnomeRROutputInfo **)g_ptr_array_free (parser->outputs, FALSE);
	parser->outputs = g_ptr_array_new ();
	g_ptr_array_add (parser->configurations, parser->configuration);
	parser->configuration = NULL;
    }

    g_free (g_queue_pop_tail (parser->stack));
}

#define TOPLEVEL_ELEMENT (parser->config_file_version > 0 ? "monitors" : NULL)

static void
handle_text (GMarkupParseContext *context,
	     const gchar         *text,
	     gsize                text_len,
	     gpointer             user_data,
	     GError             **err)
{
    Parser *parser = user_data;

    if (stack_is (parser, "vendor", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->connected = TRUE;

	strncpy ((gchar*) parser->output->priv->vendor, text, 3);
	parser->output->priv->vendor[3] = 0;
    }
    else if (stack_is (parser, "clone", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	if (strcmp (text, "yes") == 0)
	    parser->configuration->priv->clone = TRUE;
    }
    else if (stack_is (parser, "product", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->connected = TRUE;

	parser->output->priv->product = parse_int (text);
    }
    else if (stack_is (parser, "serial", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->connected = TRUE;

	parser->output->priv->serial = parse_uint (text);
    }
    else if (stack_is (parser, "width", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->on = TRUE;

	parser->output->priv->width = parse_int (text);
    }
    else if (stack_is (parser, "x", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->on = TRUE;

	parser->output->priv->x = parse_int (text);
    }
    else if (stack_is (parser, "y", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->on = TRUE;

	parser->output->priv->y = parse_int (text);
    }
    else if (stack_is (parser, "height", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->on = TRUE;

	parser->output->priv->height = parse_int (text);
    }
    else if (stack_is (parser, "rate", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	parser->output->priv->on = TRUE;

	parser->output->priv->rate = parse_int (text);
    }
    else if (stack_is (parser, "rotation", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	if (strcmp (text, "normal") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_ROTATION_0;
	}
	else if (strcmp (text, "left") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_ROTATION_90;
	}
	else if (strcmp (text, "upside_down") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_ROTATION_180;
	}
	else if (strcmp (text, "right") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_ROTATION_270;
	}
    }
    else if (stack_is (parser, "reflect_x", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	if (strcmp (text, "yes") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_REFLECT_X;
	}
    }
    else if (stack_is (parser, "reflect_y", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	if (strcmp (text, "yes") == 0)
	{
	    parser->output->priv->rotation |= GNOME_RR_REFLECT_Y;
	}
    }
    else if (stack_is (parser, "primary", "output", "configuration", TOPLEVEL_ELEMENT, NULL))
    {
	if (strcmp (text, "yes") == 0)
	{
	    parser->output->priv->primary = TRUE;
	}
    }
    else
    {
	/* Ignore other properties so we can expand the format in the future */
    }
}

static void
parser_free (Parser *parser)
{
    int i;
    GList *list;

    g_return_if_fail (parser != NULL);

    if (parser->output)
	g_object_unref (parser->output);

    if (parser->configuration)
	g_object_unref (parser->configuration);

    for (i = 0; i < parser->outputs->len; ++i)
    {
	GnomeRROutputInfo *output = parser->outputs->pdata[i];

	g_object_unref (output);
    }

    g_ptr_array_free (parser->outputs, TRUE);

    for (i = 0; i < parser->configurations->len; ++i)
    {
	GnomeRRConfig *config = parser->configurations->pdata[i];

	g_object_unref (config);
    }

    g_ptr_array_free (parser->configurations, TRUE);

    for (list = parser->stack->head; list; list = list->next)
	g_free (list->data);
    g_queue_free (parser->stack);

    g_free (parser);
}

static GnomeRRConfig **
configurations_read_from_file (const gchar *filename, GError **error)
{
    Parser *parser = g_new0 (Parser, 1);
    GnomeRRConfig **result;
    GMarkupParser callbacks = {
	handle_start_element,
	handle_end_element,
	handle_text,
	NULL, /* passthrough */
	NULL, /* error */
    };

    parser->config_file_version = 0;
    parser->configurations = g_ptr_array_new ();
    parser->outputs = g_ptr_array_new ();
    parser->stack = g_queue_new ();

    if (!parse_file_gmarkup (filename, &callbacks, parser, error))
    {
	result = NULL;

	g_return_val_if_fail (parser->outputs, NULL);
	goto out;
    }

    g_return_val_if_fail (parser->outputs, NULL);

    g_ptr_array_add (parser->configurations, NULL);
    result = (GnomeRRConfig **)g_ptr_array_free (parser->configurations, FALSE);
    parser->configurations = g_ptr_array_new ();

    g_return_val_if_fail (parser->outputs, NULL);
out:
    parser_free (parser);

    return result;
}

static void
gnome_rr_config_init (GnomeRRConfig *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GNOME_TYPE_RR_CONFIG, GnomeRRConfigPrivate);

    self->priv->clone = FALSE;
    self->priv->screen = NULL;
    self->priv->outputs = NULL;
}

static void
gnome_rr_config_set_property (GObject *gobject, guint property_id, const GValue *value, GParamSpec *property)
{
    GnomeRRConfig *self = GNOME_RR_CONFIG (gobject);

    switch (property_id) {
	case PROP_SCREEN:
	    self->priv->screen = g_value_dup_object (value);
	    return;
	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
    }
}

static void
gnome_rr_config_finalize (GObject *gobject)
{
    GnomeRRConfig *self = GNOME_RR_CONFIG (gobject);

    if (self->priv->screen)
	g_object_unref (self->priv->screen);

    if (self->priv->outputs) {
	int i;

        for (i = 0; self->priv->outputs[i] != NULL; i++) {
	    GnomeRROutputInfo *output = self->priv->outputs[i];
	    g_object_unref (output);
	}
	g_free (self->priv->outputs);
    }

    G_OBJECT_CLASS (gnome_rr_config_parent_class)->finalize (gobject);
}

gboolean
gnome_rr_config_load_current (GnomeRRConfig *config, GError **error)
{
    GPtrArray *a;
    GnomeRROutput **rr_outputs;
    int i;
    int clone_width = -1;
    int clone_height = -1;
    int last_x;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (config), FALSE);

    a = g_ptr_array_new ();
    rr_outputs = gnome_rr_screen_list_outputs (config->priv->screen);

    config->priv->clone = FALSE;

    for (i = 0; rr_outputs[i] != NULL; ++i)
    {
	GnomeRROutput *rr_output = rr_outputs[i];
	GnomeRROutputInfo *output = g_object_new (GNOME_TYPE_RR_OUTPUT_INFO, NULL);
	GnomeRRMode *mode = NULL;
	const guint8 *edid_data = gnome_rr_output_get_edid_data (rr_output);
	GnomeRRCrtc *crtc;

	output->priv->name = g_strdup (gnome_rr_output_get_name (rr_output));
	output->priv->connected = gnome_rr_output_is_connected (rr_output);

	if (!output->priv->connected)
	{
	    output->priv->x = -1;
	    output->priv->y = -1;
	    output->priv->width = -1;
	    output->priv->height = -1;
	    output->priv->rate = -1;
	    output->priv->rotation = GNOME_RR_ROTATION_0;
	}
	else
	{
	    MonitorInfo *info = NULL;

	    if (edid_data)
		info = decode_edid (edid_data);

	    if (info)
	    {
		memcpy (output->priv->vendor, info->manufacturer_code,
			sizeof (output->priv->vendor));

		output->priv->product = info->product_code;
		output->priv->serial = info->serial_number;
		output->priv->aspect = info->aspect_ratio;
	    }
	    else
	    {
		strcpy (output->priv->vendor, "???");
		output->priv->product = 0;
		output->priv->serial = 0;
	    }

	    if (gnome_rr_output_is_laptop (rr_output))
		output->priv->display_name = g_strdup (_("Laptop"));
	    else
		output->priv->display_name = make_display_name (info);

	    g_free (info);

	    crtc = gnome_rr_output_get_crtc (rr_output);
	    mode = crtc? gnome_rr_crtc_get_current_mode (crtc) : NULL;

	    if (crtc && mode)
	    {
		output->priv->on = TRUE;

		gnome_rr_crtc_get_position (crtc, &output->priv->x, &output->priv->y);
		output->priv->width = gnome_rr_mode_get_width (mode);
		output->priv->height = gnome_rr_mode_get_height (mode);
		output->priv->rate = gnome_rr_mode_get_freq (mode);
		output->priv->rotation = gnome_rr_crtc_get_current_rotation (crtc);

		if (output->priv->x == 0 && output->priv->y == 0) {
			if (clone_width == -1) {
				clone_width = output->priv->width;
				clone_height = output->priv->height;
			} else if (clone_width == output->priv->width &&
				   clone_height == output->priv->height) {
				config->priv->clone = TRUE;
			}
		}
	    }
	    else
	    {
		output->priv->on = FALSE;
		config->priv->clone = FALSE;
	    }

	    /* Get preferred size for the monitor */
	    mode = gnome_rr_output_get_preferred_mode (rr_output);

	    if (!mode)
	    {
		GnomeRRMode **modes = gnome_rr_output_list_modes (rr_output);

		/* FIXME: we should pick the "best" mode here, where best is
		 * sorted wrt
		 *
		 * - closest aspect ratio
		 * - mode area
		 * - refresh rate
		 * - We may want to extend randrwrap so that get_preferred
		 *   returns that - although that could also depend on
		 *   the crtc.
		 */
		if (modes[0])
		    mode = modes[0];
	    }

	    if (mode)
	    {
		output->priv->pref_width = gnome_rr_mode_get_width (mode);
		output->priv->pref_height = gnome_rr_mode_get_height (mode);
	    }
	    else
	    {
		/* Pick some random numbers. This should basically never happen */
		output->priv->pref_width = 1024;
		output->priv->pref_height = 768;
	    }
	}

        output->priv->primary = gnome_rr_output_get_is_primary (rr_output);

	g_ptr_array_add (a, output);
    }

    g_ptr_array_add (a, NULL);

    config->priv->outputs = (GnomeRROutputInfo **)g_ptr_array_free (a, FALSE);

    /* Walk the outputs computing the right-most edge of all
     * lit-up displays
     */
    last_x = 0;
    for (i = 0; config->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *output = config->priv->outputs[i];

	if (output->priv->on)
	{
	    last_x = MAX (last_x, output->priv->x + output->priv->width);
	}
    }

    /* Now position all off displays to the right of the
     * on displays
     */
    for (i = 0; config->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *output = config->priv->outputs[i];

	if (output->priv->connected && !output->priv->on)
	{
	    output->priv->x = last_x;
	    last_x = output->priv->x + output->priv->width;
	}
    }

    g_return_val_if_fail (gnome_rr_config_match (config, config), FALSE);

    return TRUE;
}

gboolean
gnome_rr_config_load_filename (GnomeRRConfig *result, const char *filename, GError **error)
{
    GnomeRRConfig *current;
    GnomeRRConfig **configs;
    gboolean found = FALSE;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (result), FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    current = gnome_rr_config_new_current (result->priv->screen, error);

    configs = configurations_read_from_file (filename, error);

    if (configs)
    {
	int i;

	for (i = 0; configs[i] != NULL; ++i)
	{
	    if (gnome_rr_config_match (configs[i], current))
	    {
		int j;
		GPtrArray *array;
		result->priv->clone = configs[i]->priv->clone;

		array = g_ptr_array_new ();
		for (j = 0; configs[i]->priv->outputs[j] != NULL; j++) {
                    g_object_ref (configs[i]->priv->outputs[j]);
		    g_ptr_array_add (array, configs[i]->priv->outputs[j]);
		}
		g_ptr_array_add (array, NULL);
		result->priv->outputs = (GnomeRROutputInfo **) g_ptr_array_free (array, FALSE);

		found = TRUE;
		break;
	    }
	    g_object_unref (configs[i]);
	}
	g_free (configs);

	if (!found)
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_NO_MATCHING_CONFIG,
			 _("none of the saved display configurations matched the active configuration"));
    }

    g_object_unref (current);
    return found;
}

static void
gnome_rr_config_class_init (GnomeRRConfigClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (GnomeRROutputInfoPrivate));

    gobject_class->set_property = gnome_rr_config_set_property;
    gobject_class->finalize = gnome_rr_config_finalize;

    g_object_class_install_property (gobject_class, PROP_SCREEN,
				     g_param_spec_object ("screen", "Screen", "The GnomeRRScreen this config applies to", GNOME_TYPE_RR_SCREEN,
							  G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

GnomeRRConfig *
gnome_rr_config_new_current (GnomeRRScreen *screen, GError **error)
{
    GnomeRRConfig *self = g_object_new (GNOME_TYPE_RR_CONFIG, "screen", screen, NULL);

    if (gnome_rr_config_load_current (self, error))
      return self;
    else
      {
	g_object_unref (self);
	return NULL;
      }
}

GnomeRRConfig *
gnome_rr_config_new_stored (GnomeRRScreen *screen, GError **error)
{
    GnomeRRConfig *self = g_object_new (GNOME_TYPE_RR_CONFIG, "screen", screen, NULL);
    char *filename;
    gboolean success;

    filename = gnome_rr_config_get_intended_filename ();

    success = gnome_rr_config_load_filename (self, filename, error);

    g_free (filename);

    if (success)
      return self;
    else
      {
	g_object_unref (self);
	return NULL;
      }
}

static gboolean
parse_file_gmarkup (const gchar          *filename,
		    const GMarkupParser  *parser,
		    gpointer             data,
		    GError              **err)
{
    GMarkupParseContext *context = NULL;
    gchar *contents = NULL;
    gboolean result = TRUE;
    gsize len;

    if (!g_file_get_contents (filename, &contents, &len, err))
    {
	result = FALSE;
	goto out;
    }

    context = g_markup_parse_context_new (parser, 0, data, NULL);

    if (!g_markup_parse_context_parse (context, contents, len, err))
    {
	result = FALSE;
	goto out;
    }

    if (!g_markup_parse_context_end_parse (context, err))
    {
	result = FALSE;
	goto out;
    }

out:
    if (contents)
	g_free (contents);

    if (context)
	g_markup_parse_context_free (context);

    return result;
}

static gboolean
output_match (GnomeRROutputInfo *output1, GnomeRROutputInfo *output2)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (output1), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (output2), FALSE);

    if (strcmp (output1->priv->name, output2->priv->name) != 0)
	return FALSE;

    if (strcmp (output1->priv->vendor, output2->priv->vendor) != 0)
	return FALSE;

    if (output1->priv->product != output2->priv->product)
	return FALSE;

    if (output1->priv->serial != output2->priv->serial)
	return FALSE;

    if (output1->priv->connected != output2->priv->connected)
	return FALSE;

    return TRUE;
}

static gboolean
output_equal (GnomeRROutputInfo *output1, GnomeRROutputInfo *output2)
{
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (output1), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_OUTPUT_INFO (output2), FALSE);

    if (!output_match (output1, output2))
	return FALSE;

    if (output1->priv->on != output2->priv->on)
	return FALSE;

    if (output1->priv->on)
    {
	if (output1->priv->width != output2->priv->width)
	    return FALSE;

	if (output1->priv->height != output2->priv->height)
	    return FALSE;

	if (output1->priv->rate != output2->priv->rate)
	    return FALSE;

	if (output1->priv->x != output2->priv->x)
	    return FALSE;

	if (output1->priv->y != output2->priv->y)
	    return FALSE;

	if (output1->priv->rotation != output2->priv->rotation)
	    return FALSE;
    }

    return TRUE;
}

static GnomeRROutputInfo *
find_output (GnomeRRConfig *config, const char *name)
{
    int i;

    for (i = 0; config->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *output = config->priv->outputs[i];

	if (strcmp (name, output->priv->name) == 0)
	    return output;
    }

    return NULL;
}

/* Match means "these configurations apply to the same hardware
 * setups"
 */
gboolean
gnome_rr_config_match (GnomeRRConfig *c1, GnomeRRConfig *c2)
{
    int i;
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (c1), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (c2), FALSE);

    for (i = 0; c1->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *output1 = c1->priv->outputs[i];
	GnomeRROutputInfo *output2;

	output2 = find_output (c2, output1->priv->name);
	if (!output2 || !output_match (output1, output2))
	    return FALSE;
    }

    return TRUE;
}

/* Equal means "the configurations will result in the same
 * modes being set on the outputs"
 */
gboolean
gnome_rr_config_equal (GnomeRRConfig  *c1,
		       GnomeRRConfig  *c2)
{
    int i;
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (c1), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (c2), FALSE);

    for (i = 0; c1->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *output1 = c1->priv->outputs[i];
	GnomeRROutputInfo *output2;

	output2 = find_output (c2, output1->priv->name);
	if (!output2 || !output_equal (output1, output2))
	    return FALSE;
    }

    return TRUE;
}

static GnomeRROutputInfo **
make_outputs (GnomeRRConfig *config)
{
    GPtrArray *outputs;
    GnomeRROutputInfo *first_on;
    int i;

    outputs = g_ptr_array_new ();

    first_on = NULL;

    for (i = 0; config->priv->outputs[i] != NULL; ++i)
    {
	GnomeRROutputInfo *old = config->priv->outputs[i];
	GnomeRROutputInfo *new = g_object_new (GNOME_TYPE_RR_OUTPUT_INFO, NULL);
	*(new->priv) = *(old->priv);
	if (old->priv->name)
	    new->priv->name = g_strdup (old->priv->name);
	if (old->priv->display_name)
	    new->priv->display_name = g_strdup (old->priv->display_name);

	if (old->priv->on && !first_on)
	    first_on = old;

	if (config->priv->clone && new->priv->on)
	{
            if (!first_on) {
                g_warn_if_reached ();
                goto end;
            }
	    new->priv->width = first_on->priv->width;
	    new->priv->height = first_on->priv->height;
	    new->priv->rotation = first_on->priv->rotation;
	    new->priv->x = 0;
	    new->priv->y = 0;
	}

	g_ptr_array_add (outputs, new);
    }

end:
    g_ptr_array_add (outputs, NULL);

    return (GnomeRROutputInfo **)g_ptr_array_free (outputs, FALSE);
}

gboolean
gnome_rr_config_applicable (GnomeRRConfig  *configuration,
			    GnomeRRScreen  *screen,
			    GError        **error)
{
    GnomeRROutputInfo **outputs;
    CrtcAssignment *assign;
    gboolean result;
    int i;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (configuration), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    outputs = make_outputs (configuration);
    assign = crtc_assignment_new (screen, outputs, error);

    if (assign)
    {
	result = TRUE;
	crtc_assignment_free (assign);
    }
    else
    {
	result = FALSE;
    }

    for (i = 0; outputs[i] != NULL; i++) {
	 g_object_unref (outputs[i]);
    }

    return result;
}

/* Database management */

static void
ensure_config_directory (void)
{
    g_mkdir_with_parents (g_get_user_config_dir (), 0700);
}

char *
gnome_rr_config_get_backup_filename (void)
{
    ensure_config_directory ();
    return g_build_filename (g_get_user_config_dir (), CONFIG_BACKUP_BASENAME, NULL);
}

char *
gnome_rr_config_get_intended_filename (void)
{
    ensure_config_directory ();
    return g_build_filename (g_get_user_config_dir (), CONFIG_INTENDED_BASENAME, NULL);
}

static const char *
get_rotation_name (GnomeRRRotation r)
{
    if (r & GNOME_RR_ROTATION_0)
	return "normal";
    if (r & GNOME_RR_ROTATION_90)
	return "left";
    if (r & GNOME_RR_ROTATION_180)
	return "upside_down";
    if (r & GNOME_RR_ROTATION_270)
	return "right";

    return "normal";
}

static const char *
yes_no (int x)
{
    return x? "yes" : "no";
}

static const char *
get_reflect_x (GnomeRRRotation r)
{
    return yes_no (r & GNOME_RR_REFLECT_X);
}

static const char *
get_reflect_y (GnomeRRRotation r)
{
    return yes_no (r & GNOME_RR_REFLECT_Y);
}

static void
emit_configuration (GnomeRRConfig *config,
		    GString *string)
{
    int j;

    g_string_append_printf (string, "  <configuration>\n");

    g_string_append_printf (string, "      <clone>%s</clone>\n", yes_no (config->priv->clone));

    for (j = 0; config->priv->outputs[j] != NULL; ++j)
    {
	GnomeRROutputInfo *output = config->priv->outputs[j];

	g_string_append_printf (
	    string, "      <output name=\"%s\">\n", output->priv->name);

	if (output->priv->connected && *output->priv->vendor != '\0')
	{
	    g_string_append_printf (
		string, "          <vendor>%s</vendor>\n", output->priv->vendor);
	    g_string_append_printf (
		string, "          <product>0x%04x</product>\n", output->priv->product);
	    g_string_append_printf (
		string, "          <serial>0x%08x</serial>\n", output->priv->serial);
	}

	/* An unconnected output which is on does not make sense */
	if (output->priv->connected && output->priv->on)
	{
	    g_string_append_printf (
		string, "          <width>%d</width>\n", output->priv->width);
	    g_string_append_printf (
		string, "          <height>%d</height>\n", output->priv->height);
	    g_string_append_printf (
		string, "          <rate>%d</rate>\n", output->priv->rate);
	    g_string_append_printf (
		string, "          <x>%d</x>\n", output->priv->x);
	    g_string_append_printf (
		string, "          <y>%d</y>\n", output->priv->y);
	    g_string_append_printf (
		string, "          <rotation>%s</rotation>\n", get_rotation_name (output->priv->rotation));
	    g_string_append_printf (
		string, "          <reflect_x>%s</reflect_x>\n", get_reflect_x (output->priv->rotation));
	    g_string_append_printf (
		string, "          <reflect_y>%s</reflect_y>\n", get_reflect_y (output->priv->rotation));
            g_string_append_printf (
                string, "          <primary>%s</primary>\n", yes_no (output->priv->primary));
	}

	g_string_append_printf (string, "      </output>\n");
    }

    g_string_append_printf (string, "  </configuration>\n");
}

void
gnome_rr_config_sanitize (GnomeRRConfig *config)
{
    int i;
    int x_offset, y_offset;
    gboolean found;

    /* Offset everything by the top/left-most coordinate to
     * make sure the configuration starts at (0, 0)
     */
    x_offset = y_offset = G_MAXINT;
    for (i = 0; config->priv->outputs[i]; ++i)
    {
	GnomeRROutputInfo *output = config->priv->outputs[i];

	if (output->priv->on)
	{
	    x_offset = MIN (x_offset, output->priv->x);
	    y_offset = MIN (y_offset, output->priv->y);
	}
    }

    for (i = 0; config->priv->outputs[i]; ++i)
    {
	GnomeRROutputInfo *output = config->priv->outputs[i];

	if (output->priv->on)
	{
	    output->priv->x -= x_offset;
	    output->priv->y -= y_offset;
	}
    }

    /* Only one primary, please */
    found = FALSE;
    for (i = 0; config->priv->outputs[i]; ++i)
    {
        if (config->priv->outputs[i]->priv->primary)
        {
            if (found)
            {
                config->priv->outputs[i]->priv->primary = FALSE;
            }
            else
            {
                found = TRUE;
            }
        }
    }
}

static gboolean
output_info_is_laptop (GnomeRROutputInfo *info)
{
        if (info->priv->name
            && (strstr (info->priv->name, "lvds") ||  /* Most drivers use an "LVDS" prefix... */
                strstr (info->priv->name, "LVDS") ||
                strstr (info->priv->name, "Lvds") ||
                strstr (info->priv->name, "LCD")))    /* ... but fglrx uses "LCD" in some versions.  Shoot me now, kthxbye. */
                return TRUE;

        return FALSE;
}

gboolean
gnome_rr_config_ensure_primary (GnomeRRConfig *configuration)
{
        int i;
        GnomeRROutputInfo  *laptop;
        GnomeRROutputInfo  *top_left;
        gboolean found;
	GnomeRRConfigPrivate *priv;

	g_return_val_if_fail (GNOME_IS_RR_CONFIG (configuration), FALSE);

        laptop = NULL;
        top_left = NULL;
        found = FALSE;
	priv = configuration->priv;

        for (i = 0; priv->outputs[i] != NULL; ++i) {
                GnomeRROutputInfo *info = priv->outputs[i];

                if (!info->priv->on) {
                       info->priv->primary = FALSE;
                       continue;
                }

                /* ensure only one */
                if (info->priv->primary) {
                        if (found) {
                                info->priv->primary = FALSE;
                        } else {
                                found = TRUE;
                        }
                }

                if (top_left == NULL
                    || (info->priv->x < top_left->priv->x
                        && info->priv->y < top_left->priv->y)) {
                        top_left = info;
                }
                if (laptop == NULL
                    && output_info_is_laptop (info)) {
                        /* shame we can't find the connector type
                           as with gnome_rr_output_is_laptop */
                        laptop = info;
                }
        }

        if (!found) {
                if (laptop != NULL) {
                        laptop->priv->primary = TRUE;
                } else if (top_left != NULL) {
                        top_left->priv->primary = TRUE;
                }
        }

        return !found;
}

GString *
gnome_rr_config_dump (GnomeRRConfig *configuration)
{
  GString *output;

  output = g_string_new ("");
  emit_configuration (configuration, output);
  return output;
}

gboolean
gnome_rr_config_save (GnomeRRConfig *configuration, GError **error)
{
    GnomeRRConfig **configurations;
    GString *output;
    int i;
    gchar *intended_filename;
    gchar *backup_filename;
    gboolean result;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (configuration), FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    output = g_string_new ("");

    backup_filename = gnome_rr_config_get_backup_filename ();
    intended_filename = gnome_rr_config_get_intended_filename ();

    configurations = configurations_read_from_file (intended_filename, NULL); /* NULL-GError */

    g_string_append_printf (output, "<monitors version=\"1\">\n");

    if (configurations)
    {
	for (i = 0; configurations[i] != NULL; ++i)
	{
	    if (!gnome_rr_config_match (configurations[i], configuration))
		emit_configuration (configurations[i], output);
	    g_object_unref (configurations[i]);
	}

	g_free (configurations);
    }

    emit_configuration (configuration, output);

    g_string_append_printf (output, "</monitors>\n");

    /* backup the file first */
    rename (intended_filename, backup_filename); /* no error checking because the intended file may not even exist */

    result = g_file_set_contents (intended_filename, output->str, -1, error);

    if (!result)
	rename (backup_filename, intended_filename); /* no error checking because the backup may not even exist */

    g_free (backup_filename);
    g_free (intended_filename);

    return result;
}

gboolean
gnome_rr_config_apply_with_time (GnomeRRConfig *config,
				 GnomeRRScreen *screen,
				 guint32        timestamp,
				 GError       **error)
{
    CrtcAssignment *assignment;
    GnomeRROutputInfo **outputs;
    gboolean result = FALSE;
    int i;

    g_return_val_if_fail (GNOME_IS_RR_CONFIG (config), FALSE);
    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), FALSE);

    outputs = make_outputs (config);

    assignment = crtc_assignment_new (screen, outputs, error);

    for (i = 0; outputs[i] != NULL; i++)
	g_object_unref (outputs[i]);
    g_free (outputs);

    if (assignment)
    {
	if (crtc_assignment_apply (assignment, timestamp, error))
	    result = TRUE;

	crtc_assignment_free (assignment);

	gdk_flush ();
    }

    return result;
}

/* gnome_rr_config_apply_from_filename_with_time:
 * @screen: A #GnomeRRScreen
 * @filename: Path of the file to look in for stored RANDR configurations.
 * @timestamp: X server timestamp from the event that causes the screen configuration to change (a user's button press, for example)
 * @error: Location to store error, or %NULL
 *
 * First, this function refreshes the @screen to match the current RANDR
 * configuration from the X server.  Then, it tries to load the file in
 * @filename and looks for suitable matching RANDR configurations in the file;
 * if one is found, that configuration will be applied to the current set of
 * RANDR outputs.
 *
 * Typically, @filename is the result of gnome_rr_config_get_intended_filename() or
 * gnome_rr_config_get_backup_filename().
 *
 * Returns: TRUE if the RANDR configuration was loaded and applied from
 * $(XDG_CONFIG_HOME)/monitors.xml, or FALSE otherwise:
 *
 * If the current RANDR configuration could not be refreshed, the @error will
 * have a domain of #GNOME_RR_ERROR and a corresponding error code.
 *
 * If the file in question is loaded successfully but the configuration cannot
 * be applied, the @error will have a domain of #GNOME_RR_ERROR.  Note that an
 * error code of #GNOME_RR_ERROR_NO_MATCHING_CONFIG is not a real error; it
 * simply means that there were no stored configurations that match the current
 * set of RANDR outputs.
 *
 * If the file in question cannot be loaded, the @error will have a domain of
 * #G_FILE_ERROR.  Note that an error code of G_FILE_ERROR_NOENT is not really
 * an error, either; it means that there was no stored configuration file and so
 * nothing is changed.
 */
gboolean
gnome_rr_config_apply_from_filename_with_time (GnomeRRScreen *screen, const char *filename, guint32 timestamp, GError **error)
{
    GnomeRRConfig *stored;
    GError *my_error;

    g_return_val_if_fail (GNOME_IS_RR_SCREEN (screen), FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    my_error = NULL;
    if (!gnome_rr_screen_refresh (screen, &my_error)) {
	    if (my_error) {
		    g_propagate_error (error, my_error);
		    return FALSE; /* This is a genuine error */
	    }

	    /* This means the screen didn't change, so just proceed */
    }

    stored = g_object_new (GNOME_TYPE_RR_CONFIG, "screen", screen, NULL);

    if (gnome_rr_config_load_filename (stored, filename, error))
    {
	gboolean result;

        gnome_rr_config_ensure_primary (stored);
	result = gnome_rr_config_apply_with_time (stored, screen, timestamp, error);

	g_object_unref (stored);
	return result;
    }
    else
    {
	g_object_unref (stored);
	return FALSE;
    }
}

/**
 * gnome_rr_config_get_outputs:
 *
 * Returns: (array zero-terminated=1) (element-type GnomeDesktop.RROutputInfo) (transfer none): the output configuration for this #GnomeRRConfig
 */
GnomeRROutputInfo **
gnome_rr_config_get_outputs (GnomeRRConfig *self)
{
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (self), NULL);

    return self->priv->outputs;
}

/**
 * gnome_rr_config_get_clone:
 *
 * Returns: whether at least two outputs are at (0, 0) offset and they
 * have the same width/height.  Those outputs are of course connected and on
 * (i.e. they have a CRTC assigned).
 */
gboolean
gnome_rr_config_get_clone (GnomeRRConfig *self)
{
    g_return_val_if_fail (GNOME_IS_RR_CONFIG (self), FALSE);

    return self->priv->clone;
}

void
gnome_rr_config_set_clone (GnomeRRConfig *self, gboolean clone)
{
    g_return_if_fail (GNOME_IS_RR_CONFIG (self));

    self->priv->clone = clone;
}

/*
 * CRTC assignment
 */
typedef struct CrtcInfo CrtcInfo;

struct CrtcInfo
{
    GnomeRRMode    *mode;
    int        x;
    int        y;
    GnomeRRRotation rotation;
    GPtrArray *outputs;
};

struct CrtcAssignment
{
    GnomeRRScreen *screen;
    GHashTable *info;
    GnomeRROutput *primary;
};

static gboolean
can_clone (CrtcInfo *info,
	   GnomeRROutput *output)
{
    int i;

    for (i = 0; i < info->outputs->len; ++i)
    {
	GnomeRROutput *clone = info->outputs->pdata[i];

	if (!gnome_rr_output_can_clone (clone, output))
	    return FALSE;
    }

    return TRUE;
}

static gboolean
crtc_assignment_assign (CrtcAssignment   *assign,
			GnomeRRCrtc      *crtc,
			GnomeRRMode      *mode,
			int               x,
			int               y,
			GnomeRRRotation   rotation,
                        gboolean          primary,
			GnomeRROutput    *output,
			GError          **error)
{
    CrtcInfo *info = g_hash_table_lookup (assign->info, crtc);
    guint32 crtc_id;
    const char *output_name;

    crtc_id = gnome_rr_crtc_get_id (crtc);
    output_name = gnome_rr_output_get_name (output);

    if (!gnome_rr_crtc_can_drive_output (crtc, output))
    {
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
		     _("CRTC %d cannot drive output %s"), crtc_id, output_name);
	return FALSE;
    }

    if (!gnome_rr_output_supports_mode (output, mode))
    {
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
		     _("output %s does not support mode %dx%d@%dHz"),
		     output_name,
		     gnome_rr_mode_get_width (mode),
		     gnome_rr_mode_get_height (mode),
		     gnome_rr_mode_get_freq (mode));
	return FALSE;
    }

    if (!gnome_rr_crtc_supports_rotation (crtc, rotation))
    {
	g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
		     _("CRTC %d does not support rotation=%s"),
		     crtc_id,
		     get_rotation_name (rotation));
	return FALSE;
    }

    if (info)
    {
	if (!(info->mode == mode	&&
	      info->x == x		&&
	      info->y == y		&&
	      info->rotation == rotation))
	{
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("output %s does not have the same parameters as another cloned output:\n"
			   "existing mode = %d, new mode = %d\n"
			   "existing coordinates = (%d, %d), new coordinates = (%d, %d)\n"
			   "existing rotation = %s, new rotation = %s"),
			 output_name,
			 gnome_rr_mode_get_id (info->mode), gnome_rr_mode_get_id (mode),
			 info->x, info->y,
			 x, y,
			 get_rotation_name (info->rotation), get_rotation_name (rotation));
	    return FALSE;
	}

	if (!can_clone (info, output))
	{
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("cannot clone to output %s"),
			 output_name);
	    return FALSE;
	}

	g_ptr_array_add (info->outputs, output);

	if (primary && !assign->primary)
	{
	    assign->primary = output;
	}

	return TRUE;
    }
    else
    {
	CrtcInfo *info = g_new0 (CrtcInfo, 1);

	info->mode = mode;
	info->x = x;
	info->y = y;
	info->rotation = rotation;
	info->outputs = g_ptr_array_new ();

	g_ptr_array_add (info->outputs, output);

	g_hash_table_insert (assign->info, crtc, info);

        if (primary && !assign->primary)
        {
            assign->primary = output;
        }

	return TRUE;
    }
}

static void
crtc_assignment_unassign (CrtcAssignment *assign,
			  GnomeRRCrtc         *crtc,
			  GnomeRROutput       *output)
{
    CrtcInfo *info = g_hash_table_lookup (assign->info, crtc);

    if (info)
    {
	g_ptr_array_remove (info->outputs, output);

        if (assign->primary == output)
        {
            assign->primary = NULL;
        }

	if (info->outputs->len == 0)
	    g_hash_table_remove (assign->info, crtc);
    }
}

static void
crtc_assignment_free (CrtcAssignment *assign)
{
    g_hash_table_destroy (assign->info);

    g_free (assign);
}

typedef struct {
    guint32 timestamp;
    gboolean has_error;
    GError **error;
} ConfigureCrtcState;

static void
configure_crtc (gpointer key,
		gpointer value,
		gpointer data)
{
    GnomeRRCrtc *crtc = key;
    CrtcInfo *info = value;
    ConfigureCrtcState *state = data;

    if (state->has_error)
	return;

    if (!gnome_rr_crtc_set_config_with_time (crtc,
					     state->timestamp,
					     info->x, info->y,
					     info->mode,
					     info->rotation,
					     (GnomeRROutput **)info->outputs->pdata,
					     info->outputs->len,
					     state->error))
	state->has_error = TRUE;
}

static gboolean
mode_is_rotated (CrtcInfo *info)
{
    if ((info->rotation & GNOME_RR_ROTATION_270)		||
	(info->rotation & GNOME_RR_ROTATION_90))
    {
	return TRUE;
    }
    return FALSE;
}

static gboolean
crtc_is_rotated (GnomeRRCrtc *crtc)
{
    GnomeRRRotation r = gnome_rr_crtc_get_current_rotation (crtc);

    if ((r & GNOME_RR_ROTATION_270)		||
	(r & GNOME_RR_ROTATION_90))
    {
	return TRUE;
    }

    return FALSE;
}

static void
accumulate_error (GString *accumulated_error, GError *error)
{
    g_string_append_printf (accumulated_error, "    %s\n", error->message);
    g_error_free (error);
}

/* Check whether the given set of settings can be used
 * at the same time -- ie. whether there is an assignment
 * of CRTC's to outputs.
 *
 * Brute force - the number of objects involved is small
 * enough that it doesn't matter.
 */
static gboolean
real_assign_crtcs (GnomeRRScreen *screen,
		   GnomeRROutputInfo **outputs,
		   CrtcAssignment *assignment,
		   GError **error)
{
    GnomeRRCrtc **crtcs = gnome_rr_screen_list_crtcs (screen);
    GnomeRROutputInfo *output;
    int i;
    gboolean tried_mode;
    GError *my_error;
    GString *accumulated_error;
    gboolean success;

    output = *outputs;
    if (!output)
	return TRUE;

    /* It is always allowed for an output to be turned off */
    if (!output->priv->on)
    {
	return real_assign_crtcs (screen, outputs + 1, assignment, error);
    }

    success = FALSE;
    tried_mode = FALSE;
    accumulated_error = g_string_new (NULL);

    for (i = 0; crtcs[i] != NULL; ++i)
    {
	GnomeRRCrtc *crtc = crtcs[i];
	int crtc_id = gnome_rr_crtc_get_id (crtc);
	int pass;

	g_string_append_printf (accumulated_error,
				_("Trying modes for CRTC %d\n"),
				crtc_id);

	/* Make two passes, one where frequencies must match, then
	 * one where they don't have to
	 */
	for (pass = 0; pass < 2; ++pass)
	{
	    GnomeRROutput *gnome_rr_output = gnome_rr_screen_get_output_by_name (screen, output->priv->name);
	    GnomeRRMode **modes = gnome_rr_output_list_modes (gnome_rr_output);
	    int j;

	    for (j = 0; modes[j] != NULL; ++j)
	    {
		GnomeRRMode *mode = modes[j];
		int mode_width;
		int mode_height;
		int mode_freq;

		mode_width = gnome_rr_mode_get_width (mode);
		mode_height = gnome_rr_mode_get_height (mode);
		mode_freq = gnome_rr_mode_get_freq (mode);

		g_string_append_printf (accumulated_error,
					_("CRTC %d: trying mode %dx%d@%dHz with output at %dx%d@%dHz (pass %d)\n"),
					crtc_id,
					mode_width, mode_height, mode_freq,
					output->priv->width, output->priv->height, output->priv->rate,
					pass);

		if (mode_width == output->priv->width	&&
		    mode_height == output->priv->height &&
		    (pass == 1 || mode_freq == output->priv->rate))
		{
		    tried_mode = TRUE;

		    my_error = NULL;
		    if (crtc_assignment_assign (
			    assignment, crtc, modes[j],
			    output->priv->x, output->priv->y,
			    output->priv->rotation,
                            output->priv->primary,
			    gnome_rr_output,
			    &my_error))
		    {
			my_error = NULL;
			if (real_assign_crtcs (screen, outputs + 1, assignment, &my_error)) {
			    success = TRUE;
			    goto out;
			} else
			    accumulate_error (accumulated_error, my_error);

			crtc_assignment_unassign (assignment, crtc, gnome_rr_output);
		    } else
			accumulate_error (accumulated_error, my_error);
		}
	    }
	}
    }

out:

    if (success)
	g_string_free (accumulated_error, TRUE);
    else {
	char *str;

	str = g_string_free (accumulated_error, FALSE);

	if (tried_mode)
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("could not assign CRTCs to outputs:\n%s"),
			 str);
	else
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_CRTC_ASSIGNMENT,
			 _("none of the selected modes were compatible with the possible modes:\n%s"),
			 str);

	g_free (str);
    }

    return success;
}

static void
crtc_info_free (CrtcInfo *info)
{
    g_ptr_array_free (info->outputs, TRUE);
    g_free (info);
}

static void
get_required_virtual_size (CrtcAssignment *assign, int *width, int *height)
{
    GList *active_crtcs = g_hash_table_get_keys (assign->info);
    GList *list;
    int d;

    if (!width)
	width = &d;
    if (!height)
	height = &d;

    /* Compute size of the screen */
    *width = *height = 1;
    for (list = active_crtcs; list != NULL; list = list->next)
    {
	GnomeRRCrtc *crtc = list->data;
	CrtcInfo *info = g_hash_table_lookup (assign->info, crtc);
	int w, h;

	w = gnome_rr_mode_get_width (info->mode);
	h = gnome_rr_mode_get_height (info->mode);

	if (mode_is_rotated (info))
	{
	    int tmp = h;
	    h = w;
	    w = tmp;
	}

	*width = MAX (*width, info->x + w);
	*height = MAX (*height, info->y + h);
    }

    g_list_free (active_crtcs);
}

static CrtcAssignment *
crtc_assignment_new (GnomeRRScreen *screen, GnomeRROutputInfo **outputs, GError **error)
{
    CrtcAssignment *assignment = g_new0 (CrtcAssignment, 1);

    assignment->info = g_hash_table_new_full (
	g_direct_hash, g_direct_equal, NULL, (GFreeFunc)crtc_info_free);

    if (real_assign_crtcs (screen, outputs, assignment, error))
    {
	int width, height;
	int min_width, max_width, min_height, max_height;
	int required_pixels, min_pixels, max_pixels;

	get_required_virtual_size (assignment, &width, &height);

	gnome_rr_screen_get_ranges (
	    screen, &min_width, &max_width, &min_height, &max_height);

	required_pixels = width * height;
	min_pixels = min_width * min_height;
	max_pixels = max_width * max_height;

	if (required_pixels < min_pixels || required_pixels > max_pixels)
	{
	    g_set_error (error, GNOME_RR_ERROR, GNOME_RR_ERROR_BOUNDS_ERROR,
			 /* Translators: the "requested", "minimum", and
			  * "maximum" words here are not keywords; please
			  * translate them as usual. */
			 _("required virtual size does not fit available size: "
			   "requested=(%d, %d), minimum=(%d, %d), maximum=(%d, %d)"),
			 width, height,
			 min_width, min_height,
			 max_width, max_height);
	    goto fail;
	}

	assignment->screen = screen;

	return assignment;
    }

fail:
    crtc_assignment_free (assignment);

    return NULL;
}

static gboolean
crtc_assignment_apply (CrtcAssignment *assign, guint32 timestamp, GError **error)
{
    GnomeRRCrtc **all_crtcs = gnome_rr_screen_list_crtcs (assign->screen);
    int width, height;
    int i;
    int min_width, max_width, min_height, max_height;
    int width_mm, height_mm;
    gboolean success = TRUE;

    /* Compute size of the screen */
    get_required_virtual_size (assign, &width, &height);

    gnome_rr_screen_get_ranges (
	assign->screen, &min_width, &max_width, &min_height, &max_height);

    /* We should never get here if the dimensions don't fit in the virtual size,
     * but just in case we do, fix it up.
     */
    width = MAX (min_width, width);
    width = MIN (max_width, width);
    height = MAX (min_height, height);
    height = MIN (max_height, height);

    /* FMQ: do we need to check the sizes instead of clamping them? */

    /* Grab the server while we fiddle with the CRTCs and the screen, so that
     * apps that listen for RANDR notifications will only receive the final
     * status.
     */
#ifdef HAVE_X11
    gdk_x11_display_grab (gdk_screen_get_display (assign->screen->priv->gdk_screen));
#endif

    /* Turn off all crtcs that are currently displaying outside the new screen,
     * or are not used in the new setup
     */
    for (i = 0; all_crtcs[i] != NULL; ++i)
    {
	GnomeRRCrtc *crtc = all_crtcs[i];
	GnomeRRMode *mode = gnome_rr_crtc_get_current_mode (crtc);
	int x, y;

	if (mode)
	{
	    int w, h;
	    gnome_rr_crtc_get_position (crtc, &x, &y);

	    w = gnome_rr_mode_get_width (mode);
	    h = gnome_rr_mode_get_height (mode);

	    if (crtc_is_rotated (crtc))
	    {
		int tmp = h;
		h = w;
		w = tmp;
	    }

	    if (x + w > width || y + h > height || !g_hash_table_lookup (assign->info, crtc))
	    {
		if (!gnome_rr_crtc_set_config_with_time (crtc, timestamp, 0, 0, NULL, GNOME_RR_ROTATION_0, NULL, 0, error))
		{
		    success = FALSE;
		    break;
		}

	    }
	}
    }

    /* The 'physical size' of an X screen is meaningless if that screen
     * can consist of many monitors. So just pick a size that make the
     * dpi 96.
     *
     * Firefox and Evince apparently believe what X tells them.
     */
    width_mm = (width / 96.0) * 25.4 + 0.5;
    height_mm = (height / 96.0) * 25.4 + 0.5;

    if (success)
    {
	ConfigureCrtcState state;

	gnome_rr_screen_set_size (assign->screen, width, height, width_mm, height_mm);

	state.timestamp = timestamp;
	state.has_error = FALSE;
	state.error = error;

	g_hash_table_foreach (assign->info, configure_crtc, &state);

	success = !state.has_error;
    }

    gnome_rr_screen_set_primary_output (assign->screen, assign->primary);

#ifdef HAVE_X11
    gdk_x11_display_ungrab (gdk_screen_get_display (assign->screen->priv->gdk_screen));
#endif
    return success;
}
