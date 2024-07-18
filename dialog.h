#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef void (*dialog_cb)(GFile *file, gpointer user_data);

void dialog_folder_select(GtkWindow *parent,
                             const gchar *name,
                             const gchar *yes,
                             dialog_cb cb,
                             gpointer user_data);

G_END_DECLS