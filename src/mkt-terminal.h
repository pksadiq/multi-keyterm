/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* mkt-terminal.h
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

#include "mkt-device.h"
#include "mkt-settings.h"
#include "mkt-controller.h"

G_BEGIN_DECLS

#define MKT_TYPE_TERMINAL (mkt_terminal_get_type ())

G_DECLARE_FINAL_TYPE (MktTerminal, mkt_terminal, MKT, TERMINAL, GtkFlowBoxChild)

GtkWidget *mkt_terminal_new        (MktController *controller,
                                    MktSettings   *settings,
                                    MktDevice     *device);
MktDevice *mkt_terminal_get_device (MktTerminal   *self);

G_END_DECLS
