/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* settings.c
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

#undef NDEBUG
#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS
#undef G_LOG_DOMAIN

#include <glib.h>

#include "mkt-settings.h"
#include "mkt-log.h"

static void
test_settings_first_run (void)
{
  MktSettings *settings;
  GSettings *gsettings;

  /* Reset the first-run settings */
  gsettings = g_settings_new ("org.sadiqpk.multi-keyterm");
  g_settings_reset (gsettings, "version");
  g_object_unref (gsettings);

  settings = mkt_settings_new ();
  g_assert_true (MKT_IS_SETTINGS (settings));
  g_assert_true (mkt_settings_get_is_first_run (settings));
  g_object_unref (settings);

  /* create a new object, and check again */
  settings = mkt_settings_new ();
  g_assert_true (MKT_IS_SETTINGS (settings));
  g_assert_false (mkt_settings_get_is_first_run (settings));
  g_object_unref (settings);

  /*
   * Set a custom version and check, this test assumes that
   * version (ie, PACKAGE_VERSION) is never set to 0.0.0.0
  */
  gsettings = g_settings_new ("org.sadiqpk.multi-keyterm");
  g_settings_set_string (gsettings, "version", "0.0.0.0");
  g_object_unref (gsettings);

  settings = mkt_settings_new ();
  g_assert_true (MKT_IS_SETTINGS (settings));
  g_assert_true (mkt_settings_get_is_first_run (settings));
  g_object_unref (settings);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  mkt_log_init ();
  /* Set enough verbosity */
  mkt_log_increase_verbosity ();
  mkt_log_increase_verbosity ();
  mkt_log_increase_verbosity ();
  mkt_log_increase_verbosity ();
  mkt_log_increase_verbosity ();

  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  g_test_add_func ("/settings/first_run", test_settings_first_run);

  return g_test_run ();
}
