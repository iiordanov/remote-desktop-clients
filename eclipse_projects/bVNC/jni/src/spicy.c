/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010-2011 Red Hat, Inc.

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <glib/gi18n.h>

#include <sys/stat.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef USE_SMARTCARD
#include <vreader.h>
#include "smartcard-manager.h"
#endif

#include "glib-compat.h"
#include "spice-widget.h"
#include "spice-gtk-session.h"
#include "spice-audio.h"
#include "spice-common.h"
#include "spice-cmdline.h"
#include "spice-option.h"
#include "usb-device-widget.h"

typedef struct spice_connection spice_connection;

enum {
    STATE_SCROLL_LOCK,
    STATE_CAPS_LOCK,
    STATE_NUM_LOCK,
    STATE_MAX,
};

#define SPICE_TYPE_WINDOW                  (spice_window_get_type ())
#define SPICE_WINDOW(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPICE_TYPE_WINDOW, SpiceWindow))
#define SPICE_IS_WINDOW(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPICE_TYPE_WINDOW))
#define SPICE_WINDOW_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), SPICE_TYPE_WINDOW, SpiceWindowClass))
#define SPICE_IS_WINDOW_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), SPICE_TYPE_WINDOW))
#define SPICE_WINDOW_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), SPICE_TYPE_WINDOW, SpiceWindowClass))

typedef struct _SpiceWindow SpiceWindow;
typedef struct _SpiceWindowClass SpiceWindowClass;

struct _SpiceWindow {
    GObject          object;
    spice_connection *conn;
    gint             id;
    gint             monitor_id;
    GtkWidget        *toplevel, *spice;
    GtkWidget        *menubar, *toolbar;
    GtkWidget        *ritem, *rmenu;
    GtkWidget        *statusbar, *status, *st[STATE_MAX];
    GtkActionGroup   *ag;
    GtkUIManager     *ui;
    bool             fullscreen;
    bool             mouse_grabbed;
    SpiceChannel     *display_channel;
#ifdef WIN32
    gint             win_x;
    gint             win_y;
#endif
    bool             enable_accels_save;
    bool             enable_mnemonics_save;
};

struct _SpiceWindowClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (SpiceWindow, spice_window, G_TYPE_OBJECT);

#define CHANNELID_MAX 4
#define MONITORID_MAX 4

// FIXME: turn this into an object, get rid of fixed wins array, use
// signals to replace the various callback that iterate over wins array
struct spice_connection {
    SpiceSession     *session;
    SpiceGtkSession  *gtk_session;
    SpiceMainChannel *main;
    SpiceWindow     *wins[CHANNELID_MAX * MONITORID_MAX];
    SpiceAudio       *audio;
    const char       *mouse_state;
    const char       *agent_state;
    gboolean         agent_connected;
    int              channels;
    int              disconnecting;
};

static spice_connection *connection_new(void);
static void connection_connect(spice_connection *conn);
static void connection_disconnect(spice_connection *conn);
static void connection_destroy(spice_connection *conn);
static void usb_connect_failed(GObject               *object,
                               SpiceUsbDevice        *device,
                               GError                *error,
                               gpointer               data);
static gboolean is_gtk_session_property(const gchar *property);
static void del_window(spice_connection *conn, SpiceWindow *win);

/* options */
static gboolean fullscreen = false;
static gboolean version = false;
static char *spicy_title = NULL;
/* globals */
static GMainLoop     *mainloop = NULL;
static int           connections = 0;
static GKeyFile      *keyfile = NULL;
static SpicePortChannel*stdin_port = NULL;

/* ------------------------------------------------------------------ */

static int ask_user(GtkWidget *parent, char *title, char *message,
                    char *dest, int dlen, int hide)
{
    GtkWidget *dialog, *area, *label, *entry;
    const char *txt;
    int retval;

    /* Create the widgets */
    dialog = gtk_dialog_new_with_buttons(title,
                                         parent ? GTK_WINDOW(parent) : NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK,
                                         GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_REJECT,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    label = gtk_label_new(message);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(area), label, FALSE, FALSE, 5);

    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), dest);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    if (hide)
        gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    gtk_box_pack_start(GTK_BOX(area), entry, FALSE, FALSE, 5);

    /* show and wait for response */
    gtk_widget_show_all(dialog);
    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
    case GTK_RESPONSE_ACCEPT:
        txt = gtk_entry_get_text(GTK_ENTRY(entry));
        snprintf(dest, dlen, "%s", txt);
        retval = 0;
        break;
    default:
        retval = -1;
        break;
    }
    gtk_widget_destroy(dialog);
    return retval;
}

static struct {
    const char *text;
    const char *prop;
    GtkWidget *entry;
} connect_entries[] = {
    { .text = N_("Hostname"),   .prop = "host"      },
    { .text = N_("Port"),       .prop = "port"      },
    { .text = N_("TLS Port"),   .prop = "tls-port"  },
};

#ifndef WIN32
static void recent_selection_changed_dialog_cb(GtkRecentChooser *chooser, gpointer data)
{
    GtkRecentInfo *info;
    gchar *txt = NULL;
    const gchar *uri;
    SpiceSession *session = data;

    info = gtk_recent_chooser_get_current_item(chooser);
    if (info == NULL)
        return;

    uri = gtk_recent_info_get_uri(info);
    g_return_if_fail(uri != NULL);

    g_object_set(session, "uri", uri, NULL);

    g_object_get(session, "host", &txt, NULL);
    gtk_entry_set_text(GTK_ENTRY(connect_entries[0].entry), txt ? txt : "");
    g_free(txt);

    g_object_get(session, "port", &txt, NULL);
    gtk_entry_set_text(GTK_ENTRY(connect_entries[1].entry), txt ? txt : "");
    g_free(txt);

    g_object_get(session, "tls-port", &txt, NULL);
    gtk_entry_set_text(GTK_ENTRY(connect_entries[2].entry), txt ? txt : "");
    g_free(txt);

    gtk_recent_info_unref(info);
}

static void recent_item_activated_dialog_cb(GtkRecentChooser *chooser, gpointer data)
{
   gtk_dialog_response (GTK_DIALOG (data), GTK_RESPONSE_ACCEPT);
}
#endif

