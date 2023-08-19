/* mkt-terminal.c
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

#define G_LOG_DOMAIN "mkt-terminal"

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "version.h"
#endif

#include <ctype.h>
#include <pwd.h>
#include <vte/vte.h>
#include <glib/gi18n.h>

#include "mkt-controller.h"
#include "mkt-device.h"
#include "mkt-utils.h"
#include "mkt-terminal.h"
#include "mkt-log.h"

struct _MktTerminal
{
  GtkFlowBoxChild  parent_instance;

  GtkWidget       *main_stack;
  GtkWidget       *empty_view;
  GtkWidget       *empty_subtitle;
  GtkWidget       *terminal;

  MktController   *controller;
  MktSettings     *settings;
  MktDevice       *device;
  guint            position;

  double           default_scale;
  gboolean         has_shell;
};

G_DEFINE_TYPE (MktTerminal, mkt_terminal, GTK_TYPE_FLOW_BOX_CHILD)


static void
device_key_pressed_cb (MktTerminal  *self,
                       MktDeviceKey *key)
{
  char buffer[6];
  double scale;

  g_assert (MKT_IS_TERMINAL (self));

  scale = vte_terminal_get_font_scale (VTE_TERMINAL (self->terminal));

  if (((key->keyval == GDK_KEY_plus ||
        key->keyval == GDK_KEY_equal) &&
       key->modifier & GDK_CONTROL_MASK) ||
      (key->keyval == GDK_KEY_KP_Add &&
       key->modifier == GDK_CONTROL_MASK))
    {
      vte_terminal_set_font_scale (VTE_TERMINAL (self->terminal), scale + 0.05);
    }
  else if ((key->keyval == GDK_KEY_minus ||
            key->keyval == GDK_KEY_KP_Subtract) &&
           key->modifier == GDK_CONTROL_MASK)
    {
      vte_terminal_set_font_scale (VTE_TERMINAL (self->terminal), scale - 0.05);
    }
  else if (((key->keyval == GDK_KEY_0 ||
             key->keyval == GDK_KEY_KP_0) &&
            key->modifier == GDK_CONTROL_MASK))
    {
      vte_terminal_set_font_scale (VTE_TERMINAL (self->terminal), self->default_scale);
    }
  else if (key->modifier == GDK_CONTROL_MASK &&
           toupper (key->keyval) >= 'A' && toupper (key->keyval) <= 'Z')
    {
      buffer[0] = toupper (key->keyval) - 'A' + 1;
      buffer[1] = '\0';
      vte_terminal_feed_child (VTE_TERMINAL (self->terminal), buffer, -1);
    }
  else if (key->keyval >= GDK_KEY_Left &&
           key->keyval <= GDK_KEY_Down)
    {
      buffer[0] = '\033';
      buffer[1] = '[';
      if (key->keyval == GDK_KEY_Up)
        buffer[2] = 'A';
      else if (key->keyval == GDK_KEY_Down)
        buffer[2] = 'B';
      else if (key->keyval == GDK_KEY_Right)
        buffer[2] = 'C';
      else
        buffer[2] = 'D';

      buffer[3] = '\0';
      vte_terminal_feed_child (VTE_TERMINAL (self->terminal), buffer, -1);
    }
  else
    {
      guint len;

      len = g_unichar_to_utf8 (gdk_keyval_to_unicode (key->keyval), buffer);
      buffer[len] = '\0';
      vte_terminal_feed_child (VTE_TERMINAL (self->terminal), buffer, -1);
    }
}

static void
child_ready_cb (VteTerminal *terminal,
                GPid         pid,
                GError      *error,
                gpointer     user_data)
{
  g_autoptr (MktTerminal) self = user_data;

  if (error)
    g_warning ("error: %s", error->message);

  self->has_shell = !error;
}

static void
terminal_start_bash (MktTerminal *self)
{
  g_autoptr(GPtrArray) commands = NULL;
  g_autofree char *shell = NULL;
  g_autofree char *user = NULL;
  g_autofree char *cwd = NULL;
  char **argv = NULL;
  size_t len = 0;

  if (self->has_shell)
    return;

  shell = vte_get_user_shell ();

  if (!shell)
    shell = g_strdup ("/bin/sh");

  commands = g_ptr_array_new_full (0, g_free);
  g_ptr_array_add (commands, g_strdup (shell));

  user = g_strdup (g_getenv ("SUDO_USER"));

  if (!user && g_getenv ("PKEXEC_UID"))
    {
      struct passwd *passwd = NULL;
      gint64 uid;

      uid = g_ascii_strtoll (g_getenv ("PKEXEC_UID"), NULL, 10);

      if (uid >= 1000)
        passwd = getpwuid (uid);

      if (passwd)
        user = g_strdup (passwd->pw_name);
    }

  /* Rough estimate of valid unix username, avoids cases like “$” */
  if (user)
    len = strspn (user, "abcdefghijklmnopqrstuvwxyz"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ_-01234567890");

  if (g_strcmp0 (g_getenv ("USER"), "root") == 0 &&
      user && len < 63 && len == strlen (user))
    {
      char *command;

      /* XXX: Unsafe? */
      command = g_strdup_printf ("sudo -u %s %s", user, shell);
      cwd = g_strdup_printf ("/home/%s", user);

      g_ptr_array_add (commands, g_strdup ("-c"));
      g_ptr_array_add (commands, command);
    }

  g_ptr_array_add (commands, NULL);
  argv = (char **)commands->pdata;
  if (!cwd)
    cwd = g_strdup_printf ("/home/%s", g_getenv ("USER"));

  vte_terminal_spawn_async (VTE_TERMINAL (self->terminal),
                            VTE_PTY_DEFAULT,
                            cwd, argv, NULL, G_SPAWN_SEARCH_PATH,
                            NULL, NULL, NULL, -1,
                            NULL,
                            child_ready_cb, g_object_ref (self));
}

