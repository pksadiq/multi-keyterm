/* mkt-controller.h
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

#include "mkt-device.h"

G_BEGIN_DECLS

#define MKT_TYPE_CONTROLLER (mkt_controller_get_type ())
G_DECLARE_FINAL_TYPE (MktController, mkt_controller, MKT, CONTROLLER, GObject)

MktController *mkt_controller_new             (void);
GListModel    *mkt_controller_get_device_list (MktController *self);
void           mkt_controller_remove_device   (MktController *self,
                                               MktDevice     *device);
void           mkt_controller_ignore_keypress (MktController *self,
                                               gboolean       ignore);
const char    *mkt_controller_get_error       (MktController *self);

G_END_DECLS