static int connect_dialog(SpiceSession *session)
{
    GtkWidget *dialog, *area, *label;
    GtkTable *table;
    int i, retval;

    /* Create the widgets */
    dialog = gtk_dialog_new_with_buttons(_("Connect to SPICE"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_REJECT,
                                         GTK_STOCK_CONNECT,
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    table = GTK_TABLE(gtk_table_new(3, 2, 0));
    gtk_box_pack_start(GTK_BOX(area), GTK_WIDGET(table), TRUE, TRUE, 0);
    gtk_table_set_row_spacings(table, 5);
    gtk_table_set_col_spacings(table, 5);

    for (i = 0; i < SPICE_N_ELEMENTS(connect_entries); i++) {
        gchar *txt;
        label = gtk_label_new(connect_entries[i].text);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        gtk_table_attach_defaults(table, label, 0, 1, i, i+1);
        connect_entries[i].entry = GTK_WIDGET(gtk_entry_new());
        gtk_table_attach_defaults(table, connect_entries[i].entry, 1, 2, i, i+1);
        g_object_get(session, connect_entries[i].prop, &txt, NULL);
        SPICE_DEBUG("%s: #%i [%s]: \"%s\"",
                __FUNCTION__, i, connect_entries[i].prop, txt);
        if (txt) {
            gtk_entry_set_text(GTK_ENTRY(connect_entries[i].entry), txt);
            g_free(txt);
        }
    }

    label = gtk_label_new("Recent connections:");
    gtk_box_pack_start(GTK_BOX(area), label, TRUE, TRUE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
#ifndef WIN32
    GtkRecentFilter *rfilter;
    GtkWidget *recent;

    recent = GTK_WIDGET(gtk_recent_chooser_widget_new());
    gtk_recent_chooser_set_show_icons(GTK_RECENT_CHOOSER(recent), FALSE);
    gtk_box_pack_start(GTK_BOX(area), recent, TRUE, TRUE, 0);

    rfilter = gtk_recent_filter_new();
    gtk_recent_filter_add_mime_type(rfilter, "application/x-spice");
    gtk_recent_chooser_set_filter(GTK_RECENT_CHOOSER(recent), rfilter);
    gtk_recent_chooser_set_local_only(GTK_RECENT_CHOOSER(recent), FALSE);
    g_signal_connect(recent, "selection-changed",
                     G_CALLBACK(recent_selection_changed_dialog_cb), session);
    g_signal_connect(recent, "item-activated",
                     G_CALLBACK(recent_item_activated_dialog_cb), dialog);
#endif
    /* show and wait for response */
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        for (i = 0; i < SPICE_N_ELEMENTS(connect_entries); i++) {
            const gchar *txt;
            txt = gtk_entry_get_text(GTK_ENTRY(connect_entries[i].entry));
            g_object_set(session, connect_entries[i].prop, txt, NULL);
        }
        retval = 0;
    } else
        retval = -1;
    gtk_widget_destroy(dialog);
    return retval;
}

/* ------------------------------------------------------------------ */

static void update_status_window(SpiceWindow *win)
{
    gchar *status;

    if (win == NULL)
        return;

    if (win->mouse_grabbed) {
        SpiceGrabSequence *sequence = spice_display_get_grab_keys(SPICE_DISPLAY(win->spice));
        gchar *seq = spice_grab_sequence_as_string(sequence);
        status = g_strdup_printf(_("Use %s to ungrab mouse."), seq);
        g_free(seq);
    } else {
        status = g_strdup_printf(_("mouse: %s, agent: %s"),
                 win->conn->mouse_state, win->conn->agent_state);
    }

    gtk_label_set_text(GTK_LABEL(win->status), status);
    g_free(status);
}

static void update_status(struct spice_connection *conn)
{
    int i;

    for (i = 0; i < SPICE_N_ELEMENTS(conn->wins); i++) {
        if (conn->wins[i] == NULL)
            continue;
        update_status_window(conn->wins[i]);
    }
}

static const char *spice_edit_properties[] = {
    "CopyToGuest",
    "PasteFromGuest",
};

static void update_edit_menu_window(SpiceWindow *win)
{
    int i;
    GtkAction *toggle;

    if (win == NULL) {
        return;
    }

    /* Make "CopyToGuest" and "PasteFromGuest" insensitive if spice
     * agent is not connected */
    for (i = 0; i < G_N_ELEMENTS(spice_edit_properties); i++) {
        toggle = gtk_action_group_get_action(win->ag, spice_edit_properties[i]);
        if (toggle) {
            gtk_action_set_sensitive(toggle, win->conn->agent_connected);
        }
    }
}

static void update_edit_menu(struct spice_connection *conn)
{
    int i;

    for (i = 0; i < SPICE_N_ELEMENTS(conn->wins); i++) {
        if (conn->wins[i]) {
            update_edit_menu_window(conn->wins[i]);
        }
    }
}

static void menu_cb_connect(GtkAction *action, void *data)
{
    struct spice_connection *conn;

    conn = connection_new();
    connection_connect(conn);
}

static void menu_cb_close(GtkAction *action, void *data)
{
    SpiceWindow *win = data;

    connection_disconnect(win->conn);
}

static void menu_cb_copy(GtkAction *action, void *data)
{
    SpiceWindow *win = data;

    spice_gtk_session_copy_to_guest(win->conn->gtk_session);
}

static void menu_cb_paste(GtkAction *action, void *data)
{
    SpiceWindow *win = data;

    spice_gtk_session_paste_from_guest(win->conn->gtk_session);
}

static void window_set_fullscreen(SpiceWindow *win, gboolean fs)
{
    if (fs) {
#ifdef WIN32
        gtk_window_get_position(GTK_WINDOW(win->toplevel), &win->win_x, &win->win_y);
#endif
        gtk_window_fullscreen(GTK_WINDOW(win->toplevel));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(win->toplevel));
#ifdef WIN32
        gtk_window_move(GTK_WINDOW(win->toplevel), win->win_x, win->win_y);
#endif
    }
}

static void menu_cb_fullscreen(GtkAction *action, void *data)
{
    SpiceWindow *win = data;

    window_set_fullscreen(win, !win->fullscreen);
}

#ifdef USE_SMARTCARD
static void enable_smartcard_actions(SpiceWindow *win, VReader *reader,
                                     gboolean can_insert, gboolean can_remove)
{
    GtkAction *action;

    if ((reader != NULL) && (!spice_smartcard_reader_is_software((SpiceSmartcardReader*)reader)))
    {
        /* Having menu actions to insert/remove smartcards only makes sense
         * for software smartcard readers, don't do anything when the event
         * we received was for a "real" smartcard reader.
         */
        return;
    }
    action = gtk_action_group_get_action(win->ag, "InsertSmartcard");
    g_return_if_fail(action != NULL);
    gtk_action_set_sensitive(action, can_insert);
    action = gtk_action_group_get_action(win->ag, "RemoveSmartcard");
    g_return_if_fail(action != NULL);
    gtk_action_set_sensitive(action, can_remove);
}


static void reader_added_cb(SpiceSmartcardManager *manager, VReader *reader,
                            gpointer user_data)
{
    enable_smartcard_actions(user_data, reader, TRUE, FALSE);
}

static void reader_removed_cb(SpiceSmartcardManager *manager, VReader *reader,
                              gpointer user_data)
{
    enable_smartcard_actions(user_data, reader, FALSE, FALSE);
}

static void card_inserted_cb(SpiceSmartcardManager *manager, VReader *reader,
                             gpointer user_data)
{
    enable_smartcard_actions(user_data, reader, FALSE, TRUE);
}

static void card_removed_cb(SpiceSmartcardManager *manager, VReader *reader,
                            gpointer user_data)
{
    enable_smartcard_actions(user_data, reader, TRUE, FALSE);
}

static void menu_cb_insert_smartcard(GtkAction *action, void *data)
{
    spice_smartcard_manager_insert_card(spice_smartcard_manager_get());
}

static void menu_cb_remove_smartcard(GtkAction *action, void *data)
{
    spice_smartcard_manager_remove_card(spice_smartcard_manager_get());
}
#endif

#ifdef USE_USBREDIR
static void remove_cb(GtkContainer *container, GtkWidget *widget, void *data)
{
    gtk_window_resize(GTK_WINDOW(data), 1, 1);
}

static void menu_cb_select_usb_devices(GtkAction *action, void *data)
{
    GtkWidget *dialog, *area, *usb_device_widget;
    SpiceWindow *win = data;

    /* Create the widgets */
    dialog = gtk_dialog_new_with_buttons(
                    _("Select USB devices for redirection"),
                    GTK_WINDOW(win->toplevel),
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                    NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
    gtk_box_set_spacing(GTK_BOX(gtk_bin_get_child(GTK_BIN(dialog))), 12);

    area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    usb_device_widget = spice_usb_device_widget_new(win->conn->session,
                                                    NULL); /* default format */
    g_signal_connect(usb_device_widget, "connect-failed",
                     G_CALLBACK(usb_connect_failed), NULL);
    gtk_box_pack_start(GTK_BOX(area), usb_device_widget, TRUE, TRUE, 0);

    /* This shrinks the dialog when USB devices are unplugged */
    g_signal_connect(usb_device_widget, "remove",
                     G_CALLBACK(remove_cb), dialog);

    /* show and run */
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
#endif

static void menu_cb_bool_prop(GtkToggleAction *action, gpointer data)
{
    SpiceWindow *win = data;
    gboolean state = gtk_toggle_action_get_active(action);
    const char *name;
    gpointer object;

    name = gtk_action_get_name(GTK_ACTION(action));
    SPICE_DEBUG("%s: %s = %s", __FUNCTION__, name, state ? _("yes") : _("no"));

    g_key_file_set_boolean(keyfile, "general", name, state);

    if (is_gtk_session_property(name)) {
        object = win->conn->gtk_session;
    } else {
        object = win->spice;
    }
    g_object_set(object, name, state, NULL);
}

static void menu_cb_conn_bool_prop_changed(GObject    *gobject,
                                           GParamSpec *pspec,
                                           gpointer    user_data)
{
    SpiceWindow *win = user_data;
    const gchar *property = g_param_spec_get_name(pspec);
    GtkAction *toggle;
    gboolean state;

    toggle = gtk_action_group_get_action(win->ag, property);
    g_object_get(win->conn->gtk_session, property, &state, NULL);
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(toggle), state);
}

static void menu_cb_toolbar(GtkToggleAction *action, gpointer data)
{
    SpiceWindow *win = data;
    gboolean state = gtk_toggle_action_get_active(action);

    gtk_widget_set_visible(win->toolbar, state);
    g_key_file_set_boolean(keyfile, "ui", "toolbar", state);
}

static void menu_cb_statusbar(GtkToggleAction *action, gpointer data)
{
    SpiceWindow *win = data;
    gboolean state = gtk_toggle_action_get_active(action);

    gtk_widget_set_visible(win->statusbar, state);
    g_key_file_set_boolean(keyfile, "ui", "statusbar", state);
}

static void menu_cb_about(GtkAction *action, void *data)
{
    char *comments = _("gtk test client app for the\n"
        "spice remote desktop protocol");
    static const char *copyright = "(c) 2010 Red Hat";
    static const char *website = "http://www.spice-space.org";
    static const char *authors[] = { "Gerd Hoffmann <kraxel@redhat.com>",
                               "Marc-André Lureau <marcandre.lureau@redhat.com>",
                               NULL };
    SpiceWindow *win = data;

    gtk_show_about_dialog(GTK_WINDOW(win->toplevel),
                          "authors",         authors,
                          "comments",        comments,
                          "copyright",       copyright,
                          "logo-icon-name",  GTK_STOCK_ABOUT,
                          "website",         website,
                          "version",         PACKAGE_VERSION,
                          "license",         "LGPLv2.1",
                          NULL);
}

static gboolean delete_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    SpiceWindow *win = data;

    if (win->monitor_id == 0)
        connection_disconnect(win->conn);
    else
        del_window(win->conn, win);

    return true;
}

static gboolean window_state_cb(GtkWidget *widget, GdkEventWindowState *event,
                                gpointer data)
{
    SpiceWindow *win = data;
    if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
        win->fullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;
        if (win->fullscreen) {
            gtk_widget_hide(win->menubar);
            gtk_widget_hide(win->toolbar);
            gtk_widget_hide(win->statusbar);
            gtk_widget_grab_focus(win->spice);
        } else {
            gboolean state;
            GtkAction *toggle;

            gtk_widget_show(win->menubar);
            toggle = gtk_action_group_get_action(win->ag, "Toolbar");
            state = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(toggle));
            gtk_widget_set_visible(win->toolbar, state);
            toggle = gtk_action_group_get_action(win->ag, "Statusbar");
            state = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(toggle));
            gtk_widget_set_visible(win->statusbar, state);
        }
    }
    return TRUE;
}

