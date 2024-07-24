#include "editor_page.h"
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <yaml.h>

#include "utils.h"

G_DEFINE_TYPE(EditorPage, editor_page, G_TYPE_OBJECT)

typedef enum {
  PROP_HEADING = 1,
  PROP_CONTENT,
  N_PROPERTIES
} EditorPageProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
  NULL,
};

enum editor_page_signals {
  EDITOR_PAGE_SWITCH = 0,
  EDITOR_PAGE_NEW_ANCHOR,
  EDITOR_PAGE_LAST
};

static guint editor_signals[EDITOR_PAGE_LAST] = { 0 };

typedef void (*create_cb)(gpointer, gpointer);

struct add_link_ctx {
  GtkTextMark *start_mark;
  GtkTextMark *stop_mark;
  EditorPage *page;
};

enum yaml_items {
  YAML_EVENT_NONE = 0,
  YAML_EVENT_TITLE,
  YAML_EVENT_DRAFT,
  YAML_EVENT_TAG
};

static const char *available_styles[] = {
  "< Select style >", "None",      "Bold",      "Code",
  "Heading 1",        "Heading 2", "Heading 3", NULL
};

/* Parse the YAML header in the files */
static void
parse_header(const gchar *input, gchar **title, gchar **draft, GPtrArray *tags)
{
  yaml_parser_t parser = { 0 };
  yaml_event_t event = { 0 };
  enum yaml_items current = YAML_EVENT_NONE;

  yaml_parser_initialize(&parser);
  yaml_parser_set_input_string(&parser, (const guchar *) input, strlen(input));

  while (yaml_parser_parse(&parser, &event)) {
    if (event.type == YAML_STREAM_END_EVENT) {
      yaml_event_delete(&event);
      break;
    }

    if (event.type == YAML_SCALAR_EVENT) {
      switch (current) {
      case YAML_EVENT_NONE:
        if (g_strcmp0((gchar *) event.data.scalar.value, "title") == 0) {
          current = YAML_EVENT_TITLE;
        } else if (g_strcmp0((gchar *) event.data.scalar.value, "draft") == 0) {
          current = YAML_EVENT_DRAFT;
        } else if (g_strcmp0((gchar *) event.data.scalar.value, "tags") == 0) {
          current = YAML_EVENT_TAG;
        }
        break;
      case YAML_EVENT_TITLE:
        *title = g_strdup((gchar *) event.data.scalar.value);
        current = YAML_EVENT_NONE;
        break;
      case YAML_EVENT_DRAFT:
        *draft = g_strdup((gchar *) event.data.scalar.value);
        current = YAML_EVENT_NONE;
        break;
      case YAML_EVENT_TAG:
        g_ptr_array_add(tags, g_strdup((gchar *) event.data.scalar.value));
        break;
      }
    } else if (event.type == YAML_SEQUENCE_END_EVENT) {
      current = YAML_EVENT_NONE;
    }

    yaml_event_delete(&event);
  }

  yaml_parser_delete(&parser);
}

static gboolean
validate_name(const gchar *name)
{
  const gchar *iter;

  g_assert(name);

  if (!g_str_has_prefix(name, "[[") || !g_str_has_suffix(name, "]")) {
    return FALSE;
  }

  iter = name + 2;
  while (iter[0] != ']') {
    gunichar utf_c;

    utf_c = g_utf8_get_char(iter);

    if (!g_unichar_isprint(utf_c)) {
      return FALSE;
    }

    if (g_unichar_isspace(utf_c) && iter[0] != ' ') {
      return FALSE;
    }

    iter = g_utf8_next_char(iter);
  }

  return TRUE;
}

static gboolean
validate_name_only(const gchar *name)
{
  g_assert(name);

  while (name[0] != '\0') {
    gunichar utf_c;

    utf_c = g_utf8_get_char(name);

    if (!g_unichar_isprint(utf_c)) {
      return FALSE;
    }

    if (g_unichar_isspace(utf_c) && name[0] != ' ') {
      return FALSE;
    }

    name = g_utf8_next_char(name);
  }

  return TRUE;
}

