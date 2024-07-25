#include <glib.h>
#include <gtk/gtk.h>

#include "editor_page.h"
#include "utils.h"

#define ALL_TAGS                    \
  "---\n"                           \
  "title: \"Test MD file\"\n"       \
  "draft: true\n"                   \
  "tags:\n"                         \
  "  - Tag 1\n"                     \
  "---\n"                           \
  "# Header 1\n"                    \
  "some text\nsome more text\n\n"   \
  "## Header 2\n"                   \
  "And some text. **bold text**.\n" \
  "### Header 3\n"                  \
  "some text\n"                     \
  "````\nCode in monospace\n````\n" \
  "Ending text"

void
test_match_bold(void)
{
  GtkTextBuffer *content;
  const gchar *text = "First words **then bold text** and then words";
  gboolean res;
  GtkTextIter start_res;
  GtkTextIter stop_res;

  content = gtk_text_buffer_new(NULL);
  gtk_text_buffer_set_text(content, text, -1);

  res = utils_match_pattern("**?**", content, NULL, NULL, &start_res, &stop_res);

  g_assert_true(res);
  g_assert_cmpstr("**then bold text**", ==,
                  gtk_text_iter_get_text(&start_res, &stop_res));

  g_message("Bold: %s\n", gtk_text_iter_get_text(&start_res, &stop_res));
}

void
test_match_code(void)
{
  GtkTextBuffer *content;
  const gchar *text = "First words then ````some code\nwith new lines\nin "
                      "it```` and then words";
  gboolean res;
  GtkTextIter start_res;
  GtkTextIter stop_res;

  content = gtk_text_buffer_new(NULL);
  gtk_text_buffer_set_text(content, text, -1);

  res = utils_match_pattern(PATTERN_CODE, content, NULL, NULL, &start_res,
                            &stop_res);

  g_assert_true(res);
  g_assert_cmpstr("````some code\nwith new lines\nin it````", ==,
                  gtk_text_iter_get_text(&start_res, &stop_res));

  g_message("Code: %s\n", gtk_text_iter_get_text(&start_res, &stop_res));
}

void
test_match_h1_start(void)
{
  GtkTextBuffer *content;
  const gchar *text = "# This is the beginning\nAnd then some text";
  gboolean res;
  GtkTextIter start_res;
  GtkTextIter stop_res;

  content = gtk_text_buffer_new(NULL);
  gtk_text_buffer_set_text(content, text, -1);

  res = utils_match_pattern(PATTERN_H1, content, NULL, NULL, &start_res,
                            &stop_res);

  g_assert_true(res);
  g_assert_cmpstr("# This is the beginning\n", ==,
                  gtk_text_iter_get_text(&start_res, &stop_res));

  g_message("H1 start: %s\n", gtk_text_iter_get_text(&start_res, &stop_res));
}

void
test_match_h1_middle(void)
{
  GtkTextBuffer *content;
  const gchar *text = "This is some text\n\n# This is a header\nAnd then some "
                      "text";
  gboolean res;
  GtkTextIter start_res;
  GtkTextIter stop_res;

  content = gtk_text_buffer_new(NULL);
  gtk_text_buffer_set_text(content, text, -1);

  res = utils_match_pattern(PATTERN_H1, content, NULL, NULL, &start_res,
                            &stop_res);

  g_assert_true(res);
  g_assert_cmpstr("# This is a header\n", ==,
                  gtk_text_iter_get_text(&start_res, &stop_res));

  g_message("H1 middle: %s\n", gtk_text_iter_get_text(&start_res, &stop_res));
}

void
test_match_save_unchanged(void)
{
  EditorPage *p;
  GHashTable *pages = g_hash_table_new(g_str_hash, g_str_equal);
  GString *md;
  gchar *input;

  input = g_strdup(ALL_TAGS);
  p = editor_page_load(pages, input, NULL, NULL);
  md = editor_page_to_md(p);

  g_message("Testing %s ", ALL_TAGS);

  g_assert_cmpstr(ALL_TAGS, ==, md->str);

  g_free(input);
  g_string_free(md, TRUE);
  g_object_unref(p);
}

int
main(int argc, char *argv[])
{
  g_test_init(&argc, &argv, NULL);

  // Define the tests.
  g_test_add_func("/textbuffer/match/bold", test_match_bold);
  g_test_add_func("/textbuffer/match/code", test_match_code);
  g_test_add_func("/textbuffer/match/h1/start", test_match_h1_start);
  g_test_add_func("/textbuffer/match/h1/middle", test_match_h1_middle);
  g_test_add_func("/textbuffer/match/save/unchanged", test_match_save_unchanged);

  return g_test_run();
}