static void grab_keys_pressed_cb(GtkWidget *widget, gpointer data)
{
    SpiceWindow *win = data;

    /* since mnemonics are disabled, we leave fullscreen when
       ungrabbing mouse. Perhaps we should have a different handling
       of fullscreen key, or simply use a UI, like vinagre */
    window_set_fullscreen(win, FALSE);
}

static void mouse_grab_cb(GtkWidget *widget, gint grabbed, gpointer data)
{
    SpiceWindow *win = data;

    win->mouse_grabbed = grabbed;
    update_status(win->conn);
}

static void keyboard_grab_cb(GtkWidget *widget, gint grabbed, gpointer data)
{
    SpiceWindow *win = data;
    GtkSettings *settings = gtk_widget_get_settings (widget);

    if (grabbed) {
        /* disable mnemonics & accels */
        g_object_get(settings,
                     "gtk-enable-accels", &win->enable_accels_save,
                     "gtk-enable-mnemonics", &win->enable_mnemonics_save,
                     NULL);
        g_object_set(settings,
                     "gtk-enable-accels", FALSE,
                     "gtk-enable-mnemonics", FALSE,
                     NULL);
    } else {
        g_object_set(settings,
                     "gtk-enable-accels", win->enable_accels_save,
                     "gtk-enable-mnemonics", win->enable_mnemonics_save,
                     NULL);
    }
}

static void restore_configuration(SpiceWindow *win)
{
    gboolean state;
    gchar *str;
    gchar **keys = NULL;
    gsize nkeys, i;
    GError *error = NULL;
    gpointer object;

    keys = g_key_file_get_keys(keyfile, "general", &nkeys, &error);
    if (error != NULL) {
        if (error->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND)
            g_warning("Failed to read configuration file keys: %s", error->message);
        g_clear_error(&error);
        return;
    }

    if (nkeys > 0)
        g_return_if_fail(keys != NULL);

    for (i = 0; i < nkeys; ++i) {
        if (g_str_equal(keys[i], "grab-sequence"))
            continue;
        state = g_key_file_get_boolean(keyfile, "general", keys[i], &error);
        if (error != NULL) {
            g_clear_error(&error);
            continue;
        }

        if (is_gtk_session_property(keys[i])) {
            object = win->conn->gtk_session;
        } else {
            object = win->spice;
        }
        g_object_set(object, keys[i], state, NULL);
    }

    g_strfreev(keys);

    str = g_key_file_get_string(keyfile, "general", "grab-sequence", &error);
    if (error == NULL) {
        SpiceGrabSequence *seq = spice_grab_sequence_new_from_string(str);
        spice_display_set_grab_keys(SPICE_DISPLAY(win->spice), seq);
        spice_grab_sequence_free(seq);
        g_free(str);
    }
    g_clear_error(&error);


    state = g_key_file_get_boolean(keyfile, "ui", "toolbar", &error);
    if (error == NULL)
        gtk_widget_set_visible(win->toolbar, state);
    g_clear_error(&error);

    state = g_key_file_get_boolean(keyfile, "ui", "statusbar", &error);
    if (error == NULL)
        gtk_widget_set_visible(win->statusbar, state);
    g_clear_error(&error);
}

/* ------------------------------------------------------------------ */