static gboolean
add_link_anchor(gpointer user_data)
{
  struct add_link_ctx *ctx = (struct add_link_ctx *) user_data;
  EditorPage *other;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  // GtkWidget *label;
  gchar *name;
  GtkTextChildAnchor *anchor;
  GtkWidget *button;

  buffer = ctx->page->content;
  /*
  start_mark = gtk_text_buffer_get_mark(buffer, "start-new-link");
  stop_mark = gtk_text_buffer_get_mark(buffer, "stop-new-link");
*/

  gtk_text_buffer_get_iter_at_mark(buffer, &start, ctx->start_mark);
  gtk_text_buffer_get_iter_at_mark(buffer, &end, ctx->stop_mark);

  name = gtk_text_iter_get_text(&start, &end);
  g_print("ADD_LINK NAME %s\n", name);

  gtk_text_iter_backward_char(&start);
  gtk_text_iter_backward_char(&start);
  gtk_text_iter_forward_char(&end);
  gtk_text_iter_forward_char(&end);
  gtk_text_buffer_delete(buffer, &start, &end);

  anchor = gtk_text_buffer_create_child_anchor(buffer, &start);

  other = g_hash_table_lookup(ctx->page->pages, name);

  if (!other) {
    other = editor_page_new(name, NULL, ctx->page->pages, ctx->page->created_cb,
                            ctx->page->user_data);
  }
  g_object_set_data(G_OBJECT(anchor), "target", other);

  g_ptr_array_add(ctx->page->anchors, g_object_ref(anchor));

  // gtk_text_view_add_child_at_anchor(textarea, widget, anchor);
  /* EMIT new anchor */
  button = editor_page_in_content_button(other);
  g_object_set_data(G_OBJECT(button), "anchor", anchor);
  g_object_set_data(G_OBJECT(button), "target", ctx->page);

  g_signal_emit(ctx->page, editor_signals[EDITOR_PAGE_NEW_ANCHOR], 0, anchor,
                button);

  g_free(name);
  g_free(ctx);

  return FALSE;
}

static void
insert_text(GtkTextBuffer *self,
            const GtkTextIter *location,
            gchar *text,
            gint len,
            gpointer user_data)
{
  static gchar last = ' ';
  EditorPage *page = EDITOR_PAGE(user_data);

  if (len != 1) {
    return;
  }

  if (text[0] == '[' && last == '[') {
    g_debug("Start link");
    last = ' ';

    gtk_text_buffer_create_mark(self, "start-link", location, TRUE);
    return;
  }

  if (text[0] == ']' && last == ']') {
    GtkTextMark *start = gtk_text_buffer_get_mark(self, "start-link");

    if (start != NULL) {
      GtkTextIter start_location;

      gtk_text_buffer_get_iter_at_mark(self, &start_location, start);
      (void) gtk_text_iter_backward_char(&start_location);
      gchar *name = gtk_text_iter_get_text(&start_location, location);

      if (validate_name(name)) {
        struct add_link_ctx *ctx = g_malloc0(sizeof(*ctx));
        GtkTextIter link_end = *location;
        (void) gtk_text_iter_backward_char(&link_end);

        gtk_text_iter_forward_char(&start_location);
        gtk_text_iter_forward_char(&start_location);
        g_print("NAME: %s\n", name);

        ctx->start_mark = gtk_text_buffer_create_mark(self, NULL,
                                                      &start_location, TRUE);
        ctx->stop_mark = gtk_text_buffer_create_mark(self, NULL, &link_end,
                                                     TRUE);
        ctx->page = page;
        g_idle_add(add_link_anchor, ctx);
      }
      g_debug("Stop link");
      g_free(name);
    }
    last = ' ';
    return;
  }
  last = text[0];
  g_print("Inserted txt: %s\n", text);
}

