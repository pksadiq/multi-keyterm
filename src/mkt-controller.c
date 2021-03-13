/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* mkt-controller.c
 *
 * Copyright 2021 Mohammed Sadiq <sadiq@sadiqpk.org>
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
#include "mkt-device.h"
#include "mkt-controller.h"
#include "mkt-log.h"

#define INITIAL_REPEAT_TIMEOUT 250 /* ms */
#define REPEAT_TIMEOUT         33  /* ms */

struct _MktController
{
  GObject          parent_instance;

  GListStore      *device_list;
  GListStore      *full_device_list;
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
  MktDevice *device;
  struct libinput_device *dev;
  guint position;

  g_assert (MKT_IS_CONTROLLER (self));
  g_assert (ev);

  dev = libinput_event_get_device (ev);
  device = libinput_device_get_user_data (dev);

  MKT_DEBUG_MSG ("Removed device: %p, libinput device: %p", device, dev);

  if (!device)
    return;

  libinput_device_set_user_data (dev, NULL);

  if (g_list_store_find (self->device_list, device, &position))
    g_list_store_remove (self->device_list, position);

  if (g_list_store_find (self->full_device_list, device, &position))
    g_list_store_remove (self->full_device_list, position);
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
  GListModel *device_list;
  guint n_items;

  device_list = G_LIST_MODEL (self->full_device_list);
  n_items = g_list_model_get_n_items (device_list);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MktDevice) device = NULL;

      device = g_list_model_get_item (device_list, i);
      mkt_device_update_leds (device);
    }

  return G_SOURCE_REMOVE;
}

static void
handle_keyboard_event (MktController         *self,
                       struct libinput_event *ev)
{
  struct libinput_event_keyboard *key_event;
  struct libinput_device *dev;
  MktDevice *device;
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
      device = mkt_device_new (dev);
      g_list_store_append (self->full_device_list, device);
      /* Update LED status us we sets Num Lock when device is added */
      g_timeout_add (1, update_keyboard_leds, g_object_ref (self));
      g_object_unref (device);
    }

  device = libinput_device_get_user_data (dev);

  was_enabled = mkt_device_get_enabled (device);
  if (!was_enabled && direction == XKB_KEY_DOWN)
    {
      guint index = 0;

      if (g_list_store_find (self->device_list, device, &index))
        index = index + XKB_KEY_1;
      else
        index = XKB_KEY_1 + g_list_model_get_n_items (G_LIST_MODEL (self->device_list));

      mkt_device_set_index (device, index);
    }

  sym = mkt_device_feed_key (device, direction, key);

  /*
   * When lock keys are pressed, the system may set LEDs for all keyboards.  We delay
   * a bit updating LEDs so that they are re-updated after systems sets them
   */
  if (sym == XKB_KEY_Caps_Lock ||
      sym == XKB_KEY_Num_Lock ||
      sym == XKB_KEY_Scroll_Lock)
    g_timeout_add (1, update_keyboard_leds, g_object_ref (self));

  if (!was_enabled &&
      mkt_device_get_enabled (device) &&
      !g_list_store_find (self->device_list, device, NULL))
    {
      MKT_DEBUG_MSG ("Added new device %p for libinput device %p (%s)", device, dev,
                     libinput_device_get_name (dev));
      g_list_store_append (self->device_list, device);
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
  GListModel *device_list;
  guint n_items;
  xkb_keycode_t num_lock = 0, caps_lock = 0, scroll_lock = 0;

  MKT_TRACE_MSG ("disposing controller");

  device_list = G_LIST_MODEL (self->full_device_list);
  n_items = g_list_model_get_n_items (device_list);

  if (n_items)
    {
      struct xkb_keymap *xkb_keymap;
      struct xkb_context *context;
      struct xkb_rule_names names;
      GdkDisplay *display;
      GdkKeymap *key_map;

      names.rules = "evdev";
      names.model = "pc105";
      names.layout = "us";
      names.variant = "";
      names.options = "";

      context = xkb_context_new (0);
      xkb_keymap = xkb_keymap_new_from_names (context, &names, 0);
      xkb_context_unref (context);
      display = gdk_display_get_default ();
      key_map = gdk_keymap_get_for_display (display);

      if (gdk_keymap_get_caps_lock_state (key_map))
        caps_lock = xkb_keymap_key_by_name (xkb_keymap, "CAPS");
      if (gdk_keymap_get_num_lock_state (key_map))
        num_lock = xkb_keymap_key_by_name (xkb_keymap, "NMLK");
      if (gdk_keymap_get_scroll_lock_state (key_map))
        scroll_lock = xkb_keymap_key_by_name (xkb_keymap, "SCLK");
    }

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MktDevice) device = NULL;

      device = g_list_model_get_item (device_list, i);
      mkt_device_reset (device, FALSE);

      if (caps_lock)
        mkt_device_feed_key (device, XKB_KEY_DOWN, caps_lock);
      if (num_lock)
        mkt_device_feed_key (device, XKB_KEY_DOWN, num_lock);
      if (scroll_lock)
        mkt_device_feed_key (device, XKB_KEY_DOWN, scroll_lock);

      mkt_device_update_leds (device);
    }

  g_free (self->error);
  g_clear_object (&self->device_list);
  g_clear_object (&self->full_device_list);
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

  self->device_list = g_list_store_new (MKT_TYPE_DEVICE);
  self->full_device_list = g_list_store_new (MKT_TYPE_DEVICE);

  c = g_io_channel_unix_new (libinput_get_fd (self->li));
  g_io_channel_set_encoding (c, NULL, NULL);
  g_io_add_watch (c, G_IO_IN, handle_event_libinput, self->li);
  handle_event_libinput (NULL, 0, self->li);
}

MktController *
mkt_controller_new (void)
{
  return g_object_new (MKT_TYPE_CONTROLLER, NULL);
}

GListModel *
mkt_controller_get_device_list (MktController *self)
{
  g_return_val_if_fail (MKT_IS_CONTROLLER (self), NULL);

  return G_LIST_MODEL (self->device_list);
}

void
mkt_controller_remove_device (MktController *self,
                              MktDevice     *device)
{
  guint position;

  if (g_list_store_find (self->device_list, device, &position))
    g_list_store_remove (self->device_list, position);
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

  list = G_LIST_MODEL (self->full_device_list);
  n_items = g_list_model_get_n_items (list);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(MktDevice) device = NULL;

      device = g_list_model_get_item (list, i);
      mkt_device_reset (device, TRUE);
    }
}

const char *
mkt_controller_get_error (MktController *self)
{
  g_return_val_if_fail (MKT_IS_CONTROLLER (self), NULL);

  return self->error;
}
