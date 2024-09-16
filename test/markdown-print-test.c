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
  gchar *res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("# Header 1\n", content, &ctx);

  res = markdown_from_text_buffer(content);

  g_assert_cmpstr("# Header 1\n", ==, res);

  g_clear_object(&content);
  g_free(res);
}

void
test_match_h1_middle(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  gchar *res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("This is some text\n\n# Header 1", content, &ctx);

  res = markdown_from_text_buffer(content);

  g_assert_cmpstr("This is some text\n\n# Header 1\n", ==, res);

  g_clear_object(&content);
  g_free(res);
}

void
test_match_h2_start(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  gchar *res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("## Header 2", content, &ctx);

  res = markdown_from_text_buffer(content);

  g_assert_cmpstr("## Header 2\n", ==, res);
  g_clear_object(&content);
  g_free(res);
}

void
test_match_h3_start(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  gchar *res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("### Header 3", content, &ctx);

  res = markdown_from_text_buffer(content);

  g_assert_cmpstr("### Header 3\n", ==, res);

  g_clear_object(&content);
  g_free(res);
}

void
test_match_code_start(void)
{
  GtkTextBuffer *content;
  struct report_anchor ctx = { 0 };
  gchar *res;

  ctx.link = assert_not_called;
  ctx.img = assert_not_called;

  content = gtk_text_buffer_new(NULL);
  markdown_setup_tags(content, NULL);

  markdown_to_text_buffer("````\nThis is some\ncode\n````", content, &ctx);

  res = markdown_from_text_buffer(content);

  g_assert_cmpstr("```  \nThis is some\ncode\n```\n", ==, res);

  g_clear_object(&content);
  g_free(res);
}

int
main(int argc, char *argv[])
{
  g_test_init(&argc, &argv, NULL);

  // Define the tests.
  g_test_add_func("/markdown/print/h1/start", test_match_h1_start);
  g_test_add_func("/markdown/print/h1/middle", test_match_h1_middle);

  g_test_add_func("/markdown/print/h2/start", test_match_h2_start);
  g_test_add_func("/markdown/print/h3/start", test_match_h3_start);

  g_test_add_func("/markdown/print/code/start", test_match_code_start);

  return g_test_run();
}