static GtkTextMark *
fix_last_anchor(EditorPage *page, GtkTextMark *start_mark)
{
  GtkTextChildAnchor *anchor;
  EditorPage *other;
  GtkTextBuffer *buffer;
  GtkTextIter start;
  GtkTextIter match_begin_start;
  GtkTextIter match_begin_end;
  GtkTextIter match_stop_start;
  GtkTextIter match_stop_end;
  gchar *name;
  GtkTextMark *return_mark = NULL;
  GtkWidget *button;

  buffer = page->content;

  if (start_mark != NULL) {
    gtk_text_buffer_get_iter_at_mark(buffer, &start, start_mark);
  } else {
    gtk_text_buffer_get_end_iter(buffer, &start);
  }

  if (!gtk_text_iter_backward_search(&start, "]]", GTK_TEXT_SEARCH_TEXT_ONLY,
                                     &match_stop_start, &match_stop_end, NULL)) {
    g_print("DID NOT FIND ]]\n");
    return NULL;
  }

  if (!gtk_text_iter_backward_search(&match_stop_start, "[[",
                                     GTK_TEXT_SEARCH_TEXT_ONLY,
                                     &match_begin_start, &match_begin_end,
                                     NULL)) {
    g_print("DID NOT FIND [[\n");
    return NULL;
  }

  gtk_text_iter_backward_char(&match_begin_start);
  return_mark = gtk_text_buffer_create_mark(buffer, NULL, &match_begin_start,
                                            TRUE);
  gtk_text_iter_forward_char(&match_begin_start);

  name = gtk_text_iter_get_text(&match_begin_end, &match_stop_start);

  g_print("FOUND NAME: %s\n", name);
  if (!validate_name_only(name)) {
    return_mark = gtk_text_buffer_create_mark(buffer, NULL, &match_begin_start,
                                              TRUE);
    goto out;
  }

  return_mark = gtk_text_buffer_create_mark(buffer, NULL, &match_begin_start,
                                            TRUE);

  /* Invalidates all the iterators above */
  gtk_text_buffer_delete(buffer, &match_begin_start, &match_stop_end);

  gtk_text_buffer_get_iter_at_mark(buffer, &start, return_mark);
  anchor = gtk_text_buffer_create_child_anchor(buffer, &start);

  other = g_hash_table_lookup(page->pages, name);

  if (!other) {
    other = editor_page_new(name, NULL, page->pages, page->created_cb,
                            page->user_data);
  }

  g_object_set_data(G_OBJECT(anchor), "target", other);

  g_ptr_array_add(page->anchors, g_object_ref(anchor));

  // gtk_text_view_add_child_at_anchor(textarea, widget, anchor);
  /* EMIT new anchor */
  button = editor_page_in_content_button(other);
  g_object_set_data(G_OBJECT(button), "anchor", anchor);
  g_object_set_data(G_OBJECT(button), "target", page);

  g_signal_emit(page, editor_signals[EDITOR_PAGE_NEW_ANCHOR], 0, anchor, button);
  g_print("Returning a mark\n");
out:
  g_free(name);
  return return_mark;
}

