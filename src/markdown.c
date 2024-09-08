#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "markdown.h"

static void parse_document(cmark_node *document,
                           GtkTextBuffer *buffer,
                           struct report_anchor *ctx);

static gpointer
g_calloc0(gsize num, gsize s)
{
  return g_malloc0(num * s);
}

static gchar *
preprocess_md(const gchar *md, GError **err)
{
  GRegex *links;
  gchar *replaced;

  g_assert(md);
  g_assert(err == NULL || *err == NULL);

  links = g_regex_new("\\(\\{\\{< (.*) >\\}\\}\\)", 0, 0, err);
  if (!links) {
    return NULL;
  }

  replaced = g_regex_replace(links, md, -1, 0, "(\\1)", 0, err);

  g_regex_unref(links);

  return replaced;
}

static void
set_mem(cmark_mem *mem)
{
  mem->calloc = g_calloc0;
  mem->free = g_free;
  mem->realloc = g_realloc;
}

static GtkTextTag *
get_tag(GtkTextBuffer *buffer, const gchar *name)
{
  GtkTextTagTable *table;

  g_assert(buffer);
  g_assert(name);

  table = gtk_text_buffer_get_tag_table(buffer);

  return gtk_text_tag_table_lookup(table, name);
}

static GtkTextTag *
get_heading_tag(GtkTextBuffer *buffer, gint level)
{
  gchar name[3];
  g_assert(buffer);
  g_assert(level > 0 && level < 10);

  g_snprintf(name, 3, "h%d", level);

  return get_tag(buffer, name);
}

static void
parse_node(cmark_node *node, GtkTextBuffer *buffer, struct report_anchor *ctx)
{
  GtkTextIter end;
  cmark_node *child;
  GtkTextTag *tag;

  switch (cmark_node_get_type(node)) {
  case CMARK_NODE_TEXT:
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, cmark_node_get_literal(node), -1);
    break;

  case CMARK_NODE_STRONG:
    child = cmark_node_first_child(node);
    if (child == NULL || cmark_node_get_type(child) != CMARK_NODE_TEXT) {
      /* Don't add empty headings */
      return;
    }

    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &end,
                                             cmark_node_get_literal(child), -1,
                                             "bold", NULL);
    break;

  case CMARK_NODE_EMPH:
    child = cmark_node_first_child(node);
    if (child == NULL || cmark_node_get_type(child) != CMARK_NODE_TEXT) {
      /* Don't add empty headings */
      return;
    }

    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &end,
                                             cmark_node_get_literal(child), -1,
                                             "emph", NULL);
    break;

  case CMARK_NODE_HEADING:
    child = cmark_node_first_child(node);
    if (child == NULL || cmark_node_get_type(child) != CMARK_NODE_TEXT) {
      /* Don't add empty headings */
      g_print("Empty heading, but literal %s", cmark_node_get_literal(node));
      return;
    }

    tag = get_heading_tag(buffer, cmark_node_get_heading_level(node));
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, "\n\n", -1);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert_with_tags(buffer, &end, cmark_node_get_literal(child),
                                     -1, tag, NULL);
    break;

  case CMARK_NODE_PARAGRAPH:
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, "\n\n", -1);

    parse_document(node, buffer, ctx);
    break;

  case CMARK_NODE_LINK:
    if (g_str_has_prefix(cmark_node_get_url(node), "ref=\"") &&
        g_str_has_suffix(cmark_node_get_url(node), "\"")) {
      child = cmark_node_first_child(node);
      if (child == NULL || cmark_node_get_type(child) != CMARK_NODE_TEXT) {
        /* Don't add empty headings */
        g_print("Empty link, but literal %s", cmark_node_get_literal(node));
        return;
      }
      GtkTextChildAnchor *anchor;

      gtk_text_buffer_get_end_iter(buffer, &end);
      anchor = gtk_text_buffer_create_child_anchor(buffer, &end);
      ctx->link(cmark_node_get_literal(child), anchor, ctx->link_user_data);

    } else {
      g_message("non-internal link added: %s", cmark_node_get_url(node));
    }
    break;

  case CMARK_NODE_CODE_BLOCK:
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, "\n\n", -1);

    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &end,
                                             cmark_node_get_literal(node), -1,
                                             "code", NULL);
    break;

  default:
    g_print("Node name: %s\n", cmark_node_get_type_string(node));
  }
}

