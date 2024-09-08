#include "editor_page.h"
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <pango/pango.h> /* style */
#include <yaml.h>

#include "markdown.h"
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
add_link_anchor(gpointer user_data)
{
  struct add_link_ctx *ctx = (struct add_link_ctx *) user_data;
  EditorPage *other = NULL;
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

  gtk_text_iter_backward_char(&start);
  gtk_text_iter_backward_char(&start);
  gtk_text_iter_forward_char(&end);
  gtk_text_iter_forward_char(&end);
  gtk_text_buffer_delete(buffer, &start, &end);

  anchor = gtk_text_buffer_create_child_anchor(buffer, &start);

  if (ctx->page->fetch_page != NULL) {
    other = ctx->page->fetch_page(name, ctx->page->fetch_page_user_data);
  }

  if (!other) {
    other = editor_page_new(name, NULL, ctx->page->fetch_page,
                            ctx->page->fetch_page_user_data,
                            ctx->page->created_cb, ctx->page->user_data);
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
}

static void
editor_page_class_init(EditorPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = editor_page_finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

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

  self->bold = gtk_text_buffer_create_tag(self->content, "emph", "style",
                                          PANGO_STYLE_ITALIC, NULL);

  self->code = gtk_text_buffer_create_tag(self->content, "code", "family",
                                          "Monospace", NULL);
  self->headings[0] = gtk_text_buffer_create_tag(self->content, "h1", "weight",
                                                 800, "size-points", 20.0, NULL);
  self->headings[1] = gtk_text_buffer_create_tag(self->content, "h2", "weight",
                                                 800, "size-points", 16.0, NULL);
  self->headings[2] = gtk_text_buffer_create_tag(self->content, "h3", "weight",
                                                 800, "size-points", 12.0, NULL);
}

static void
change_page(G_GNUC_UNUSED GObject *button, EditorPage *self)
{
  g_signal_emit(self, editor_signals[EDITOR_PAGE_SWITCH], 0, self);
  // EMIT change page
}

EditorPage *
editor_page_new(const gchar *heading,
                GPtrArray *tags,
                fetch_page_fn fetch_page,
                gpointer fetch_page_user_data,
                GCallback created_cb,
                gpointer user_data)
{
  static guint css_num = 0;
  EditorPage *self;

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
  self->fetch_page = fetch_page;
  self->fetch_page_user_data = fetch_page_user_data;

  g_signal_connect(self->content, "insert-text", G_CALLBACK(insert_text), self);

  self->created_cb = created_cb;
  self->user_data = user_data;
  if (self->created_cb != NULL) {
    ((create_cb) *self->created_cb)(self, self->user_data);
  }

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
  gunichar prev;
  GtkTextChildAnchor *anchor;
  gboolean code = FALSE;
  gboolean bold = FALSE;

  /* write header */
  g_string_append_printf(res, "---\ntitle: \"%s\"\ndraft: %s\ntags:\n",
                         self->heading, self->draft);

  for (guint i = 0; i < self->tags->len; i++) {
    g_string_append_printf(res, "  - %s\n", (gchar *) self->tags->pdata[i]);
  }
  g_string_append(res, "---\n");

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
      bold = !bold;
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
    prev = c;
  }

  if (code) {
    g_string_append(res, "````\n");
  }

  if (bold) {
    g_string_append(res, "**");
  }

  return res;
}

EditorPage *
editor_page_load(gchar *content,
                 fetch_page_fn fetch_page,
                 gpointer fetch_page_user_data,
                 GCallback created_cb,
                 gpointer user_data)
{
  gchar *name;
  EditorPage *page = NULL;
  gchar *text;
  gchar *draft = NULL;
  GPtrArray *tags;

  if (!g_str_has_prefix(content, "---")) {
    return NULL;
  }

  tags = g_ptr_array_new_with_free_func(g_free);

  parse_header(content, &name, &draft, tags);

  if (fetch_page != NULL) {
    page = fetch_page(name, fetch_page_user_data);
  }

  if (page == NULL) {
    page = editor_page_new(name, tags, fetch_page, fetch_page_user_data,
                           created_cb, user_data);
  }

  if (draft != NULL) {
    g_free(page->draft);
    page->draft = draft;
  }

  text = g_strstr_len(content + 4, -1, "---");

  GString *c = g_string_new(text + 3);

  g_strstrip(c->str);
  c->len = strlen(c->str);

  g_string_append(c, "\n");
  page->waiting = c->str;
  g_print("Setting waiting to %s", page->waiting);

  g_string_free(c, FALSE);

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
new_link_anchor_cb(const gchar *heading,
                   GtkTextChildAnchor *anchor,
                   gpointer user_data)
{
  EditorPage *self = EDITOR_PAGE(user_data);
  EditorPage *other = NULL;
  GtkWidget *button;

  g_assert(heading);
  g_assert(anchor);

  if (self->fetch_page != NULL) {
    other = self->fetch_page(heading, self->fetch_page_user_data);
  }

  if (!other) {
    other = editor_page_new("New page", NULL, self->fetch_page,
                            self->fetch_page_user_data, self->created_cb,
                            self->user_data);
  }

  g_object_set_data(G_OBJECT(anchor), "target", other);

  g_ptr_array_add(self->anchors, g_object_ref(anchor));

  // gtk_text_view_add_child_at_anchor(textarea, widget, anchor);
  /* EMIT new anchor */
  button = editor_page_in_content_button(other);
  g_object_set_data(G_OBJECT(button), "anchor", anchor);
  g_object_set_data(G_OBJECT(button), "target", self);

  g_signal_emit(self, editor_signals[EDITOR_PAGE_NEW_ANCHOR], 0, anchor, button);
}

void
editor_page_fix_content(EditorPage *page)
{
  struct report_anchor ctx = { 0 };

  if (page->waiting == NULL) {
    g_print("No content waiting for %s", page->heading);
    return;
  }

  ctx.link = new_link_anchor_cb;
  ctx.link_user_data = page;
  markdown_to_text_buffer(page->waiting, page->content, &ctx);
}

const gchar *const *
editor_page_get_styles(void)
{
  return available_styles;
}

void
editor_page_add_anchor(EditorPage *self, EditorPage *other)
{
  GtkWidget *button;
  GtkTextChildAnchor *anchor;
  GtkTextIter iter;
  GtkTextMark *insert;

  g_return_if_fail(self != NULL);

  insert = gtk_text_buffer_get_insert(self->content);
  gtk_text_buffer_get_iter_at_mark(self->content, &iter, insert);
  anchor = gtk_text_buffer_create_child_anchor(self->content, &iter);

  if (!other) {
    other = editor_page_new("New page", NULL, self->fetch_page,
                            self->fetch_page_user_data, self->created_cb,
                            self->user_data);
  }

  g_object_set_data(G_OBJECT(anchor), "target", other);

  g_ptr_array_add(self->anchors, g_object_ref(anchor));

  // gtk_text_view_add_child_at_anchor(textarea, widget, anchor);
  /* EMIT new anchor */
  button = editor_page_in_content_button(other);
  g_object_set_data(G_OBJECT(button), "anchor", anchor);
  g_object_set_data(G_OBJECT(button), "target", self);

  g_signal_emit(self, editor_signals[EDITOR_PAGE_NEW_ANCHOR], 0, anchor, button);
}