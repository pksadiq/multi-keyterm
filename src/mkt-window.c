/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* mkt-window.c
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

#define G_LOG_DOMAIN "mkt-window"

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "version.h"
#endif

#include <vte/vte.h>
#include <glib/gi18n.h>

#include "mkt-keyboard.h"
#include "mkt-controller.h"
#include "mkt-terminal.h"
#include "mkt-preferences-window.h"
#include "mkt-settings.h"
#include "mkt-window.h"
#include "mkt-log.h"

struct _MktWindow
{
  AdwApplicationWindow  parent_instance;

  GtkWidget            *menu_button;
  GtkWidget            *focus_revealer;
  GtkWidget            *button_revealer;

  GtkWidget            *main_stack;
  GtkWidget            *status_page;
  GtkWidget            *terminal_grid;

  MktSettings          *settings;
  MktController        *controller;
  GtkEventController   *key_controller;
};

G_DEFINE_TYPE (MktWindow, mkt_window, ADW_TYPE_APPLICATION_WINDOW)

static gboolean
window_key_event_cb (MktWindow *self)
{
  g_assert (MKT_IS_WINDOW (self));

  /* Simply swallow the key press/release */
  /* We handle them by other means where appropriate */
  return TRUE;
}

static void
controller_failed_cb (MktWindow *self)
{
  const char *error;

  g_assert (MKT_IS_WINDOW (self));

  error = mkt_controller_get_error (self->controller);

  if (!error)
    return;

  adw_status_page_set_icon_name (ADW_STATUS_PAGE (self->status_page), "dialog-warning-symbolic");
  adw_status_page_set_title (ADW_STATUS_PAGE (self->status_page), error);
  adw_status_page_set_description (ADW_STATUS_PAGE (self->status_page),
                                   "Try running as root, or add yourself "
                                   "to “input” user group");
}

GtkWidget *
terminal_new (MktKeyboard *keyboard,
              MktWindow   *self)
{
  return mkt_terminal_new (self->controller, self->settings, keyboard);
}

static void
window_update_terminal_style (MktWindow *self)
{
  GListModel *keyboard_list;
  guint n_items, children_count = 2;
  bool h_split;

  g_assert (MKT_IS_WINDOW(self));

  keyboard_list = mkt_controller_get_keyboard_list (self->controller);
  n_items = g_list_model_get_n_items (keyboard_list);
  h_split = mkt_settings_get_prefer_horizontal_split (self->settings);

  if (n_items <= 1)
    {
      children_count = 1;
    }
  else if (h_split)
    {
      int term_height, height;

      height = gtk_widget_get_height (self->terminal_grid);
      term_height = mkt_settings_get_min_terminal_height (self->settings);

      if (term_height * n_items <= height)
        children_count = 1;
    }

  g_object_set (self->terminal_grid,
                "min-children-per-line", children_count,
                "max-children-per-line", children_count,
                NULL);
}


static void
keyboard_list_changed_cb (MktWindow *self)
{
  GListModel *keyboard_list;
  GtkWidget *child;
  guint n_items;

  g_assert (MKT_IS_WINDOW (self));

  keyboard_list = mkt_controller_get_keyboard_list (self->controller);
  n_items = g_list_model_get_n_items (keyboard_list);

  if (n_items >= 1)
    child = self->terminal_grid;
  else
    child = self->status_page;

  window_update_terminal_style (self);
  gtk_stack_set_visible_child (GTK_STACK (self->main_stack), child);
}

static void
mkt_window_fullscreen_clicked_cb (MktWindow *self)
{
  g_assert (MKT_IS_WINDOW (self));

  if (gtk_window_is_fullscreen (GTK_WINDOW (self)))
    gtk_window_unfullscreen (GTK_WINDOW (self));
  else
    gtk_window_fullscreen (GTK_WINDOW (self));
}

static void
mkt_window_show_preferences (MktWindow *self)
{
  GtkWidget *preferences = NULL;

  g_assert (MKT_IS_WINDOW (self));

  preferences = mkt_preferences_window_new (GTK_WINDOW (self), self->settings);

  gtk_window_present (GTK_WINDOW (preferences));
}

static void
mkt_window_show_about (MktWindow *self)
{
  const char *authors[] = {
    "Mohammed Sadiq https://www.sadiqpk.org",
    NULL
  };

  g_assert (MKT_IS_WINDOW (self));

  adw_show_about_window (GTK_WINDOW (self),
                         "application-name", _("Multi Key Term"),
                         "website", "https://sadiq.gitlab.io/multi-keyterm",
                         "version", PACKAGE_VCS_VERSION,
                         "copyright", "Copyright © 2023 Mohammed Sadiq",
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "developer-name", "Mohammed Sadiq",
                         "developers", authors,
                         "application-icon", PACKAGE_ID,
                         "translator-credits", _("translator-credits"),
                         NULL);
}

