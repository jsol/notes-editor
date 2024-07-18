#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include "editor_page.h"

G_BEGIN_DECLS

/*
 * Type declaration.
 */

#define NOTES_TYPE_TAG_LIST notes_tag_list_get_type()
G_DECLARE_FINAL_TYPE(NotesTagList, notes_tag_list, NOTES, TAG_LIST, GtkBox)

/*
 * Method definitions.
 */
GtkWidget *notes_tag_list_new(void);

void notes_tag_list_add(NotesTagList *self, EditorPage *page);

G_END_DECLS