static const GtkActionEntry entries[] = {
    {
        .name        = "FileMenu",
        .label       = "_File",
    },{
        .name        = "FileRecentMenu",
        .label       = "_Recent",
    },{
        .name        = "EditMenu",
        .label       = "_Edit",
    },{
        .name        = "ViewMenu",
        .label       = "_View",
    },{
        .name        = "InputMenu",
        .label       = "_Input",
    },{
        .name        = "OptionMenu",
        .label       = "_Options",
    },{
        .name        = "HelpMenu",
        .label       = "_Help",
    },{

        /* File menu */
        .name        = "Connect",
        .stock_id    = GTK_STOCK_CONNECT,
        .label       = N_("_Connect ..."),
        .callback    = G_CALLBACK(menu_cb_connect),
    },{
        .name        = "Close",
        .stock_id    = GTK_STOCK_CLOSE,
        .label       = N_("_Close"),
        .callback    = G_CALLBACK(menu_cb_close),
        .accelerator = "", /* none (disable default "<control>W") */
    },{

        /* Edit menu */
        .name        = "CopyToGuest",
        .stock_id    = GTK_STOCK_COPY,
        .label       = N_("_Copy to guest"),
        .callback    = G_CALLBACK(menu_cb_copy),
        .accelerator = "", /* none (disable default "<control>C") */
    },{
        .name        = "PasteFromGuest",
        .stock_id    = GTK_STOCK_PASTE,
        .label       = N_("_Paste from guest"),
        .callback    = G_CALLBACK(menu_cb_paste),
        .accelerator = "", /* none (disable default "<control>V") */
    },{

        /* View menu */
        .name        = "Fullscreen",
        .stock_id    = GTK_STOCK_FULLSCREEN,
        .label       = N_("_Fullscreen"),
        .callback    = G_CALLBACK(menu_cb_fullscreen),
        .accelerator = "<shift>F11",
    },{
#ifdef USE_SMARTCARD
	.name        = "InsertSmartcard",
	.label       = N_("_Insert Smartcard"),
	.callback    = G_CALLBACK(menu_cb_insert_smartcard),
        .accelerator = "<shift>F8",
    },{
	.name        = "RemoveSmartcard",
	.label       = N_("_Remove Smartcard"),
	.callback    = G_CALLBACK(menu_cb_remove_smartcard),
        .accelerator = "<shift>F9",
    },{
#endif

#ifdef USE_USBREDIR
        .name        = "SelectUsbDevices",
        .label       = N_("_Select USB Devices for redirection"),
        .callback    = G_CALLBACK(menu_cb_select_usb_devices),
        .accelerator = "<shift>F10",
    },{
#endif

        /* Help menu */
        .name        = "About",
        .stock_id    = GTK_STOCK_ABOUT,
        .label       = N_("_About ..."),
        .callback    = G_CALLBACK(menu_cb_about),
    }
};

static const char *spice_display_properties[] = {
    "grab-keyboard",
    "grab-mouse",
    "resize-guest",
    "scaling",
    "disable-inputs",
};

static const char *spice_gtk_session_properties[] = {
    "auto-clipboard",
    "auto-usbredir",
};

static const GtkToggleActionEntry tentries[] = {
    {
        .name        = "grab-keyboard",
        .label       = N_("Grab keyboard when active and focused"),
        .callback    = G_CALLBACK(menu_cb_bool_prop),
    },{
        .name        = "grab-mouse",
        .label       = N_("Grab mouse in server mode (no tabled/vdagent)"),
        .callback    = G_CALLBACK(menu_cb_bool_prop),
    },{
        .name        = "resize-guest",
        .label       = N_("Resize guest to match window size"),
        .callback    = G_CALLBACK(menu_cb_bool_prop),
    },{
        .name        = "scaling",
        .label       = N_("Scale display"),
        .callback    = G_CALLBACK(menu_cb_bool_prop),
    },{
        .name        = "disable-inputs",
        .label       = N_("Disable inputs"),
        .callback    = G_CALLBACK(menu_cb_bool_prop),
    },{
        .name        = "auto-clipboard",
        .label       = N_("Automagic clipboard sharing between host and guest"),
        .callback    = G_CALLBACK(menu_cb_bool_prop),
    },{
        .name        = "auto-usbredir",
        .label       = N_("Auto redirect newly plugged in USB devices"),
        .callback    = G_CALLBACK(menu_cb_bool_prop),
    },{
        .name        = "Statusbar",
        .label       = N_("Statusbar"),
        .callback    = G_CALLBACK(menu_cb_statusbar),
    },{
        .name        = "Toolbar",
        .label       = N_("Toolbar"),
        .callback    = G_CALLBACK(menu_cb_toolbar),
    }
};

static char ui_xml[] =
"<ui>\n"
"  <menubar action='MainMenu'>\n"
"    <menu action='FileMenu'>\n"
"      <menuitem action='Connect'/>\n"
"      <menu action='FileRecentMenu'/>\n"
"      <separator/>\n"
"      <menuitem action='Close'/>\n"
"    </menu>\n"
"    <menu action='EditMenu'>\n"
"      <menuitem action='CopyToGuest'/>\n"
"      <menuitem action='PasteFromGuest'/>\n"
"    </menu>\n"
"    <menu action='ViewMenu'>\n"
"      <menuitem action='Fullscreen'/>\n"
"      <menuitem action='Toolbar'/>\n"
"      <menuitem action='Statusbar'/>\n"
"    </menu>\n"
"    <menu action='InputMenu'>\n"
#ifdef USE_SMARTCARD
"      <menuitem action='InsertSmartcard'/>\n"
"      <menuitem action='RemoveSmartcard'/>\n"
#endif
#ifdef USE_USBREDIR
"      <menuitem action='SelectUsbDevices'/>\n"
#endif
"    </menu>\n"
"    <menu action='OptionMenu'>\n"
"      <menuitem action='grab-keyboard'/>\n"
"      <menuitem action='grab-mouse'/>\n"
"      <menuitem action='resize-guest'/>\n"
"      <menuitem action='scaling'/>\n"
"      <menuitem action='disable-inputs'/>\n"
"      <menuitem action='auto-clipboard'/>\n"
"      <menuitem action='auto-usbredir'/>\n"
"    </menu>\n"
"    <menu action='HelpMenu'>\n"
"      <menuitem action='About'/>\n"
"    </menu>\n"
"  </menubar>\n"
"  <toolbar action='ToolBar'>\n"
"    <toolitem action='Close'/>\n"
"    <separator/>\n"
"    <toolitem action='CopyToGuest'/>\n"
"    <toolitem action='PasteFromGuest'/>\n"
"    <separator/>\n"
"    <toolitem action='Fullscreen'/>\n"
"  </toolbar>\n"
"</ui>\n";

static gboolean is_gtk_session_property(const gchar *property)
{
    int i;

    for (i = 0; i < G_N_ELEMENTS(spice_gtk_session_properties); i++) {
        if (!strcmp(spice_gtk_session_properties[i], property)) {
            return TRUE;
        }
    }
    return FALSE;
}

#ifndef WIN32
static void recent_item_activated_cb(GtkRecentChooser *chooser, gpointer data)
{
    GtkRecentInfo *info;
    struct spice_connection *conn;
    const char *uri;

    info = gtk_recent_chooser_get_current_item(chooser);

    uri = gtk_recent_info_get_uri(info);
    g_return_if_fail(uri != NULL);

    conn = connection_new();
    g_object_set(conn->session, "uri", uri, NULL);
    gtk_recent_info_unref(info);
    connection_connect(conn);
}
#endif

