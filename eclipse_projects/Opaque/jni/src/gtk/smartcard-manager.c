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
#include "config.h"

#include <glib-object.h>
#include <string.h>

#include "glib-compat.h"

#ifdef USE_SMARTCARD
#include <vcard_emul.h>
#include <vevent.h>
#include <vreader.h>
#endif

#include "spice-client.h"
#include "smartcard-manager.h"
#include "smartcard-manager-priv.h"
#include "spice-marshal.h"

/**
 * SECTION:smartcard-manager
 * @short_description: smartcard management
 * @title: Spice Smartcard Manager
 * @section_id:
 * @see_also:
 * @stability: Stable
 * @include: smartcard-manager.h
 *
 * #SpiceSmartcardManager monitors smartcard reader plugging/unplugging,
 * and smartcard insertions/removals. It also provides methods to handle
 * software smartcards (to emulate a smartcard reader/smartcard on the
 * guest using 3 certificates available to the client).
 */

/* ------------------------------------------------------------------ */
/* gobject glue                                                       */

#define SPICE_SMARTCARD_MANAGER_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SPICE_TYPE_SMARTCARD_MANAGER, SpiceSmartcardManagerPrivate))

struct _SpiceSmartcardManagerPrivate {
    guint monitor_id;

    /* software smartcard reader, the certificates to use for this reader
     * were given at the channel creation time. This reader has no physical
     * existence, it's all controlled by explicit software
     * insertion/removal of cards
     */
#ifdef USE_SMARTCARD
    VReader *software_reader;
#endif
};

G_DEFINE_TYPE(SpiceSmartcardManager, spice_smartcard_manager, G_TYPE_OBJECT)
#ifdef USE_SMARTCARD
G_DEFINE_BOXED_TYPE(VReader, spice_smartcard_reader, vreader_reference, vreader_free)
#else
typedef GObject VReader;
G_DEFINE_BOXED_TYPE(VReader, spice_smartcard_reader, g_object_ref, g_object_unref)
#endif

/* Properties */
enum {
    PROP_0,
};

/* Signals */
enum {
    SPICE_SMARTCARD_MANAGER_READER_ADDED,
    SPICE_SMARTCARD_MANAGER_READER_REMOVED,
    SPICE_SMARTCARD_MANAGER_CARD_INSERTED,
    SPICE_SMARTCARD_MANAGER_CARD_REMOVED,

    SPICE_SMARTCARD_MANAGER_LAST_SIGNAL,
};

static guint signals[SPICE_SMARTCARD_MANAGER_LAST_SIGNAL];

#ifdef USE_SMARTCARD
typedef gboolean (*SmartcardSourceFunc)(VEvent *event, gpointer user_data);
static gboolean smartcard_monitor_dispatch(VEvent *event, gpointer user_data);
#endif

/* ------------------------------------------------------------------ */

static void spice_smartcard_manager_init(SpiceSmartcardManager *smartcard_manager)
{
    SpiceSmartcardManagerPrivate *priv;

    priv = SPICE_SMARTCARD_MANAGER_GET_PRIVATE(smartcard_manager);
    smartcard_manager->priv = priv;
}

static void spice_smartcard_manager_dispose(GObject *gobject)
{
    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_smartcard_manager_parent_class)->dispose)
        G_OBJECT_CLASS(spice_smartcard_manager_parent_class)->dispose(gobject);
}

static void spice_smartcard_manager_finalize(GObject *gobject)
{
    SpiceSmartcardManager *manager = SPICE_SMARTCARD_MANAGER(gobject);
    SpiceSmartcardManagerPrivate *priv = manager->priv;

    if (priv->monitor_id != 0) {
        g_source_remove(priv->monitor_id);
        priv->monitor_id = 0;
    }

#ifdef USE_SMARTCARD
    if (priv->software_reader != NULL) {
        vreader_free(priv->software_reader);
        priv->software_reader = NULL;
    }
#endif

    /* Chain up to the parent class */
    if (G_OBJECT_CLASS(spice_smartcard_manager_parent_class)->finalize)
        G_OBJECT_CLASS(spice_smartcard_manager_parent_class)->finalize(gobject);
}

