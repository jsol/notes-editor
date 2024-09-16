#include <glib.h>
#include <gtk/gtk.h>

#include "markdown.h"

static void
assert_not_called(G_GNUC_UNUSED const gchar *heading,
                  G_GNUC_UNUSED GtkTextChildAnchor *anchor,
                  G_GNUC_UNUSED gpointer user_data)
{
  g_assert_true(false);
}

void
test_match_h1_start(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  GtkTextIter start_res;
  GtkTextIter stop_res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("# Header 1", content, &ctx);

  gtk_text_buffer_get_start_iter(content, &start_res);
  gtk_text_buffer_get_end_iter(content, &stop_res);

  g_assert_cmpstr("Header 1", ==,
                  gtk_text_iter_get_visible_text(&start_res, &stop_res));

  g_clear_object(&content);
}

void
test_match_h1_middle(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  GtkTextIter start_res;
  GtkTextIter stop_res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("This is some initial text\n\n\n\n# Header 1",
                          content, &ctx);

  gtk_text_buffer_get_start_iter(content, &start_res);
  gtk_text_buffer_get_end_iter(content, &stop_res);

  g_assert_cmpstr("This is some initial text\n\nHeader 1", ==,
                  gtk_text_iter_get_visible_text(&start_res, &stop_res));

  g_clear_object(&content);
}

void
test_match_h2_start(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  GtkTextIter start_res;
  GtkTextIter stop_res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("## Header 2", content, &ctx);

  gtk_text_buffer_get_start_iter(content, &start_res);
  gtk_text_buffer_get_end_iter(content, &stop_res);

  g_assert_cmpstr("Header 2", ==,
                  gtk_text_iter_get_visible_text(&start_res, &stop_res));

  g_clear_object(&content);
}

void
test_match_h3_start(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  GtkTextIter start_res;
  GtkTextIter stop_res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("### Header 3", content, &ctx);

  gtk_text_buffer_get_start_iter(content, &start_res);
  gtk_text_buffer_get_end_iter(content, &stop_res);

  g_assert_cmpstr("Header 3", ==,
                  gtk_text_iter_get_visible_text(&start_res, &stop_res));

  g_clear_object(&content);
}

void
test_match_code_start(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  GtkTextIter start_res;
  GtkTextIter stop_res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("````\nThis is some code\n````", content, &ctx);

  gtk_text_buffer_get_start_iter(content, &start_res);
  gtk_text_buffer_get_end_iter(content, &stop_res);

  g_assert_cmpstr("\n\nThis is some code\n", ==,
                  gtk_text_iter_get_visible_text(&start_res, &stop_res));

  g_clear_object(&content);
}

void
test_match_code_middle(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  GtkTextIter start_res;
  GtkTextIter stop_res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("Some generic text\n\n````\nThis is some "
                          "code\n````\n\nSome ending text",
                          content, &ctx);

  gtk_text_buffer_get_start_iter(content, &start_res);
  gtk_text_buffer_get_end_iter(content, &stop_res);

  g_assert_cmpstr("Some generic text\n\nThis is some "
                  "code\n\n\nSome ending text",
                  ==, gtk_text_iter_get_visible_text(&start_res, &stop_res));

  g_clear_object(&content);
}

void
test_match_bold_start(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  GtkTextIter start_res;
  GtkTextIter stop_res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("**This is bold text**\n", content, &ctx);

  gtk_text_buffer_get_start_iter(content, &start_res);
  gtk_text_buffer_get_end_iter(content, &stop_res);

  g_assert_cmpstr("This is bold text", ==,
                  gtk_text_iter_get_visible_text(&start_res, &stop_res));

  g_clear_object(&content);
}

void
test_match_emph_start(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  GtkTextIter start_res;
  GtkTextIter stop_res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("*This is emph text*\n", content, &ctx);

  gtk_text_buffer_get_start_iter(content, &start_res);
  gtk_text_buffer_get_end_iter(content, &stop_res);

  g_assert_cmpstr("This is emph text", ==,
                  gtk_text_iter_get_visible_text(&start_res, &stop_res));

  g_clear_object(&content);
}

int
main(int argc, char *argv[])
{
  g_test_init(&argc, &argv, NULL);

  // Define the tests.
  g_test_add_func("/markdown/parse/h1/start", test_match_h1_start);
  g_test_add_func("/markdown/parse/h1/middle", test_match_h1_middle);

  g_test_add_func("/markdown/parse/h2/start", test_match_h2_start);
  g_test_add_func("/markdown/parse/h3/start", test_match_h3_start);

  g_test_add_func("/markdown/parse/bold/start", test_match_bold_start);

  g_test_add_func("/markdown/parse/emph/start", test_match_emph_start);

  g_test_add_func("/markdown/parse/code/start", test_match_code_start);
  g_test_add_func("/markdown/parse/code/middle", test_match_code_middle);

  return g_test_run();
}