static gboolean configure_event_cb(GtkWidget         *widget,
                                   GdkEventConfigure *event,
                                   gpointer           data)
{
    gboolean resize_guest;
    SpiceWindow *win = data;

    g_return_val_if_fail(win != NULL, FALSE);
    g_return_val_if_fail(win->conn != NULL, FALSE);

    g_object_get(win->spice, "resize-guest", &resize_guest, NULL);
    if (resize_guest && win->conn->agent_connected)
        return FALSE;

    return FALSE;
}

static void
spice_window_class_init (SpiceWindowClass *klass)
{
}

static void
spice_window_init (SpiceWindow *self)
{
}

static SpiceWindow *create_spice_window(spice_connection *conn, SpiceChannel *channel, int id, gint monitor_id)
{
    char title[32];
    SpiceWindow *win;
    GtkAction *toggle;
    gboolean state;
    GtkWidget *vbox, *frame;
    GError *err = NULL;
    int i;
    SpiceGrabSequence *seq;

    win = g_object_new(SPICE_TYPE_WINDOW, NULL);
    win->id = id;
    win->monitor_id = monitor_id;
    win->conn = conn;
    win->display_channel = channel;

    /* toplevel */
    win->toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (spicy_title == NULL) {
        snprintf(title, sizeof(title), _("spice display %d:%d"), id, monitor_id);
    } else {
        snprintf(title, sizeof(title), "%s", spicy_title);
    }

    gtk_window_set_title(GTK_WINDOW(win->toplevel), title);
    g_signal_connect(G_OBJECT(win->toplevel), "window-state-event",
                     G_CALLBACK(window_state_cb), win);
    g_signal_connect(G_OBJECT(win->toplevel), "delete-event",
                     G_CALLBACK(delete_cb), win);

    /* menu + toolbar */
    win->ui = gtk_ui_manager_new();
    win->ag = gtk_action_group_new("MenuActions");
    gtk_action_group_add_actions(win->ag, entries, G_N_ELEMENTS(entries), win);
    gtk_action_group_add_toggle_actions(win->ag, tentries,
                                        G_N_ELEMENTS(tentries), win);
    gtk_ui_manager_insert_action_group(win->ui, win->ag, 0);
    gtk_window_add_accel_group(GTK_WINDOW(win->toplevel),
                               gtk_ui_manager_get_accel_group(win->ui));

    err = NULL;
    if (!gtk_ui_manager_add_ui_from_string(win->ui, ui_xml, -1, &err)) {
        g_warning("building menus failed: %s", err->message);
        g_error_free(err);
        exit(1);
    }
    win->menubar = gtk_ui_manager_get_widget(win->ui, "/MainMenu");
    win->toolbar = gtk_ui_manager_get_widget(win->ui, "/ToolBar");

    /* recent menu */
    win->ritem  = gtk_ui_manager_get_widget
        (win->ui, "/MainMenu/FileMenu/FileRecentMenu");

#ifndef WIN32
    GtkRecentFilter  *rfilter;

    win->rmenu = gtk_recent_chooser_menu_new();
    gtk_recent_chooser_set_show_icons(GTK_RECENT_CHOOSER(win->rmenu), FALSE);
    rfilter = gtk_recent_filter_new();
    gtk_recent_filter_add_mime_type(rfilter, "application/x-spice");
    gtk_recent_chooser_add_filter(GTK_RECENT_CHOOSER(win->rmenu), rfilter);
    gtk_recent_chooser_set_local_only(GTK_RECENT_CHOOSER(win->rmenu), FALSE);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(win->ritem), win->rmenu);
    g_signal_connect(win->rmenu, "item-activated",
                     G_CALLBACK(recent_item_activated_cb), win);
#endif

    /* spice display */
    win->spice = GTK_WIDGET(spice_display_new_with_monitor(conn->session, id, monitor_id));
    g_signal_connect(win->spice, "configure-event", G_CALLBACK(configure_event_cb), win);
    seq = spice_grab_sequence_new_from_string("Shift_L+F12");
    spice_display_set_grab_keys(SPICE_DISPLAY(win->spice), seq);
    spice_grab_sequence_free(seq);

    g_signal_connect(G_OBJECT(win->spice), "mouse-grab",
                     G_CALLBACK(mouse_grab_cb), win);
    g_signal_connect(G_OBJECT(win->spice), "keyboard-grab",
                     G_CALLBACK(keyboard_grab_cb), win);
    g_signal_connect(G_OBJECT(win->spice), "grab-keys-pressed",
                     G_CALLBACK(grab_keys_pressed_cb), win);

    /* status line */
#if GTK_CHECK_VERSION(3,0,0)
    win->statusbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
#else
    win->statusbar = gtk_hbox_new(FALSE, 1);
#endif

    win->status = gtk_label_new("status line");
    gtk_misc_set_alignment(GTK_MISC(win->status), 0, 0.5);
    gtk_misc_set_padding(GTK_MISC(win->status), 3, 1);
    update_status_window(win);

    frame = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(win->statusbar), frame, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(frame), win->status);

    for (i = 0; i < STATE_MAX; i++) {
        win->st[i] = gtk_label_new(_("?"));
        gtk_label_set_width_chars(GTK_LABEL(win->st[i]), 5);
        frame = gtk_frame_new(NULL);
        gtk_box_pack_end(GTK_BOX(win->statusbar), frame, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(frame), win->st[i]);
    }

    /* Make a vbox and put stuff in */
#if GTK_CHECK_VERSION(3,0,0)
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
#else
    vbox = gtk_vbox_new(FALSE, 1);
#endif
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
    gtk_container_add(GTK_CONTAINER(win->toplevel), vbox);
    gtk_box_pack_start(GTK_BOX(vbox), win->menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), win->toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), win->spice, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(vbox), win->statusbar, FALSE, TRUE, 0);

    /* show window */
    if (fullscreen)
        gtk_window_fullscreen(GTK_WINDOW(win->toplevel));

    gtk_widget_show_all(vbox);
    restore_configuration(win);

    /* init toggle actions */
    for (i = 0; i < G_N_ELEMENTS(spice_display_properties); i++) {
        toggle = gtk_action_group_get_action(win->ag,
                                             spice_display_properties[i]);
        g_object_get(win->spice, spice_display_properties[i], &state, NULL);
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(toggle), state);
    }

    for (i = 0; i < G_N_ELEMENTS(spice_gtk_session_properties); i++) {
        char notify[64];

        toggle = gtk_action_group_get_action(win->ag,
                                             spice_gtk_session_properties[i]);
        g_object_get(win->conn->gtk_session, spice_gtk_session_properties[i],
                     &state, NULL);
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(toggle), state);

        snprintf(notify, sizeof(notify), "notify::%s",
                 spice_gtk_session_properties[i]);
        spice_g_signal_connect_object(win->conn->gtk_session, notify,
                                      G_CALLBACK(menu_cb_conn_bool_prop_changed),
                                      win, 0);
    }

    update_edit_menu_window(win);

    toggle = gtk_action_group_get_action(win->ag, "Toolbar");
    state = gtk_widget_get_visible(win->toolbar);
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(toggle), state);

    toggle = gtk_action_group_get_action(win->ag, "Statusbar");
    state = gtk_widget_get_visible(win->statusbar);
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(toggle), state);

#ifdef USE_SMARTCARD
    gboolean smartcard;

    enable_smartcard_actions(win, NULL, FALSE, FALSE);
    g_object_get(G_OBJECT(conn->session),
                 "enable-smartcard", &smartcard,
                 NULL);
    if (smartcard) {
        g_signal_connect(G_OBJECT(spice_smartcard_manager_get()), "reader-added",
                         (GCallback)reader_added_cb, win);
        g_signal_connect(G_OBJECT(spice_smartcard_manager_get()), "reader-removed",
                         (GCallback)reader_removed_cb, win);
        g_signal_connect(G_OBJECT(spice_smartcard_manager_get()), "card-inserted",
                         (GCallback)card_inserted_cb, win);
        g_signal_connect(G_OBJECT(spice_smartcard_manager_get()), "card-removed",
                         (GCallback)card_removed_cb, win);
    }
