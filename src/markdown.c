#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "markdown.h"

struct nodes;

typedef cmark_node *(*create_node)(GtkTextIter *iter,
                                   cmark_node *parent,
                                   GHashTable *links,
                                   struct nodes *info,
                                   guint index);

struct nodes {
  cmark_node_type type;
  create_node create_func;
  GtkTextTag *tag;
  gpointer user_data;
};

/* This must match the ENUMS */
static const gchar *tag_names[] = { "bold", "emph", "code", "h1", "h2", "h3" };

#define NODE_COUNT 4

static void parse_document(cmark_node *document,
                           GtkTextBuffer *buffer,
                           struct report_anchor *ctx);

static void iterate_buffer(GtkTextIter *iter,
                           struct nodes *info,
                           cmark_node *cur,
                           GHashTable *links,
                           GtkTextTag *end,
                           gboolean check_first_tag);

static void iterate_text_buffer(GtkTextIter *iter,
                                cmark_node *node,
                                GtkTextTag *end);

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

static cmark_node *
create_text_node(GtkTextIter *iter,
                 cmark_node *parent,
                 GHashTable *links,
                 struct nodes *info,
                 guint index)
{
  cmark_node *node = cmark_node_new(info[index].type);
  g_message("Iterating node %u for sub nodes", index);
  iterate_buffer(iter, info, node, links, info[index].tag, FALSE);

  return node;
}

static cmark_node *
create_literal_node(GtkTextIter *iter,
                    cmark_node *parent,
                    GHashTable *links,
                    struct nodes *info,
                    guint index)
{
  cmark_node *node = cmark_node_new(info[index].type);
  g_message("Iterating node %u for sub nodes", index);
  iterate_text_buffer(iter, node, info[index].tag);

  return node;
}

static cmark_node *
create_code_node(GtkTextIter *iter,
                 cmark_node *parent,
                 GHashTable *links,
                 struct nodes *info,
                 guint index)
{
  cmark_node *node;

  node = create_literal_node(iter, parent, links, info, index);

  cmark_node_set_fenced(node, 1, 3, 0, '`');
  cmark_node_set_fence_info(node, " "); /* Store language type? */

  return node;
}

static cmark_node *
create_heading_node(GtkTextIter *iter,
                    cmark_node *parent,
                    GHashTable *links,
                    struct nodes *info,
                    guint index)
{
  cmark_node *node;

  node = create_text_node(iter, parent, links, info, index);

  cmark_node_set_heading_level(node, GPOINTER_TO_INT(info[index].user_data));

  return node;
}

static struct nodes *
get_nodes(GtkTextBuffer *buffer)
{
  struct nodes *n = g_new0(struct nodes, NODE_COUNT);

  n[0].tag = get_heading_tag(buffer, 1);
  n[0].type = CMARK_NODE_HEADING;
  n[0].user_data = GINT_TO_POINTER(1);
  n[0].create_func = create_heading_node;

  n[1].tag = get_heading_tag(buffer, 2);
  n[1].type = CMARK_NODE_HEADING;
  n[1].user_data = GINT_TO_POINTER(2);
  n[1].create_func = create_heading_node;

  n[2].tag = get_heading_tag(buffer, 3);
  n[2].type = CMARK_NODE_HEADING;
  n[2].user_data = GINT_TO_POINTER(3);
  n[2].create_func = create_heading_node;

  n[3].tag = get_tag(buffer, "code");
  n[3].type = CMARK_NODE_CODE_BLOCK;
  n[3].user_data = NULL;
  n[3].create_func = create_code_node;

  return n;
}