static void spice_smartcard_manager_class_init(SpiceSmartcardManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    /**
     * SpiceSmartcardManager::reader-added:
     * @manager: the #SpiceSmartcardManager that emitted the signal
     * @vreader: #VReader boxed object corresponding to the added reader
     *
     * The #SpiceSmartcardManager::reader-added signal is emitted whenever
     * a new smartcard reader (software or hardware) has been plugged in.
     **/
    signals[SPICE_SMARTCARD_MANAGER_READER_ADDED] =
        g_signal_new("reader-added",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceSmartcardManagerClass, reader_added),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE,
                     1,
                     SPICE_TYPE_SMARTCARD_READER);

    /**
     * SpiceSmartcardManager::reader-removed:
     * @manager: the #SpiceSmartcardManager that emitted the signal
     * @vreader: #VReader boxed object corresponding to the removed reader
     *
     * The #SpiceSmartcardManager::reader-removed signal is emitted whenever
     * a smartcard reader (software or hardware) has been removed.
     **/
    signals[SPICE_SMARTCARD_MANAGER_READER_REMOVED] =
        g_signal_new("reader-removed",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceSmartcardManagerClass, reader_removed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE,
                     1,
                     SPICE_TYPE_SMARTCARD_READER);

    /**
     * SpiceSmartcardManager::card-inserted:
     * @manager: the #SpiceSmartcardManager that emitted the signal
     * @vreader: #VReader boxed object corresponding to the reader a new
     * card was inserted in
     *
     * The #SpiceSmartcardManager::card-inserted signal is emitted whenever
     * a smartcard is inserted in a reader
     **/
    signals[SPICE_SMARTCARD_MANAGER_CARD_INSERTED] =
        g_signal_new("card-inserted",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceSmartcardManagerClass, card_inserted),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE,
                     1,
                     SPICE_TYPE_SMARTCARD_READER);

    /**
     * SpiceSmartcardManager::card-removed:
     * @manager: the #SpiceSmartcardManager that emitted the signal
     * @vreader: #VReader boxed object corresponding to the reader a card
     * was removed from
     *
     * The #SpiceSmartcardManager::card-removed signal is emitted whenever
     * a smartcard was removed from a reader.
     **/
    signals[SPICE_SMARTCARD_MANAGER_CARD_REMOVED] =
        g_signal_new("card-removed",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceSmartcardManagerClass, card_removed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__BOXED,
                     G_TYPE_NONE,
                     1,
                     SPICE_TYPE_SMARTCARD_READER);
    gobject_class->dispose      = spice_smartcard_manager_dispose;
    gobject_class->finalize     = spice_smartcard_manager_finalize;

    g_type_class_add_private(klass, sizeof(SpiceSmartcardManagerPrivate));
}

/* ------------------------------------------------------------------ */
/* private api                                                        */

static SpiceSmartcardManager *spice_smartcard_manager_new(void)
{
    return g_object_new(SPICE_TYPE_SMARTCARD_MANAGER, NULL);
}

/* ------------------------------------------------------------------ */
/* public api                                                         */

/**
 * spice_smartcard_manager_get:
 *
 * #SpiceSmartcardManager is a singleton, use this function to get a pointer
 * to it. A new SpiceSmartcardManager instance will be created the first
 * time this function is called
 *
 * Returns: (transfer none): a weak reference to the #SpiceSmartcardManager
 */
SpiceSmartcardManager *spice_smartcard_manager_get(void)
{
    static GOnce manager_singleton_once = G_ONCE_INIT;

    return g_once(&manager_singleton_once,
                  (GThreadFunc)spice_smartcard_manager_new,
                  NULL);
}

#ifdef USE_SMARTCARD
static gboolean smartcard_monitor_dispatch(VEvent *event, gpointer user_data)
{
    g_return_val_if_fail(event != NULL, TRUE);
    SpiceSmartcardManager *manager = SPICE_SMARTCARD_MANAGER(user_data);

    switch (event->type) {
        case VEVENT_READER_INSERT:
            if (spice_smartcard_reader_is_software((SpiceSmartcardReader*)event->reader)) {
                g_warn_if_fail(manager->priv->software_reader == NULL);
                manager->priv->software_reader = vreader_reference(event->reader);
            }
            SPICE_DEBUG("smartcard: reader-added");
            g_signal_emit(G_OBJECT(user_data),
                          signals[SPICE_SMARTCARD_MANAGER_READER_ADDED],
                          0, event->reader);
            break;

        case VEVENT_READER_REMOVE:
            if (spice_smartcard_reader_is_software((SpiceSmartcardReader*)event->reader)) {
                g_warn_if_fail(manager->priv->software_reader != NULL);
                vreader_free(manager->priv->software_reader);
                manager->priv->software_reader = NULL;
            }
            SPICE_DEBUG("smartcard: reader-removed");
            g_signal_emit(G_OBJECT(user_data),
                          signals[SPICE_SMARTCARD_MANAGER_READER_REMOVED],
                          0, event->reader);
            break;

        case VEVENT_CARD_INSERT:
            SPICE_DEBUG("smartcard: card-inserted");
            g_signal_emit(G_OBJECT(user_data),
                          signals[SPICE_SMARTCARD_MANAGER_CARD_INSERTED],
                          0, event->reader);
            break;
        case VEVENT_CARD_REMOVE:
            SPICE_DEBUG("smartcard: card-removed");
            g_signal_emit(G_OBJECT(user_data),
                          signals[SPICE_SMARTCARD_MANAGER_CARD_REMOVED],
                          0, event->reader);
            break;
        case VEVENT_LAST:
            break;
    }

    return TRUE;
}