#endif

#ifndef USE_USBREDIR
    GtkAction *usbredir = gtk_action_group_get_action(win->ag, "auto-usbredir");
    gtk_action_set_visible(usbredir, FALSE);
#endif

    gtk_widget_grab_focus(win->spice);

    return win;
}

static void destroy_spice_window(SpiceWindow *win)
{
    if (win == NULL)
        return;

    SPICE_DEBUG("destroy window (#%d:%d)", win->id, win->monitor_id);
    g_object_unref(win->ag);
    g_object_unref(win->ui);
    gtk_widget_destroy(win->toplevel);
    g_object_unref(win);
}

/* ------------------------------------------------------------------ */

static void recent_add(SpiceSession *session)
{
    GtkRecentManager *recent;
    GtkRecentData meta = {
        .mime_type    = (char*)"application/x-spice",
        .app_name     = (char*)"spicy",
        .app_exec     = (char*)"spicy --uri=%u",
    };
    char *uri;

    g_object_get(session, "uri", &uri, NULL);
    SPICE_DEBUG("%s: %s", __FUNCTION__, uri);

    g_return_if_fail(g_str_has_prefix(uri, "spice://"));

    recent = gtk_recent_manager_get_default();
    meta.display_name = uri + 8;
    if (!gtk_recent_manager_add_full(recent, uri, &meta))
        g_warning("Recent item couldn't be added successfully");

    g_free(uri);
}

static void main_channel_event(SpiceChannel *channel, SpiceChannelEvent event,
                               gpointer data)
{
    spice_connection *conn = data;
    char password[64];
    int rc;

    switch (event) {
    case SPICE_CHANNEL_OPENED:
        g_message("main channel: opened");
        recent_add(conn->session);
        break;
    case SPICE_CHANNEL_SWITCHING:
        g_message("main channel: switching host");
        break;
    case SPICE_CHANNEL_CLOSED:
        /* this event is only sent if the channel was succesfully opened before */
        g_message("main channel: closed");
        connection_disconnect(conn);
        break;
    case SPICE_CHANNEL_ERROR_IO:
        connection_disconnect(conn);
        break;
    case SPICE_CHANNEL_ERROR_TLS:
    case SPICE_CHANNEL_ERROR_LINK:
    case SPICE_CHANNEL_ERROR_CONNECT:
        g_message("main channel: failed to connect");
        rc = connect_dialog(conn->session);
        if (rc == 0) {
            connection_connect(conn);
        } else {
            connection_disconnect(conn);
        }
        break;
    case SPICE_CHANNEL_ERROR_AUTH:
        g_warning("main channel: auth failure (wrong password?)");
        strcpy(password, "");
        /* FIXME i18 */
        rc = ask_user(NULL, _("Authentication"),
                      _("Please enter the spice server password"),
                      password, sizeof(password), true);
        if (rc == 0) {
            g_object_set(conn->session, "password", password, NULL);
            connection_connect(conn);
        } else {
            connection_disconnect(conn);
        }
        break;
    default:
        /* TODO: more sophisticated error handling */
        g_warning("unknown main channel event: %d", event);
        /* connection_disconnect(conn); */
        break;
    }
}

static void main_mouse_update(SpiceChannel *channel, gpointer data)
{
    spice_connection *conn = data;
    gint mode;

    g_object_get(channel, "mouse-mode", &mode, NULL);
    switch (mode) {
    case SPICE_MOUSE_MODE_SERVER:
        conn->mouse_state = "server";
        break;
    case SPICE_MOUSE_MODE_CLIENT:
        conn->mouse_state = "client";
        break;
    default:
        conn->mouse_state = "?";
        break;
    }
    update_status(conn);
}

static void main_agent_update(SpiceChannel *channel, gpointer data)
{
    spice_connection *conn = data;

    g_object_get(channel, "agent-connected", &conn->agent_connected, NULL);
    conn->agent_state = conn->agent_connected ? _("yes") : _("no");
    update_status(conn);
    update_edit_menu(conn);
}

static void inputs_modifiers(SpiceChannel *channel, gpointer data)
{
    spice_connection *conn = data;
    int m, i;

    g_object_get(channel, "key-modifiers", &m, NULL);
    for (i = 0; i < SPICE_N_ELEMENTS(conn->wins); i++) {
        if (conn->wins[i] == NULL)
            continue;

        gtk_label_set_text(GTK_LABEL(conn->wins[i]->st[STATE_SCROLL_LOCK]),
                           m & SPICE_KEYBOARD_MODIFIER_FLAGS_SCROLL_LOCK ? _("SCROLL") : "");
        gtk_label_set_text(GTK_LABEL(conn->wins[i]->st[STATE_CAPS_LOCK]),
                           m & SPICE_KEYBOARD_MODIFIER_FLAGS_CAPS_LOCK ? _("CAPS") : "");
        gtk_label_set_text(GTK_LABEL(conn->wins[i]->st[STATE_NUM_LOCK]),
                           m & SPICE_KEYBOARD_MODIFIER_FLAGS_NUM_LOCK ? _("NUM") : "");
    }
}

static void display_mark(SpiceChannel *channel, gint mark, SpiceWindow *win)
{
    g_return_if_fail(win != NULL);
    g_return_if_fail(win->toplevel != NULL);

    if (mark == TRUE) {
        gtk_widget_show(win->toplevel);
    } else {
        gtk_widget_hide(win->toplevel);
    }
}

static void update_auto_usbredir_sensitive(spice_connection *conn)
{
#ifdef USE_USBREDIR
    int i;
    GtkAction *ac;
    gboolean sensitive;

    sensitive = spice_session_has_channel_type(conn->session,
                                               SPICE_CHANNEL_USBREDIR);
    for (i = 0; i < SPICE_N_ELEMENTS(conn->wins); i++) {
        if (conn->wins[i] == NULL)
            continue;
        ac = gtk_action_group_get_action(conn->wins[i]->ag, "auto-usbredir");
        gtk_action_set_sensitive(ac, sensitive);
    }
#endif
}

static SpiceWindow* get_window(spice_connection *conn, int channel_id, int monitor_id)
{
    g_return_val_if_fail(channel_id < CHANNELID_MAX, NULL);
    g_return_val_if_fail(monitor_id < MONITORID_MAX, NULL);

    return conn->wins[channel_id * CHANNELID_MAX + monitor_id];
}

static void add_window(spice_connection *conn, SpiceWindow *win)
{
    g_return_if_fail(win != NULL);
    g_return_if_fail(win->id < CHANNELID_MAX);
    g_return_if_fail(win->monitor_id < MONITORID_MAX);
    g_return_if_fail(conn->wins[win->id * CHANNELID_MAX + win->monitor_id] == NULL);

    SPICE_DEBUG("add display monitor %d:%d", win->id, win->monitor_id);
    conn->wins[win->id * CHANNELID_MAX + win->monitor_id] = win;
}