static void
fix_anchors(EditorPage *page)
{
  GtkTextIter start_bound = { 0 };
  GtkTextIter start_res = { 0 };
  GtkTextIter stop_res = { 0 };
  GtkTextIter start_name = { 0 };
  GtkTextIter stop_name = { 0 };
  const gchar *name;

  GtkTextMark *return_mark = NULL;
  GtkTextChildAnchor *anchor;
  EditorPage *other;

  gtk_text_buffer_get_start_iter(page->content, &start_bound);

  while (utils_match_pattern("[?]({{< ref \"?\" >}} \"?\")", page->content,
                             &start_bound, NULL, &start_res, &stop_res)) {
    utils_match_pattern("[?]", page->content, &start_res, &stop_res,
                        &start_name, &stop_name);

    gtk_text_iter_forward_char(&start_name);
    gtk_text_iter_backward_char(&stop_name);
    name = gtk_text_iter_get_text(&start_name, &stop_name);

    /* place mark, modify buffer, continue search from mark */
    return_mark = gtk_text_buffer_create_mark(page->content, NULL, &stop_res,
                                              TRUE);

    /* Invalidates all the iterators above */
    gtk_text_buffer_delete(page->content, &start_res, &stop_res);

    gtk_text_buffer_get_iter_at_mark(page->content, &start_bound, return_mark);
    anchor = gtk_text_buffer_create_child_anchor(page->content, &start_bound);

    other = g_hash_table_lookup(page->pages, name);

    if (!other) {
      other = editor_page_new(name, NULL, page->pages, page->created_cb,
                              page->user_data);
    }

    g_object_set_data(G_OBJECT(anchor), "target", other);

    g_ptr_array_add(page->anchors, g_object_ref(anchor));

    // gtk_text_view_add_child_at_anchor(textarea, widget, anchor);
    /* EMIT new anchor */
    GtkWidget *button;
    button = editor_page_in_content_button(other);
    g_object_set_data(G_OBJECT(button), "anchor", anchor);
    g_object_set_data(G_OBJECT(button), "target", page);

    g_print("Emitting signal\n");
    g_signal_emit(page, editor_signals[EDITOR_PAGE_NEW_ANCHOR], 0, anchor,
                  button);
  }

  if (1) {
    /* old solution */
    GtkTextMark *mark = NULL;

    mark = fix_last_anchor(page, mark);
    while (mark != NULL) {
      mark = fix_last_anchor(page, mark);
    }
  }
}

static void
fix_tags(EditorPage *page)
{
  utils_fix_specific_tag(page->content, "code", PATTERN_CODE, TRIM_PATTERN_CODE);
  utils_fix_specific_tag(page->content, "h3", PATTERN_H1, TRIM_PATTERN_H1);
  utils_fix_specific_tag(page->content, "h2", PATTERN_H2, TRIM_PATTERN_H2);
  utils_fix_specific_tag(page->content, "h1", PATTERN_H3, TRIM_PATTERN_H3);
  utils_fix_specific_tag(page->content, "bold", PATTERN_BOLD, TRIM_PATTERN_BOLD);

  /* Old:
  GtkTextIter start;
  GtkTextIter match_start;
  GtkTextIter match_end;
  GtkTextIter bold_end_iter;
  GtkTextMark *mark = NULL;
  GtkTextMark *bold_end = NULL;
  gboolean bold;

  gtk_text_buffer_get_end_iter(page->content, &start);

  while (true) {
    if (!gtk_text_iter_backward_search(&start, "**",
  GTK_TEXT_SEARCH_TEXT_ONLY, &match_start, &match_end, NULL)) { return;
    }

    mark = gtk_text_buffer_create_mark(page->content, NULL, &match_start,
  TRUE);

    if (!bold) {
      bold_end = gtk_text_buffer_create_mark(page->content, NULL,
  &match_start, TRUE); bold = TRUE; } else {
      gtk_text_buffer_get_iter_at_mark(page->content, &bold_end_iter,
  bold_end); gtk_text_buffer_apply_tag_by_name(page->content, "bold",
  &match_end, &bold_end_iter); bold = FALSE;
    }

    gtk_text_buffer_delete(page->content, &match_start, &match_end);

    gtk_text_buffer_get_iter_at_mark(page->content, &start, mark);
    */
}

static void
foreach_button_name(gpointer data, gpointer user_data)
{
  GtkButton *button = GTK_BUTTON(data);
  GtkLabel *label;
  gchar *name = (gchar *) user_data;

  label = GTK_LABEL(gtk_button_get_child(button));
  gtk_label_set_text(label, name);
}

static void
update_name(EditorPage *self, const gchar *old_name)
{
  if (!self->buttons) {
    return;
  }

  g_ptr_array_foreach(self->buttons, foreach_button_name, self->heading);

  if (self->pages != NULL) {
    g_hash_table_insert(self->pages, g_strdup(self->heading), self);
    g_hash_table_remove(self->pages, old_name);
  }
}

