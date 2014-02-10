/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

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
#include "spice-client.h"
#include "spice-common.h"
#include "spice-channel-priv.h"

/**
 * SECTION:channel-inputs
 * @short_description: control the server mouse and keyboard
 * @title: Inputs Channel
 * @section_id:
 * @see_also: #SpiceChannel, and the GTK widget #SpiceDisplay
 * @stability: Stable
 * @include: channel-inputs.h
 *
 * Spice supports sending keyboard key events and keyboard leds
 * synchronization. The key events are sent using
 * spice_inputs_key_press() and spice_inputs_key_release() using PC AT
 * scancode.
 *
 * Guest keyboard leds state can be manipulated with
 * spice_inputs_set_key_locks(). When key lock change, a notification
 * is emitted with #SpiceInputsChannel::inputs-modifiers signal.
 */

#define SPICE_INPUTS_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_INPUTS_CHANNEL, SpiceInputsChannelPrivate))

struct _SpiceInputsChannelPrivate {
    int                         bs;
    int                         dx, dy;
    unsigned int                x, y, dpy;
    int                         motion_count;
    int                         modifiers;
    guint32                     locks;
};

G_DEFINE_TYPE(SpiceInputsChannel, spice_inputs_channel, SPICE_TYPE_CHANNEL)

/* Properties */
enum {
    PROP_0,
    PROP_KEY_MODIFIERS,
};

/* Signals */
enum {
    SPICE_INPUTS_MODIFIERS,

    SPICE_INPUTS_LAST_SIGNAL,
};

static guint signals[SPICE_INPUTS_LAST_SIGNAL];

static void spice_inputs_channel_up(SpiceChannel *channel);
static void spice_inputs_channel_reset(SpiceChannel *channel, gboolean migrating);
static void channel_set_handlers(SpiceChannelClass *klass);

/* ------------------------------------------------------------------ */

static void spice_inputs_channel_init(SpiceInputsChannel *channel)
{
    channel->priv = SPICE_INPUTS_CHANNEL_GET_PRIVATE(channel);
}

