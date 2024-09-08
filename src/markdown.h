#pragma once

#include <glib.h>
#include <gtk/gtk.h>

typedef void (*new_link_anchor)(const gchar *heading,
                                 GtkTextChildAnchor *anchor,
                                 gpointer user_data);
typedef void (*new_img_anchor)(const gchar *file,
                                GtkTextChildAnchor *anchor,
                                gpointer user_data);

struct report_anchor {
  new_link_anchor link;
  gpointer link_user_data;

  new_img_anchor img;
  gpointer img_user_data;
};

gboolean markdown_to_text_buffer(const gchar *markdown,
                                 GtkTextBuffer *buffer,
                                 struct report_anchor *ctx);

gchar *markdown_from_text_buffer(GtkTextBuffer *buffer);