/* mkt-preferences-window.c
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

#define G_LOG_DOMAIN "mkt-preferences-window"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "mkt-preferences-window.h"
#include "mkt-log.h"

struct _MktPreferencesWindow
{
  AdwPreferencesWindow  parent_instance;

  GtkWidget            *use_custom_font_row;
  GtkWidget            *terminal_font_row;
  GtkWidget            *default_zoom_row;

  GtkWidget            *expand_to_fit_row;
  GtkWidget            *horizontal_split_row;
  GtkWidget            *min_height_row;

  GtkWidget            *font_chooser_dialog;

  MktSettings          *settings;
};

G_DEFINE_TYPE (MktPreferencesWindow, mkt_preferences_window, ADW_TYPE_PREFERENCES_WINDOW)

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

  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->terminal_font_row), font ?: "");
  adw_expander_row_set_enable_expansion (ADW_EXPANDER_ROW (self->use_custom_font_row),
                                         !use_system_font);
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

  g_object_bind_property (self->settings, "font-scale",
                          self->default_zoom_row, "value",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  g_object_bind_property (self->settings, "expand-to-fit",
                          self->expand_to_fit_row, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->settings, "prefer-horizontal-terminal-split",
                          self->horizontal_split_row, "enable-expansion",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->settings, "minimum-terminal-height",
                          self->min_height_row, "value",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  settings_font_changed_cb (self, self->settings);
}

static void
use_custom_font_changed_cb (MktPreferencesWindow *self)
{
  const char *font = NULL;
  gboolean active;

  g_assert (MKT_IS_PREFERENCES_WINDOW (self));

  active = !adw_expander_row_get_enable_expansion (ADW_EXPANDER_ROW (self->use_custom_font_row));
  mkt_settings_set_use_system_font (self->settings, active);

  font = mkt_settings_get_font (self->settings);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->terminal_font_row), font ?: "");
}

static void
terminal_font_row_activated_cb (MktPreferencesWindow *self)
{
  g_autofree char *font = NULL;

  g_assert (MKT_IS_PREFERENCES_WINDOW (self));

  font = g_strdup (mkt_settings_get_font (self->settings));
  if (!mkt_settings_get_use_system_font (self->settings) && font)
    gtk_font_chooser_set_font (GTK_FONT_CHOOSER (self->font_chooser_dialog), font);

  gtk_window_present (GTK_WINDOW (self->font_chooser_dialog));
}

static void
font_chooser_response_cb (MktPreferencesWindow *self,
                          guint                 response)
{
  GtkFontChooser *font_chooser;
  g_autofree char *font = NULL;

  g_assert (MKT_IS_PREFERENCES_WINDOW (self));

  gtk_widget_set_visible (self->font_chooser_dialog, FALSE);

  font_chooser = GTK_FONT_CHOOSER (self->font_chooser_dialog);
  font = gtk_font_chooser_get_font (font_chooser);

  if (response == GTK_RESPONSE_OK && font)
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

  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, use_custom_font_row);
  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, terminal_font_row);
  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, default_zoom_row);

  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, expand_to_fit_row);
  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, horizontal_split_row);
  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, min_height_row);

  gtk_widget_class_bind_template_child (widget_class, MktPreferencesWindow, font_chooser_dialog);

  gtk_widget_class_bind_template_callback (widget_class, use_custom_font_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, terminal_font_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, font_chooser_response_cb);
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
                       NULL);
  preferences_window_set_settings (self, settings);

  return GTK_WIDGET (self);
}
