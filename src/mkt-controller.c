/* mkt-controller.c
 *
 * Copyright 2021, 2023  Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "mkt-controller"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <libudev.h>
#include <libinput.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <xkbcommon/xkbcommon.h>

#include "mkt-utils.h"
#include "mkt-controller.h"
#include "mkt-log.h"

#define INITIAL_REPEAT_TIMEOUT 250 /* ms */
#define REPEAT_TIMEOUT         33  /* ms */

struct _MktController
{
  GObject          parent_instance;

  MktSettings     *settings;
  GListStore      *keyboard_list;
  GListStore      *full_keyboard_list;
  struct libinput *li;
  char            *error;

  gboolean         ignore_keypress;
};

G_DEFINE_TYPE (MktController, mkt_controller, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_FAILED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/* Adapted from libinput gui debug example */
static int
open_restricted (const char *path,
                 int         flags,
                 void       *user_data)
{
  MktController *self = user_data;
  int fd = open (path, flags);

  if (fd < 0)
    {
      g_warning ("Failed to open %s (%s)", path, strerror (errno));

      if (!self->error)
        {
          self->error = g_strdup ("libinput error: Failed to open input event");
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FAILED]);
        }
    }

  return fd < 0 ? -errno : fd;
}

static void
close_restricted (int   fd,
                  void *user_data)
{
  close (fd);
}

static const struct libinput_interface interface = {
  .open_restricted = open_restricted,
  .close_restricted = close_restricted,
};

static void
handle_device_removed_event (MktController         *self,
                             struct libinput_event *ev)
{
  MktKeyboard *keyboard;
  struct libinput_device *dev;
  guint position;

  g_assert (MKT_IS_CONTROLLER (self));
  g_assert (ev);

  dev = libinput_event_get_device (ev);
  keyboard = libinput_device_get_user_data (dev);

  MKT_DEBUG_MSG ("Removed keyboard: %p, libinput device: %p", keyboard, dev);

  if (!keyboard)
    return;

  libinput_device_set_user_data (dev, NULL);

  if (g_list_store_find (self->keyboard_list, keyboard, &position))
    g_list_store_remove (self->keyboard_list, position);

  if (g_list_store_find (self->full_keyboard_list, keyboard, &position))
    g_list_store_remove (self->full_keyboard_list, position);
}

/* We update LEDs from all keyboards.  The system
 * may update LEDs for all keyboards for Lock status
 * change from any keyboard, and so we have to set
 * them again so that they match the correspondint
 * device xkb_state.
 */
static gboolean
update_keyboard_leds (gpointer user_data)
{
  g_autoptr(MktController) self = user_data;
  GListModel *keyboard_list;
  guint n_items;

  keyboard_list = G_LIST_MODEL (self->full_keyboard_list);
  n_items = g_list_model_get_n_items (keyboard_list);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MktKeyboard) keyboard = NULL;

      keyboard = g_list_model_get_item (keyboard_list, i);
      mkt_keyboard_update_leds (keyboard);
    }

  return G_SOURCE_REMOVE;
}

static void
handle_keyboard_event (MktController         *self,
                       struct libinput_event *ev)
{
  struct libinput_event_keyboard *key_event;
  struct libinput_device *dev;
  MktKeyboard *keyboard;
  enum xkb_key_direction direction = XKB_KEY_DOWN;
  gboolean was_enabled;
  uint32_t key, sym;

  g_assert (MKT_IS_CONTROLLER (self));
  g_assert (ev);

  dev = libinput_event_get_device (ev);
  key_event = libinput_event_get_keyboard_event (ev);
  key = libinput_event_keyboard_get_key (key_event);

  if (libinput_event_keyboard_get_key_state (key_event) == LIBINPUT_KEY_STATE_RELEASED)
    direction = XKB_KEY_UP;

  if (!libinput_device_get_user_data (dev))
    {
      keyboard = mkt_keyboard_new (dev);
      mkt_keyboard_set_layout (keyboard, mkt_settings_get_kbd_layout (self->settings));
      g_list_store_append (self->full_keyboard_list, keyboard);
      /* Update LED status as we sets Num Lock when keyboard is added */
      g_timeout_add (1, update_keyboard_leds, g_object_ref (self));
      g_object_unref (keyboard);
    }

