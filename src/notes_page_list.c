#include "notes_page_list.h"
#include <glib-object.h>
#include <glib.h>
#include "notes_page_store.h"

struct _NotesPageList {
  GtkBox parent;
  NotesPageStore *pages;
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
  EditorPage *selected;

  selected = gtk_drop_down_get_selected_item(drop_down);

  if (notes_page_store_page_noop(selected)) {
    return;
  }

  if (notes_page_store_page_new(selected)) {
    g_print("Creating new page");
    gtk_drop_down_set_selected(drop_down, 0);
    editor_page_add_anchor(self->current, NULL);

    return;
  }

  g_print("Selecting page %s.\n", selected->heading);

  editor_page_add_anchor(self->current, selected);

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

  g_clear_object(&self->pages);

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

  self->pages = notes_page_store_new();
  drop_down = gtk_drop_down_new(G_LIST_MODEL(self->pages),
                                gtk_property_expression_new(EDITOR_TYPE_PAGE,
                                                            NULL, "heading"));

  gtk_box_append(GTK_BOX(self), drop_down);

  g_signal_connect(drop_down, "notify::selected-item", G_CALLBACK(add_link),
                   self);
}

NotesPageList *
notes_page_list_new(void)
{
  return g_object_new(NOTES_TYPE_PAGE_LIST, NULL);
}

EditorPage *
notes_page_list_find(NotesPageList *self, const gchar *heading)
{
  return notes_page_store_find(self->pages, heading);
}

void
notes_page_list_add(NotesPageList *self, EditorPage *page)
{
  g_return_if_fail(self != NULL);
  g_return_if_fail(page != NULL);

  notes_page_store_add(self->pages, page);
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

  notes_page_store_foreach(self->pages, (GFunc) fn, user_data);
}

void
notes_page_list_set_current(NotesPageList *self, EditorPage *page)
{
  g_clear_object(&self->current);
  self->current = g_object_ref(page);
}
