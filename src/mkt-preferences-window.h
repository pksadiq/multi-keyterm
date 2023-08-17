/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* mkt-preferences-window.h
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

#include <adwaita.h>

#include "mkt-settings.h"

G_BEGIN_DECLS

#define MKT_TYPE_PREFERENCES_WINDOW (mkt_preferences_window_get_type ())

G_DECLARE_FINAL_TYPE (MktPreferencesWindow, mkt_preferences_window, MKT, PREFERENCES_WINDOW, AdwPreferencesWindow)

GtkWidget *mkt_preferences_window_new (GtkWindow   *parent_window,
                                       MktSettings *settings);

G_END_DECLS
