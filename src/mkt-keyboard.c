/* mkt-keyboard.c
 *
 * Copyright 2021, 2023 Mohammed Sadiq <sadiq@sadiqpk.org>
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

#define G_LOG_DOMAIN "mkt-keyboard"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libinput.h>
#include <xkbcommon/xkbcommon.h>

#include "mkt-keyboard.h"
#include "mkt-log.h"

#define INITIAL_REPEAT_TIMEOUT 250 /* ms */
#define REPEAT_TIMEOUT         33  /* ms */

struct _MktKeyboard
{
  GObject  parent_instance;

  struct libinput_device *device;
  struct xkb_keymap      *xkb_us_keymap;
  struct xkb_state       *xkb_us_state;
  struct xkb_keymap      *xkb_keymap;
  struct xkb_state       *xkb_state;

  xkb_keysym_t index_sym;

  guint        repeat_id;
  gboolean     enabled;
};

G_DEFINE_TYPE (MktKeyboard, mkt_keyboard, G_TYPE_OBJECT)

enum {
  KEY_PRESSED,
  KEY_RELEASED,
  N_SIGNALS
};

enum {
  PROP_0,
  PROP_ENABLED,
  N_PROPS
};

static guint signals[N_SIGNALS];
static GParamSpec *properties[N_PROPS];

static GdkModifierType
get_active_modifiers (MktKeyboard *self)
{
  GdkModifierType modifiers = 0;

  g_assert (MKT_IS_KEYBOARD (self));

#define MODE_IS_ACTIVE(state, name) xkb_state_mod_name_is_active (state, name, \
                                                                  XKB_STATE_MODS_EFFECTIVE)
  if (MODE_IS_ACTIVE (self->xkb_us_state, XKB_MOD_NAME_CTRL))
    modifiers |= GDK_CONTROL_MASK;
  if (MODE_IS_ACTIVE (self->xkb_us_state, XKB_MOD_NAME_SHIFT))
    modifiers |= GDK_SHIFT_MASK;
  if (MODE_IS_ACTIVE (self->xkb_us_state, XKB_MOD_NAME_ALT))
    modifiers |= GDK_ALT_MASK;
  if (MODE_IS_ACTIVE (self->xkb_us_state, "Meta"))
    modifiers |= GDK_META_MASK;
  if (MODE_IS_ACTIVE (self->xkb_us_state, "Super"))
    modifiers |= GDK_SUPER_MASK;

#undef MODE_IS_ACTIVE

  return modifiers;
}

static void
mkt_keyboard_set_lock (MktKeyboard *self,
                       const char  *key)
{
  xkb_keycode_t lock;

  lock = xkb_keymap_key_by_name (self->xkb_us_keymap, key);
  mkt_keyboard_feed_key (self, XKB_KEY_DOWN, lock);
  mkt_keyboard_feed_key (self, XKB_KEY_UP, lock);
}

static void
show_key_log (MktKeyboard            *self,
              xkb_keysym_t            sym,
              enum xkb_key_direction  direction,
              gboolean                is_repeat)
{
  g_autoptr(GString) keys = NULL;
  char buf[64] = {0};

  keys = g_string_new ("");

#define MODE_IS_ACTIVE(state, name) xkb_state_mod_name_is_active (state, name, \
                                                                  XKB_STATE_MODS_EFFECTIVE)
  if (MODE_IS_ACTIVE (self->xkb_us_state, "Super"))
    g_string_append (keys, "Super + ");
  if (MODE_IS_ACTIVE (self->xkb_us_state, XKB_MOD_NAME_CTRL) &&
      (sym != XKB_KEY_Control_L &&
       sym != XKB_KEY_Control_R))
    g_string_append (keys, "Control + ");
  if ((MODE_IS_ACTIVE (self->xkb_us_state, XKB_MOD_NAME_ALT) ||
       MODE_IS_ACTIVE (self->xkb_us_state, "Meta")) &&
      (sym != XKB_KEY_Alt_L &&
       sym != XKB_KEY_Alt_R))
    g_string_append (keys, "Alt + ");
  if (MODE_IS_ACTIVE (self->xkb_us_state, XKB_MOD_NAME_SHIFT) &&
      (sym == XKB_KEY_Shift_L &&
       sym == XKB_KEY_Shift_R))
    g_string_append (keys, "Shift + ");
