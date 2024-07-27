#pragma once

#include <glib.h>
#include "editor_page.h"

G_BEGIN_DECLS


#define NOTES_TYPE_PAGE_LIST notes_page_list_get_type()
G_DECLARE_FINAL_TYPE(NotesPageList, notes_page_list, NOTES, PAGE_LIST, GtkBox)

typedef void (*pages_for_each)(EditorPage *page, gpointer user_data);
/*
 * Method definitions.
 */
NotesPageList *notes_page_list_new(void);

EditorPage *notes_page_list_find(NotesPageList *self, const gchar *name);

void notes_page_list_add(NotesPageList *self, EditorPage *page);
void notes_page_list_remove(NotesPageList *self, EditorPage *page);

void notes_page_list_for_each(NotesPageList *self,
                              pages_for_each fn,
                              gpointer user_data);

G_END_DECLS
