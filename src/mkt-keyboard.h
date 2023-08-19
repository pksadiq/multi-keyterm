/* mkt-keyboard.h
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _MktKeyboardKey {
  GdkModifierType modifier;
  guint           keycode;
  guint           keyval;
} MktKeyboardKey;

#define MKT_TYPE_KEYBOARD (mkt_keyboard_get_type ())
G_DECLARE_FINAL_TYPE (MktKeyboard, mkt_keyboard, MKT, KEYBOARD, GObject)

MktKeyboard *mkt_keyboard_new         (gpointer      libinput_device);
void         mkt_keyboard_set_layout  (MktKeyboard  *self,
                                       const char   *layout);
void         mkt_keyboard_set_device  (MktKeyboard  *self,
                                       gpointer      libinput_device);
void         mkt_keyboard_reset       (MktKeyboard  *self,
                                       gboolean      keep_locks);
void         mkt_keyboard_set_index   (MktKeyboard  *self,
                                       guint32       index);
gboolean     mkt_keyboard_get_enabled (MktKeyboard  *self);
void         mkt_keyboard_set_enabled (MktKeyboard  *self,
                                       gboolean      enabled);
guint32      mkt_keyboard_feed_key    (MktKeyboard  *self,
                                       guint32       direction,
                                       guint32       key);
void        mkt_keyboard_update_leds (MktKeyboard  *self);

G_END_DECLS
