#pragma once

#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm.h>
#include <glib.h>

extern cmark_node_type CMARK_NODE_REFLINK;
cmark_syntax_extension *create_reflink_extension(void);