static void
device_enable_changed_cb (MktTerminal *self)
{
  GtkWidget *child;

  g_assert (MKT_IS_TERMINAL (self));

  if (mkt_device_get_enabled (self->device))
    terminal_start_bash (self);

  if (self->device &&
      mkt_device_get_enabled (self->device))
    child = self->terminal;
  else
    child = self->empty_view;

  gtk_stack_set_visible_child (GTK_STACK (self->main_stack), child);
}

static void
terminal_font_changed_cb (MktTerminal *self,
                          MktSettings *settings)
{
  PangoFontDescription *font_desc = NULL;
  const char *font = NULL;

  g_assert (MKT_IS_TERMINAL (self));
  g_assert (MKT_IS_SETTINGS (settings));

  font = mkt_settings_get_font (settings);

  if (font)
    font_desc = pango_font_description_from_string (font);

  vte_terminal_set_font (VTE_TERMINAL (self->terminal), font_desc);
}

static void
mkt_terminal_close (MktTerminal *self)
{
  g_assert (MKT_IS_TERMINAL (self));

  mkt_controller_remove_device (self->controller, self->device);
}

static void
terminal_child_exited_cb (MktTerminal *self)
{
  GListModel *device_list;

  g_assert (MKT_IS_TERMINAL (self));

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  self->has_shell = FALSE;
  vte_terminal_reset (VTE_TERMINAL (self->terminal), TRUE, TRUE);
  mkt_device_set_enabled (self->device, FALSE);

  device_list = mkt_controller_get_device_list (self->controller);
  /* If there is only one terminal, Let's just close */
  if (g_list_model_get_n_items (device_list) == 1)
    {
      mkt_terminal_close (self);
    }
  else
    {
      g_autofree char *label = NULL;
      guint index = 0;

      mkt_utils_get_item_position (device_list, self->device, &index);
      label = g_strdup_printf ("Press “%d” to start the terminal", index + 1);
      gtk_label_set_text (GTK_LABEL (self->empty_subtitle), label);
    }
}

static void
mkt_terminal_finalize (GObject *object)
{
  MktTerminal *self = (MktTerminal *)object;

  g_clear_object (&self->device);
  g_clear_object (&self->settings);
  g_clear_object (&self->controller);

  G_OBJECT_CLASS (mkt_terminal_parent_class)->finalize (object);
}

static void
mkt_terminal_class_init (MktTerminalClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = mkt_terminal_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/sadiqpk/multi-keyterm/"
                                               "ui/mkt-terminal.ui");

  gtk_widget_class_bind_template_child (widget_class, MktTerminal, main_stack);
  gtk_widget_class_bind_template_child (widget_class, MktTerminal, empty_view);
  gtk_widget_class_bind_template_child (widget_class, MktTerminal, empty_subtitle);
  gtk_widget_class_bind_template_child (widget_class, MktTerminal, terminal);

  gtk_widget_class_bind_template_callback (widget_class, mkt_terminal_close);
}

static void
mkt_terminal_init (MktTerminal *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  g_signal_connect_object (self->terminal, "child-exited",
                           G_CALLBACK (terminal_child_exited_cb),
                           self, G_CONNECT_SWAPPED);
  self->default_scale = vte_terminal_get_font_scale (VTE_TERMINAL (self->terminal));
}

GtkWidget *
mkt_terminal_new (MktController *controller,
                  MktSettings   *settings,
                  MktDevice     *device)
{
  MktTerminal *self;

  g_assert (MKT_IS_CONTROLLER (controller));
  g_assert (MKT_IS_SETTINGS (settings));
  g_assert (MKT_IS_DEVICE (device));

  self = g_object_new (MKT_TYPE_TERMINAL, NULL);
  self->controller = g_object_ref (controller);
  self->settings = g_object_ref (settings);
  self->device = g_object_ref (device);

  g_signal_connect_object (device, "key-pressed",
                           G_CALLBACK (device_key_pressed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (device, "notify::enabled",
                           G_CALLBACK (device_enable_changed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->settings, "font-changed",
                           G_CALLBACK (terminal_font_changed_cb),
                           self, G_CONNECT_SWAPPED);
  terminal_font_changed_cb (self, settings);
  device_enable_changed_cb (self);

  return GTK_WIDGET (self);
}

MktDevice *
mkt_terminal_get_device (MktTerminal *self)
{
  g_return_val_if_fail (MKT_IS_TERMINAL (self), NULL);

  return self->device;
}
