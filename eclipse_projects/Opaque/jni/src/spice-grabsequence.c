/*
 * GTK VNC Widget
 *
 * Copyright (C) 2010 Daniel P. Berrange <dan@berrange.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <config.h>

#include <string.h>
#include <gdk/gdk.h>

#include "spice-grabsequence.h"

GType spice_grab_sequence_get_type(void)
{
	static GType grab_sequence_type = 0;
	static volatile gsize grab_sequence_type_volatile;

	if (g_once_init_enter(&grab_sequence_type_volatile)) {
		grab_sequence_type = g_boxed_type_register_static
			("SpiceGrabSequence",
			 (GBoxedCopyFunc)spice_grab_sequence_copy,
			 (GBoxedFreeFunc)spice_grab_sequence_free);
		g_once_init_leave(&grab_sequence_type_volatile,
				  grab_sequence_type);
	}

	return grab_sequence_type;
}


/**
 * spice_grab_sequence_new:
 * @nkeysyms: length of @keysyms
 * @keysyms: (array length=nkeysyms): the keysym values
 *
 * Creates a new grab sequence from a list of keysym values
 *
 * Returns: (transfer full): a new grab sequence object
 */
SpiceGrabSequence *spice_grab_sequence_new(guint nkeysyms, guint *keysyms)
{
	SpiceGrabSequence *sequence;

	sequence = g_slice_new0(SpiceGrabSequence);
	sequence->nkeysyms = nkeysyms;
	sequence->keysyms = g_new0(guint, nkeysyms);
	memcpy(sequence->keysyms, keysyms, sizeof(guint)*nkeysyms);

	return sequence;
}


/**
 * spice_grab_sequence_new_from_string:
 * @str: a string of '+' seperated key names (ex: "Control_L+Alt_L")
 *
 * Returns: a new #SpiceGrabSequence.
 **/
SpiceGrabSequence *spice_grab_sequence_new_from_string(const gchar *str)
{
	gchar **keysymstr;
	int i;
	SpiceGrabSequence *sequence;

	sequence = g_slice_new0(SpiceGrabSequence);

	keysymstr = g_strsplit(str, "+", 5);

	sequence->nkeysyms = 0;
	while (keysymstr[sequence->nkeysyms])
		sequence->nkeysyms++;

	sequence->keysyms = g_new0(guint, sequence->nkeysyms);
	for (i = 0 ; i < sequence->nkeysyms ; i++) {
		sequence->keysyms[i] =
			(guint)gdk_keyval_from_name(keysymstr[i]);
                if (sequence->keysyms[i] == 0) {
                        g_critical("Invalid key: %s", keysymstr[i]);
                }
        }
	g_strfreev(keysymstr);

	return sequence;

}


/**
 * spice_grab_sequence_copy:
 * @sequence: sequence to copy
 *
 * Returns: (transfer full): a copy of @sequence
 **/
SpiceGrabSequence *spice_grab_sequence_copy(SpiceGrabSequence *srcSequence)
{
	SpiceGrabSequence *sequence;

	sequence = g_slice_dup(SpiceGrabSequence, srcSequence);
	sequence->keysyms = g_new0(guint, srcSequence->nkeysyms);
	memcpy(sequence->keysyms, srcSequence->keysyms,
	       sizeof(guint) * sequence->nkeysyms);

	return sequence;
}


/**
 * spice_grab_sequence_free:
 * @sequence:
 *
 * Free @sequence.
 **/
void spice_grab_sequence_free(SpiceGrabSequence *sequence)
{
	g_free(sequence->keysyms);
	g_slice_free(SpiceGrabSequence, sequence);
}


/**
 * spice_grab_sequence_as_string:
 * @sequence:
 *
 * Returns: (transfer full): a newly allocated string representing the key sequence
 **/
gchar *spice_grab_sequence_as_string(SpiceGrabSequence *sequence)
{
	GString *str = g_string_new("");
	int i;

	for (i = 0 ; i < sequence->nkeysyms ; i++) {
		if (i > 0)
			g_string_append_c(str, '+');
		g_string_append(str, gdk_keyval_name(sequence->keysyms[i]));
	}

	return g_string_free(str, FALSE);

}


/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
