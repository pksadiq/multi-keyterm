/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* mkt-application.c
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

#define G_LOG_DOMAIN "mkt-application"

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "version.h"
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "mkt-settings.h"
#include "mkt-window.h"
#include "mkt-application.h"
#include "mkt-log.h"

/**
 * SECTION: mkt-application
 * @title: MktApplication
 * @short_description: Base Application class
 * @include: "mkt-application.h"
 */

struct _MktApplication
{
  GtkApplication  parent_instance;

  MktSettings    *settings;
};

G_DEFINE_TYPE (MktApplication, mkt_application, GTK_TYPE_APPLICATION)


static gboolean
cmd_verbose_cb (const char *option_name,
                const char *value,
                gpointer    data,
                GError    **error);

static GOptionEntry cmd_options[] = {
  {
    "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, cmd_verbose_cb,
    N_("Show verbose logs"), NULL
  },
  {
    "version", 0, G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL,
    N_("Show release version"), NULL
  },
  { NULL }
};

static gboolean
cmd_verbose_cb (const char  *option_name,
                const char  *value,
                gpointer     data,
                GError     **error)
{
  mkt_log_increase_verbosity ();

  return TRUE;
}

static int
mkt_application_handle_local_options (GApplication *application,
                                      GVariantDict *options)
{
  if (g_variant_dict_contains (options, "version"))
    {
      g_print ("%s %s\n", PACKAGE_NAME, PACKAGE_VCS_VERSION);
      return 0;
    }

  return -1;
}

static void
mkt_application_startup (GApplication *application)
{
  MktApplication *self = (MktApplication *)application;
  g_autoptr(GtkCssProvider) css_provider = NULL;

  g_info ("%s %s, git version: %s", PACKAGE_NAME,
          PACKAGE_VERSION, PACKAGE_VCS_VERSION);

  G_APPLICATION_CLASS (mkt_application_parent_class)->startup (application);

  g_set_application_name (_("Multi Key Term"));
  gtk_window_set_default_icon_name (PACKAGE_ID);
  self->settings = mkt_settings_new ();

  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (css_provider,
                                       "/org/sadiqpk/multi-keyterm/css/gtk.css");

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (css_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
mkt_application_activate (GApplication *application)
{
  MktApplication *self = (MktApplication *)application;
  GtkWindow *window;

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  if (window == NULL)
    window = GTK_WINDOW (mkt_window_new (GTK_APPLICATION (self), self->settings));

  gtk_window_present (window);
}

static void
mkt_application_finalize (GObject *object)
{
  MktApplication *self = (MktApplication *)object;

  MKT_TRACE_MSG ("disposing application");
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (mkt_application_parent_class)->finalize (object);
}

static void
mkt_application_class_init (MktApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = mkt_application_finalize;

  application_class->handle_local_options = mkt_application_handle_local_options;
  application_class->startup = mkt_application_startup;
  application_class->activate = mkt_application_activate;
}

static void
mkt_application_init (MktApplication *self)
{
  g_application_add_main_option_entries (G_APPLICATION (self), cmd_options);
}

MktApplication *
mkt_application_new (void)
{
  return g_object_new (MKT_TYPE_APPLICATION,
                       "application-id", PACKAGE_ID,
                       "flags", G_APPLICATION_FLAGS_NONE,
                       NULL);
}
