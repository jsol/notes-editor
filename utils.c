#include <glib.h>
#include <gtk/gtk.h>

#include "utils.h"

gboolean
utils_match_pattern(const gchar *pattern,
                    GtkTextBuffer *buffer,
                    GtkTextIter *start_bound,
                    GtkTextIter *stop_bound,
                    GtkTextIter *start_res,
                    GtkTextIter *stop_res)
{
  /* pattern: [?}({{< ref "?" >}} "?") */

  GtkTextIter iter;
  GtkTextIter stop;
  GtkTextIter start_buff;
  gunichar c;
  gchar buff[6];
  gint pos = 0;

  if (g_str_has_suffix(pattern, "?")) {
    g_warning("Invalid pattern: %s", pattern);
    return FALSE;
  }

  g_message("Matching pattern %s\n", pattern);

  if (stop_bound == NULL) {
    g_message("Setting offset stop \n");
    gtk_text_buffer_get_end_iter(buffer, &stop);
    stop_bound = &stop;
  }

  if (start_bound == NULL) {
    gtk_text_buffer_get_start_iter(buffer, &iter);
  } else {
    iter = *start_bound;
  }
  gtk_text_buffer_get_start_iter(buffer, &start_buff);

  while (!gtk_text_iter_equal(&iter, stop_bound)) {
    c = gtk_text_iter_get_char(&iter);
    g_unichar_to_utf8(c, buff);

    if (pattern[pos] == '\n' && pos == 0 &&
        (gtk_text_iter_starts_line(&iter) ||
         gtk_text_iter_equal(&start_buff, &iter))) {
      if (gtk_text_iter_equal(&start_buff, &iter)) {
        *start_res = iter;
        pos++;
        continue;
      }
      goto have_match;
    }

    if (pattern[pos] == '\n' && pos > 0 && gtk_text_iter_ends_line(&iter)) {
      goto have_match;
    }

    if (pattern[pos] == '\n') {
      g_message("Failed due to not being at start / stop of line with pattern "
                "\\n (was %c)",
                buff[0]);
      goto failed_match;
    }

    if (pattern[pos] == '?') {
      pos++;
      while (!gtk_text_iter_equal(&iter, stop_bound) && pattern[pos] != buff[0]) {
        gtk_text_iter_forward_char(&iter);
        c = gtk_text_iter_get_char(&iter);
        g_unichar_to_utf8(c, buff);
      }
    }

    if (buff[0] == pattern[pos]) {
      goto have_match;
    }

    goto failed_match;

  have_match:
    if (pos == 0) {
      *start_res = iter;
    }

    pos++;
    gtk_text_iter_forward_char(&iter);

    if (pos == strlen(pattern)) {
      /* complete match */
      *stop_res = iter;
      g_message("Completed match\n");

      c = gtk_text_iter_get_char(start_res);
      g_unichar_to_utf8(c, buff);

      while (buff[0] == '\n') {
        gtk_text_iter_forward_char(start_res);
        c = gtk_text_iter_get_char(start_res);
        g_unichar_to_utf8(c, buff);
        g_message("Checking %c", buff[0]);
      }

      return true;
    }

    continue;

  failed_match:
    pos = 0;
    gtk_text_iter_forward_char(&iter);
  }

  g_message("No match\n");
  return FALSE;
}

gboolean
utils_has_tags(GtkTextIter *iter)
{
  GSList *tags;
  tags = gtk_text_iter_get_tags(iter);

  if (tags == NULL) {
    return FALSE;
  }

  g_slist_free(tags);
  return TRUE;
}

/*  PATTERN_H3 "\n### ?\n" =>
*              "sxxxx?s"

*
*   PATTERN_CODE "````?````" =>
*                 xxxx?xxxx
*/
void
utils_insert_styling_tag(GtkTextBuffer *buffer,
                         GtkTextIter *start,
                         GtkTextIter *stop,
                         const gchar *pattern,
                         const gchar *tag_name)
{
  GtkTextIter start_tag;
  GtkTextIter stop_tag;
  GtkTextIter start_del;
  GtkTextIter stop_del;

  GtkTextMark *delete_start = NULL;
  GtkTextMark *delete_stop = NULL;
  start_tag = *start;
  stop_tag = *stop;
  gboolean do_stop_del = FALSE;
  gboolean do_start_del = FALSE;
  gint content_pos = 0;

  for (gint i = strlen(pattern) - 1; i >= 0; i--) {
    if (pattern[i] == '?') {
      content_pos = i;
      break;
    }
    if (pattern[i] == 's' || pattern[i] == 'x') {
      gtk_text_iter_backward_char(&stop_tag);
    }
  }

  for (gint i = 0; i < strlen(pattern); i++) {
    if (pattern[i] == '?') {
      break;
    }

    if (pattern[i] == 's' || pattern[i] == 'x') {
      gtk_text_iter_forward_char(&start_tag);
    }
  }

  gtk_text_buffer_apply_tag_by_name(buffer, tag_name, &start_tag, &stop_tag);

  start_del = start_tag;
  stop_del = stop_tag;

  for (gint i = content_pos - 1; i >= 0; i--) {
    if (pattern[i] == 'x') {
      gtk_text_iter_backward_char(&start_del);
      do_start_del = TRUE;
    } else {
      break;
    }
  }

  for (gint i = content_pos + 1; i < strlen(pattern); i++) {
    if (pattern[i] == 'x') {
      gtk_text_iter_forward_char(&stop_del);
      do_stop_del = TRUE;
    } else {
      break;
    }
  }

  if (do_stop_del && do_start_del) {
    delete_start = gtk_text_buffer_create_mark(buffer, NULL, &stop_tag, TRUE);
    delete_stop = gtk_text_buffer_create_mark(buffer, NULL, &stop_del, TRUE);
  }

  if (do_start_del) {
    gtk_text_buffer_delete(buffer, &start_del, &start_tag);
  }

  if (do_stop_del) {
    gtk_text_buffer_get_iter_at_mark(buffer, &stop_tag, delete_start);
    gtk_text_buffer_get_iter_at_mark(buffer, &stop_del, delete_stop);
    gtk_text_buffer_delete(buffer, &stop_tag, &stop_del);
  }
}

void
utils_fix_specific_tag(GtkTextBuffer *buffer,
                       const gchar *name,
                       const gchar *pattern,
                       const gchar *trim)

{
  GtkTextIter start_bound;
  GtkTextIter start_res;
  GtkTextIter stop_res;
  GtkTextMark *search_start;

  gtk_text_buffer_get_start_iter(buffer, &start_bound);

  while (utils_match_pattern(pattern, buffer, &start_bound, NULL, &start_res,
                             &stop_res)) {
    if (!utils_has_tags(&start_res) && !utils_has_tags(&stop_res)) {
      search_start = gtk_text_buffer_create_mark(buffer, NULL, &stop_res, TRUE);

      utils_insert_styling_tag(buffer, &start_res, &stop_res, trim, name);
      gtk_text_buffer_get_iter_at_mark(buffer, &start_bound, search_start);
    } else {
      start_bound = stop_res;
    }
  }
}