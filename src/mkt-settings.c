/* mkt-settings.c
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

#define G_LOG_DOMAIN "mkt-settings"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mkt-settings.h"
#include "mkt-log.h"

/**
 * SECTION: mkt-settings
 * @title: MktSettings
 * @short_description: The Application settings
 * @include: "mkt-settings.h"
 *
 * A class that handles application specific settings, and
 * to store them to disk.
 */

struct _MktSettings
{
  GObject    parent_instance;

  GSettings *settings;
  GSettings *desktop_settings;
  GSettings *input_settings;

  char      *font;
  char      *keyboard_layout;
  gboolean   first_run;
  gboolean   use_system_font;
};

G_DEFINE_TYPE (MktSettings, mkt_settings, G_TYPE_OBJECT)


enum {
  FONT_CHANGED,
  KBD_LAYOUT_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
settings_kbd_layout_changed_cb (MktSettings *self,
                                char        *key)
{
  g_assert (MKT_IS_SETTINGS (self));

  if (g_strcmp0 (key, "mru-sources") == 0)
    {
      g_autoptr(GVariant) value = NULL;

      value = g_settings_get_value (self->input_settings, key);
      if (g_variant_n_children (value) > 0)
        {
          const char *layout, *type;

          g_variant_get_child (value, 0, "(&s&s)", &type, &layout);

          if (g_strcmp0 (type, "xkb") == 0)
            {
              g_free (self->keyboard_layout);
              self->keyboard_layout = g_strdup (layout);
              g_debug ("Keyboard layout changed to '%s'", layout);

              g_signal_emit (self, signals[KBD_LAYOUT_CHANGED], 0);
            }
        }
    }
}

static void
mkt_settings_dispose (GObject *object)
{
  MktSettings *self = (MktSettings *)object;

  MKT_TRACE_MSG ("disposing settings");

  if (self->settings)
    {
      g_settings_set_string (self->settings, "version", PACKAGE_VERSION);
      g_settings_apply (self->settings);
    }

  g_clear_object (&self->settings);
  g_clear_object (&self->desktop_settings);
  g_clear_pointer (&self->font, g_free);
  g_clear_pointer (&self->keyboard_layout, g_free);

  G_OBJECT_CLASS (mkt_settings_parent_class)->dispose (object);
}

static void
mkt_settings_class_init (MktSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = mkt_settings_dispose;

  signals [FONT_CHANGED] =
    g_signal_new ("font-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals [KBD_LAYOUT_CHANGED] =
    g_signal_new ("kbd-layout-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
mkt_settings_init (MktSettings *self)
{
  GSettingsSchema *schema = NULL;
  g_autofree char *version = NULL;

  self->settings = g_settings_new (PACKAGE_ID);

  version = g_settings_get_string (self->settings, "version");

  if (!g_str_equal (version, PACKAGE_VERSION))
    self->first_run = TRUE;

  g_settings_delay (self->settings);
  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                            "org.gnome.desktop.interface", TRUE);

  if (schema)
    self->desktop_settings = g_settings_new_full (schema, NULL, NULL);

  g_clear_pointer (&schema, g_settings_schema_unref);

  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                            "org.gnome.desktop.input-sources", TRUE);

  if (schema)
    {
      self->input_settings = g_settings_new_full (schema, NULL, NULL);
      g_signal_connect_object (self->input_settings,
                               "changed",
                               G_CALLBACK (settings_kbd_layout_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
      settings_kbd_layout_changed_cb (self, "mru-sources");
    }

  g_clear_pointer (&schema, g_settings_schema_unref);

  self->use_system_font = g_settings_get_boolean (self->settings, "use-system-font");
  if (self->use_system_font)
    {
      if (self->desktop_settings)
        self->font = g_settings_get_string (self->desktop_settings, "monospace-font-name");

      if (!self->font)
        self->font = g_strdup ("Monospace 11");
    }
  else
    {
      self->font = g_settings_get_string (self->settings, "font");
    }
}

/**
 * mkt_settings_new:
 *
 * Create a new #MktSettings
 *
 * Returns: (transfer full): A #MktSettings.
 * Free with g_object_unref().
 */
MktSettings *
mkt_settings_new (void)
{
  return g_object_new (MKT_TYPE_SETTINGS, NULL);
}

/**
 * mkt_settings_save:
 * @self: A #MktSettings
 *
 * Save modified settings to disk.  By default,
 * the modified settings are saved to disk only
 * when #MktSettings is disposed.  Use this
 * to force save to disk.
 */
void
mkt_settings_save (MktSettings *self)
{
  g_return_if_fail (MKT_IS_SETTINGS (self));

  g_settings_apply (self->settings);
}

/**
 * mkt_settings_get_is_first_run:
 * @self: A #MktSettings
 *
 * Get if the application has ever launched after install
 * or update.
 *
 * Returns: %TRUE for the first launch of application after
 * install or update.  %FALSE otherwise.
 */
gboolean
mkt_settings_get_is_first_run (MktSettings *self)
{
  g_return_val_if_fail (MKT_IS_SETTINGS (self), FALSE);

  return self->first_run;
}

gboolean
mkt_settings_get_use_system_font (MktSettings *self)
{
  g_return_val_if_fail (MKT_IS_SETTINGS (self), FALSE);

  return self->use_system_font;
}

void
mkt_settings_set_use_system_font (MktSettings *self,
                                  gboolean     use_system_font)
{
  g_return_if_fail (MKT_IS_SETTINGS (self));

  use_system_font = !!use_system_font;

  if (self->use_system_font == use_system_font)
    return;

  g_clear_pointer (&self->font, g_free);
  self->use_system_font = use_system_font;
  g_settings_set_boolean (self->settings, "use-system-font", use_system_font);

  if (use_system_font)
    {
      g_settings_set_string (self->settings, "font", "");

      if (self->desktop_settings)
        self->font = g_settings_get_string (self->desktop_settings, "monospace-font-name");

      if (!self->font)
        self->font = g_strdup ("Monospace 11");
    }

  g_signal_emit (self, signals[FONT_CHANGED], 0);
}

const char *
mkt_settings_get_font (MktSettings *self)
{
  g_return_val_if_fail (MKT_IS_SETTINGS (self), NULL);

  if (self->font)
    return self->font;

  return "Monospace 11";
}

void
mkt_settings_set_font (MktSettings *self,
                       const char  *font)
{
  g_return_if_fail (MKT_IS_SETTINGS (self));
  g_return_if_fail (font && *font);

  if (g_strcmp0 (self->font, font) == 0)
    return;

  g_free (self->font);
  self->font = g_strdup (font);
  g_settings_set_string (self->settings, "font", font);

  g_signal_emit (self, signals[FONT_CHANGED], 0);
}

const char *
mkt_settings_get_kbd_layout (MktSettings *self)
{
  g_return_val_if_fail (MKT_IS_SETTINGS (self), NULL);

  if (self->keyboard_layout)
    return self->keyboard_layout;

  return "us";
}