/* ------------------------------------------------------------------ */
/* smartcard monitoring GSource                                       */
struct _SmartcardSource {
    GSource parent_source;
    VEvent *pending_event;
};
typedef struct _SmartcardSource SmartcardSource;

static gboolean smartcard_source_prepare(GSource *source, gint *timeout)
{
    SmartcardSource *smartcard_source = (SmartcardSource *)source;

    if (smartcard_source->pending_event == NULL)
        smartcard_source->pending_event = vevent_get_next_vevent();

    if (timeout != NULL)
        *timeout = -1;

    return (smartcard_source->pending_event != NULL);
}

static gboolean smartcard_source_check(GSource *source)
{
    return smartcard_source_prepare(source, NULL);
}

static gboolean smartcard_source_dispatch(GSource *source,
                                          GSourceFunc callback,
                                          gpointer user_data)
{
    SmartcardSource *smartcard_source = (SmartcardSource *)source;
    SmartcardSourceFunc smartcard_callback = (SmartcardSourceFunc)callback;

    g_return_val_if_fail(smartcard_source->pending_event != NULL, FALSE);

    if (callback) {
        gboolean event_consumed;
        event_consumed = smartcard_callback(smartcard_source->pending_event,
                                            user_data);
        if (event_consumed) {
            vevent_delete(smartcard_source->pending_event);
            smartcard_source->pending_event = NULL;
        }
    }

    return TRUE;
}

static void smartcard_source_finalize(GSource *source)
{
    SmartcardSource *smartcard_source = (SmartcardSource *)source;

    if (smartcard_source->pending_event) {
        vevent_delete(smartcard_source->pending_event);
        smartcard_source->pending_event = NULL;
    }
}

static GSource *smartcard_monitor_source_new(void)
{
    static GSourceFuncs source_funcs = {
        .prepare = smartcard_source_prepare,
        .check = smartcard_source_check,
        .dispatch = smartcard_source_dispatch,
        .finalize = smartcard_source_finalize
    };
    GSource *source;

    source = g_source_new(&source_funcs, sizeof(SmartcardSource));
    g_source_set_name(source, "Smartcard event source");
    return source;
}

static guint smartcard_monitor_add(SmartcardSourceFunc callback,
                                   gpointer user_data)
{
    GSource *source;
    guint id;

    source = smartcard_monitor_source_new();
    g_source_set_callback(source, (GSourceFunc)callback, user_data, NULL);
    id = g_source_attach(source, NULL);
    g_source_unref(source);

    return id;
}

static void
spice_smartcard_manager_update_monitor(void)
{
    SpiceSmartcardManager *self = spice_smartcard_manager_get();
    SpiceSmartcardManagerPrivate *priv = self->priv;

    if (priv->monitor_id != 0)
        return;

    priv->monitor_id = smartcard_monitor_add(smartcard_monitor_dispatch, self);
}

#define SPICE_SOFTWARE_READER_NAME "Spice Software Smartcard"

typedef struct {
    SpiceSession *session;
    GCancellable *cancellable;
    GError *err;
} SmartcardManagerInitArgs;

