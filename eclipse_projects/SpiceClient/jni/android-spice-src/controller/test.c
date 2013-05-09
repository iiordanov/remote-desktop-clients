/* Copyright (C) 2011 Red Hat, Inc. */

/* This library is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU Lesser General Public */
/* License as published by the Free Software Foundation; either */
/* version 2.1 of the License, or (at your option) any later version. */

/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU */
/* Lesser General Public License for more details. */

/* You should have received a copy of the GNU Lesser General Public */
/* License along with this library; if not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <spice/controller_prot.h>

#include "spice-controller.h"

#ifdef WIN32
#include <windows.h>
#define PIPE_NAME TEXT("\\\\.\\pipe\\SpiceController-%lu")
static HANDLE pipe = INVALID_HANDLE_VALUE;
#else

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define PIPE_NAME "/tmp/test"
static int sock = -1;

#endif

#define PIPE_NAME_MAX_LEN 256

void write_to_pipe (const void* data, size_t len)
{
#ifdef WIN32
    DWORD written;
    if (!WriteFile (pipe, data, len, &written, NULL) || written != len) {
        printf("Write to pipe failed %u\n", GetLastError());
    }
#else
    if (send (sock, data, len, 0) != len) {
        printf ("send failed, (%d) %s\n", errno, strerror(errno));
    }
#endif
}

gboolean send_init (void)
{
    ControllerInit msg = {
      { CONTROLLER_MAGIC, CONTROLLER_VERSION, sizeof (msg) },
      0,
      CONTROLLER_FLAG_EXCLUSIVE
    };

    write_to_pipe(&msg, sizeof (msg));
    return FALSE;
}

void send_msg (uint32_t id)
{
    ControllerMsg msg = {
      id, sizeof (msg)
    };

    write_to_pipe (&msg, sizeof (msg));
}

void send_value (uint32_t id, uint32_t value)
{
    ControllerValue msg = {
      { id, sizeof(msg) },
      value
    };

    write_to_pipe (&msg, sizeof (msg));
}

void send_data (uint32_t id, uint8_t* data, size_t data_size)
{
    size_t size = sizeof (ControllerData) + data_size;
    ControllerData* msg = (ControllerData*)malloc (size);

    msg->base.id = id;
    msg->base.size = (uint32_t)size;
    memcpy (msg->data, data, data_size);
    write_to_pipe (msg, size);
    free (msg);
}

ssize_t read_from_pipe (void* data, size_t size)
{
    ssize_t read;
#ifdef WIN32
    if (!ReadFile (pipe, data, size, &read, NULL)) {
        printf ("Read from pipe failed %u\n", GetLastError());
    }
#else
    read = recv (sock, data, size, 0);
    if ((read == -1 || read == 0)) {
        printf ("recv failed, (%d) %s\n", errno, strerror (errno));
    }
#endif
    return read;
}

#define HOST "localhost"
#define PORT 5931
#define SPORT 0
#define PWD "P@s5w0rd"
#define SECURE_CHANNELS "main,inputs,playback"
#define DISABLED_CHANNELS "playback,record"
#define TITLE "Hello from controller"
#define HOTKEYS "toggle-fullscreen=shift+f1,release-cursor=shift+f2"
#define MENU "0\r4864\rS&end Ctrl+Alt+Del\tCtrl+Alt+End\r0\r\n" \
    "0\r5120\r&Toggle full screen\tShift+F11\r0\r\n" \
    "0\r1\r&Special keys\r4\r\n" \
    "1\r5376\r&Send Shift+F11\r0\r\n" \
    "1\r5632\r&Send Shift+F12\r0\r\n" \
    "1\r5888\r&Send Ctrl+Alt+End\r0\r\n" \
    "0\r1\r-\r1\r\n" \
    "0\r2\rChange CD\r4\r\n" \
    "2\r3\rNo CDs\r0\r\n" \
    "2\r4\r[Eject]\r0\r\n" \
    "0\r5\r-\r1\r\n" \
    "0\r6\rPlay\r0\r\n" \
    "0\r7\rSuspend\r0\r\n" \
    "0\r8\rStop\r0\r\n"

#define TLS_CIPHERS "TLS_C1PHERS"
#define CA_FILE "C@_FILE"
#define HOST_SUBJECT "Host_SUBJ3CT"

SpiceCtrlController *ctrl;
GMainLoop *loop;

void signaled (GObject    *gobject, const gchar *signal_name)
{
    g_message ("signaled: %s", signal_name);
    if (g_str_equal (signal_name, "hide")) {
      spice_ctrl_controller_menu_item_click_msg (ctrl, 42);
      g_timeout_add (1000, (GSourceFunc)g_main_loop_quit, loop);
    }
}

void notified (GObject    *gobject, GParamSpec *pspec,
               gpointer    user_data)
{
    GValue value = { 0, };
    GValue strvalue = { 0, };

    g_return_if_fail (gobject != NULL);
    g_return_if_fail (pspec != NULL);

    g_value_init (&value, pspec->value_type);
    g_value_init (&strvalue, G_TYPE_STRING);
    g_object_get_property (gobject, pspec->name, &value);

    if (pspec->value_type == G_TYPE_STRV) {
      gchar** p = (gchar **)g_value_get_boxed (&value);
      g_message ("notify::%s == ", pspec->name);
      while (*p)
        g_message (*p++);
    } else if (pspec->value_type == G_TYPE_OBJECT) {
      GObject *o = g_value_get_object (&value);
      g_message ("notify::%s == %s", G_OBJECT_TYPE_NAME (o));
    } else {
      g_value_transform (&value, &strvalue);
      g_message ("notify::%s  = %s", pspec->name, g_value_get_string (&strvalue));
    }

    g_value_unset (&value);
    g_value_unset (&strvalue);
}

int main (int argc, char *argv[])
{
    int spicec_pid = (argc > 1 ? atoi (argv[1]) : 0);
    char* host = (argc > 2 ? argv[2] : (char*)HOST);
    int port = (argc > 3 ? atoi (argv[3]) : PORT);
    char pipe_name[PIPE_NAME_MAX_LEN];
    ControllerValue msg;
    ssize_t read;

    g_type_init ();
    ctrl = spice_ctrl_controller_new ();
    loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect (ctrl, "notify", G_CALLBACK (notified), NULL);
    g_signal_connect (ctrl, "show", G_CALLBACK (signaled), "show");
    g_signal_connect (ctrl, "hide", G_CALLBACK (signaled), "hide");
    g_signal_connect (ctrl, "do_connect", G_CALLBACK (signaled), "do_connect");

#ifdef WIN32
    snprintf (pipe_name, PIPE_NAME_MAX_LEN, PIPE_NAME, spicec_pid);
    spice_ctrl_controller_listen (ctrl, pipe_name, NULL, NULL);

    printf ("Creating Spice controller connection %s\n", pipe_name);
    pipe = CreateFile (pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        printf ("Could not open pipe %u\n", GetLastError());
        return -1;
    }
#else
    spice_ctrl_controller_listen (ctrl, PIPE_NAME, NULL, NULL);

    snprintf (pipe_name, PIPE_NAME_MAX_LEN, PIPE_NAME);
    printf ("Creating a controller connection %s\n", pipe_name);
    struct sockaddr_un remote;
    if ((sock = socket (AF_UNIX, SOCK_STREAM, 0)) == -1) {
        printf ("Could not open socket, (%d) %s\n", errno, strerror(errno));
        return -1;
    }
    remote.sun_family = AF_UNIX;
    strcpy (remote.sun_path, pipe_name);
    if (connect (sock, (struct sockaddr *)&remote,
                strlen (remote.sun_path) + sizeof(remote.sun_family)) == -1) {
        printf ("Socket connect failed, (%d) %s\n", errno, strerror(errno));
        close (sock);
        return -1;
    }
#endif

    /* TODO: we rely on socket / pipe buffer... which is lame :) */
    send_init ();

    send_data (CONTROLLER_HOST, (uint8_t*)host, strlen(host) + 1);
    send_value (CONTROLLER_PORT, port);
    send_value (CONTROLLER_SPORT, SPORT);
    send_data (CONTROLLER_PASSWORD, (uint8_t*)PWD, sizeof(PWD) + 1);
    send_data (CONTROLLER_SECURE_CHANNELS, (uint8_t*)SECURE_CHANNELS, sizeof(SECURE_CHANNELS) + 1);
    send_data (CONTROLLER_DISABLE_CHANNELS, (uint8_t*)DISABLED_CHANNELS, sizeof(DISABLED_CHANNELS) + 1);
    send_data (CONTROLLER_TLS_CIPHERS, (uint8_t*)TLS_CIPHERS, sizeof(TLS_CIPHERS) + 1);
    send_data (CONTROLLER_CA_FILE, (uint8_t*)CA_FILE, sizeof(CA_FILE) + 1);
    send_data (CONTROLLER_HOST_SUBJECT, (uint8_t*)HOST_SUBJECT, sizeof(HOST_SUBJECT) + 1);
    send_data (CONTROLLER_SET_TITLE, (uint8_t*)TITLE, sizeof(TITLE) + 1);
    send_data (CONTROLLER_HOTKEYS, (uint8_t*)HOTKEYS, sizeof(HOTKEYS) + 1);
    send_data (CONTROLLER_CREATE_MENU, (uint8_t*)MENU, sizeof(MENU));

    send_value (CONTROLLER_FULL_SCREEN, /*CONTROLLER_SET_FULL_SCREEN |*/ CONTROLLER_AUTO_DISPLAY_RES);

    send_msg (CONTROLLER_SHOW);
    send_msg (CONTROLLER_CONNECT);
    send_msg (CONTROLLER_SHOW);
    send_msg (CONTROLLER_DELETE_MENU);
    send_msg (CONTROLLER_HIDE);

    g_main_loop_run (loop);

    while ((read = read_from_pipe (&msg, sizeof(msg))) == sizeof(msg)) {
        printf ("Received id %u, size %u, value %u\n", msg.base.id, msg.base.size, msg.value);
        if (msg.value == 42)
          break;
    }

#ifdef WIN32
    CloseHandle (pipe);
#else
    close (sock);
#endif
    g_object_unref (ctrl);
    return 0;
}
