#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>


G_BEGIN_DECLS
#define PATTERN_H1        "\n# ?\n"
#define PATTERN_H2        "\n## ?\n"
#define PATTERN_H3        "\n### ?\n"
#define PATTERN_CODE      "\n````\n?````\n"
#define PATTERN_BOLD      "**?**"
#define TRIM_PATTERN_H1   "xx?s"
#define TRIM_PATTERN_H2   "xxx?s"
#define TRIM_PATTERN_H3   "xxxx?s"
#define TRIM_PATTERN_CODE "xxxxx?xxxxx"
#define TRIM_PATTERN_BOLD "xx?xx"


enum style {
  STYLE_NO_CHOICE = 0,
  STYLE_NONE,
  STYLE_BOLD,
  STYLE_CODE,
  STYLE_H1,
  STYLE_H2,
  STYLE_H3
};

#define EDITOR_TYPE_PAGE editor_page_get_type()
G_DECLARE_FINAL_TYPE(EditorPage, editor_page, EDITOR, PAGE, GObject)

typedef EditorPage *(*fetch_page_fn)(const gchar *heading, gpointer user_data);

/** Public variables. Move to .c file to make private */
struct _EditorPage {
  GObject parent;

  gchar *heading;
  GtkTextBuffer *content;
  GPtrArray *anchors;
  GPtrArray *buttons;
  GPtrArray *tags;
  gchar *draft;
  gchar *waiting;

  gchar *css_name;
  GdkRGBA color;

  GCallback created_cb;
  gpointer user_data;

  fetch_page_fn fetch_page;
  gpointer fetch_page_user_data;

  GtkTextTag *bold;
  GtkTextTag *emph;
  GtkTextTag *code;
  GtkTextTag *headings[3];
};

/*
 * Type declaration.
 */

/*
 * Method definitions.
 */
EditorPage *editor_page_new(const gchar *heading,
                            GPtrArray *tags,
                            fetch_page_fn fetch_page,
                            gpointer fetch_page_user_data,
                            GCallback created_cb,
                            gpointer user_data);

GtkWidget *editor_page_in_content_button(EditorPage *self);

GtkWidget *editor_page_in_list_button(EditorPage *self);

GString *editor_page_to_md(EditorPage *self);

EditorPage *editor_page_load(gchar *content,
                             fetch_page_fn fetch_page,
                             gpointer fetch_page_user_data,
                             GCallback created_cb,
                             gpointer user_data);
void editor_page_fix_content(EditorPage *page);

void editor_page_add_anchor(EditorPage *self, EditorPage *other);

void editor_page_update_style(EditorPage *self, enum style style_id);

gchar *editor_page_name_to_filename(const gchar *name);

const gchar *const *editor_page_get_styles(void);

G_END_DECLS
