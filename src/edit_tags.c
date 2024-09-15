#include <adwaita.h>
#include <glib.h>

#include "edit_tags.h"
#include "editor_page.h"
#include "notes_tag_list.h"

static void
removed_clicked(GObject *button, gchar *tag_name)
{
  EditorPage *page;
  GtkWidget *label;
  guint index;

  page = g_object_get_data(button, "page");
  label = g_object_get_data(button, "label");

  if (g_ptr_array_find_with_equal_func(page->tags, tag_name, g_str_equal,
                                       &index)) {
    g_ptr_array_remove_index(page->tags, index);
#if GTK_MAJOR_VERSION >= 4 && GTK_MINOR_VERSION >= 10
    gtk_widget_set_visible(GTK_WIDGET(label), FALSE);
    gtk_widget_set_visible(GTK_WIDGET(button), FALSE);
#else
    gtk_widget_hide(GTK_WIDGET(button));
    gtk_widget_hide(GTK_WIDGET(label));
#endif
  }
}

static void
add_tag_to_grid(GtkGrid *grid, const gchar *tag, guint row, EditorPage *page)
{
  GtkWidget *l = gtk_label_new(tag);
  GtkWidget *b = gtk_button_new_from_icon_name("edit-delete");

  gtk_grid_attach(grid, l, 0, row, 5, 1);
  gtk_grid_attach(grid, b, 6, row, 1, 1);
  g_object_set_data(G_OBJECT(b), "page", page);
  g_object_set_data(G_OBJECT(b), "label", l);
  g_signal_connect_data(G_OBJECT(b), "clicked", G_CALLBACK(removed_clicked),
                        g_strdup(tag), (GClosureNotify) g_free, 0);
}

static void
add_clicked(GObject *button, GtkGrid *grid)
{
  EditorPage *page;
  GtkDropDown *drop_down = NULL;
  GtkEntry *entry = NULL;
  GtkEntryBuffer *buffer = NULL;
  const gchar *tag = NULL;
  NotesTagList *tags_list;

  page = g_object_get_data(button, "page");
  tags_list = g_object_get_data(button, "tags_list");
  drop_down = g_object_get_data(button, "drop_down");
  entry = g_object_get_data(button, "entry");

  if (entry != NULL) {
    buffer = gtk_entry_get_buffer(entry);
    tag = gtk_entry_buffer_get_text(buffer);
  } else if (drop_down != NULL) {
    tag = gtk_string_object_get_string(
      GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(drop_down)));
  }

  if (tag != NULL) {
    g_ptr_array_add(page->tags, g_strdup(tag));
    add_tag_to_grid(grid, tag, page->tags->len, page);
    notes_tag_list_add(tags_list, page);
  }
}

void
edit_tags_show(EditorPage *page, NotesTagList *tags_list)
{
  GtkWidget *title;
  GtkWidget *window;
  GtkWidget *header;
  GtkWidget *scroll;
  GtkWidget *box;
  GtkWidget *tag_grid;
  GtkWidget *add;
  GtkWidget *drop_down;
  GtkWidget *add_grid;
  GtkWidget *new_tag;
  GtkWidget *write_new_tag;
  gchar *heading;
  gchar **tags_opts;
  GtkStringList *available_tags;
  guint lines;

  heading = g_strdup_printf("Tags for %s", page->heading);
  window = adw_window_new();
  title = adw_window_title_new(heading, NULL);
  header = adw_header_bar_new();

  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  tag_grid = gtk_grid_new();
  add_grid = gtk_grid_new();

  gtk_grid_set_column_spacing(GTK_GRID(tag_grid), 5);
  gtk_grid_set_column_spacing(GTK_GRID(add_grid), 5);

  for (lines = 0; lines < page->tags->len; lines++) {
    add_tag_to_grid(GTK_GRID(tag_grid), page->tags->pdata[lines], lines, page);
  }

  tags_opts = notes_tag_list_get_tags_not_on_page(tags_list, page);
  available_tags = gtk_string_list_new(NULL);

  for (guint i = 0; tags_opts[i] != NULL; i++) {
    gtk_string_list_append(available_tags, tags_opts[i]);
  }

  drop_down = gtk_drop_down_new(G_LIST_MODEL(available_tags), NULL);

  add = gtk_button_new_with_label("Add selected");
  g_object_set_data(G_OBJECT(add), "drop_down", drop_down);
  g_object_set_data(G_OBJECT(add), "page", page);
  g_object_set_data(G_OBJECT(add), "tags_list", tags_list);
  g_signal_connect(add, "clicked", G_CALLBACK(add_clicked), tag_grid);

  gtk_grid_attach(GTK_GRID(add_grid), drop_down, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(add_grid), add, 1, 0, 1, 1);

  new_tag = gtk_button_new_with_label("Add new tag");
  write_new_tag = gtk_entry_new();
  g_object_set_data(G_OBJECT(new_tag), "entry", write_new_tag);
  g_object_set_data(G_OBJECT(new_tag), "page", page);
  g_object_set_data(G_OBJECT(new_tag), "tags_list", tags_list);
  g_signal_connect(new_tag, "clicked", G_CALLBACK(add_clicked), tag_grid);

  gtk_grid_attach(GTK_GRID(add_grid), write_new_tag, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(add_grid), new_tag, 1, 1, 1, 1);

  scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
  adw_window_set_content(ADW_WINDOW(window), scroll);
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 800);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title);

  gtk_box_append(GTK_BOX(box), header);

  gtk_box_append(GTK_BOX(box), tag_grid);
  gtk_box_append(GTK_BOX(box), add_grid);

  g_object_set_data(G_OBJECT(window), "tags_list", tags_list);
  gtk_window_present(GTK_WINDOW(window));
  g_free(heading);
  g_strfreev(tags_opts);
}
