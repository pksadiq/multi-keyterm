/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* mkt-device.h
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _MktDeviceKey {
  GdkModifierType modifier;
  guint           keycode;
  guint           keyval;
} MktDeviceKey;

#define MKT_TYPE_DEVICE (mkt_device_get_type ())
G_DECLARE_FINAL_TYPE (MktDevice, mkt_device, MKT, DEVICE, GObject)

MktDevice *mkt_device_new         (gpointer   libinput_device);
void       mkt_device_set_device  (MktDevice *self,
                                   gpointer   libinput_device);
void       mkt_device_reset       (MktDevice *self,
                                   gboolean   keep_locks);
void       mkt_device_set_index   (MktDevice *self,
                                   guint32    index);
gboolean   mkt_device_get_enabled (MktDevice *self);
void       mkt_device_set_enabled (MktDevice *self,
                                   gboolean   enabled);
guint32    mkt_device_feed_key    (MktDevice *self,
                                   guint32    direction,
                                   guint32    key);
void       mkt_device_update_leds (MktDevice *self);

G_END_DECLS