static void
parse_node(cmark_node *node, GtkTextBuffer *buffer, struct report_anchor *ctx)
{
  GtkTextIter start;
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
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    if (!gtk_text_iter_equal(&start, &end)) {
      gtk_text_buffer_insert(buffer, &end, "\n\n", -1);
    }
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert_with_tags(buffer, &end, cmark_node_get_literal(child),
                                     -1, tag, NULL);
    break;

  case CMARK_NODE_PARAGRAPH:
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    if (!gtk_text_iter_equal(&start, &end)) {
      gtk_text_buffer_insert(buffer, &end, "\n\n", -1);
    }

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

static cmark_node *
get_link(GtkTextChildAnchor *anchor, GHashTable *links)
{
  cmark_node *link;
  cmark_node *text;
  const gchar *heading;
  const gchar *uuid;

  heading = g_object_get_data(G_OBJECT(anchor), "target-heading");
  uuid = g_object_get_data(G_OBJECT(anchor), "target-uuid");

  if (heading == NULL || uuid == NULL) {
    g_warning("No info for link");
    return NULL;
  }

  link = cmark_node_new(CMARK_NODE_LINK);
  text = cmark_node_new(CMARK_NODE_TEXT);

  cmark_node_set_literal(text, heading);
  cmark_node_append_child(link, text);
  cmark_node_set_url(link, uuid);

  g_hash_table_insert(links, g_strdup(uuid), g_strdup(heading));

  return link;
}

static void
add_string_to_node_and_reset(cmark_node *cur, GString *text)
{
  cmark_node *node;
  cmark_node *paragraph;

  if (text->len == 0) {
    return;
  }

  node = cmark_node_new(CMARK_NODE_TEXT);

  if (cmark_node_get_type(cur) == CMARK_NODE_DOCUMENT) {
    paragraph = cmark_node_new(CMARK_NODE_PARAGRAPH);
    cmark_node_append_child(paragraph, node);
    cmark_node_append_child(cur, paragraph);
  } else {
    cmark_node_append_child(cur, node);
  }

  cmark_node_set_literal(node, text->str);

  g_string_set_size(text, 0);
}

static void
iterate_text_buffer(GtkTextIter *iter, cmark_node *node, GtkTextTag *end)
{
  gunichar c;
  GString *text;
  gboolean first = TRUE;

  g_assert(iter);
  g_assert(node);
  g_assert(end);

  text = g_string_new("");

  while ((c = gtk_text_iter_get_char(iter)) > 0) {
    if (!first && gtk_text_iter_starts_tag(iter, NULL)) {
      g_warning("Unexpected tag found");
    }

    if (c == 0xFFFC) {
      g_warning("Unexpected anchor found");
      continue;
    }
    g_string_append_unichar(text, c);

    if (gtk_text_iter_ends_tag(iter, end)) {
      break;
    }

    first = FALSE;
    gtk_text_iter_forward_char(iter);
  }

  cmark_node_set_literal(node, text->str);
  (void) g_string_free(text, TRUE);
}

static void
iterate_buffer(GtkTextIter *iter,
               struct nodes *info,
               cmark_node *cur,
               GHashTable *links,
               GtkTextTag *end,
               gboolean check_first_tag)
{
  gunichar c;
  cmark_node *node;
  GString *text;
  gboolean last_n = FALSE;
  GtkTextChildAnchor *anchor;

  text = g_string_new("");

  g_message("#### Iterating buffer #######");

  while ((c = gtk_text_iter_get_char(iter)) > 0) {
    if (check_first_tag) {
      for (guint i = 0; i < NODE_COUNT; i++) {
        if (gtk_text_iter_starts_tag(iter, info[i].tag)) {
          g_message("#### STARTING TAG #######");
          add_string_to_node_and_reset(cur, text);

          node = info[i].create_func(iter, cur, links, info, i);
          cmark_node_append_child(cur, node);

          goto loop;
        }
      }
    }

    if (c == 0xFFFC) {
      /** unknown char - deal with anchors */

      add_string_to_node_and_reset(cur, text);

      anchor = gtk_text_iter_get_child_anchor(iter);
      node = get_link(anchor, links);
      cmark_node_append_child(cur, node);
    } else if (c == '\n') {
      if (last_n) {
        add_string_to_node_and_reset(cur, text);
      }
      last_n = TRUE;
    } else {
      last_n = FALSE;
      g_message("Character: %c", c);
      g_string_append_unichar(text, c);
    }

    if (end != NULL && gtk_text_iter_ends_tag(iter, end)) {
      g_message("#### ENDING TAG #######");
      add_string_to_node_and_reset(cur, text);
      gtk_text_iter_forward_char(iter);
      break;
    }

  loop:
    check_first_tag = TRUE;
    gtk_text_iter_forward_char(iter);
  }

  g_message("#### End iterate buffer (%s)", text->str);
  add_string_to_node_and_reset(cur, text);
  (void) g_string_free(text, TRUE);
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

gchar *
markdown_from_text_buffer(GtkTextBuffer *buffer)
{
  gchar *res;
  GtkTextIter iter;
  GHashTable *links;
  cmark_node *root;
  cmark_mem mem = { 0 };

  links = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  root = cmark_node_new(CMARK_NODE_DOCUMENT);

  gtk_text_buffer_get_start_iter(buffer, &iter);

  iterate_buffer(&iter, get_nodes(buffer), root, links, NULL, TRUE);

  set_mem(&mem);
  res = cmark_render_commonmark_with_mem(root, CMARK_OPT_UNSAFE, 80, &mem);

  /* TODO: Do post process */

  g_hash_table_unref(links);
  return res;
}

void
markdown_setup_tags(GtkTextBuffer *buffer, style_tag cb)
{
  GtkTextTag *tag;

  g_return_if_fail(buffer != NULL);

  for (guint i = 0; i < MARKDOWN_TAG_LAST; i++) {
    tag = gtk_text_buffer_create_tag(buffer, tag_names[i], NULL);
    if (cb != NULL) {
      cb((enum markdown_tag_types) i, tag);
    }
  }
}