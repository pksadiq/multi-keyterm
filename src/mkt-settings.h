/* mkt-settings.h
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

#include <stdbool.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MKT_TYPE_SETTINGS (mkt_settings_get_type ())

G_DECLARE_FINAL_TYPE (MktSettings, mkt_settings, MKT, SETTINGS, GObject)

MktSettings *mkt_settings_new                  (void);
void         mkt_settings_save                 (MktSettings *self);

gboolean     mkt_settings_get_is_first_run     (MktSettings *self);
gboolean     mkt_settings_get_use_system_font  (MktSettings *self);
void         mkt_settings_set_use_system_font  (MktSettings *self,
                                                gboolean     use_system_font);
const char  *mkt_settings_get_font             (MktSettings *self);
void         mkt_settings_set_font             (MktSettings *self,
                                                const char  *font);
bool         mkt_settings_expand_terminal_to_fit (MktSettings *self);
double       mkt_settings_get_font_scale       (MktSettings *self);
int          mkt_settings_get_min_terminal_height (MktSettings *self);
bool         mkt_settings_get_prefer_horizontal_split (MktSettings *self);
const char  *mkt_settings_get_kbd_layout       (MktSettings *self);

G_END_DECLS
