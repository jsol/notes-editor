#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "editor_page.h"
#include "notes_tag.h"
#include "notes_tag_list.h"

struct _NotesTagList {
  GtkWidget parent;
  GPtrArray *tags;
};

G_DEFINE_TYPE(NotesTagList, notes_tag_list, GTK_TYPE_BOX)

static void
notes_tag_list_dispose(GObject *obj)
{
  NotesTagList *self = NOTES_TAG_LIST(obj);

  g_assert(self);

  /* Do unrefs of objects and such. The object might be used after dispose,
   * and dispose might be called several times on the same object
   */

  /* Always chain up to the parent dispose function to complete object
   * destruction. */
  G_OBJECT_CLASS(notes_tag_list_parent_class)->dispose(obj);
}

static void
notes_tag_list_finalize(GObject *obj)
{
  NotesTagList *self = NOTES_TAG_LIST(obj);

  g_assert(self);

  g_clear_pointer(&self->tags, g_ptr_array_unref);

  /* free stuff */

  /* Always chain up to the parent finalize function to complete object
   * destruction. */
  G_OBJECT_CLASS(notes_tag_list_parent_class)->finalize(obj);
}

static void
notes_tag_list_class_init(NotesTagListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = notes_tag_list_dispose;
  object_class->finalize = notes_tag_list_finalize;
}

static void
notes_tag_list_init(NotesTagList *self)
{
  /* initialize all public and private members to reasonable default values.
   * They are all automatically initialized to 0 to begin with. */
  self->tags = g_ptr_array_new_with_free_func(g_object_unref);
}

GtkWidget *
notes_tag_list_new(void)
{
  return g_object_new(NOTES_TYPE_TAG_LIST, "orientation",
                      GTK_ORIENTATION_VERTICAL, "spacing", 5, NULL);
}

static gboolean
tag_equal(gconstpointer a, gconstpointer b)
{
  /* a is tag object from array, b is gchar tag name */
  NotesTag *t = NOTES_TAG((gpointer) a);
  const gchar *name = (const gchar *) b;

  return g_strcmp0(name, notes_tag_name(t)) == 0;
}

static gint
tag_sort(gconstpointer a, gconstpointer b)
{
  NotesTag *ta = *((NotesTag **) a);
  NotesTag *tb = *((NotesTag **) b);

  return g_strcmp0(notes_tag_name(ta), notes_tag_name(tb));
}

void
notes_tag_list_add(NotesTagList *self, EditorPage *page)
{
  NotesTag *t = NULL;
  g_return_if_fail(self != NULL);
  g_return_if_fail(page != NULL);

  for (guint i = 0; i < page->tags->len; i++) {
    guint index;
    const gchar *tag_name = (const gchar *) page->tags->pdata[i];
    if (g_ptr_array_find_with_equal_func(self->tags, tag_name, tag_equal,
                                         &index)) {
      /* Tag already exists */
      g_print("Tag exists: %s\n", tag_name);
      t = NOTES_TAG(g_ptr_array_index(self->tags, index));
    } else {
      t = notes_tag_new(tag_name);
      g_ptr_array_add(self->tags, t);
      g_ptr_array_sort(self->tags, tag_sort);
      g_ptr_array_find(self->tags, t, &index);
      if (index == 0) {
        gtk_box_prepend(GTK_BOX(self), GTK_WIDGET(t));
      } else {
        g_print("Tag inserted after: %s\n", tag_name);
        NotesTag *after = NOTES_TAG(g_ptr_array_index(self->tags, index - 1));
        gtk_box_insert_child_after(GTK_BOX(self), GTK_WIDGET(t),
                                   GTK_WIDGET(after));
      }
    }
    g_print("Adding page %s to tag %s\n", page->heading, tag_name);
    notes_tag_add_page(t, page);
  }
}