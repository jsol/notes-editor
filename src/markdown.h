#pragma once

#include <glib.h>
#include <gtk/gtk.h>

enum markdown_tag_types {
  MARKDOWN_TAG_BOLD = 0,
  MARKDOWN_TAG_EMPH,
  MARKDOWN_TAG_CODE,
  MARKDOWN_TAG_H1,
  MARKDOWN_TAG_H2,
  MARKDOWN_TAG_H3,
  MARKDOWN_TAG_LAST
};

typedef void (*new_link_anchor)(const gchar *heading,
                                GtkTextChildAnchor *anchor,
                                gpointer user_data);
typedef void (*new_img_anchor)(const gchar *file,
                               GtkTextChildAnchor *anchor,
                               gpointer user_data);

typedef void (*style_tag)(enum markdown_tag_types type, GtkTextTag *tag);

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

void markdown_setup_tags(GtkTextBuffer *buffer, style_tag cb);