static void del_window(spice_connection *conn, SpiceWindow *win)
{
    if (win == NULL)
        return;

    g_return_if_fail(win->id < CHANNELID_MAX);
    g_return_if_fail(win->monitor_id < MONITORID_MAX);

    g_debug("del display monitor %d:%d", win->id, win->monitor_id);
    conn->wins[win->id * CHANNELID_MAX + win->monitor_id] = NULL;
    if (win->id > 0)
        spice_main_set_display_enabled(conn->main, win->id, FALSE);
    else
        spice_main_set_display_enabled(conn->main, win->monitor_id, FALSE);
    spice_main_send_monitor_config(conn->main);

    destroy_spice_window(win);
}

static void display_monitors(SpiceChannel *display, GParamSpec *pspec,
                             spice_connection *conn)
{
    GArray *monitors = NULL;
    int id;
    guint i;

    g_object_get(display,
                 "channel-id", &id,
                 "monitors", &monitors,
                 NULL);
    g_return_if_fail(monitors != NULL);

    for (i = 0; i < monitors->len; i++) {
        SpiceWindow *w;

        if (!get_window(conn, id, i)) {
            w = create_spice_window(conn, display, id, i);
            add_window(conn, w);
            spice_g_signal_connect_object(display, "display-mark",
                                          G_CALLBACK(display_mark), w, 0);
            gtk_widget_show(w->toplevel);
            update_auto_usbredir_sensitive(conn);
        }
    }

    for (; i < MONITORID_MAX; i++)
        del_window(conn, get_window(conn, id, i));

    g_clear_pointer(&monitors, g_array_unref);
}

static void port_write_cb(GObject *source_object,
                          GAsyncResult *res,
                          gpointer user_data)
{
    SpicePortChannel *port = SPICE_PORT_CHANNEL(source_object);
    GError *error = NULL;

    spice_port_write_finish(port, res, &error);
    if (error != NULL)
        g_warning("%s", error->message);
    g_clear_error(&error);
}

static void port_flushed_cb(GObject *source_object,
                            GAsyncResult *res,
                            gpointer user_data)
{
    SpiceChannel *channel = SPICE_CHANNEL(source_object);
    GError *error = NULL;

    spice_channel_flush_finish(channel, res, &error);
    if (error != NULL)
        g_warning("%s", error->message);
    g_clear_error(&error);

    spice_channel_disconnect(channel, SPICE_CHANNEL_CLOSED);
}

static gboolean input_cb(GIOChannel *gin, GIOCondition condition, gpointer data)
{
    char buf[4096];
    gsize bytes_read;
    GIOStatus status;

    if (!(condition & G_IO_IN))
        return FALSE;

    status = g_io_channel_read_chars(gin, buf, sizeof(buf), &bytes_read, NULL);
    if (status != G_IO_STATUS_NORMAL)
        return FALSE;

    if (stdin_port != NULL)
        spice_port_write_async(stdin_port, buf, bytes_read, NULL, port_write_cb, NULL);

    return TRUE;
}

static void port_opened(SpiceChannel *channel, GParamSpec *pspec,
                        spice_connection *conn)
{
    SpicePortChannel *port = SPICE_PORT_CHANNEL(channel);
    gchar *name = NULL;
    gboolean opened = FALSE;

    g_object_get(channel,
                 "port-name", &name,
                 "port-opened", &opened,
                 NULL);

    g_printerr("port %p %s: %s\n", channel, name, opened ? "opened" : "closed");

    if (opened) {
        /* only send a break event and disconnect */
        if (g_strcmp0(name, "org.spice.spicy.break") == 0) {
            spice_port_event(port, SPICE_PORT_EVENT_BREAK);
        }

        /* handle the first spicy port and connect it to stdin/out */
        if (g_strcmp0(name, "org.spice.spicy") == 0 && stdin_port == NULL) {
            stdin_port = port;
            goto end;
        }

        if (port == stdin_port)
            goto end;

        spice_channel_flush_async(channel, NULL, port_flushed_cb, conn);
    } else {
        if (port == stdin_port)
            stdin_port = NULL;
    }

end:
    g_free(name);
}

static void port_data(SpicePortChannel *port,
                      gpointer data, int size, spice_connection *conn)
{
    int r;

    if (port != stdin_port)
        return;

    r = write(fileno(stdout), data, size);
    if (r != size) {
        g_warning("port write failed result %d/%d errno %d", r, size, errno);
    }
}

static void channel_new(SpiceSession *s, SpiceChannel *channel, gpointer data)
{
    spice_connection *conn = data;
    int id;

    g_object_get(channel, "channel-id", &id, NULL);
    conn->channels++;
    SPICE_DEBUG("new channel (#%d)", id);

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        SPICE_DEBUG("new main channel");
        conn->main = SPICE_MAIN_CHANNEL(channel);
        g_signal_connect(channel, "channel-event",
                         G_CALLBACK(main_channel_event), conn);
        g_signal_connect(channel, "main-mouse-update",
                         G_CALLBACK(main_mouse_update), conn);
        g_signal_connect(channel, "main-agent-update",
                         G_CALLBACK(main_agent_update), conn);
        main_mouse_update(channel, conn);
        main_agent_update(channel, conn);
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        if (id >= SPICE_N_ELEMENTS(conn->wins))
            return;
        if (conn->wins[id] != NULL)
            return;
        SPICE_DEBUG("new display channel (#%d)", id);
        g_signal_connect(channel, "notify::monitors",
                         G_CALLBACK(display_monitors), conn);
        spice_channel_connect(channel);
    }

    if (SPICE_IS_INPUTS_CHANNEL(channel)) {
        SPICE_DEBUG("new inputs channel");
        g_signal_connect(channel, "inputs-modifiers",
                         G_CALLBACK(inputs_modifiers), conn);
    }

    if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
        SPICE_DEBUG("new audio channel");
        conn->audio = spice_audio_get(s, NULL);
    }

    if (SPICE_IS_USBREDIR_CHANNEL(channel)) {
        update_auto_usbredir_sensitive(conn);
    }

    if (SPICE_IS_PORT_CHANNEL(channel)) {
        g_signal_connect(channel, "notify::port-opened",
                         G_CALLBACK(port_opened), conn);
        g_signal_connect(channel, "port-data",
                         G_CALLBACK(port_data), conn);
        spice_channel_connect(channel);
    }
}

static void channel_destroy(SpiceSession *s, SpiceChannel *channel, gpointer data)
{
    spice_connection *conn = data;
    int id;

    g_object_get(channel, "channel-id", &id, NULL);
    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        SPICE_DEBUG("zap main channel");
        conn->main = NULL;
    }

    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        if (id >= SPICE_N_ELEMENTS(conn->wins))
            return;
        SPICE_DEBUG("zap display channel (#%d)", id);
        /* FIXME destroy widget only */
    }

    if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
        SPICE_DEBUG("zap audio channel");
    }

    if (SPICE_IS_USBREDIR_CHANNEL(channel)) {
        update_auto_usbredir_sensitive(conn);
    }

    if (SPICE_IS_PORT_CHANNEL(channel)) {
        if (SPICE_PORT_CHANNEL(channel) == stdin_port)
            stdin_port = NULL;
    }

    conn->channels--;
    if (conn->channels > 0) {
        return;
    }

    connection_destroy(conn);
}

static void migration_state(GObject *session,
                            GParamSpec *pspec, gpointer data)
{
    SpiceSessionMigration mig;

    g_object_get(session, "migration-state", &mig, NULL);
    if (mig == SPICE_SESSION_MIGRATION_SWITCHING)
        g_message("migrating session");
}