#undef MODE_IS_ACTIVE

  xkb_keysym_get_name (sym, buf, sizeof (buf));

  if (sym == XKB_KEY_Tab)
    g_string_append (keys, "Tab");
  else if (*buf)
    g_string_append (keys, buf);

  if (is_repeat)
    MKT_TRACE ("Repeat effective keys: '%s', dev: %p", keys->str, self);
  else
    MKT_TRACE ("effective keys: '%s', %s '%s', dev: %p", keys->str,
               direction == XKB_KEY_DOWN ? "pressed" : "released", buf, self);
}

static void
emit_event (MktKeyboard            *self,
            enum xkb_key_direction  direction,
            xkb_keysym_t            sym)
{
  GdkModifierType modifier;
  MktKeyboardKey key = {0};

  modifier = get_active_modifiers (self);

  /* Skip Alt-Tab */
  if (modifier == GDK_ALT_MASK && sym == GDK_KEY_Tab)
    return;

  key.keycode = sym;
  key.keyval = sym;
  key.modifier = modifier;

  if (direction == XKB_KEY_DOWN)
    g_signal_emit (self, signals[KEY_PRESSED], 0, &key);
  else
    g_signal_emit (self, signals[KEY_RELEASED], 0, &key);
}


static gboolean
repeat_key_cb (GTask    *task,
               gboolean  repeat)
{
  MktKeyboard *self;
  xkb_keysym_t sym;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  sym = GPOINTER_TO_INT (g_task_get_task_data (task));
  emit_event (self, XKB_KEY_DOWN, sym);
  emit_event (self, XKB_KEY_UP, sym);

  if (mkt_log_get_verbosity () > 3)
    show_key_log (self, sym, 0, TRUE);

  return repeat;
}

static gboolean
repeat_key (gpointer user_data)
{
  return repeat_key_cb (user_data, G_SOURCE_CONTINUE);
}

static gboolean
initial_repeat_key (gpointer user_data)
{
  MktKeyboard *self;

  self = g_task_get_source_object (user_data);

  g_clear_handle_id (&self->repeat_id, g_source_remove);

  self->repeat_id = g_timeout_add_full (G_PRIORITY_HIGH,
                                        REPEAT_TIMEOUT,
                                        repeat_key, g_object_ref (user_data),
                                        g_object_unref);

  return repeat_key_cb (user_data, G_SOURCE_REMOVE);
}

static void
mkt_keyboard_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  MktKeyboard *self = (MktKeyboard *)object;

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, self->enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mkt_keyboard_finalize (GObject *object)
{
  MktKeyboard *self = (MktKeyboard *)object;

  MKT_TRACE_MSG ("Finalizing keyboard: %p", self);

  if (self->device)
    libinput_device_set_user_data (self->device, NULL);
  g_clear_handle_id (&self->repeat_id, g_source_remove);
  g_clear_pointer (&self->device, libinput_device_unref);

  G_OBJECT_CLASS (mkt_keyboard_parent_class)->finalize (object);
}

