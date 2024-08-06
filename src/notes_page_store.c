#include "notes_page_store.h"
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>

#include "editor_page.h"

struct _NotesPageStore {
  GObject parent;

  GType item_type;
  GPtrArray *store;
};

static void list_model_interface_init(GListModelInterface *iface);
G_DEFINE_TYPE_WITH_CODE(NotesPageStore,
                        notes_page_store,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL,
                                              list_model_interface_init))

typedef enum {
  PROP_ITEM_TYPE = 1,
  PROP_N_ITEMS,
  N_PROPERTIES
} NotesPageStoreProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
  NULL,
};

static gint
page_sort(gconstpointer a, gconstpointer b)
{
  gint *sva;
  gint *svb;

  const EditorPage *pa = *((EditorPage **) a);
  const EditorPage *pb = *((EditorPage **) b);

  sva = g_object_get_data(G_OBJECT(pa), "sort-val");
  svb = g_object_get_data(G_OBJECT(pb), "sort-val");

  if (sva != NULL && svb == NULL) {
    return -1;
  }

  if (sva == NULL && svb != NULL) {
    return 1;
  }

  if (sva != NULL && svb != NULL) {
    return (*svb) - (*sva);
  }

  return g_strcmp0(pa->heading, pb->heading);
}

static gboolean
page_heading_equal(gconstpointer a, gconstpointer b)
{
  g_print("In exuals func (%p vs %s)\n", a, (gchar *) b);
  /* a is tag object from array, b is gchar tag name */

  EditorPage *p = EDITOR_PAGE((gpointer) a);

  g_print("1\n");
  if (p == NULL) {
    g_print("Looking for a NULL page...\n");
    return FALSE;
  }
  g_print("2\n");
  const gchar *heading = (const gchar *) b;

  g_print("Comparing %s with %s\n", heading, p->heading);

  return g_strcmp0(heading, p->heading) == 0;
}

static void
changed(NotesPageStore *self)
{
  g_ptr_array_sort(self->store, page_sort);
  /* Notify changes */
  g_list_model_items_changed(G_LIST_MODEL(self), 0, self->store->len - 1,
                             self->store->len);

  g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_N_ITEMS]);
}

static void
changed_heading(G_GNUC_UNUSED EditorPage *page,
                G_GNUC_UNUSED GParamSpec *spec,
                NotesPageStore *self)
{
  g_assert(self);

  g_ptr_array_sort(self->store, page_sort);
  /* Notify changes */
  g_list_model_items_changed(G_LIST_MODEL(self), 0, self->store->len,
                             self->store->len);
}

static GType
get_item_type(GListModel *list)
{
  NotesPageStore *self = NOTES_PAGE_STORE(list);

  return self->item_type;
}

static guint
get_n_items(GListModel *list)
{
  NotesPageStore *self = NOTES_PAGE_STORE(list);

  return self->store->len;
}

static gpointer
get_item(GListModel *list, guint position)
{
  NotesPageStore *self = NOTES_PAGE_STORE(list);

  if (position >= self->store->len) {
    return NULL;
  }

  return g_object_ref(self->store->pdata[position]);
}

