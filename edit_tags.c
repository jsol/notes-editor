#include <adwaita.h>
#include <glib.h>

#include "edit_tags.h"
#include "editor_page.h"
#include "notes_tag_list.h"

static void
tag_changed(GtkEditableLabel *label,
            G_GNUC_UNUSED GParamSpec *pspec,
            EditorPage *page)
{
  gchar *old_name = NULL;
  guint index = 0;

  g_assert(label);
  g_assert(page);
  g_print("In tag changed\n");

  g_assert(page->tags);

  if (gtk_editable_label_get_editing(label) != FALSE) {
    return;
  }

  old_name = g_object_get_data(G_OBJECT(label), "old_name");

  if (g_strcmp0(old_name, gtk_editable_get_text(GTK_EDITABLE(label))) == 0) {
    g_warning("No change: %s vs %s", old_name,
              gtk_editable_get_text(GTK_EDITABLE(label)));
    return;
  }

  if (!g_ptr_array_find_with_equal_func(page->tags, old_name, g_str_equal,
                                        &index)) {
    g_warning("Could not find old tag name %s", old_name);
    return;
  }

  g_ptr_array_add(page->tags,
                  g_strdup(gtk_editable_get_text(GTK_EDITABLE(label))));
  g_ptr_array_remove_index_fast(page->tags, index);

  g_object_set_data_full(G_OBJECT(label), "old_name",
                         g_strdup(gtk_editable_get_text(GTK_EDITABLE(label))),
                         g_free);
}

static void
add_button(GtkBox *box, const gchar *text, EditorPage *page)
{
  GtkWidget *tag;
  g_assert(box);
  g_assert(text);
  g_assert(page);
  g_assert(page->tags);

  tag = gtk_editable_label_new(text);
  g_object_set_data_full(G_OBJECT(tag), "old_name", g_strdup(text), g_free);

  gtk_box_append(box, tag);
  g_print("Adding cb for page %s\n", page->heading);
  g_signal_connect(tag, "notify::editing", G_CALLBACK(tag_changed), page);
}

static void
add_clicked(GObject *button, GObject *tag_list)
{
  EditorPage *page;

  page = g_object_get_data(button, "page");
  g_ptr_array_add(page->tags, g_strdup("Unnamed tag"));
  add_button(GTK_BOX(tag_list), "Unnamed tag", page);
}

static void
close_window(GObject *window, EditorPage *page)
{
  NotesTagList *tags_list;

  tags_list = g_object_get_data(window, "tags_list");
  notes_tag_list_add(tags_list, page);
}

void
edit_tags_show(EditorPage *page, NotesTagList *tags_list)
{
  GtkWidget *title;
  GtkWidget *window;
  GtkWidget *header;
  GtkWidget *scroll;
  GtkWidget *box;
  GtkWidget *tag_box;
  GtkWidget *add;
  gchar *heading;

  heading = g_strdup_printf("Tags for %s", page->heading);
  window = adw_window_new();
  title = adw_window_title_new(heading, NULL);
  header = adw_header_bar_new();

  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  tag_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

  for (guint i = 0; i < page->tags->len; i++) {
    add_button(GTK_BOX(tag_box), page->tags->pdata[i], page);
  }

  add = gtk_button_new_with_label("Add new tag");
  g_object_set_data(G_OBJECT(add), "page", page);
  g_signal_connect(add, "clicked", G_CALLBACK(add_clicked), tag_box);

  scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
  adw_window_set_content(ADW_WINDOW(window), scroll);
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 800);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title);
  gtk_box_append(GTK_BOX(box), header);

  gtk_box_append(GTK_BOX(box), tag_box);
  gtk_box_append(GTK_BOX(box), add);

  g_object_set_data(G_OBJECT(window), "tags_list", tags_list);
  g_signal_connect(window, "close-request", G_CALLBACK(close_window), page);
  gtk_window_present(GTK_WINDOW(window));
  g_free(heading);
}
