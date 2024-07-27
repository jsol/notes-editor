#include "notes_page_list.h"
#include <glib-object.h>
#include <glib.h>

struct _NotesPageList {
  GtkBox parent;
  GPtrArray *pages;
};

G_DEFINE_TYPE(NotesPageList, notes_page_list, GTK_TYPE_BOX)

static void
notes_page_list_dispose(GObject *obj)
{
  NotesPageList *self = NOTES_PAGE_LIST(obj);

  g_assert(self);

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

  self->pages = g_ptr_array_new_with_free_func(g_object_unref);
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