static spice_connection *connection_new(void)
{
    spice_connection *conn;
    SpiceUsbDeviceManager *manager;

    conn = g_new0(spice_connection, 1);
    conn->session = spice_session_new();
    conn->gtk_session = spice_gtk_session_get(conn->session);
    g_signal_connect(conn->session, "channel-new",
                     G_CALLBACK(channel_new), conn);
    g_signal_connect(conn->session, "channel-destroy",
                     G_CALLBACK(channel_destroy), conn);
    g_signal_connect(conn->session, "notify::migration-state",
                     G_CALLBACK(migration_state), conn);

    manager = spice_usb_device_manager_get(conn->session, NULL);
    if (manager) {
        g_signal_connect(manager, "auto-connect-failed",
                         G_CALLBACK(usb_connect_failed), NULL);
        g_signal_connect(manager, "device-error",
                         G_CALLBACK(usb_connect_failed), NULL);
    }

    connections++;
    SPICE_DEBUG("%s (%d)", __FUNCTION__, connections);
    return conn;
}

static void connection_connect(spice_connection *conn)
{
    conn->disconnecting = false;
    spice_session_connect(conn->session);
}

static void connection_disconnect(spice_connection *conn)
{
    if (conn->disconnecting)
        return;
    conn->disconnecting = true;
    spice_session_disconnect(conn->session);
}

static void connection_destroy(spice_connection *conn)
{
    g_object_unref(conn->session);
    free(conn);

    connections--;
    SPICE_DEBUG("%s (%d)", __FUNCTION__, connections);
    if (connections > 0) {
        return;
    }

    g_main_loop_quit(mainloop);
}

/* ------------------------------------------------------------------ */

static GOptionEntry cmd_entries[] = {
    {
        .long_name        = "full-screen",
        .short_name       = 'f',
        .arg              = G_OPTION_ARG_NONE,
        .arg_data         = &fullscreen,
        .description      = N_("Open in full screen mode"),
    },{
        .long_name        = "version",
        .arg              = G_OPTION_ARG_NONE,
        .arg_data         = &version,
        .description      = N_("Display version and quit"),
    },{
        .long_name        = "title",
        .arg              = G_OPTION_ARG_STRING,
        .arg_data         = &spicy_title,
        .description      = N_("Set the window title"),
        .arg_description  = N_("<title>"),
    },{
        /* end of list */
    }
};

static void (* segv_handler) (int) = SIG_DFL;
static void (* abrt_handler) (int) = SIG_DFL;
static void (* fpe_handler)  (int) = SIG_DFL;
static void (* ill_handler)  (int) = SIG_DFL;
#ifndef WIN32
static void (* bus_handler)  (int) = SIG_DFL;
#endif

static void
signal_handler(int signum)
{
    static gint recursion = FALSE;

    /*
     * reset all signal handlers: any further crashes should just be allowed
     * to crash normally.
     * */
    signal(SIGSEGV, segv_handler);
    signal(SIGABRT, abrt_handler);
    signal(SIGFPE,  fpe_handler);
    signal(SIGILL,  ill_handler);
#ifndef WIN32
    signal(SIGBUS,  bus_handler);
#endif

    /* Stop bizarre loops */
    if (recursion)
        abort ();

    recursion = TRUE;

    g_main_loop_quit(mainloop);
}

static void usb_connect_failed(GObject               *object,
                               SpiceUsbDevice        *device,
                               GError                *error,
                               gpointer               data)
{
    GtkWidget *dialog;

    if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_CANCELLED)
        return;

    dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_CLOSE,
                                    "USB redirection error");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             "%s", error->message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void setup_terminal(gboolean reset)
{
    int stdinfd = fileno(stdin);

    if (!isatty(stdinfd))
        return;

#ifdef HAVE_TERMIOS_H
    static struct termios saved_tios;
    struct termios tios;

    if (reset)
        tios = saved_tios;
    else {
        tcgetattr(stdinfd, &tios);
        saved_tios = tios;
        tios.c_lflag &= ~(ICANON | ECHO);
    }

    tcsetattr(stdinfd, TCSANOW, &tios);
#endif
}

static void watch_stdin(void)
{
    int stdinfd = fileno(stdin);
    GIOChannel *gin;

    setup_terminal(false);
    gin = g_io_channel_unix_new(stdinfd);
    g_io_channel_set_flags(gin, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch(gin, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, input_cb, NULL);
}

int main(int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context;
    spice_connection *conn;
    gchar *conf_file, *conf;
    char *host = NULL, *port = NULL, *tls_port = NULL;

#if !GLIB_CHECK_VERSION(2,31,18)
    g_thread_init(NULL);
#endif
    bindtextdomain(GETTEXT_PACKAGE, SPICE_GTK_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    segv_handler = signal(SIGSEGV, signal_handler);
    abrt_handler = signal(SIGABRT, signal_handler);
    fpe_handler  = signal(SIGFPE,  signal_handler);
    ill_handler  = signal(SIGILL,  signal_handler);
#ifndef WIN32
    signal(SIGHUP, signal_handler);
    bus_handler  = signal(SIGBUS,  signal_handler);
#endif

    keyfile = g_key_file_new();

    int mode = S_IRWXU;
    conf_file = g_build_filename(g_get_user_config_dir(), "spicy", NULL);
    if (g_mkdir_with_parents(conf_file, mode) == -1)
        SPICE_DEBUG("failed to create config directory");
    g_free(conf_file);

    conf_file = g_build_filename(g_get_user_config_dir(), "spicy", "settings", NULL);
    if (!g_key_file_load_from_file(keyfile, conf_file,
                                   G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, &error)) {
        SPICE_DEBUG("Couldn't load configuration: %s", error->message);
        g_clear_error(&error);
    }

    /* parse opts */
    gtk_init(&argc, &argv);
    context = g_option_context_new(_("- spice client test application"));
    g_option_context_set_summary(context, _("Gtk+ test client to connect to Spice servers."));
    g_option_context_set_description(context, _("Report bugs to " PACKAGE_BUGREPORT "."));
    g_option_context_add_group(context, spice_get_option_group());
    g_option_context_set_main_group(context, spice_cmdline_get_option_group());
    g_option_context_add_main_entries(context, cmd_entries, NULL);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print(_("option parsing failed: %s\n"), error->message);
        exit(1);
    }
    g_option_context_free(context);

    if (version) {
        g_print("spicy " PACKAGE_VERSION "\n");
        exit(0);
    }

    g_type_init();
    mainloop = g_main_loop_new(NULL, false);

    conn = connection_new();
    spice_set_session_option(conn->session);
    spice_cmdline_session_setup(conn->session);

    g_object_get(conn->session,
                 "host", &host,
                 "port", &port,
                 "tls-port", &tls_port,
                 NULL);
    /* If user doesn't provide hostname and port, show the dialog window
       instead of connecting to server automatically */
    if (host == NULL || (port == NULL && tls_port == NULL)) {
        int ret = connect_dialog(conn->session);
        if (ret != 0) {
            exit(0);
        }
    }
    g_free(host);
    g_free(port);
    g_free(tls_port);

    watch_stdin();

    connection_connect(conn);
    if (connections > 0)
        g_main_loop_run(mainloop);
    g_main_loop_unref(mainloop);

    if ((conf = g_key_file_to_data(keyfile, NULL, &error)) == NULL ||
        !g_file_set_contents(conf_file, conf, -1, &error)) {
        SPICE_DEBUG("Couldn't save configuration: %s", error->message);
        g_error_free(error);
        error = NULL;
    }

    g_free(conf_file);
    g_free(conf);
    g_key_file_free(keyfile);

    g_free(spicy_title);

    setup_terminal(true);
    return 0;
}