static gboolean smartcard_manager_init(SmartcardManagerInitArgs *args)
{
    gchar *emul_args = NULL;
    VCardEmulOptions *options = NULL;
    VCardEmulError emul_init_status;
    gchar *dbname = NULL;
    GStrv certificates = NULL;
    gboolean retval = FALSE;

    SPICE_DEBUG("smartcard_manager_init");
    g_return_val_if_fail(SPICE_IS_SESSION(args->session), FALSE);
    g_object_get(G_OBJECT(args->session),
                 "smartcard-db", &dbname,
                 "smartcard-certificates", &certificates,
                 NULL);

    if ((certificates == NULL) || (g_strv_length(certificates) != 3))
        goto init;

    if (dbname) {
        emul_args = g_strdup_printf("db=\"%s\" use_hw=no "
                                    "soft=(,%s,CAC,,%s,%s,%s)",
                                    dbname, SPICE_SOFTWARE_READER_NAME,
                                    certificates[0], certificates[1],
                                    certificates[2]);
    } else {
        emul_args = g_strdup_printf("use_hw=no soft=(,%s,CAC,,%s,%s,%s)",
                                    SPICE_SOFTWARE_READER_NAME,
                                    certificates[0], certificates[1],
                                    certificates[2]);
    }

    options = vcard_emul_options(emul_args);
    if (options == NULL) {
        args->err = g_error_new(SPICE_CLIENT_ERROR,
                                SPICE_CLIENT_ERROR_FAILED,
                                "vcard_emul_options() failed!");
        goto end;
    }

    if (g_cancellable_set_error_if_cancelled(args->cancellable, &args->err))
        goto end;

init:
    SPICE_DEBUG("vcard_emul_init");
    emul_init_status = vcard_emul_init(options);
    if ((emul_init_status != VCARD_EMUL_OK)
            && (emul_init_status != VCARD_EMUL_INIT_ALREADY_INITED)) {
        args->err = g_error_new(SPICE_CLIENT_ERROR,
                                SPICE_CLIENT_ERROR_FAILED,
                                "Failed to initialize smartcard");
        goto end;
    }

    retval = TRUE;

end:
    SPICE_DEBUG("smartcard_manager_init end: %d", retval);
    g_free(emul_args);
    g_free(dbname);
    g_strfreev(certificates);
    return retval;
}

static void smartcard_manager_init_helper(GSimpleAsyncResult *res,
                                          GObject *object,
                                          GCancellable *cancellable)
{
    static GOnce smartcard_manager_once = G_ONCE_INIT;
    SmartcardManagerInitArgs args;

    args.session = SPICE_SESSION(object);
    args.cancellable = cancellable;
    args.err = NULL;


    g_once(&smartcard_manager_once,
           (GThreadFunc)smartcard_manager_init,
           &args);
    if (args.err != NULL) {
        g_simple_async_result_set_from_error(res, args.err);
        g_error_free(args.err);
    }
}


G_GNUC_INTERNAL
void spice_smartcard_manager_init_async(SpiceSession *session,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer opaque)
{
    GSimpleAsyncResult *res;

    res = g_simple_async_result_new(G_OBJECT(session),
                                    callback,
                                    opaque,
                                    spice_smartcard_manager_init);
    g_simple_async_result_run_in_thread(res,
                                        smartcard_manager_init_helper,
                                        G_PRIORITY_DEFAULT,
                                        cancellable);
    g_object_unref(res);
}

G_GNUC_INTERNAL
gboolean spice_smartcard_manager_init_finish(SpiceSession *session,
                                             GAsyncResult *result,
                                             GError **err)
{
    GSimpleAsyncResult *simple;

    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);
    g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(result), FALSE);

    SPICE_DEBUG("smartcard_manager_finish");

    simple = G_SIMPLE_ASYNC_RESULT(result);
    g_return_val_if_fail(g_simple_async_result_get_source_tag(simple) == spice_smartcard_manager_init, FALSE);
    if (g_simple_async_result_propagate_error(simple, err))
        return FALSE;

    spice_smartcard_manager_update_monitor();

    return TRUE;
}

/**
 * spice_smartcard_reader_is_software:
 * @reader: a #SpiceSmartcardReader
 *
 * Tests if @reader is a software (emulated) smartcard reader.
 *
 * Returns: TRUE if @reader is a software (emulated) smartcard reader,
 * FALSE otherwise
 */
gboolean spice_smartcard_reader_is_software(SpiceSmartcardReader *reader)
{
    g_return_val_if_fail(reader != NULL, FALSE);
    return (strcmp(vreader_get_name((VReader*)reader), SPICE_SOFTWARE_READER_NAME) == 0);
}

/**
 * spice_smartcard_reader_insert_card:
 * @reader: a #SpiceSmartcardReader
 *
 * Simulates insertion of a smartcard in the software smartcard reader
 * @reader. If @reader is not a software smartcard reader, FALSE will be
 * returned.
 *
 * Returns: TRUE if insertion of a card was successfully simulated, FALSE
 * otherwise
 */
gboolean spice_smartcard_reader_insert_card(SpiceSmartcardReader *reader)
{
    VCardEmulError status;

    g_return_val_if_fail(spice_smartcard_reader_is_software(reader), FALSE);

    status = vcard_emul_force_card_insert((VReader *)reader);

    return (status == VCARD_EMUL_OK);
}

/**
 * spice_smartcard_reader_remove_card:
 * @reader: a #SpiceSmartcardReader
 *
 * Simulates removal of a smartcard from the software smartcard reader
 * @reader. If @reader is not a software smartcard reader, FALSE will be
 * returned.
 *
 * Returns: TRUE if removal of a card was successfully simulated, FALSE
 * otherwise
 */
