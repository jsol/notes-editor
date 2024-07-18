#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include "editor_page.h"

G_BEGIN_DECLS

/*
 * Type declaration.
 */

#define NOTES_TYPE_TAG notes_tag_get_type()
G_DECLARE_FINAL_TYPE(NotesTag, notes_tag, NOTES, TAG, GtkBox)

/*
 * Method definitions.
 */
NotesTag *notes_tag_new(const gchar *name);

const gchar *notes_tag_name(NotesTag *t);

void notes_tag_add_page(NotesTag *self, EditorPage *page);

G_END_DECLS