static void spice_inputs_get_property(GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
    SpiceInputsChannelPrivate *c = SPICE_INPUTS_CHANNEL(object)->priv;

    switch (prop_id) {
    case PROP_KEY_MODIFIERS:
        g_value_set_int(value, c->modifiers);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void spice_inputs_channel_finalize(GObject *obj)
{
    if (G_OBJECT_CLASS(spice_inputs_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_inputs_channel_parent_class)->finalize(obj);
}

static void spice_inputs_channel_class_init(SpiceInputsChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->finalize     = spice_inputs_channel_finalize;
    gobject_class->get_property = spice_inputs_get_property;
    channel_class->channel_up   = spice_inputs_channel_up;
    channel_class->channel_reset = spice_inputs_channel_reset;

    g_object_class_install_property
        (gobject_class, PROP_KEY_MODIFIERS,
         g_param_spec_int("key-modifiers",
                          "Key modifiers",
                          "Guest keyboard lock/led state",
                          0, INT_MAX, 0,
                          G_PARAM_READABLE |
                          G_PARAM_STATIC_NAME |
                          G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

    /**
     * SpiceInputsChannel::inputs-modifier:
     * @display: the #SpiceInputsChannel that emitted the signal
     *
     * The #SpiceInputsChannel::inputs-modifier signal is emitted when
     * the guest keyboard locks are changed. You can read the current
     * state from #SpiceInputsChannel:key-modifiers property.
     **/
    /* TODO: use notify instead? */
    signals[SPICE_INPUTS_MODIFIERS] =
        g_signal_new("inputs-modifiers",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpiceInputsChannelClass, inputs_modifiers),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    g_type_class_add_private(klass, sizeof(SpiceInputsChannelPrivate));
    channel_set_handlers(SPICE_CHANNEL_CLASS(klass));
}

/* signal trampoline---------------------------------------------------------- */

struct SPICE_INPUTS_MODIFIERS {
};

/* main context */
static void do_emit_main_context(GObject *object, int signum, gpointer params)
{
    switch (signum) {
    case SPICE_INPUTS_MODIFIERS: {
        g_signal_emit(object, signals[signum], 0);
        break;
    }
    default:
        g_warn_if_reached();
    }
}

/* ------------------------------------------------------------------ */

static SpiceMsgOut* mouse_motion(SpiceInputsChannel *channel)
{
    SpiceInputsChannelPrivate *c = channel->priv;
    SpiceMsgcMouseMotion motion;
    SpiceMsgOut *msg;

    if (!c->dx && !c->dy)
        return NULL;

    motion.buttons_state = c->bs;
    motion.dx            = c->dx;
    motion.dy            = c->dy;
    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_MOTION);
    msg->marshallers->msgc_inputs_mouse_motion(msg->marshaller, &motion);

    c->motion_count++;
    c->dx = 0;
    c->dy = 0;

    return msg;
}

static SpiceMsgOut* mouse_position(SpiceInputsChannel *channel)
{
    SpiceInputsChannelPrivate *c = channel->priv;
    SpiceMsgcMousePosition position;
    SpiceMsgOut *msg;

    if (c->dpy == -1)
        return NULL;

    /* CHANNEL_DEBUG(channel, "%s: +%d+%d", __FUNCTION__, c->x, c->y); */
    position.buttons_state = c->bs;
    position.x             = c->x;
    position.y             = c->y;
    position.display_id    = c->dpy;
    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_POSITION);
    msg->marshallers->msgc_inputs_mouse_position(msg->marshaller, &position);

    c->motion_count++;
    c->dpy = -1;

    return msg;
}

/* main context */
static void send_position(SpiceInputsChannel *channel)
{
    SpiceMsgOut *msg;

    if (spice_channel_get_read_only(SPICE_CHANNEL(channel)))
        return;

    msg = mouse_position(channel);
    if (!msg) /* if no motion */
        return;

    spice_msg_out_send(msg);
}

/* main context */
static void send_motion(SpiceInputsChannel *channel)
{
    SpiceMsgOut *msg;

    if (spice_channel_get_read_only(SPICE_CHANNEL(channel)))
        return;

    msg = mouse_motion(channel);
    if (!msg) /* if no motion */
        return;

    spice_msg_out_send(msg);
}

/* coroutine context */
static void inputs_handle_init(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceInputsChannelPrivate *c = SPICE_INPUTS_CHANNEL(channel)->priv;
    SpiceMsgInputsInit *init = spice_msg_in_parsed(in);

    c->modifiers = init->keyboard_modifiers;
    emit_main_context(channel, SPICE_INPUTS_MODIFIERS);
}

/* coroutine context */
static void inputs_handle_modifiers(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceInputsChannelPrivate *c = SPICE_INPUTS_CHANNEL(channel)->priv;
    SpiceMsgInputsKeyModifiers *modifiers = spice_msg_in_parsed(in);

    c->modifiers = modifiers->modifiers;
    emit_main_context(channel, SPICE_INPUTS_MODIFIERS);
}

/* coroutine context */
static void inputs_handle_ack(SpiceChannel *channel, SpiceMsgIn *in)
{
    SpiceInputsChannelPrivate *c = SPICE_INPUTS_CHANNEL(channel)->priv;
    SpiceMsgOut *msg;

    c->motion_count -= SPICE_INPUT_MOTION_ACK_BUNCH;

    msg = mouse_motion(SPICE_INPUTS_CHANNEL(channel));
    if (msg) { /* if no motion, msg == NULL */
        spice_msg_out_send_internal(msg);
    }

    msg = mouse_position(SPICE_INPUTS_CHANNEL(channel));
    if (msg) {
        spice_msg_out_send_internal(msg);
    }
}

static void channel_set_handlers(SpiceChannelClass *klass)
{
    static const spice_msg_handler handlers[] = {
        [ SPICE_MSG_INPUTS_INIT ]              = inputs_handle_init,
        [ SPICE_MSG_INPUTS_KEY_MODIFIERS ]     = inputs_handle_modifiers,
        [ SPICE_MSG_INPUTS_MOUSE_MOTION_ACK ]  = inputs_handle_ack,
    };

    spice_channel_set_handlers(klass, handlers, G_N_ELEMENTS(handlers));
}

/**
 * spice_inputs_motion:
 * @channel:
 * @dx: delta X mouse coordinates
 * @dy: delta Y mouse coordinates
 * @button_state: SPICE_MOUSE_BUTTON_MASK flags
 *
 * Change mouse position (used in SPICE_MOUSE_MODE_CLIENT).
 **/
void spice_inputs_motion(SpiceInputsChannel *channel, gint dx, gint dy,
                         gint button_state)
{
    SpiceInputsChannelPrivate *c;

    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_UNCONNECTED);
    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;

    if (dx == 0 && dy == 0)
        return;

    c = channel->priv;
    c->bs  = button_state;
    c->dx += dx;
    c->dy += dy;

    if (c->motion_count < SPICE_INPUT_MOTION_ACK_BUNCH * 2) {
        send_motion(channel);
    }
}

/**
 * spice_inputs_position:
 * @channel:
 * @x: X mouse coordinates
 * @y: Y mouse coordinates
 * @display: display channel id
 * @button_state: SPICE_MOUSE_BUTTON_MASK flags
 *
 * Change mouse position (used in SPICE_MOUSE_MODE_CLIENT).
 **/
void spice_inputs_position(SpiceInputsChannel *channel, gint x, gint y,
                           gint display, gint button_state)
{
    SpiceInputsChannelPrivate *c;

    g_return_if_fail(channel != NULL);

    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;

    c = channel->priv;
    c->bs  = button_state;
    c->x   = x;
    c->y   = y;
    c->dpy = display;

    if (c->motion_count < SPICE_INPUT_MOTION_ACK_BUNCH * 2) {
        send_position(channel);
    } else {
        CHANNEL_DEBUG(channel, "over SPICE_INPUT_MOTION_ACK_BUNCH * 2, dropping");
    }
}

/**
 * spice_inputs_button_press:
 * @channel:
 * @button: a SPICE_MOUSE_BUTTON
 * @button_state: SPICE_MOUSE_BUTTON_MASK flags
 *
 * Press a mouse button.
 **/
void spice_inputs_button_press(SpiceInputsChannel *channel, gint button,
                               gint button_state)
{
    SpiceInputsChannelPrivate *c;
    SpiceMsgcMousePress press;
    SpiceMsgOut *msg;

    g_return_if_fail(channel != NULL);

    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;
    if (spice_channel_get_read_only(SPICE_CHANNEL(channel)))
        return;

    c = channel->priv;
    switch (button) {
    case SPICE_MOUSE_BUTTON_LEFT:
        button_state |= SPICE_MOUSE_BUTTON_MASK_LEFT;
        break;
    case SPICE_MOUSE_BUTTON_MIDDLE:
        button_state |= SPICE_MOUSE_BUTTON_MASK_MIDDLE;
        break;
    case SPICE_MOUSE_BUTTON_RIGHT:
        button_state |= SPICE_MOUSE_BUTTON_MASK_RIGHT;
        break;
    }

    c->bs  = button_state;
    send_motion(channel);
    send_position(channel);

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_PRESS);
    press.button = button;
    press.buttons_state = button_state;
    msg->marshallers->msgc_inputs_mouse_press(msg->marshaller, &press);
    spice_msg_out_send(msg);
}

/**
 * spice_inputs_button_release:
 * @channel:
 * @button: a SPICE_MOUSE_BUTTON
 * @button_state: SPICE_MOUSE_BUTTON_MASK flags
 *
 * Release a button.
 **/
void spice_inputs_button_release(SpiceInputsChannel *channel, gint button,
                                 gint button_state)
{
    SpiceInputsChannelPrivate *c;
    SpiceMsgcMouseRelease release;
    SpiceMsgOut *msg;

    g_return_if_fail(channel != NULL);

    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;
    if (spice_channel_get_read_only(SPICE_CHANNEL(channel)))
        return;

    c = channel->priv;
    switch (button) {
    case SPICE_MOUSE_BUTTON_LEFT:
        button_state &= ~SPICE_MOUSE_BUTTON_MASK_LEFT;
        break;
    case SPICE_MOUSE_BUTTON_MIDDLE:
        button_state &= ~SPICE_MOUSE_BUTTON_MASK_MIDDLE;
        break;
    case SPICE_MOUSE_BUTTON_RIGHT:
        button_state &= ~SPICE_MOUSE_BUTTON_MASK_RIGHT;
        break;
    }

    c->bs = button_state;
    send_motion(channel);
    send_position(channel);

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_MOUSE_RELEASE);
    release.button = button;
    release.buttons_state = button_state;
    msg->marshallers->msgc_inputs_mouse_release(msg->marshaller, &release);
    spice_msg_out_send(msg);
}

