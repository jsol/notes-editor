#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>
#include <glib.h>

#include "cmark_reflink.h"

static const char *
get_type_string(cmark_syntax_extension *extension, cmark_node *node)
{
  return cmark_node_get_type(node) == CMARK_NODE_REFLINK ? "reflink" :
                                                           "<unknown>";
}

static int
can_contain(cmark_syntax_extension *extension,
            cmark_node *node,
            cmark_node_type child_type)
{
  if (node->type != CMARK_NODE_REFLINK)
    return false;

  return CMARK_NODE_TYPE_INLINE_P(child_type);
}

static void
commonmark_render(cmark_syntax_extension *extension,
                  cmark_renderer *renderer,
                  cmark_node *node,
                  cmark_event_type ev_type,
                  int options)
{
  renderer->out(renderer, node, "[", false, LITERAL);

  renderer->out(renderer, node, "]", false, LITERAL);
}

static cmark_node *
match(cmark_syntax_extension *self,
      cmark_parser *parser,
      cmark_node *parent,
      unsigned char character,
      cmark_inline_parser *inline_parser)
{
  cmark_node *res = NULL;
  int left_flanking, right_flanking, punct_before, punct_after, delims;
  char buffer[101];

  if (character != '[')
    return NULL;

  while (character != ')') {
    
    cmark_inline_parser_advance_offset(inline_parser);
    if (cmark_inline_parser_is_eof(inline_parser)) {
      break;
    }
  }

  delims = cmark_inline_parser_scan_delimiters(inline_parser,
                                               sizeof(buffer) - 1, '~',
                                               &left_flanking, &right_flanking,
                                               &punct_before, &punct_after);

  memset(buffer, '~', delims);
  buffer[delims] = 0;

  res = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
  cmark_node_set_literal(res, buffer);
  res->start_line = res->end_line = cmark_inline_parser_get_line(inline_parser);
  res->start_column = cmark_inline_parser_get_column(inline_parser) - delims;

  if ((left_flanking || right_flanking) &&
      (delims == 2 ||
       (!(parser->options & CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE) &&
        delims == 1))) {
    cmark_inline_parser_push_delimiter(inline_parser, character, left_flanking,
                                       right_flanking, res);
  }

  return res;
}

cmark_syntax_extension *
create_reflink_extension(void)
{
  cmark_syntax_extension *ext = cmark_syntax_extension_new("reflink");
  cmark_llist *special_chars = NULL;

  cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  cmark_syntax_extension_set_commonmark_render_func(ext, commonmark_render);
  CMARK_NODE_REFLINK = cmark_syntax_extension_add_node(1);

  cmark_syntax_extension_set_match_inline_func(ext, match);
  cmark_syntax_extension_set_inline_from_delim_func(ext, insert);

  cmark_mem *mem = cmark_get_default_mem_allocator();
  special_chars = cmark_llist_append(mem, special_chars, (void *) '[');
  cmark_syntax_extension_set_special_inline_chars(ext, special_chars);

  cmark_syntax_extension_set_emphasis(ext, 1);
}