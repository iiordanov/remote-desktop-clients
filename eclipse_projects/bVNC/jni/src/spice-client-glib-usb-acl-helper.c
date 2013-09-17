/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2011,2012 Red Hat, Inc.
   Copyright (C) 2009 Kay Sievers <kay.sievers@vrfy.org>

   Red Hat Authors:
   Hans de Goede <hdegoede@redhat.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2 of the License,
   or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gio/gunixinputstream.h>
#include <polkit/polkit.h>
#include <acl/libacl.h>

#include "glib-compat.h"

#define FATAL_ERROR(...) \
    do { \
        /* We print the error both to stdout, for the app invoking us and \
           stderr for the end user */ \
        fprintf(stdout, "Error " __VA_ARGS__); \
        fprintf(stderr, "spice-client-glib-usb-helper: Error " __VA_ARGS__); \
        exit_status = 1; \
        cleanup(); \
    } while (0)

#define ERROR(...) \
    do { \
        fprintf(stdout, __VA_ARGS__); \
        cleanup(); \
    } while (0)

enum state {
    STATE_WAITING_FOR_BUS_N_DEV,
    STATE_WAITING_FOR_POL_KIT,
    STATE_WAITING_FOR_STDIN_EOF,
};

static enum state state = STATE_WAITING_FOR_BUS_N_DEV;
static int exit_status;
static int busnum, devnum;
static char path[PATH_MAX];
static GMainLoop *loop;
static GDataInputStream *stdin_stream;
static GCancellable *polkit_cancellable;
static PolkitSubject *subject;
static PolkitAuthority *authority;

/*
 * This function is a copy of the same function in udev, written by Kay
 * Sievers, you can find it in udev in extras/udev-acl/udev-acl.c
 */
static int set_facl(const char* filename, uid_t uid, int add)
{
    int get;
    acl_t acl;
    acl_entry_t entry = NULL;
    acl_entry_t e;
    acl_permset_t permset;
    int ret;

    /* don't touch ACLs for root */
    if (uid == 0)
        return 0;

    /* read current record */
    acl = acl_get_file(filename, ACL_TYPE_ACCESS);
    if (!acl)
        return -1;

    /* locate ACL_USER entry for uid */
    get = acl_get_entry(acl, ACL_FIRST_ENTRY, &e);
    while (get == 1) {
        acl_tag_t t;

        acl_get_tag_type(e, &t);
        if (t == ACL_USER) {
            uid_t *u;

            u = (uid_t*)acl_get_qualifier(e);
            if (u == NULL) {
                ret = -1;
                goto out;
            }
            if (*u == uid) {
                entry = e;
                acl_free(u);
                break;
            }
            acl_free(u);
        }

        get = acl_get_entry(acl, ACL_NEXT_ENTRY, &e);
    }

    /* remove ACL_USER entry for uid */
    if (!add) {
        if (entry == NULL) {
            ret = 0;
            goto out;
        }
        acl_delete_entry(acl, entry);
        goto update;
    }

    /* create ACL_USER entry for uid */
    if (entry == NULL) {
        ret = acl_create_entry(&acl, &entry);
        if (ret != 0)
            goto out;
        acl_set_tag_type(entry, ACL_USER);
        acl_set_qualifier(entry, &uid);
    }

    /* add permissions for uid */
    acl_get_permset(entry, &permset);
    acl_add_perm(permset, ACL_READ|ACL_WRITE);
update:
    /* update record */
    acl_calc_mask(&acl);
    ret = acl_set_file(filename, ACL_TYPE_ACCESS, acl);
    if (ret != 0)
        goto out;
out:
    acl_free(acl);
    return ret;
}

static void cleanup(void)
{
    if (polkit_cancellable)
        g_cancellable_cancel(polkit_cancellable);

    if (state == STATE_WAITING_FOR_STDIN_EOF)
        set_facl(path, getuid(), 0);

    if (loop)
        g_main_loop_quit(loop);
}

/* Not available in polkit < 0.101 */
#if !HAVE_POLKIT_AUTHORIZATION_RESULT_GET_DISMISSED
static gboolean
polkit_authorization_result_get_dismissed(PolkitAuthorizationResult *result)
{
    gboolean ret;
    PolkitDetails *details;

    g_return_val_if_fail(POLKIT_IS_AUTHORIZATION_RESULT(result), FALSE);

    ret = FALSE;
    details = polkit_authorization_result_get_details(result);
    if (details != NULL && polkit_details_lookup(details, "polkit.dismissed"))
        ret = TRUE;

    return ret;
}
#endif

