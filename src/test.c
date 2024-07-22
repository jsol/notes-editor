
#include <glib.h>
#include <yaml.h>

enum yaml_items {
  YAML_EVENT_NONE = 0,
  YAML_EVENT_TITLE,
  YAML_EVENT_DRAFT,
  YAML_EVENT_TAG
};

void
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

int
main(int argc, char *argv[])
{
  gchar *title;
  GPtrArray *tags;
  gchar *draft;
  gchar *input;


  g_file_get_contents("/home/jenson/git_repos/github.com/jsol/recipes-hugo/content/recipes/zucchinipaj.md", &input, NULL, NULL);

  tags = g_ptr_array_new_with_free_func(g_free);
  parse_header(input, &title, &draft, tags);

  g_print("Title: %s\n", title);
  g_print("Draft: %s\n", draft);
  g_print("Tags:\n");
  for (guint i = 0; i < tags->len; i++) {
    g_print("  -> %s\n", (gchar *) tags->pdata[i]);
  }
}