  keyboard = libinput_device_get_user_data (dev);

  was_enabled = mkt_keyboard_get_enabled (keyboard);
  if (!was_enabled && direction == XKB_KEY_DOWN)
    {
      guint index = 0;

      if (g_list_store_find (self->keyboard_list, keyboard, &index))
        index = index + XKB_KEY_1;
      else
        index = XKB_KEY_1 + g_list_model_get_n_items (G_LIST_MODEL (self->keyboard_list));

      mkt_keyboard_set_index (keyboard, index);
    }

  sym = mkt_keyboard_feed_key (keyboard, direction, key);

  /*
   * When lock keys are pressed, the system may set LEDs for all keyboards.  We delay
   * a bit updating LEDs so that they are re-updated after systems sets them
   */
  if (sym == XKB_KEY_Caps_Lock ||
      sym == XKB_KEY_Num_Lock ||
      sym == XKB_KEY_Scroll_Lock)
    g_timeout_add (1, update_keyboard_leds, g_object_ref (self));

  if (!was_enabled &&
      mkt_keyboard_get_enabled (keyboard) &&
      !g_list_store_find (self->keyboard_list, keyboard, NULL))
    {
      MKT_DEBUG_MSG ("Added new keyboard %p for libinput keyboard %p (%s)", keyboard, dev,
                     libinput_device_get_name (dev));
      g_list_store_append (self->keyboard_list, keyboard);
    }
}

static gboolean
handle_event_libinput (GIOChannel   *source,
                       GIOCondition  condition,
                       gpointer      user_data)
{
  struct libinput *li = user_data;
  MktController *self = libinput_get_user_data (li);
  struct libinput_event *ev;

  g_assert (MKT_IS_MAIN_THREAD ());

  libinput_dispatch (li);

  while ((ev = libinput_get_event (li)))
    {
      switch ((int)libinput_event_get_type (ev))
        {
        case LIBINPUT_EVENT_DEVICE_REMOVED:
          handle_device_removed_event (self, ev);
          break;

        case LIBINPUT_EVENT_KEYBOARD_KEY:
          if (!self->ignore_keypress)
            handle_keyboard_event (self, ev);
          break;
    }

    libinput_event_destroy (ev);
  }

  return TRUE;
}

static void
mkt_controller_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  MktController *self = (MktController *)object;

  switch (prop_id)
    {
    case PROP_FAILED:
      g_value_set_boolean (value, !!self->error);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mkt_controller_finalize (GObject *object)
{
  MktController *self = (MktController *)object;
  GListModel *keyboard_list;
  guint n_items;
  xkb_keycode_t num_lock = 0, caps_lock = 0, scroll_lock = 0;

  MKT_TRACE_MSG ("disposing controller");

  keyboard_list = G_LIST_MODEL (self->full_keyboard_list);
  n_items = g_list_model_get_n_items (keyboard_list);

  if (n_items)
    {
      struct xkb_keymap *xkb_keymap;
      struct xkb_context *context;
      struct xkb_rule_names names;
      GdkDisplay *display;
      GdkDevice *device;
      GdkSeat *seat;

      names.rules = "evdev";
      names.model = "pc105";
      names.layout = "us";
      names.variant = "";
      names.options = "";

      context = xkb_context_new (0);
      xkb_keymap = xkb_keymap_new_from_names (context, &names, 0);
      xkb_context_unref (context);
      display = gdk_display_get_default ();
      seat = gdk_display_get_default_seat (display);
      device = gdk_seat_get_keyboard (seat);

      if (gdk_device_get_caps_lock_state (device))
        caps_lock = xkb_keymap_key_by_name (xkb_keymap, "CAPS");
      if (gdk_device_get_num_lock_state (device))
        num_lock = xkb_keymap_key_by_name (xkb_keymap, "NMLK");
    }

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MktKeyboard) keyboard = NULL;

      keyboard = g_list_model_get_item (keyboard_list, i);
      mkt_keyboard_reset (keyboard, FALSE);

      if (caps_lock)
        mkt_keyboard_feed_key (keyboard, XKB_KEY_DOWN, caps_lock);
      if (num_lock)
        mkt_keyboard_feed_key (keyboard, XKB_KEY_DOWN, num_lock);
      if (scroll_lock)
        mkt_keyboard_feed_key (keyboard, XKB_KEY_DOWN, scroll_lock);

      mkt_keyboard_update_leds (keyboard);
    }

  g_free (self->error);
  g_clear_object (&self->keyboard_list);
  g_clear_object (&self->full_keyboard_list);
  libinput_set_user_data (self->li, NULL);
  g_clear_pointer (&self->li, libinput_unref);

  G_OBJECT_CLASS (mkt_controller_parent_class)->finalize (object);
}