static void
editor_page_finalize(GObject *obj)
{
  EditorPage *self = EDITOR_PAGE(obj);

  g_assert(self);

  /*free stuff */

  g_free(self->heading);

  g_clear_object(&self->content);

  /* Always chain up to the parent finalize function to complete object
   * destruction. */
  G_OBJECT_CLASS(editor_page_parent_class)->finalize(obj);
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  EditorPage *self = EDITOR_PAGE(object);

  switch ((EditorPageProperty) property_id) {
  case PROP_HEADING:
    g_value_set_string(value, self->heading);
    break;

  case PROP_CONTENT:
    g_value_set_object(value, self->content);
    break;

  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void
set_property(GObject *object,
             guint property_id,
             const GValue *value,
             GParamSpec *pspec)
{
  EditorPage *self = EDITOR_PAGE(object);

  g_print("Kalling set property\n");

  switch ((EditorPageProperty) property_id) {
  case PROP_HEADING:
    gchar *old_name = self->heading;

    self->heading = g_value_dup_string(value);

    update_name(self, old_name);
    g_free(old_name);
    break;
  case PROP_CONTENT:
    g_clear_object(&self->content);
    self->content = g_value_get_object(value);
    break;

  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }

  g_print("After set property\n");
}

static void
editor_page_class_init(EditorPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = editor_page_finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

  g_print("Klass init\n");

  obj_properties[PROP_HEADING] = g_param_spec_string("heading", "Heading",
                                                     "Placeholder "
                                                     "description.",
                                                     NULL, G_PARAM_READWRITE);

  obj_properties[PROP_CONTENT] = g_param_spec_object("content", "Content",
                                                     "Placeholder "
                                                     "description.",
                                                     GTK_TYPE_TEXT_BUFFER, /* default
                                                                            */
                                                     G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);

  editor_signals[EDITOR_PAGE_SWITCH] = g_signal_newv("switch-page",
                                                     G_TYPE_FROM_CLASS(klass),
                                                     G_SIGNAL_RUN_LAST |
                                                       G_SIGNAL_NO_RECURSE |
                                                       G_SIGNAL_NO_HOOKS,
                                                     NULL, NULL, NULL, NULL,
                                                     G_TYPE_NONE, 0, NULL);

  GType params[] = { G_TYPE_OBJECT, G_TYPE_OBJECT };
  editor_signals[EDITOR_PAGE_NEW_ANCHOR] = g_signal_newv("new-anchor",
                                                         G_TYPE_FROM_CLASS(klass),
                                                         G_SIGNAL_RUN_LAST |
                                                           G_SIGNAL_NO_RECURSE |
                                                           G_SIGNAL_NO_HOOKS,
                                                         NULL, NULL, NULL, NULL,
                                                         G_TYPE_NONE, 2, params);
}

static void
editor_page_init(EditorPage *self)
{
  /* initialize all public and private members to reasonable default values.
   * They are all automatically initialized to 0 to begin with. */

  if (self->heading == NULL) {
    g_print("Before the args\n");
  } else {
    g_print("After the args\n");
  }
  /* Not sharing tags (for now at least) */
  self->content = gtk_text_buffer_new(NULL);
  self->anchors = g_ptr_array_new();
  self->buttons = g_ptr_array_new();
  self->color.red = .7;
  self->color.green = .7;
  self->color.blue = 1.0;
  self->color.alpha = 1.0;
  self->draft = g_strdup("true");

  self->bold = gtk_text_buffer_create_tag(self->content, "bold", "weight", 800,
                                          NULL);

  self->code = gtk_text_buffer_create_tag(self->content, "code", "family",
                                          "Monospace", NULL);
  self->headings[0] = gtk_text_buffer_create_tag(self->content, "h1", "weight",
                                                 800, "size-points", 32.0, NULL);
  self->headings[1] = gtk_text_buffer_create_tag(self->content, "h2", "weight",
                                                 800, "size-points", 24.0, NULL);
  self->headings[2] = gtk_text_buffer_create_tag(self->content, "h3", "weight",
                                                 800, "size-points", 18.0, NULL);

  g_print("After init\n");
}

static void
change_page(G_GNUC_UNUSED GObject *button, EditorPage *self)
{
  g_print("Emit change page\n");
  g_signal_emit(self, editor_signals[EDITOR_PAGE_SWITCH], 0, self);
  // EMIT change page
}

EditorPage *
editor_page_new(const gchar *heading,
                GPtrArray *tags,
                GHashTable *pages,
                GCallback created_cb,
                gpointer user_data)
{
  static guint css_num = 0;
  EditorPage *self;

  g_assert(pages);

  if (tags == NULL) {
    tags = g_ptr_array_new_with_free_func(g_free);
  }

  if (tags->len == 0) {
    g_ptr_array_add(tags, g_strdup("Not tagged"));
  }

  g_message("Creating GObject");
  self = g_object_new(EDITOR_TYPE_PAGE, "heading", heading, NULL);
  g_message("After creating GObject");

  css_num++;
  self->css_name = g_strdup_printf("page%u", css_num);
  self->tags = tags;
  self->pages = pages;
  g_message("Call insert");
  g_hash_table_insert(pages, g_strdup(heading), self);
  g_message("after insert");

  g_signal_connect(self->content, "insert-text", G_CALLBACK(insert_text), self);

  self->created_cb = created_cb;
  self->user_data = user_data;
  ((create_cb) *self->created_cb)(self, self->user_data);

  g_message("New page done");

  return self;
}

GtkWidget *
editor_page_in_content_button(EditorPage *self)
{
  GtkWidget *button;

  button = gtk_button_new_with_label(self->heading);
  gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
  g_signal_connect(button, "clicked", G_CALLBACK(change_page), self);
  gtk_widget_add_css_class(button, "in-text-button");
  gtk_widget_add_css_class(button, self->css_name);

  g_ptr_array_add(self->buttons, g_object_ref(button));
  return button;
}

GtkWidget *
editor_page_in_list_button(EditorPage *self)
{
  GtkWidget *button;
  GtkWidget *label;

  button = gtk_button_new_with_label(self->heading);
  label = gtk_button_get_child(GTK_BUTTON(button));

  gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);

  gtk_label_set_xalign(GTK_LABEL(label), 0.0);

  g_signal_connect(button, "clicked", G_CALLBACK(change_page), self);
  gtk_widget_add_css_class(button, "in-list-button");
  gtk_widget_add_css_class(button, self->css_name);

  g_ptr_array_add(self->buttons, g_object_ref(button));
  return button;
}

void
editor_page_update_style(EditorPage *self, enum style style_id)
{
  const gchar *name = NULL;
  GtkTextIter start;
  GtkTextIter end;

  if (!gtk_text_buffer_get_selection_bounds(self->content, &start, &end)) {
    return;
  }

  if (style_id == STYLE_H1 || style_id == STYLE_H2 || style_id == STYLE_H3) {
    gtk_text_iter_forward_line(&end);
    gtk_text_iter_backward_char(&end);

    gtk_text_iter_backward_line(&start);
    gtk_text_iter_forward_char(&start);
  }

  if (style_id != STYLE_NO_CHOICE) {
    gtk_text_buffer_remove_all_tags(self->content, &start, &end);
  }

  switch (style_id) {
  case STYLE_NO_CHOICE:
  case STYLE_NONE:
    return;

  case STYLE_BOLD:
    name = "bold";
    break;
  case STYLE_CODE:
    name = "code";
    break;
  case STYLE_H1:
    name = "h1";
  case STYLE_H2:
    name = "h2";
    break;
  case STYLE_H3:
    name = "h3";
    break;
  default:
    return;
  }

  gtk_text_buffer_apply_tag_by_name(self->content, name, &start, &end);
}

GString *
editor_page_to_md(EditorPage *self)
{
  GString *res = g_string_new("");
  GtkTextIter iter;
  gunichar c;
  GtkTextChildAnchor *anchor;

  /* write header */
  g_string_append_printf(res, "---\ntitle: \"%s\"\ndraft: %s\ntags:\n",
                         self->heading, self->draft);

  for (guint i = 0; i < self->tags->len; i++) {
    g_string_append_printf(res, "  - %s\n", (gchar *) self->tags->pdata[i]);
  }
  g_string_append(res, "---\n\n");

  /* translate styled doc to md format
   header -> #[#[#]] Text
   anchor --> [Title]({{< ref "filename.md" >}} "Title")
   code -> \n````\ntext \n````\n
   bold -> **text**
  */
  gtk_text_buffer_get_start_iter(self->content, &iter);

  while ((c = gtk_text_iter_get_char(&iter)) > 0) {
    if (gtk_text_iter_starts_tag(&iter, self->bold) ||
        gtk_text_iter_ends_tag(&iter, self->bold)) {
      g_string_append(res, "**");
    }
    if (gtk_text_iter_starts_tag(&iter, self->code) ||
        gtk_text_iter_ends_tag(&iter, self->code)) {
      g_string_append(res, "\n````\n");
    }
    if (gtk_text_iter_starts_tag(&iter, self->headings[0])) {
      g_string_append(res, "\n# ");
    }
    if (gtk_text_iter_starts_tag(&iter, self->headings[1])) {
      g_string_append(res, "\n## ");
    }
    if (gtk_text_iter_starts_tag(&iter, self->headings[2])) {
      g_string_append(res, "\n### ");
    }

    if (c == 0xFFFC) {
      /** unknown char - deal with anchors */
      anchor = gtk_text_iter_get_child_anchor(&iter);
      if (anchor != NULL) {
        EditorPage *target = g_object_get_data(G_OBJECT(anchor), "target");
        gchar *file = editor_page_name_to_filename(target->heading);
        g_string_append_printf(res, "[%s]({{< ref \"%s\" >}} \"%s\")",
                               target->heading, file, target->heading);
        g_free(file);
      }
    } else {
      g_string_append_unichar(res, c);
    }

    gtk_text_iter_forward_char(&iter);
  }

  return res;
}

EditorPage *
editor_page_load(GHashTable *pages,
                 gchar *filename,
                 GCallback created_cb,
                 gpointer user_data)
{
  GError *lerr = NULL;
  gchar *content = NULL;
  gsize size;
  gchar *name;
  EditorPage *page;
  gchar *text;
  gchar *draft;
  GPtrArray *tags;

  g_assert(pages);

  if (!g_file_get_contents(filename, &content, &size, &lerr)) {
    g_warning("Could not open file: %s", lerr->message);
    g_clear_error(&lerr);
    return NULL;
  }

  if (!g_str_has_prefix(content, "---")) {
    return NULL;
  }

  tags = g_ptr_array_new_with_free_func(g_free);

  parse_header(content, &name, &draft, tags);

  g_print("Name: %s\n", name);

  page = g_hash_table_lookup(pages, name);

  if (page == NULL) {
    page = editor_page_new(name, tags, pages, created_cb, user_data);
  }

  g_free(page->draft);
  page->draft = draft;

  text = g_strstr_len(content + 4, -1, "---");
  text += 3;
  g_strstrip(text);

  gtk_text_buffer_set_text(page->content, text, -1);

  g_free(content);

  return page;
}

gchar *
editor_page_name_to_filename(const gchar *name)
{
  GString *filename;
  gchar *ascii;
  g_return_val_if_fail(name != NULL, g_strdup("invalid_name.md"));

  ascii = g_str_to_ascii(name, "C");
  filename = g_string_new(ascii);
  g_free(ascii);

  g_string_ascii_down(filename);
  g_string_replace(filename, " ", "_", -1);
  g_string_append(filename, ".md");

  return g_string_free(filename, FALSE);
}

void
editor_page_fix_content(EditorPage *page)
{
  g_print("Fixing anchors \n");
  fix_anchors(page);

  fix_tags(page);

  g_print("Free content\n");
}

const gchar *const *
editor_page_get_styles(void)
{
  return available_styles;
}