static void check_authorization_cb(PolkitAuthority *authority,
                                   GAsyncResult *res, gpointer data)
{
    PolkitAuthorizationResult *result;
    GError *err = NULL;
    struct stat stat_buf;

    g_clear_object(&polkit_cancellable);

    result = polkit_authority_check_authorization_finish(authority, res, &err);
    if (err) {
        FATAL_ERROR("PoliciKit error: %s\n", err->message);
        g_error_free(err);
        return;
    }

    if (polkit_authorization_result_get_dismissed(result)) {
        ERROR("CANCELED\n");
        return;
    }

    if (!polkit_authorization_result_get_is_authorized(result)) {
        ERROR("Not authorized\n");
        return;
    }

    snprintf(path, PATH_MAX, "/dev/bus/usb/%03d/%03d", busnum, devnum);

    if (stat(path, &stat_buf) != 0) {
        FATAL_ERROR("statting %s: %s\n", path, strerror(errno));
        return;
    }
    if (!S_ISCHR(stat_buf.st_mode)) {
        FATAL_ERROR("%s is not a character device\n", path);
        return;
    }

    if (set_facl(path, getuid(), 1)) {
        FATAL_ERROR("setting facl: %s\n", strerror(errno));
        return;
    }

    fprintf(stdout, "SUCCESS\n");
    fflush(stdout);
    state = STATE_WAITING_FOR_STDIN_EOF;
}

static void stdin_read_complete(GObject *src, GAsyncResult *res, gpointer data)
{
    char *s, *ep;
    GError *err = NULL;
    gsize len;

    s = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(src), res,
                                             &len, &err);
    if (!s) {
        if (err) {
            FATAL_ERROR("Reading from stdin: %s\n", err->message);
            g_error_free(err);
            return;
        }

        switch (state) {
        case STATE_WAITING_FOR_BUS_N_DEV:
            FATAL_ERROR("EOF while waiting for bus and device num\n");
            break;
        case STATE_WAITING_FOR_POL_KIT:
            ERROR("Cancelled while waiting for authorization\n");
            break;
        case STATE_WAITING_FOR_STDIN_EOF:
            cleanup();
            break;
        }
        return;
    }

    switch (state) {
    case STATE_WAITING_FOR_BUS_N_DEV:
        busnum = strtol(s, &ep, 10);
        if (!isspace(*ep)) {
            FATAL_ERROR("Invalid busnum / devnum: %s\n", s);
            break;
        }
        devnum = strtol(ep, &ep, 10);
        if (*ep != '\0') {
            FATAL_ERROR("Invalid busnum / devnum: %s\n", s);
            break;
        }

        /*
         * The set_facl() call is a no-op for root, so no need to ask PolKit
         * and then if ok call set_facl(), when called by a root process.
         */
        if (getuid() != 0) {
            polkit_cancellable = g_cancellable_new();
            polkit_authority_check_authorization(
                authority, subject, "org.spice-space.lowlevelusbaccess", NULL,
                POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                polkit_cancellable,
                (GAsyncReadyCallback)check_authorization_cb, NULL);
            state = STATE_WAITING_FOR_POL_KIT;
        } else {
            fprintf(stdout, "SUCCESS\n");
            fflush(stdout);
            state = STATE_WAITING_FOR_STDIN_EOF;
        }

        g_data_input_stream_read_line_async(stdin_stream, G_PRIORITY_DEFAULT,
                                            NULL, stdin_read_complete, NULL);
        break;
    default:
        FATAL_ERROR("Unexpected extra input in state %d: %s\n", state, s);
    }
    g_free(s);
}

/* Fix for polkit 0.97 and later */
#if !HAVE_POLKIT_AUTHORITY_GET_SYNC
static PolkitAuthority *
polkit_authority_get_sync (GCancellable *cancellable, GError **error)
{
    PolkitAuthority *authority;

    authority = polkit_authority_get ();
    if (!authority)
        g_set_error (error, 0, 0, "failed to get the PolicyKit authority");

    return authority;
}
#endif

#ifndef HAVE_CLEARENV
extern char **environ;

static int
clearenv (void)
{
        if (environ != NULL)
                environ[0] = NULL;
        return 0;
}
#endif

int main(void)
{
    pid_t parent_pid;
    GInputStream *stdin_unix_stream;

  /* Nuke the environment to get a well-known and sanitized
   * environment to avoid attacks via e.g. the DBUS_SYSTEM_BUS_ADDRESS
   * environment variable and similar.
   */
    if (clearenv () != 0) {
        FATAL_ERROR("Error clearing environment: %s\n", g_strerror (errno));
        return 1;
    }

    g_type_init();

    loop = g_main_loop_new(NULL, FALSE);

    authority = polkit_authority_get_sync(NULL, NULL);
    parent_pid = getppid ();
    if (parent_pid == 1) {
            FATAL_ERROR("Parent process was reaped by init(1)\n");
            return 1;
    }
    subject = polkit_unix_process_new(parent_pid);

    stdin_unix_stream = g_unix_input_stream_new(STDIN_FILENO, 0);
    stdin_stream = g_data_input_stream_new(stdin_unix_stream);
    g_data_input_stream_set_newline_type(stdin_stream,
                                         G_DATA_STREAM_NEWLINE_TYPE_LF);
    g_clear_object(&stdin_unix_stream);
    g_data_input_stream_read_line_async(stdin_stream, G_PRIORITY_DEFAULT, NULL,
                                        stdin_read_complete, NULL);

    g_main_loop_run(loop);

    if (polkit_cancellable)
        g_clear_object(&polkit_cancellable);
    g_object_unref(stdin_stream);
    g_object_unref(authority);
    g_object_unref(subject);
    g_main_loop_unref(loop);

    return exit_status;
}
