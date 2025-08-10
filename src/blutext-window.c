/* blutext-window.c
 *
 * Copyright 2025 Aryana Diaz
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "blutext-window.h"

struct _BlutextWindow
{
  AdwApplicationWindow parent_instance;

  /* Template widgets */
  GtkTextView *main_text_view;
  GtkButton *open_button;
  GtkLabel *cursor_pos;
};

G_DEFINE_FINAL_TYPE (BlutextWindow, blutext_window, ADW_TYPE_APPLICATION_WINDOW)

static void
blutext_window_class_init (BlutextWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (
      widget_class, "/org/menenggala/blutext/blutext-window.ui");
  gtk_widget_class_bind_template_child (
      widget_class, BlutextWindow, main_text_view);
  gtk_widget_class_bind_template_child (
      widget_class, BlutextWindow, open_button);
  gtk_widget_class_bind_template_child (
      widget_class, BlutextWindow, cursor_pos);
}

static void
show_error_dialog (BlutextWindow *self,
                   const gchar *heading,
                   const gchar *body)
{
  AdwDialog *dialog;

  dialog = adw_alert_dialog_new (heading, NULL);

  adw_alert_dialog_set_body (ADW_ALERT_DIALOG (dialog), body);

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", ("_Understood, My Liege"),
                                  NULL);

  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "cancel");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

  adw_dialog_present (dialog, NULL);
}

static void
open_file_complete_gacb (
    GObject *source_object,
    GAsyncResult *result,
    BlutextWindow *self)
{
  GFile *file = G_FILE (source_object);

  g_autofree char *contents = NULL;
  gsize length = 0;

  g_autoptr (GError) error = NULL;
  g_file_load_contents_finish (file, result, &contents, &length, NULL, &error);
  if (error != NULL)
    {
      g_printerr (
          "unable to open %s: %s\n", g_file_peek_path (file), error->message);
      return;
    }

  if (!g_utf8_validate (contents, length, NULL))
    {
      g_printerr (
          "Unable to load the contents of %s: "
          "the file is not encoded with UTF-8\n",
          g_file_peek_path (file));
      show_error_dialog (self, "What the fuck? Just Use UTF-8 !",
                         "This file is not UTF-8 encoded.\n"
                         "GTK demands UTF-8, peasant.");
      return;
    }

  g_autofree char *display_name = NULL;
  g_autoptr (GFileInfo) info = g_file_query_info (
      file, "standard::display-name", G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (info != NULL)
    {
      display_name = g_strdup (
          g_file_info_get_attribute_string (info, "standard::display-name"));
    }
  else
    {
      display_name = g_file_get_basename (file);
    }

  GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->main_text_view);
  gtk_text_buffer_set_text (buffer, contents, length);

  GtkTextIter start;
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_place_cursor (buffer, &start);

  gtk_window_set_title (GTK_WINDOW (self), display_name);
}

static void
open_file (BlutextWindow *self, GFile *file)
{
  g_file_load_contents_async (
      file, NULL, (GAsyncReadyCallback) open_file_complete_gacb, self);
}

static void
on_open_response_gacb (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  BlutextWindow *self = user_data;
  g_autoptr (GFile) file = gtk_file_dialog_open_finish (dialog, result, NULL);
  if (file != NULL)
    open_file (self, file);
}

static void
open_file_dialog_gcb (
    GAction *action,
    GVariant *parameter,
    BlutextWindow *self)
{
  g_autoptr (GtkFileDialog) dialog = gtk_file_dialog_new ();
  gtk_file_dialog_open (
      dialog, GTK_WINDOW (self), NULL, on_open_response_gacb, self);
}

static void
update_cursor_position_gcb (
    GtkTextBuffer *buffer,
    GParamSpec *pspec,
    BlutextWindow *self)
{
  gint cursor_pos = 0;
  g_object_get (buffer, "cursor-position", &cursor_pos, NULL);

  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, cursor_pos);

  g_autofree char *cursor_str = g_strdup_printf (
      "Ln %d, Col %d",
      gtk_text_iter_get_line (&iter) + 1,
      gtk_text_iter_get_line_offset (&iter) + 1);
  gtk_label_set_text (self->cursor_pos, cursor_str);
}

static void
save_file_complete_gacb (
    GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
  GFile *file = G_FILE (source_object);

  g_autoptr (GError) error = NULL;
  g_file_replace_contents_finish (file, result, NULL, &error);

  // Query the display name for the file
  g_autofree char *display_name = NULL;
  g_autoptr (GFileInfo) info = g_file_query_info (
      file, "standard::display-name", G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (info != NULL)
    {
      display_name = g_strdup (
          g_file_info_get_attribute_string (info, "standard::display-name"));
    }
  else
    {
      display_name = g_file_get_basename (file);
    }

  if (error != NULL)
    {
      g_printerr ("Unable to save “%s”: %s\n", display_name, error->message);
    }
}

static void
save_file (BlutextWindow *self, GFile *file)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->main_text_view);

  GtkTextIter start; // do not use pointer it will become UB !
  gtk_text_buffer_get_start_iter (buffer, &start);

  GtkTextIter end; // do not use pointer it will become UB !
  gtk_text_buffer_get_end_iter (buffer, &end);

  // save all content visible to user
  gchar *text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  if (text == NULL)
    return;

  g_autoptr (GBytes) bytes = g_bytes_new_take (text, strlen (text));
  g_file_replace_contents_bytes_async (
      file,
      bytes,
      NULL,
      FALSE,
      G_FILE_CREATE_NONE,
      NULL,
      save_file_complete_gacb,
      self);
}

static void
on_save_response_gacb (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  g_autoptr (GFile) file = gtk_file_dialog_save_finish (dialog, result, NULL);

  if (file != NULL)
    save_file (user_data, file);
}

static void
save_file_dialog_gcb (
    GAction *action,
    GVariant *param,
    BlutextWindow *self)
{
  g_autoptr (GtkFileDialog) dialog = gtk_file_dialog_new ();
  gtk_file_dialog_save (
      dialog, GTK_WINDOW (self), NULL, on_save_response_gacb, self);
}

static void
blutext_window_init (BlutextWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_autoptr (GSimpleAction) open_action = g_simple_action_new ("open", NULL);
  g_signal_connect (
      open_action,
      "activate",
      G_CALLBACK (open_file_dialog_gcb),
      self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (open_action));

  g_autoptr (GSimpleAction) save_as_action =
      g_simple_action_new ("save-as", NULL);
  g_signal_connect (
      save_as_action,
      "activate",
      G_CALLBACK (save_file_dialog_gcb),
      self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (save_as_action));

  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (open_action));
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->main_text_view);
  g_signal_connect (
      buffer,
      "notify::cursor-position",
      G_CALLBACK (update_cursor_position_gcb),
      self);
}