static void
window_fullscreen_changed_cb (MktWindow *self)
{
  gboolean fullscreen;

  g_assert (MKT_IS_WINDOW (self));

  fullscreen = gtk_window_is_fullscreen (GTK_WINDOW (self));
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->button_revealer), fullscreen);
}

static void
window_focus_changed_cb (MktWindow *self)
{
  gboolean has_focus;

  g_assert (MKT_IS_WINDOW (self));

  has_focus = gtk_window_is_active (GTK_WINDOW (self));

  mkt_controller_ignore_keypress (self->controller, !has_focus);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->focus_revealer), !has_focus);
}

static void
mkt_window_map (GtkWidget *widget)
{
  GdkSurface *surface;

  GTK_WIDGET_CLASS (mkt_window_parent_class)->map (widget);

  surface = gtk_native_get_surface (GTK_NATIVE (widget));
  g_signal_connect_object (surface,
                           "notify::height",
                           G_CALLBACK (window_update_terminal_style),
                           widget, G_CONNECT_SWAPPED);
}

static void
mkt_window_finalize (GObject *object)
{
  MktWindow *self = (MktWindow *)object;

  g_clear_object (&self->settings);
  g_clear_object (&self->controller);

  G_OBJECT_CLASS (mkt_window_parent_class)->finalize (object);
}

static void
mkt_window_class_init (MktWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = mkt_window_finalize;

  widget_class->map = mkt_window_map;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/sadiqpk/multi-keyterm/"
                                               "ui/mkt-window.ui");

  gtk_widget_class_bind_template_child (widget_class, MktWindow, menu_button);
  gtk_widget_class_bind_template_child (widget_class, MktWindow, focus_revealer);
  gtk_widget_class_bind_template_child (widget_class, MktWindow, button_revealer);

  gtk_widget_class_bind_template_child (widget_class, MktWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, MktWindow, status_page);
  gtk_widget_class_bind_template_child (widget_class, MktWindow, terminal_grid);

  gtk_widget_class_bind_template_callback (widget_class, window_focus_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, window_fullscreen_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, mkt_window_fullscreen_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, mkt_window_show_preferences);
  gtk_widget_class_bind_template_callback (widget_class, mkt_window_show_about);
}

static void
mkt_window_init (MktWindow *self)
{
  GtkEventController *event_controller;
  AdwStyleManager *style_manager;

  gtk_widget_init_template (GTK_WIDGET (self));

  style_manager = adw_style_manager_get_default ();
  adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_FORCE_DARK);

  event_controller = gtk_event_controller_key_new ();
  gtk_widget_add_controller (GTK_WIDGET (self), event_controller);
  gtk_event_controller_set_propagation_phase (event_controller, GTK_PHASE_CAPTURE);

  g_signal_connect_object (event_controller,
                           "key-pressed",
                           G_CALLBACK (window_key_event_cb),
                           self,
                           G_CONNECT_SWAPPED);
    g_signal_connect_object (event_controller,
                           "key-released",
                           G_CALLBACK (window_key_event_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

GtkWidget *
mkt_window_new (GtkApplication *application,
                MktSettings    *settings)
{
  GListModel *keyboard_list;
  MktWindow *self;

  g_assert (GTK_IS_APPLICATION (application));
  g_assert (MKT_IS_SETTINGS (settings));

  self = g_object_new (MKT_TYPE_WINDOW,
                       "application", application,
                       NULL);
  self->settings = g_object_ref (settings);
  gtk_window_maximize (GTK_WINDOW (self));

  self->controller = mkt_controller_new (settings);
  g_signal_connect_object (self->controller, "notify::failed",
                           G_CALLBACK (controller_failed_cb),
                           self, G_CONNECT_SWAPPED);
  controller_failed_cb (self);

  keyboard_list = mkt_controller_get_keyboard_list (self->controller);
  gtk_flow_box_bind_model (GTK_FLOW_BOX (self->terminal_grid),
                           keyboard_list,
                           (GtkFlowBoxCreateWidgetFunc)terminal_new,
                           g_object_ref (self), g_object_unref);
  g_signal_connect_object (keyboard_list, "items-changed",
                           G_CALLBACK (keyboard_list_changed_cb),
                           self, G_CONNECT_SWAPPED);
  keyboard_list_changed_cb (self);

  g_signal_connect_object (self->settings,
                           "notify::prefer-horizontal-terminal-split",
                           G_CALLBACK (window_update_terminal_style),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->settings,
                           "notify::minimum-terminal-height",
                           G_CALLBACK (window_update_terminal_style),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (keyboard_list,
                           "items-changed",
                           G_CALLBACK (window_update_terminal_style),
                           self, G_CONNECT_SWAPPED);
  window_update_terminal_style (self);

  return GTK_WIDGET (self);
}
