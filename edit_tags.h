#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

#include "editor_page.h"
#include "notes_tag_list.h"

G_BEGIN_DECLS


void edit_tags_show(EditorPage *page, NotesTagList *tags_list);

G_END_DECLS