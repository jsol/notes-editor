#include "notes_page_list.h"
#include <glib-object.h>
#include <glib.h>

struct _NotesPageList {
  GtkBox parent;
  GPtrArray *pages;
  GtkStringList *pages_list;
  gchar *link_to;
  gchar *new_page;
  EditorPage *current;
};

G_DEFINE_TYPE(NotesPageList, notes_page_list, GTK_TYPE_BOX)

static void
add_link(GtkDropDown *drop_down,
         G_GNUC_UNUSED GParamSpec *spec,
         NotesPageList *self)
{
  guint selected;

  selected = gtk_drop_down_get_selected(drop_down);

  if (selected == 0) {
    g_print("Not selected anything\n");
    return;
  }

  if (selected == 1) {
    g_print("Creating new page");
    gtk_drop_down_set_selected(drop_down, 0);
    editor_page_add_anchor(self->current, NULL);

    return;
  }

  EditorPage *s = (EditorPage *) self->pages->pdata[selected - 2];
  g_print("Selecting page %s.\n", s->heading);

  editor_page_add_anchor(self->current, s);

  gtk_drop_down_set_selected(drop_down, 0);
}

static void
notes_page_list_dispose(GObject *obj)
{
  NotesPageList *self = NOTES_PAGE_LIST(obj);

  g_assert(self);

  g_clear_object(&self->current);

  /* TODO: Remove all listeners to page changes */

  G_OBJECT_CLASS(notes_page_list_parent_class)->dispose(obj);
}

static void
notes_page_list_finalize(GObject *obj)
{
  NotesPageList *self = NOTES_PAGE_LIST(obj);

  g_assert(self);

  g_clear_pointer(&self->pages, g_ptr_array_unref);

  /* Always chain up to the parent finalize function to complete object
   * destruction. */
  G_OBJECT_CLASS(notes_page_list_parent_class)->finalize(obj);
}

static void
notes_page_list_class_init(NotesPageListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = notes_page_list_dispose;
  object_class->finalize = notes_page_list_finalize;
}

static void
notes_page_list_init(NotesPageList *self)
{
  g_assert(self);
  GtkWidget *drop_down;

  self->pages = g_ptr_array_new_with_free_func(g_object_unref);
  self->pages_list = gtk_string_list_new(NULL);
  drop_down = gtk_drop_down_new(G_LIST_MODEL(self->pages_list), NULL);
  self->link_to = g_strdup("< Link to >");
  self->new_page = g_strdup("< Link to >");

  gtk_box_append(GTK_BOX(self), drop_down);
  gtk_string_list_append(self->pages_list, "< Link to >");
  gtk_string_list_append(self->pages_list, "New page");

  g_signal_connect(drop_down, "notify::selected-item", G_CALLBACK(add_link),
                   self);
}

NotesPageList *
notes_page_list_new(void)
{
  return g_object_new(NOTES_TYPE_PAGE_LIST, NULL);
}

static gboolean
page_heading_equal(gconstpointer a, gconstpointer b)
{
  /* a is tag object from array, b is gchar tag name */
  EditorPage *p = EDITOR_PAGE((gpointer) a);
  const gchar *heading = (const gchar *) b;

  return g_strcmp0(heading, p->heading) == 0;
}

EditorPage *
notes_page_list_find(NotesPageList *self, const gchar *heading)
{
  guint index = 0;
  g_return_val_if_fail(self != NULL, NULL);
  g_return_val_if_fail(heading != NULL, NULL);

  if (!g_ptr_array_find_with_equal_func(self->pages, heading,
                                        page_heading_equal, &index)) {
    return NULL;
  }

  return self->pages->pdata[index];
}

void
notes_page_list_add(NotesPageList *self, EditorPage *page)
{
  g_return_if_fail(self != NULL);
  g_return_if_fail(page != NULL);

  g_ptr_array_add(self->pages, page);
  gtk_string_list_append(self->pages_list, page->heading);
}

void
notes_page_list_remove(NotesPageList *self, EditorPage *page)
{
}

void
notes_page_list_for_each(NotesPageList *self,
                         pages_for_each fn,
                         gpointer user_data)

{
  g_return_if_fail(self != NULL);
  g_return_if_fail(fn != NULL);

  g_ptr_array_foreach(self->pages, (GFunc) fn, user_data);
}

void
notes_page_list_set_current(NotesPageList *self, EditorPage *page)
{
  g_clear_object(&self->current);
  self->current = g_object_ref(page);
}