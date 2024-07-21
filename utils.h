#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean utils_match_pattern(const gchar *pattern,
                             GtkTextBuffer *buffer,
                             GtkTextIter *start_bound,
                             GtkTextIter *stop_bound,
                             GtkTextIter *start_res,
                             GtkTextIter *stop_res);

gboolean utils_has_tags(GtkTextIter *iter);

void utils_fix_specific_tag(GtkTextBuffer *buffer,
                             const gchar *name,
                             const gchar *patter,
                             const gchar *trim);

void
utils_insert_styling_tag(GtkTextBuffer *buffer,
                         GtkTextIter *start,
                         GtkTextIter *stop,
                         const gchar *pattern,
                         const gchar *tag_name);

  G_END_DECLS