/**
 * spice_inputs_key_press:
 * @channel:
 * @scancode: a PC AT key scancode
 *
 * Press a key.
 **/
void spice_inputs_key_press(SpiceInputsChannel *channel, guint scancode)
{
    SpiceMsgcKeyDown down;
    SpiceMsgOut *msg;

    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_UNCONNECTED);
    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;
    if (spice_channel_get_read_only(SPICE_CHANNEL(channel)))
        return;

    down.code = spice_make_scancode(scancode, FALSE);
    msg = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_INPUTS_KEY_DOWN);
    msg->marshallers->msgc_inputs_key_down(msg->marshaller, &down);
    spice_msg_out_send(msg);
}

/**
 * spice_inputs_key_release:
 * @channel:
 * @scancode: a PC AT key scancode
 *
 * Release a key.
 **/
void spice_inputs_key_release(SpiceInputsChannel *channel, guint scancode)
{
    SpiceMsgcKeyUp up;
    SpiceMsgOut *msg;

    g_return_if_fail(channel != NULL);
    g_return_if_fail(SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_UNCONNECTED);
    if (SPICE_CHANNEL(channel)->priv->state != SPICE_CHANNEL_STATE_READY)
        return;
    if (spice_channel_get_read_only(SPICE_CHANNEL(channel)))
        return;

    up.code = spice_make_scancode(scancode, TRUE);
    msg = spice_msg_out_new(SPICE_CHANNEL(channel), SPICE_MSGC_INPUTS_KEY_UP);
    msg->marshallers->msgc_inputs_key_up(msg->marshaller, &up);
    spice_msg_out_send(msg);
}