static void
parse_document(cmark_node *document,
               GtkTextBuffer *buffer,
               struct report_anchor *ctx)
{
  cmark_node *cur;

  cur = cmark_node_first_child(document);

  while (cur != NULL) {
    parse_node(cur, buffer, ctx);
    cur = cmark_node_next(cur);
  }
}

gboolean
markdown_to_text_buffer(const gchar *markdown,
                        GtkTextBuffer *buffer,
                        struct report_anchor *ctx)
{
  GError *lerr = NULL;
  gchar *md;
  cmark_parser *parser;
  cmark_mem mem = { 0 };
  cmark_node *document;

  md = preprocess_md(markdown, &lerr);
  if (md == NULL) {
    g_warning("Could not preprocess markdown: %s", lerr->message);
    return FALSE;
  }

  set_mem(&mem);

  // cmark_gfm_core_extensions_ensure_registered();

  // reflink = create_reflink_extension();
  parser = cmark_parser_new_with_mem(CMARK_OPT_DEFAULT, &mem);
  // cmark_parser_attach_syntax_extension(parser, reflink);
  cmark_parser_feed(parser, md, strlen(md));
  document = cmark_parser_finish(parser);

  gtk_text_buffer_begin_irreversible_action(buffer);
  parse_document(document, buffer, ctx);
  gtk_text_buffer_end_irreversible_action(buffer);

  cmark_parser_free(parser);

  g_free(md);

  return TRUE;
}

static cmark_node *
get_bold()
{
}

static cmark_node *
get_link(GtkTextChildAnchor *anchor)
{
  cmark_node *link;
  EditorPage *target = g_object_get_data(G_OBJECT(anchor), "target");
  gchar *file = editor_page_name_to_filename(target->heading);
  g_string_append_printf(res, "[%s]({{< ref \"%s\" >}} \"%s\")",
                         target->heading, file, target->heading);
  g_free(file);
}

scan_until_tag_starts(GtkTextIter *iter, GQueue *stack) {
    
}

gchar *
markdown_from_text_buffer(GtkTextBuffer *buffer)
{
  GString *str = g_string_new("");
  GtkTextIter iter;
  gunichar c;
  gunichar prev;
  GtkTextChildAnchor *anchor;
  cmark_node *root;
  cmark_node *cur;
  GQueue *stack;
  GtkTextTag *bold;
  GtkTextTag *emph;

  stack = g_queue_new();
  root = cmark_node_new(CMARK_NODE_DOCUMENT);

  bold = get_tag(buffer, "bold");
  emph = get_tag(buffer, "emph");

  gtk_text_buffer_get_start_iter(buffer, &iter);

  g_queue_push_tail(stack, root);

  while ((c = gtk_text_iter_get_char(&iter)) > 0) {
    if (c == 0xFFFC) {
      /** unknown char - deal with anchors */
      anchor = gtk_text_iter_get_child_anchor(&iter);
      cur = get_link(anchor);
      if (stack->tail == root) {
        g_queue_push_tail(stack, cmark_node_new(CMARK_NODE_PARAGRAPH));
      }
      cmark_node_append_child(stack->tail, cur);
    } else {
      g_string_append_unichar(str, c);
    }

    if (gtk_text_iter_starts_tag(&iter, bold)) {
      g_queue_push_tail(stack, cmark_node_new(CMARK_NODE_STRONG));
    }

    if (gtk_text_iter_ends_tag(&iter, bold)) {
      cur = g_queue_pop_tail(stack);
      cmark_node_append_child(stack->tail, cur);
    }

    if (gtk_text_iter_starts_tag(&iter, self->code) ||
        gtk_text_iter_ends_tag(&iter, self->code)) {
      if (prev != 0x0A) {
        g_string_append(res, "\n");
        g_message("Unichar: %d / %c", prev, c);
      }
      g_string_append(res, "````\n");
      code = !code;
    }
    if (gtk_text_iter_starts_tag(&iter, self->headings[0])) {
      g_string_append(res, "# ");
    }
    if (gtk_text_iter_starts_tag(&iter, self->headings[1])) {
      g_string_append(res, "## ");
    }
    if (gtk_text_iter_starts_tag(&iter, self->headings[2])) {
      g_string_append(res, "### ");
    }

    gtk_text_iter_forward_char(&iter);
    prev = c;
  }

  if (code) {
    g_string_append(res, "````\n");
  }

  if (bold) {
    g_string_append(res, "**");
  }
}