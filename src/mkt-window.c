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

#include "mkt-device.h"
#include "mkt-controller.h"
#include "mkt-terminal.h"
#include "mkt-preferences-window.h"
#include "mkt-window.h"
#include "mkt-log.h"

struct _MktWindow
{
  GtkApplicationWindow  parent_instance;

  GtkWidget     *menu_button;
  GtkWidget     *focus_revealer;
  GtkWidget     *button_revealer;

  GtkWidget     *main_stack;

  GtkWidget     *empty_view;
  GtkWidget     *empty_icon;
  GtkWidget     *empty_title;
  GtkWidget     *empty_subtitle;

  GtkWidget     *terminal_grid;

  MktSettings   *settings;
  MktController *controller;
  GtkEventController *key_controller;
};

G_DEFINE_TYPE (MktWindow, mkt_window, GTK_TYPE_APPLICATION_WINDOW)


static void
controller_failed_cb (MktWindow *self)
{
  const char *error;

  g_assert (MKT_IS_WINDOW (self));

  error = mkt_controller_get_error (self->controller);

  if (!error)
    return;

  g_object_set (self->empty_icon, "icon-name", "dialog-warning-symbolic", NULL);
  gtk_label_set_text (GTK_LABEL (self->empty_title), error);
  gtk_label_set_text (GTK_LABEL (self->empty_subtitle),
                      "Try running as root, or add yourself "
                      "to “input” user group");
}

static gboolean
controller_key_pressed_cb (MktWindow *self)
{
  /* Swallow every key press */
  return TRUE;
}

GtkWidget *
terminal_new (MktDevice *device,
              MktWindow *self)
{
  return mkt_terminal_new (self->controller, self->settings, device);
}

static void
device_list_changed_cb (MktWindow *self)
{
  GListModel *device_list;
  GtkWidget *child;
  guint n_items;

  g_assert (MKT_IS_WINDOW (self));

  device_list = mkt_controller_get_device_list (self->controller);
  n_items = g_list_model_get_n_items (device_list);

  if (n_items >= 1)
    child = self->terminal_grid;
  else
    child = self->empty_view;

  /* We use homogeneous spacing for children.  So if we have
   * only one child, half of the space is left free, so adjust
   * accordingly.  We ignore the free spaces when we have more
   * than 2 children.  A GtkGridView would be better here, but
   * that's GTK4 only.
   */
  if (n_items == 1)
    g_object_set (self->terminal_grid,
                  "min-children-per-line", 1,
                  "max-children-per-line", 1,
                  NULL);
  else
    g_object_set (self->terminal_grid,
                  "min-children-per-line", 2,
                  "max-children-per-line", 2,
                  NULL);

  gtk_stack_set_visible_child (GTK_STACK (self->main_stack), child);
}

static void
mkt_window_fullscreen_clicked_cb (MktWindow *self,
                                  GtkButton *button)
{
  GdkWindow *window;
  GdkWindowState state;

  g_assert (MKT_IS_WINDOW (self));
  g_assert (GTK_IS_BUTTON (button));

  window = gtk_widget_get_window (GTK_WIDGET (self));
  state = gdk_window_get_state (window);
  if (state & GDK_WINDOW_STATE_FULLSCREEN)
    gtk_window_unfullscreen (GTK_WINDOW (self));
  else
    gtk_window_fullscreen (GTK_WINDOW (self));
}

static gboolean
mkt_window_ignore (MktWindow *self)
{
  gtk_widget_error_bell (GTK_WIDGET (self));

  return TRUE;
}

static void
mkt_window_show_preferences (MktWindow *self)
{
  GtkWidget *preferences = NULL;
  GtkApplication *application;
  GList *windows;

  application = GTK_APPLICATION (g_application_get_default ());
  windows = gtk_application_get_windows (application);

  for (GList *window = windows; window; window = window->next)
    if (MKT_IS_PREFERENCES_WINDOW (window->data))
      {
        preferences = window->data;
        break;
      }

  if (!preferences)
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

  /*
   * If “program-name” is not set, it is retrieved from
   * g_get_application_name().
   */
  gtk_show_about_dialog (GTK_WINDOW (self),
                         "website", "https://sadiq.gitlab.io/multi-keyterm",
                         "version", PACKAGE_VCS_VERSION,
                         "copyright", "Copyright © 2021 Mohammed Sadiq",
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "authors", authors,
                         "logo-icon-name", PACKAGE_ID,
                         "translator-credits", _("translator-credits"),
                         NULL);
}

