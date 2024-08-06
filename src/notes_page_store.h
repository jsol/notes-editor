#pragma once

#include <glib.h>
#include <glib-object.h>

#include "editor_page.h"

G_BEGIN_DECLS

/*
 * Type declaration.
 */

#define NOTES_TYPE_PAGE_STORE notes_page_store_get_type()
G_DECLARE_FINAL_TYPE(NotesPageStore, notes_page_store, NOTES, PAGE_STORE, GObject)

/*
 * Method definitions.
 */
NotesPageStore *notes_page_store_new(void);

void notes_page_store_add(NotesPageStore *self, EditorPage *page);

gboolean notes_page_store_page_noop(EditorPage *page);

gboolean notes_page_store_page_new(EditorPage *page);

void
notes_page_store_foreach(NotesPageStore *self, GFunc fn, gpointer user_data);

EditorPage *
notes_page_store_find(NotesPageStore *self, const gchar *heading);

G_END_DECLS
