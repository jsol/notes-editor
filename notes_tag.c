#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "editor_page.h"
#include "notes_tag.h"

struct _NotesTag {
  GtkWidget parent;
  gchar *name;
  GPtrArray *pages;
  GtkWidget *exp;
  GtkWidget *box;
};

G_DEFINE_TYPE(NotesTag, notes_tag, GTK_TYPE_BOX)

static void
notes_tag_dispose(GObject *obj)
{
  NotesTag *self = NOTES_TAG(obj);

  g_assert(self);

  /* Do unrefs of objects and such. The object might be used after dispose,
   * and dispose might be called several times on the same object
   */

  /* Always chain up to the parent dispose function to complete object
   * destruction. */
  G_OBJECT_CLASS(notes_tag_parent_class)->dispose(obj);
}

static void
notes_tag_finalize(GObject *obj)
{
  NotesTag *self = NOTES_TAG(obj);

  g_assert(self);
  g_clear_pointer(&self->pages, g_ptr_array_unref);
  g_clear_object(&self->exp);
  g_clear_object(&self->box);
  g_free(self->name);

  /* free stuff */

  /* Always chain up to the parent finalize function to complete object
   * destruction. */
  G_OBJECT_CLASS(notes_tag_parent_class)->finalize(obj);
}

static void
notes_tag_class_init(NotesTagClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = notes_tag_dispose;
  object_class->finalize = notes_tag_finalize;
}

static void
notes_tag_init(NotesTag *self)
{
  /* initialize all public and private members to reasonable default values.
   * They are all automatically initialized to 0 to begin with. */
  self->pages = g_ptr_array_new_with_free_func(g_object_unref);
}

NotesTag *
notes_tag_new(const gchar *name)
{
  NotesTag *n = g_object_new(NOTES_TYPE_TAG, "orientation",
                             GTK_ORIENTATION_VERTICAL, "spacing", 0, NULL);
  n->name = g_strdup(name);
  n->exp = gtk_expander_new(n->name);
  n->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_expander_set_child(GTK_EXPANDER(n->exp), n->box);
  gtk_box_append(GTK_BOX(n), n->exp);

  return n;
}

const gchar *
notes_tag_name(NotesTag *self)
{
  g_return_val_if_fail(self != NULL, NULL);

  return self->name;
}

static gint
page_sort(gconstpointer a, gconstpointer b)
{
  const EditorPage *pa = *((EditorPage **) a);
  const EditorPage *pb = *((EditorPage **) b);

  return g_strcmp0(pa->heading, pb->heading);
}

void
notes_tag_add_page(NotesTag *self, EditorPage *page)
{
  guint index;
  GtkWidget *page_button;
  g_return_if_fail(self != NULL);

  g_return_if_fail(page != NULL);

  if (g_ptr_array_find(self->pages, page, NULL)) {
    g_print("Page already in array\n");
    return;
  }

  g_ptr_array_add(self->pages, g_object_ref(page));
  g_ptr_array_sort(self->pages, page_sort);
  g_ptr_array_find(self->pages, page, &index);

  page_button = editor_page_in_list_button(page);

  if (index == 0) {
    g_print("Adding page to tag, by prepend\n");
    gtk_box_prepend(GTK_BOX(self->box), page_button);
  } else {
    g_print("Adding page to tag\n");
    GtkWidget *after_button = NULL;
    EditorPage *after_page;

    after_page = EDITOR_PAGE(g_ptr_array_index(self->pages, index - 1));

    for (guint i = 0; i < after_page->buttons->len; i++) {
      if (gtk_widget_get_parent(GTK_WIDGET(after_page->buttons->pdata[i])) ==
          self->box) {
        after_button = GTK_WIDGET(after_page->buttons->pdata[i]);
      }
    }

    if (after_button == NULL) {
      g_warning("Could not find parent of previous button!");
      return;
    }

    gtk_box_insert_child_after(GTK_BOX(self->box), page_button,
                               GTK_WIDGET(after_button));
  }
}