/**
 * spice_inputs_key_press_and_release:
 * @channel:
 * @scancode: a PC AT key scancode
 *
 * Press and release a key event atomically (in the same message).
 *
 * Since: 0.13
 **/
void spice_inputs_key_press_and_release(SpiceInputsChannel *input_channel, guint scancode)
{
    SpiceChannel *channel = SPICE_CHANNEL(input_channel);

    g_return_if_fail(channel != NULL);
    g_return_if_fail(channel->priv->state != SPICE_CHANNEL_STATE_UNCONNECTED);

    if (channel->priv->state != SPICE_CHANNEL_STATE_READY)
        return;
    if (spice_channel_get_read_only(channel))
        return;

    if (spice_channel_test_capability(channel, SPICE_INPUTS_CAP_KEY_SCANCODE)) {
        SpiceMsgOut *msg;
        guint16 code;
        guint8 *buf;

        msg = spice_msg_out_new(channel, SPICE_MSGC_INPUTS_KEY_SCANCODE);
        if (scancode < 0x100) {
            buf = (guint8*)spice_marshaller_reserve_space(msg->marshaller, 2);
            buf[0] = spice_make_scancode(scancode, FALSE);
            buf[1] = spice_make_scancode(scancode, TRUE);
        } else {
            buf = (guint8*)spice_marshaller_reserve_space(msg->marshaller, 4);
            code = spice_make_scancode(scancode, FALSE);
            buf[0] = code & 0xff;
            buf[1] = code >> 8;
            code = spice_make_scancode(scancode, TRUE);
            buf[2] = code & 0xff;
            buf[3] = code >> 8;
        }
        spice_msg_out_send(msg);
    } else {
        CHANNEL_DEBUG(channel, "The server doesn't support atomic press and release");
        spice_inputs_key_press(input_channel, scancode);
        spice_inputs_key_release(input_channel, scancode);
    }
}

/* main or coroutine context */
static SpiceMsgOut* set_key_locks(SpiceInputsChannel *channel, guint locks)
{
    SpiceMsgcKeyModifiers modifiers;
    SpiceMsgOut *msg;
    SpiceInputsChannelPrivate *ic;
    SpiceChannelPrivate *c;

    g_return_val_if_fail(SPICE_IS_INPUTS_CHANNEL(channel), NULL);

    ic = channel->priv;
    c = SPICE_CHANNEL(channel)->priv;

    ic->locks = locks;
    if (c->state != SPICE_CHANNEL_STATE_READY)
        return NULL;

    msg = spice_msg_out_new(SPICE_CHANNEL(channel),
                            SPICE_MSGC_INPUTS_KEY_MODIFIERS);
    modifiers.modifiers = locks;
    msg->marshallers->msgc_inputs_key_modifiers(msg->marshaller, &modifiers);
    return msg;
}

/**
 * spice_inputs_set_key_locks:
 * @channel:
 * @locks: #SpiceInputsLock modifiers flags
 *
 * Set the keyboard locks on the guest (Caps, Num, Scroll..)
 **/
void spice_inputs_set_key_locks(SpiceInputsChannel *channel, guint locks)
{
    SpiceMsgOut *msg;

    if (spice_channel_get_read_only(SPICE_CHANNEL(channel)))
        return;

    msg = set_key_locks(channel, locks);
    if (!msg) /* you can set_key_locks() even if the channel is not ready */
        return;

    spice_msg_out_send(msg); /* main -> coroutine */
}

/* coroutine context */
static void spice_inputs_channel_up(SpiceChannel *channel)
{
    SpiceInputsChannelPrivate *c = SPICE_INPUTS_CHANNEL(channel)->priv;
    SpiceMsgOut *msg;

    if (spice_channel_get_read_only(channel))
        return;

    msg = set_key_locks(SPICE_INPUTS_CHANNEL(channel), c->locks);
    spice_msg_out_send_internal(msg);
}

static void spice_inputs_channel_reset(SpiceChannel *channel, gboolean migrating)
{
    SpiceInputsChannelPrivate *c = SPICE_INPUTS_CHANNEL(channel)->priv;
    c->motion_count = 0;

    SPICE_CHANNEL_CLASS(spice_inputs_channel_parent_class)->channel_reset(channel, migrating);
}