static void
mkt_keyboard_class_init (MktKeyboardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = mkt_keyboard_get_property;
  object_class->finalize = mkt_keyboard_finalize;

  properties[PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "Enabled",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [KEY_PRESSED] =
    g_signal_new ("key-pressed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
  signals [KEY_RELEASED] =
    g_signal_new ("key-released",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
mkt_keyboard_init (MktKeyboard *self)
{
  struct xkb_context *context;
  struct xkb_rule_names names;

  names.rules = "evdev";
  names.model = "pc105";
  names.layout = "us";
  names.variant = "";
  names.options = "";

  context = xkb_context_new (0);
  self->xkb_us_keymap = xkb_keymap_new_from_names (context, &names, 0);
  self->xkb_us_state = xkb_state_new (self->xkb_us_keymap);
  self->index_sym = XKB_KEY_0;

  xkb_context_unref (context);
}

MktKeyboard *
mkt_keyboard_new (gpointer libinput_device)
{
  MktKeyboard *self;

  g_return_val_if_fail (libinput_device, NULL);

  self = g_object_new (MKT_TYPE_KEYBOARD, NULL);
  mkt_keyboard_set_device (self, libinput_device);
  MKT_DEBUG_MSG ("Created new keyboard %p for %p (%s)",
                 self, self->device,
                 libinput_device_get_name (self->device));

  /* Enable Num Lock by default */
  mkt_keyboard_set_lock (self, "NMLK");

  return self;
}

void
mkt_keyboard_set_device (MktKeyboard *self,
                         gpointer     libinput_device)
{
  g_return_if_fail (MKT_IS_KEYBOARD (self));
  g_return_if_fail (libinput_device);

  if (self->device == libinput_device)
    return;

  g_return_if_fail (!libinput_device_get_user_data (libinput_device));

  if (self->device)
    libinput_device_unref (self->device);

  self->device = libinput_device_ref (libinput_device);
  libinput_device_set_user_data (libinput_device, self);
}

void
mkt_keyboard_reset (MktKeyboard *self,
                    gboolean     keep_locks)
{
  struct xkb_state *xkb_state;

  g_return_if_fail (MKT_IS_KEYBOARD (self));

  g_clear_handle_id (&self->repeat_id, g_source_remove);
  xkb_state = g_steal_pointer (&self->xkb_us_state);
  self->xkb_us_state = xkb_state_new (self->xkb_us_keymap);

  g_debug ("Resetting keyboard %p, keep-locks: %d", self, !!keep_locks);

  if (keep_locks)
    {
      if (xkb_state_led_name_is_active (xkb_state, XKB_LED_NAME_CAPS))
        mkt_keyboard_set_lock (self, "CAPS");

      if (xkb_state_led_name_is_active (xkb_state, XKB_LED_NAME_NUM))
        mkt_keyboard_set_lock (self, "NMLK");

      if (xkb_state_led_name_is_active (xkb_state, XKB_LED_NAME_SCROLL))
        mkt_keyboard_set_lock (self, "SCLK");
    }

  mkt_keyboard_update_leds (self);
  xkb_state_unref (xkb_state);
}

void
mkt_keyboard_set_index (MktKeyboard *self,
                        guint32      index)
{
  g_return_if_fail (MKT_IS_KEYBOARD (self));
  g_return_if_fail (index >= XKB_KEY_0 && index <= XKB_KEY_9);

  if (mkt_keyboard_get_enabled (self))
    return;

  MKT_TRACE_MSG ("setting keyboard index: %p, index: %u", self, index);
  self->index_sym = index;
}

gboolean
mkt_keyboard_get_enabled (MktKeyboard *self)
{
  g_return_val_if_fail (MKT_IS_KEYBOARD (self), FALSE);

  return self->enabled;
}

void
mkt_keyboard_set_enabled (MktKeyboard *self,
                          gboolean     enabled)
{
  g_return_if_fail (MKT_IS_KEYBOARD (self));

  enabled = !!enabled;

  if (self->enabled == enabled)
    return;

  self->enabled = enabled;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLED]);
}

guint32
mkt_keyboard_feed_key (MktKeyboard *self,
                       guint32      direction, /* enum xkb_key_direction  */
                       guint32      key)       /* xkb_keycode_t */
{
  struct xkb_keymap *xkb_keymap;
  xkb_keysym_t sym_us, sym;
  GdkModifierType modifier;

  g_return_val_if_fail (MKT_IS_KEYBOARD (self), 0);

  sym_us = xkb_state_key_get_one_sym (self->xkb_us_state, key + 8);

  if (self->xkb_state)
    sym = xkb_state_key_get_one_sym (self->xkb_state, key + 8);
  else
    sym = sym_us;

  if (self->xkb_keymap)
    xkb_keymap = self->xkb_keymap;
  else
    xkb_keymap = self->xkb_us_keymap;

  if (direction == XKB_KEY_DOWN)
    {
      xkb_state_update_key (self->xkb_us_state, key + 8, direction);
      if (self->xkb_state)
        xkb_state_update_key (self->xkb_state, key + 8, direction);
      modifier = get_active_modifiers (self);
    }
  else
    {
      modifier = get_active_modifiers (self);
      xkb_state_update_key (self->xkb_us_state, key + 8, direction);
      if (self->xkb_state)
        xkb_state_update_key (self->xkb_state, key + 8, direction);
    }

  /* Use US keycode if control key is active */
  if (modifier & GDK_CONTROL_MASK)
    sym = sym_us;

  g_clear_handle_id (&self->repeat_id, g_source_remove);

  if (mkt_keyboard_get_enabled (self))
    {
      GTask *task;

      task = g_task_new (self, NULL, NULL, NULL);
      g_task_set_task_data (task, GINT_TO_POINTER (sym), NULL);

      emit_event (self, direction, sym);

      if (direction == XKB_KEY_DOWN &&
          xkb_keymap_key_repeats (xkb_keymap, key + 8))
        self->repeat_id = g_timeout_add_full (G_PRIORITY_HIGH,
                                              INITIAL_REPEAT_TIMEOUT,
                                              initial_repeat_key, task,
                                              g_object_unref);
    }

  if (direction == XKB_KEY_DOWN &&
      !mkt_keyboard_get_enabled (self) &&
      (sym_us == self->index_sym||
       sym_us == self->index_sym - XKB_KEY_0 + XKB_KEY_KP_0))
    {
      if (!get_active_modifiers (self))
        mkt_keyboard_set_enabled (self, TRUE);
    }

  if (mkt_log_get_verbosity () > 3)
    show_key_log (self, sym, direction, FALSE);

  return sym;
}

void
mkt_keyboard_update_leds (MktKeyboard *self)
{
  enum libinput_led leds = 0;


  if (!self->device)
    return;

  if (xkb_state_led_name_is_active (self->xkb_us_state, XKB_LED_NAME_CAPS))
    leds |= LIBINPUT_LED_CAPS_LOCK;
  if (xkb_state_led_name_is_active (self->xkb_us_state, XKB_LED_NAME_NUM))
    leds |= LIBINPUT_LED_NUM_LOCK;
  if (xkb_state_led_name_is_active (self->xkb_us_state, XKB_LED_NAME_SCROLL))
    leds |= LIBINPUT_LED_SCROLL_LOCK;

  libinput_device_led_update (self->device, leds);

  MKT_TRACE_MSG ("Updated Keyboard %p LEDs. Caps: %d, Num: %d, Scroll: %d",
                 self,
                 !!(leds & LIBINPUT_LED_CAPS_LOCK),
                 !!(leds & LIBINPUT_LED_NUM_LOCK),
                 !!(leds & LIBINPUT_LED_SCROLL_LOCK));
}

void
mkt_keyboard_set_layout (MktKeyboard *self,
                         const char  *layout)
{
  g_auto(GStrv) strv = NULL;
  struct xkb_context *context;
  struct xkb_rule_names names;

  g_return_if_fail (MKT_IS_KEYBOARD (self));
  g_return_if_fail (layout && *layout);

  strv = g_strsplit (layout, "+", 2);
  names.rules = "evdev";
  names.model = "pc105";
  names.layout = strv[0];
  names.variant = strv[1] ?: "";
  names.options = "";

  context = xkb_context_new (0);
  self->xkb_keymap = xkb_keymap_new_from_names (context, &names, 0);
  self->xkb_state = xkb_state_new (self->xkb_keymap);

  xkb_context_unref (context);
}
