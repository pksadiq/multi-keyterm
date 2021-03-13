/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* mkt-preferences-window.c
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

#define G_LOG_DOMAIN "mkt-preferences-window"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "mkt-preferences-window.h"
#include "mkt-log.h"

struct _MktPreferencesWindow
{
  GtkDialog      parent_instance;

  GtkWidget     *use_system_font_row;
  GtkWidget     *use_system_font_switch;
  GtkWidget     *terminal_font_row;
  GtkWidget     *font_label;
  GtkWidget     *font_chooser_dialog;

  MktSettings   *settings;
};

G_DEFINE_TYPE (MktPreferencesWindow, mkt_preferences_window, GTK_TYPE_DIALOG)

static void
settings_font_changed_cb (MktPreferencesWindow *self,
                          MktSettings          *settings)
{
  const char *font = NULL;
  gboolean use_system_font;

  g_assert (MKT_IS_PREFERENCES_WINDOW (self));
  g_assert (MKT_IS_SETTINGS (settings));

  use_system_font = mkt_settings_get_use_system_font (settings);
  font = mkt_settings_get_font (settings);

  if (font)
    gtk_label_set_label (GTK_LABEL (self->font_label), font);

  gtk_switch_set_active (GTK_SWITCH (self->use_system_font_switch), use_system_font);
  mkt_settings_save (self->settings);
}

static void
preferences_window_set_settings (MktPreferencesWindow *self,
                                 MktSettings          *settings)
{
  g_assert (MKT_IS_PREFERENCES_WINDOW (self));
  g_assert (MKT_IS_SETTINGS (settings));

  if (!g_set_object (&self->settings, settings))
    return;

  g_signal_connect_object (self->settings, "font-changed",
                           G_CALLBACK (settings_font_changed_cb),
                           self, G_CONNECT_SWAPPED);
  settings_font_changed_cb (self, self->settings);
}

static void
settings_row_activated_cb (MktPreferencesWindow *self,
                           GtkListBoxRow        *row)
{
  g_assert (MKT_IS_PREFERENCES_WINDOW (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  if ((gpointer)row == self->use_system_font_row)
    {
      GtkSwitch *font_switch;
      gboolean active;

      font_switch = GTK_SWITCH (self->use_system_font_switch);
      active = gtk_switch_get_active (font_switch);
      gtk_switch_set_active (font_switch, !active);
    }
  else if ((gpointer)row == self->terminal_font_row)
    {
      g_autofree char *font = NULL;
      int response;

      font = g_strdup (mkt_settings_get_font (self->settings));
      if (!mkt_settings_get_use_system_font (self->settings) && font)
        gtk_font_chooser_set_font (GTK_FONT_CHOOSER (self->font_chooser_dialog), font);

      response = gtk_dialog_run (GTK_DIALOG (self->font_chooser_dialog));
      gtk_widget_hide (self->font_chooser_dialog);

      if (response != GTK_RESPONSE_OK && font)
        mkt_settings_set_font (self->settings, font);
    }
}

static void
use_system_font_changed_cb (MktPreferencesWindow *self)
{
  const char *font = NULL;
  gboolean active;

  g_assert (MKT_IS_PREFERENCES_WINDOW (self));

  active = gtk_switch_get_active (GTK_SWITCH (self->use_system_font_switch));
  mkt_settings_set_use_system_font (self->settings, active);

  font = mkt_settings_get_font (self->settings);

  if (!font)
    font = "";

  gtk_label_set_label (GTK_LABEL (self->font_label), font);
}

static void
font_chooser_changed_cb (MktPreferencesWindow *self)
{
  GtkFontChooser *font_chooser;
  g_autofree char *font = NULL;

  g_assert (MKT_IS_PREFERENCES_WINDOW (self));

  font_chooser = GTK_FONT_CHOOSER (self->font_chooser_dialog);
  font = gtk_font_chooser_get_font (font_chooser);
  mkt_settings_set_font (self->settings, font);
}

static void
mkt_preferences_window_finalize (GObject *object)
{
  MktPreferencesWindow *self = (MktPreferencesWindow *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (mkt_preferences_window_parent_class)->finalize (object);
}

static void
mkt_preferences_window_class_init (MktPreferencesWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = mkt_preferences_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/sadiqpk/multi-keyterm/"
                                               "ui/mkt-preferences-window.ui");

  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, use_system_font_row);
  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, use_system_font_switch);
  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, terminal_font_row);
  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, font_label);
  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, font_chooser_dialog);

  gtk_widget_class_bind_template_callback (widget_class, settings_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, use_system_font_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, font_chooser_changed_cb);
}

static void
mkt_preferences_window_init (MktPreferencesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
mkt_preferences_window_new (GtkWindow   *parent_window,
                            MktSettings *settings)
{
  MktPreferencesWindow *self;

  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);
  g_return_val_if_fail (MKT_IS_SETTINGS (settings), NULL);

  self = g_object_new (MKT_TYPE_PREFERENCES_WINDOW,
                       "transient-for", parent_window,
                       "use-header-bar", 1,
                       NULL);
  preferences_window_set_settings (self, settings);

  return GTK_WIDGET (self);
}