static gboolean
mkt_window_focus_out_event (GtkWidget     *widget,
                            GdkEventFocus *event)
{
  MktWindow *self = (MktWindow *)widget;

  MKT_TRACE_MSG ("Focus out event");
  mkt_controller_ignore_keypress (self->controller, TRUE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->focus_revealer), TRUE);

  return GTK_WIDGET_CLASS (mkt_window_parent_class)->focus_out_event (widget, event);
}

static gboolean
mkt_window_focus_in_event (GtkWidget	 *widget,
                           GdkEventFocus *event)
{
  MktWindow *self = (MktWindow *)widget;

  MKT_TRACE_MSG ("Focus in event");
  mkt_controller_ignore_keypress (self->controller, FALSE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->focus_revealer), FALSE);

  return GTK_WIDGET_CLASS (mkt_window_parent_class)->focus_in_event (widget, event);
}

static gboolean
mkt_window_window_state_event (GtkWidget           *widget,
                               GdkEventWindowState *event)
{
  MktWindow *self = (MktWindow *)widget;

  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
      gboolean reveal = !!(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN);

      gtk_revealer_set_reveal_child (GTK_REVEALER (self->button_revealer), reveal);
    }

  return GTK_WIDGET_CLASS (mkt_window_parent_class)->window_state_event (widget, event);
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

  widget_class->focus_in_event = mkt_window_focus_in_event;
  widget_class->focus_out_event = mkt_window_focus_out_event;
  widget_class->window_state_event = mkt_window_window_state_event;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/sadiqpk/multi-keyterm/"
                                               "ui/mkt-window.ui");

  gtk_widget_class_bind_template_child (widget_class, MktWindow, menu_button);
  gtk_widget_class_bind_template_child (widget_class, MktWindow, focus_revealer);
  gtk_widget_class_bind_template_child (widget_class, MktWindow, button_revealer);

  gtk_widget_class_bind_template_child (widget_class, MktWindow, main_stack);

  gtk_widget_class_bind_template_child (widget_class, MktWindow, empty_view);
  gtk_widget_class_bind_template_child (widget_class, MktWindow, empty_icon);
  gtk_widget_class_bind_template_child (widget_class, MktWindow, empty_title);
  gtk_widget_class_bind_template_child (widget_class, MktWindow, empty_subtitle);

  gtk_widget_class_bind_template_child (widget_class, MktWindow, terminal_grid);

  gtk_widget_class_bind_template_callback (widget_class, mkt_window_fullscreen_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, mkt_window_ignore);
  gtk_widget_class_bind_template_callback (widget_class, mkt_window_show_preferences);
  gtk_widget_class_bind_template_callback (widget_class, mkt_window_show_about);
}

static void
mkt_window_init (MktWindow *self)
{
  GListModel *device_list;

  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_window_maximize (GTK_WINDOW (self));

  self->controller = mkt_controller_new ();
  g_signal_connect_object (self->controller, "notify::failed",
                           G_CALLBACK (controller_failed_cb),
                           self, G_CONNECT_SWAPPED);
  controller_failed_cb (self);

  self->key_controller = gtk_event_controller_key_new (GTK_WIDGET (self));
  g_signal_connect_object (self->key_controller, "key-pressed",
                           G_CALLBACK (controller_key_pressed_cb), self,
                           G_CONNECT_SWAPPED);

  device_list = mkt_controller_get_device_list (self->controller);
  gtk_flow_box_bind_model (GTK_FLOW_BOX (self->terminal_grid),
                           device_list,
                           (GtkFlowBoxCreateWidgetFunc)terminal_new,
                           g_object_ref (self), g_object_unref);
  g_signal_connect_object (device_list, "items-changed",
                           G_CALLBACK (device_list_changed_cb),
                           self, G_CONNECT_SWAPPED);
  device_list_changed_cb (self);
}

GtkWidget *
mkt_window_new (GtkApplication *application,
                MktSettings    *settings)
{
  MktWindow *self;

  g_assert (GTK_IS_APPLICATION (application));
  g_assert (MKT_IS_SETTINGS (settings));

  self = g_object_new (MKT_TYPE_WINDOW,
                       "application", application,
                       NULL);
  self->settings = g_object_ref (settings);

  return GTK_WIDGET (self);
}