gboolean spice_smartcard_reader_remove_card(SpiceSmartcardReader *reader)
{
    VCardEmulError status;

    g_return_val_if_fail(spice_smartcard_reader_is_software(reader), FALSE);

    status = vcard_emul_force_card_remove((VReader *)reader);

    return (status == VCARD_EMUL_OK);
}

/**
 * spice_smartcard_manager_get_readers:
 *
 * manager: a #SpiceSmartcardManager
 *
 * Gets the list of smartcard readers that are currently available, they
 * can be either software (emulated) readers, or hardware ones.
 *
 * Returns: (element-type SpiceSmartcardReader) (transfer full): a newly
 * allocated list of SpiceSmartcardReader instances, or NULL if none were
 * found. When no longer needed, the list must be freed after unreferencing
 * its elements with g_boxed_free()
 *
 * Since: 0.20
 */
GList *spice_smartcard_manager_get_readers(SpiceSmartcardManager *manager)
{

    GList *readers = NULL;
    VReaderList *vreader_list;
    VReaderListEntry *entry;

    vreader_list = vreader_get_reader_list();

    if (vreader_list == NULL)
        return NULL;

    for (entry = vreader_list_get_first(vreader_list);
         entry != NULL;
         entry = vreader_list_get_next(entry)) {
        VReader *reader;

        reader = vreader_list_get_reader(entry);
        g_warn_if_fail(reader != NULL);
        readers = g_list_prepend(readers, vreader_reference(reader));
    }
    vreader_list_delete(vreader_list);

    return g_list_reverse(readers);
}

/**
 * spice_smartcard_manager_insert_card:
 * @manager: a #SpiceSmartcardManager
 *
 * Simulates the insertion of a smartcard in the guest. Valid certificates
 * must have been set in #SpiceSession:smartcard-certificates for software
 * smartcard support to work. At the moment, only one software smartcard
 * reader is supported, that's why there is no parameter to indicate which
 * reader to insert the card in.
 *
 * Returns: TRUE if smartcard insertion was successfully simulated, FALSE
 * if this failed, or if software smartcard support isn't enabled.
 *
 * Since: 0.20
 */
gboolean spice_smartcard_manager_insert_card(SpiceSmartcardManager *manager)
{
    SpiceSmartcardReader *reader;

    g_return_val_if_fail (manager->priv->software_reader != NULL, FALSE);

    reader = (SpiceSmartcardReader *)manager->priv->software_reader;

    return spice_smartcard_reader_insert_card(reader);
}

/**
 * spice_smartcard_manager_remove_card:
 * @manager: a #SpiceSmartcardManager
 *
 * Simulates the removal of a smartcard in the guest. At the moment, only
 * one software smartcard reader is supported, that's why there is no
 * parameter to indicate which reader to insert the card in.
 *
 * Returns: TRUE if smartcard removal was successfully simulated, FALSE
 * if this failed, or if software smartcard support isn't enabled.
 *
 * Since: 0.20
 */
gboolean spice_smartcard_manager_remove_card(SpiceSmartcardManager *manager)
{
    SpiceSmartcardReader *reader;

    g_return_val_if_fail (manager->priv->software_reader != NULL, FALSE);

    reader = (SpiceSmartcardReader *)manager->priv->software_reader;

    return spice_smartcard_reader_remove_card(reader);
}
#else
gboolean spice_smartcard_reader_is_software(SpiceSmartcardReader *reader)
{
    return TRUE;
}

G_GNUC_INTERNAL
void spice_smartcard_manager_init_async(SpiceSession *session,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer opaque)
{
    SPICE_DEBUG("using fake smartcard backend");
}

G_GNUC_INTERNAL
gboolean spice_smartcard_manager_init_finish(SpiceSession *session,
                                             GAsyncResult *result,
                                             GError **err)
{
    g_return_val_if_fail(SPICE_IS_SESSION(session), FALSE);

    return TRUE;
}

gboolean spice_smartcard_manager_insert_card(SpiceSmartcardManager *manager)
{
    return FALSE;
}

gboolean spice_smartcard_manager_remove_card(SpiceSmartcardManager *manager)
{
    return FALSE;
}

gboolean spice_smartcard_reader_insert_card(SpiceSmartcardReader *reader)
{
    return FALSE;
}

gboolean spice_smartcard_reader_remove_card(SpiceSmartcardReader *reader)
{
    return FALSE;
}

GList *spice_smartcard_manager_get_readers(SpiceSmartcardManager *manager)
{
    return NULL;
}

#endif /* USE_SMARTCARD */
