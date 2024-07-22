#include <gdk/gdk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "dialog.h"

static void
file_chooser_cb(GtkNativeDialog *source_object, int response)
{
  dialog_cb cb;
  gpointer user_data;
  GFile *file = NULL;

  if (response != GTK_RESPONSE_ACCEPT) {
    return;
  }

  cb = g_object_get_data(G_OBJECT(source_object), "calback");
  user_data = g_object_get_data(G_OBJECT(source_object), "calback_userdata");
  file = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(source_object));

  if (file == NULL) {
    g_warning("Error opening file");
  } else {
    cb(file, user_data);

    g_clear_object(&file);
  }
}

void
dialog_folder_select(GtkWindow *parent,
                     const gchar *title,
                     const gchar *yes,
                     dialog_cb cb,
                     gpointer user_data)
{
  GtkFileChooserNative *native;
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
  /* Todo: Deprecated, but needs GTK 4.10, ubuntu 22.04 only has 4.6 */

  native = gtk_file_chooser_native_new(title, parent, action, yes, "_Cancel");

  g_object_set_data(G_OBJECT(native), "calback", cb);
  g_object_set_data(G_OBJECT(native), "calback_userdata", user_data);

  g_signal_connect(native, "response", G_CALLBACK(file_chooser_cb), NULL);
  gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
}