static void
mkt_controller_class_init (MktControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = mkt_controller_get_property;
  object_class->finalize = mkt_controller_finalize;

  properties[PROP_FAILED] =
    g_param_spec_boolean ("failed",
                          "Failed",
                          "Failed",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mkt_controller_init (MktController *self)
{
  GIOChannel *c = NULL;
  struct udev *udev = NULL;

  udev = udev_new ();

  if (!udev)
    g_error ("Failed to initialize udev");

  self->li = libinput_udev_create_context (&interface, self, udev);

  if (!self->li)
    g_error ("Failed to initialize libinput path context");

  if (libinput_udev_assign_seat (self->li, "seat0"))
    g_error ("failed to set seat");

  libinput_set_user_data (self->li, self);

  self->keyboard_list = g_list_store_new (MKT_TYPE_KEYBOARD);
  self->full_keyboard_list = g_list_store_new (MKT_TYPE_KEYBOARD);

  c = g_io_channel_unix_new (libinput_get_fd (self->li));
  g_io_channel_set_encoding (c, NULL, NULL);
  g_io_add_watch (c, G_IO_IN, handle_event_libinput, self->li);
  handle_event_libinput (NULL, 0, self->li);
}

static void
controller_kbd_layout_changed_cb (MktController *self)
{
  GListModel *keyboards;
  const char *layout;
  guint n_items;

  g_assert (MKT_IS_CONTROLLER (self));

  layout = mkt_settings_get_kbd_layout (self->settings);
  keyboards = G_LIST_MODEL (self->full_keyboard_list);
  n_items = g_list_model_get_n_items (keyboards);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MktKeyboard) keyboard = NULL;

      keyboard = g_list_model_get_item (keyboards, i);
      mkt_keyboard_set_layout (keyboard, layout);
    }
}

MktController *
mkt_controller_new (MktSettings *settings)
{
  MktController *self;

  g_return_val_if_fail (MKT_IS_SETTINGS (settings), NULL);

  self = g_object_new (MKT_TYPE_CONTROLLER, NULL);
  g_set_object (&self->settings, settings);

  g_signal_connect_object (self->settings,
                           "kbd-layout-changed",
                           G_CALLBACK (controller_kbd_layout_changed_cb),
                           self, G_CONNECT_SWAPPED);

  return self;
}

GListModel *
mkt_controller_get_keyboard_list (MktController *self)
{
  g_return_val_if_fail (MKT_IS_CONTROLLER (self), NULL);

  return G_LIST_MODEL (self->keyboard_list);
}

void
mkt_controller_remove_keyboard (MktController *self,
                                MktKeyboard   *keyboard)
{
  guint position;

  if (g_list_store_find (self->keyboard_list, keyboard, &position))
    g_list_store_remove (self->keyboard_list, position);
}

void
mkt_controller_ignore_keypress (MktController *self,
                                gboolean       ignore)
{
  GListModel *list;
  guint n_items;

  g_return_if_fail (MKT_IS_CONTROLLER (self));

  ignore = !!ignore;

  if (self->ignore_keypress == ignore)
    return;

  self->ignore_keypress = ignore;

  list = G_LIST_MODEL (self->full_keyboard_list);
  n_items = g_list_model_get_n_items (list);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MktKeyboard) keyboard = NULL;

      keyboard = g_list_model_get_item (list, i);
      mkt_keyboard_reset (keyboard, TRUE);
    }
}

const char *
mkt_controller_get_error (MktController *self)
{
  g_return_val_if_fail (MKT_IS_CONTROLLER (self), NULL);

  return self->error;
}
