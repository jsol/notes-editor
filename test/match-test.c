#include <glib.h>
#include <gtk/gtk.h>

#include "editor_page.h"
#include "utils.h"

#define PAJ_RECIPE               \
  "## Ingredienser\n"            \
  "- 200g Färsk grönkål\n"    \
  "- 2 Rödlök\n"               \
  "- Liten Matlagningsgrädde\n" \
  "- 1dl Valnötter\n"           \
  "- 2-3 Ägg\n"                 \
  "- 1 msk Grönsaksfond\n"      \
  "\n"                           \
  "\n"                           \
  "## Instruktioner\n"           \
  "\n"                           \
  "Mixa ihop ett pajskal\n"      \
  "\n"                           \
  "**Med lite bold text**\n"

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



int
main(int argc, char *argv[])
{
  g_test_init(&argc, &argv, NULL);

  // Define the tests.
  g_test_add_func("/textbuffer/match/bold", test_match_bold);
  g_test_add_func("/textbuffer/match/code", test_match_code);
  g_test_add_func("/textbuffer/match/h1/start", test_match_h1_start);
  g_test_add_func("/textbuffer/match/h1/middle", test_match_h1_middle);


  return g_test_run();
}
