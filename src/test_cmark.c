#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>
#include <glib.h>

static gpointer
g_calloc0(gsize num, gsize s)
{
  return g_malloc0(num * s);
}

static void
print_node(cmark_node *node, cmark_event_type ev_type)
{
  gint length;
  gint offset;
  gchar character;
  switch (cmark_node_get_type(node)) {
  case CMARK_NODE_HEADING:
    if (ev_type == CMARK_EVENT_ENTER) {
      g_print("level %d: ", cmark_node_get_heading_level(node));
    } else {
      g_print("(exit level %d:) \n", cmark_node_get_heading_level(node));
    }
    break;
  case CMARK_NODE_TEXT:
    g_print("Node name: %s: ", cmark_node_get_type_string(node));
    g_print("\n<p>%s</p>\n", cmark_node_get_literal(node));
    break;

  case CMARK_NODE_LINK:
    g_print("Node name: %s: ", cmark_node_get_type_string(node));
    g_print("\n<url>%s</url>\n", cmark_node_get_url(node));
    break;
  case CMARK_NODE_CODE_BLOCK:
    g_print("Node name: %s: ", cmark_node_get_type_string(node));
    cmark_node_get_fenced(node, &length, &offset, &character);
    g_print("Fencing: %d, %d, %c\n", length, offset, character);
    g_print("\n<pre>%s</pre>\n", cmark_node_get_literal(node));
    break;
  default:

    g_print("Node name: %s\n", cmark_node_get_type_string(node));
  }
}
static void
print_content(cmark_node *document)
{
  cmark_iter *iter;
  cmark_event_type ev_type;
  cmark_node *cur;

  iter = cmark_iter_new(document);

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    print_node(cur, ev_type);
  }

  cmark_iter_free(iter);
}

int
main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
  cmark_parser *parser;
  cmark_mem mem = { 0 };
  gchar *content = NULL;
  gsize size;
  cmark_node *document;
  // cmark_syntax_extension *reflink;
  GError *lerr = NULL;
  GRegex *links;

  mem.calloc = g_calloc0;
  mem.free = g_free;
  mem.realloc = g_realloc;

  links = g_regex_new("\\(\\{\\{< (.*) >\\}\\}\\)", 0, 0, &lerr);
  if (!links) {
    g_warning("Could not create regex: %s", lerr->message);
    g_clear_error(&lerr);
    return 1;
  }

  gchar *replaced;

  if (!g_file_get_contents("test.md", &content, &size, &lerr)) {
    g_warning("Could not load md file: %s", lerr->message);
    g_clear_error(&lerr);
    return 1;
  }

  replaced = g_regex_replace(links, content, -1, 0, "(\\1)", 0, &lerr);

  if (!replaced) {
    g_warning("Could not exec regex: %s", lerr->message);
    g_clear_error(&lerr);
    return 1;
  }

  g_print(" ==== Replaced text ====\n%s\n==== End replaced text ====\n",
          replaced);

  cmark_gfm_core_extensions_ensure_registered();

  // reflink = create_reflink_extension();
  parser = cmark_parser_new_with_mem(CMARK_OPT_DEFAULT, &mem);
  // cmark_parser_attach_syntax_extension(parser, reflink);
  cmark_parser_feed(parser, replaced, size);
  document = cmark_parser_finish(parser);

  print_content(document);

  cmark_parser_free(parser);

  cmark_node *root = cmark_node_new(CMARK_NODE_DOCUMENT);

  cmark_node *child, *sub, *subsub;

  child = cmark_node_new(CMARK_NODE_HEADING);
  cmark_node_set_heading_level(child, 3);
  sub = cmark_node_new(CMARK_NODE_TEXT);
  cmark_node_set_literal(sub, "this is a heading");
  cmark_node_append_child(child, sub);
  cmark_node_append_child(root, child);

  child = cmark_node_new(CMARK_NODE_PARAGRAPH);
  sub = cmark_node_new(CMARK_NODE_TEXT);
  cmark_node_set_literal(sub, "this is a text");
  cmark_node_append_child(child, sub);
  cmark_node_append_child(root, child);

  child = cmark_node_new(CMARK_NODE_PARAGRAPH);
  sub = cmark_node_new(CMARK_NODE_LINK);
  subsub = cmark_node_new(CMARK_NODE_TEXT);

  cmark_node_set_literal(subsub, "this is a link");
  cmark_node_set_url(sub, "{{< \"Link url\" >}}");

  cmark_node_append_child(sub, subsub);
  cmark_node_append_child(child, sub);
  cmark_node_append_child(root, child);

  gchar *res = cmark_render_commonmark_with_mem(root, CMARK_OPT_UNSAFE, 80,
                                                &mem);

  g_print("==== Rendered MD ====\n%s\n==== End rendered MD ====\n", res);
}