static void
get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  NotesPageStore *self = NOTES_PAGE_STORE(object);

  switch ((NotesPageStoreProperty) property_id) {
  case PROP_ITEM_TYPE:
    g_value_set_gtype(value, get_item_type(G_LIST_MODEL(self)));
    break;

  case PROP_N_ITEMS:
    g_value_set_uint(value, get_n_items(G_LIST_MODEL(self)));
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
  NotesPageStore *self = NOTES_PAGE_STORE(object);

  switch ((NotesPageStoreProperty) property_id) {
  case PROP_ITEM_TYPE: /* construct-only */
    g_assert(g_type_is_a(g_value_get_gtype(value), G_TYPE_OBJECT));
    self->item_type = g_value_get_gtype(value);
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

static void
notes_page_store_dispose(GObject *obj)
{
  NotesPageStore *self = NOTES_PAGE_STORE(obj);

  g_assert(self);

  /* Do unrefs of objects and such. The object might be used after dispose,
   * and dispose might be called several times on the same object
   */

  /* Always chain up to the parent dispose function to complete object
   * destruction. */
  G_OBJECT_CLASS(notes_page_store_parent_class)->dispose(obj);
}

static void
notes_page_store_finalize(GObject *obj)
{
  NotesPageStore *self = NOTES_PAGE_STORE(obj);

  g_assert(self);

  /* free stuff */

  /* Always chain up to the parent finalize function to complete object
   * destruction. */
  G_OBJECT_CLASS(notes_page_store_parent_class)->finalize(obj);
}

static void
notes_page_store_class_init(NotesPageStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = notes_page_store_dispose;
  object_class->finalize = notes_page_store_finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

  obj_properties[PROP_ITEM_TYPE] = g_param_spec_gtype("item-type", "", "",
                                                      G_TYPE_OBJECT,
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS);
  obj_properties[PROP_N_ITEMS] = g_param_spec_uint("n-items", "", "", 0,
                                                   G_MAXUINT, 0,
                                                   G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void
notes_page_store_init(G_GNUC_UNUSED NotesPageStore *self)
{
  /* initialize all public and private members to reasonable default values.
   * They are all automatically initialized to 0 to begin with. */
  EditorPage *p;
  gint *sv;

  self->store = g_ptr_array_new();
  g_print("PAges len: %u\n", self->store->len);

  p = editor_page_new("< Link to >", NULL, NULL, NULL, NULL, NULL);
  sv = g_malloc0(sizeof(*sv));
  *sv = 20;
  g_object_set_data_full(G_OBJECT(p), "sort-val", sv, g_free);
  g_ptr_array_add(self->store, p);

  g_print("PAges len: %u\n", self->store->len);

  p = editor_page_new("< New Page >", NULL, NULL, NULL, NULL, NULL);
  sv = g_malloc0(sizeof(*sv));
  *sv = 10;
  g_object_set_data_full(G_OBJECT(p), "sort-val", sv, g_free);
  g_ptr_array_add(self->store, p);

  g_print("PAges len: %u\n", self->store->len);
  changed(self);
}

static void
list_model_interface_init(GListModelInterface *iface)
{ /** Add funcs to *iface */

  iface->get_item_type = get_item_type;
  iface->get_n_items = get_n_items;
  iface->get_item = get_item;
}

NotesPageStore *
notes_page_store_new(void)
{
  return g_object_new(NOTES_TYPE_PAGE_STORE, "item-type", EDITOR_TYPE_PAGE,
                      NULL);
}

void
notes_page_store_add(NotesPageStore *self, EditorPage *page)
{
  g_print("Adding page to store %s\n", page->heading);
  g_ptr_array_add(self->store, g_object_ref(page));
  g_signal_connect(page, "notify::heading", G_CALLBACK(changed_heading), self);
  changed(self);
}

gboolean
notes_page_store_page_noop(EditorPage *page)
{
  gint *val;

  val = (gint *) g_object_get_data(G_OBJECT(page), "sort-val");

  if (val == NULL) {
    return FALSE;
  }

  return *val == 20;
}

gboolean
notes_page_store_page_new(EditorPage *page)
{
  gint *val;
  val = (gint *) g_object_get_data(G_OBJECT(page), "sort-val");

  if (val == NULL) {
    return FALSE;
  }

  return *val == 10;
}

void
notes_page_store_foreach(NotesPageStore *self, GFunc fn, gpointer user_data)

{
  g_return_if_fail(self != NULL);
  g_return_if_fail(fn != NULL);

  g_print("For-eaching pages\n");

  g_ptr_array_foreach(self->store, fn, user_data);
}

EditorPage *
notes_page_store_find(NotesPageStore *self, const gchar *heading)
{
  guint index = 0;
  g_return_val_if_fail(self != NULL, NULL);
  g_return_val_if_fail(heading != NULL, NULL);

  g_print("Finding page with name %s (%u)\n", heading, self->store->len);

  if (!g_ptr_array_find_with_equal_func(self->store, heading,
                                        page_heading_equal, &index)) {
    g_print("Returning Not found\n");
    return NULL;
  }

  g_print("Returning found index %u / %u\n", index, self->store->len);

  return self->store->pdata[index];
}