#include <adwaita.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "dialog.h"
#include "edit_tags.h"
#include "editor_page.h"
#include "notes_tag_list.h"
#include "sidebar.h"

/*
TODO
- fix memory leaks (hash table for pages...),
- fix g_object_unref
- Fix delete page
- Fix removing tag from page
- Clean up various marks added to buffers
- fix header/style tags wonkyness
- style headers better
- toast on save
*/

static GtkWindow *app_window;
static gchar *workspace_path = NULL;
#define WS_NAME_FILE   ".notes-editor"
#define APPLICATION_ID "com.github.jsol.notes-editor"

static const gchar *
get_current_ws(void)
{
  GError *lerr = NULL;
  gchar *save_file;
  gchar *path = NULL;

  if (workspace_path != NULL) {
    return workspace_path;
  }

  save_file = g_build_filename(g_getenv("HOME"), WS_NAME_FILE, NULL);

  if (!g_file_get_contents(save_file, &path, NULL, &lerr)) {
    g_warning("Could not load workspace path: %s",
              lerr->message != NULL ? lerr->message : "No error message");
    g_clear_error(&lerr);
    goto out;
  }

  g_free(save_file);

  g_strstrip(path);

  workspace_path = path;
out:
  return path;
}

static void
save_current_ws(const gchar *path)
{
  GError *lerr = NULL;
  gchar *save_file;

  if (path == NULL) {
    return;
  }

  g_free(workspace_path);
  workspace_path = g_strdup(path);

  save_file = g_build_filename(g_getenv("HOME"), WS_NAME_FILE, NULL);

  if (!g_file_set_contents(save_file, path, -1, &lerr)) {
    g_warning("Could not save workspace path: %s",
              lerr->message != NULL ? lerr->message : "No error message");
  }

  g_free(save_file);
}

static void
anchors_foreach(gpointer data, gpointer user_data)
{
  GtkTextView *view = GTK_TEXT_VIEW(user_data);
  GtkTextChildAnchor *anchor = GTK_TEXT_CHILD_ANCHOR(data);
  EditorPage *target;

  if (gtk_text_child_anchor_get_deleted(anchor)) {
    return;
  }

  target = g_object_get_data(G_OBJECT(anchor), "target");

  gtk_text_view_add_child_at_anchor(view, editor_page_in_content_button(target),
                                    anchor);
}

static void
header_changed(GtkEditable *self, gpointer user_data)
{
  EditorPage *page = EDITOR_PAGE(user_data);

  g_object_set(page, "heading", gtk_editable_get_text(self), NULL);
}

static void
update_css(GHashTable *pages)
{
  GdkDisplay *display;
  GtkCssProvider *provider;
  GString *style = g_string_new(
    ".in-text-button {padding: 0px; margin: 0px;  "
    "margin-bottom: -8px;} .in-list-button "
    "{padding: 0px; margin: 0px;  margin-left: 15px; margin-bottom: -8px;"
    " font-weight: normal;}");

  g_assert(pages);

  display = gdk_display_get_default();

  provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider, style->str, strlen(style->str));

  gtk_style_context_add_provider_for_display(display,
                                             GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);

  g_string_free(style, TRUE);
}

/*
static void
remove_choice_cb(G_GNUC_UNUSED GObject *source_object,
                 G_GNUC_UNUSED GAsyncResult *res,
                 G_GNUC_UNUSED gpointer data)
{
}*/

static void
remove_page(G_GNUC_UNUSED GObject *button, EditorPage *self)
{
}

static void
set_page(EditorPage *page, GtkApplication *app)
{
  GtkWidget *content_header;
  GtkWidget *textarea;
  EditorPage *current_page;
  GtkWidget *remove_button;

  content_header = g_object_get_data(G_OBJECT(app), "content_header");
  textarea = g_object_get_data(G_OBJECT(app), "textarea");
  current_page = g_object_get_data(G_OBJECT(app), "current_page");
  remove_button = g_object_get_data(G_OBJECT(app), "remove_button");

  if (current_page == page) {
    return;
  }
  g_signal_handlers_disconnect_matched(content_header, G_SIGNAL_MATCH_FUNC, 0,
                                       0, NULL, header_changed, NULL);
  g_signal_handlers_disconnect_matched(remove_button, G_SIGNAL_MATCH_FUNC, 0, 0,
                                       NULL, remove_page, NULL);

  gtk_text_view_set_buffer(GTK_TEXT_VIEW(textarea), page->content);

  gtk_editable_set_text(GTK_EDITABLE(content_header), page->heading);

  g_ptr_array_foreach(page->anchors, anchors_foreach, textarea);

  g_signal_connect(content_header, "changed", G_CALLBACK(header_changed), page);
  g_signal_connect(remove_button, "clicked", G_CALLBACK(remove_page), page);

  g_object_set_data(G_OBJECT(app), "current_page", page);
  g_print("Set current Page %p , app %p\n", page, app);
}

static void
single_anchor(EditorPage *page,
              GtkTextChildAnchor *anchor,
              GtkWidget *button,
              GObject *app)
{
  GtkTextView *textarea;
  EditorPage *current_page;

  current_page = g_object_get_data(app, "current_page");
  g_print("Page %p vs %p, app %p\n", page, current_page, app);
  if (page != current_page) {
    g_print("Bailing due to not active page\n");
    return;
  }

  textarea = g_object_get_data(app, "textarea");

  gtk_text_view_add_child_at_anchor(textarea, button, anchor);
}

static void
set_heading(GtkDropDown *drop_down, G_GNUC_UNUSED GParamSpec *spec, GObject *app)
{
  EditorPage *current_page;

  current_page = g_object_get_data(app, "current_page");

  if (current_page == NULL) {
    return;
  }
  editor_page_update_style(current_page, gtk_drop_down_get_selected(drop_down));
  gtk_drop_down_set_selected(drop_down, 0);
}

static void
set_tags(G_GNUC_UNUSED GObject *button, GObject *app)
{
  EditorPage *current_page;
  NotesTagList *tags_list;

  current_page = g_object_get_data(app, "current_page");

  tags_list = g_object_get_data(app, "tags_list");

  g_print("Tags: %s\n", current_page->heading);
  edit_tags_show(current_page, tags_list);
}

static void
page_created(EditorPage *page, GObject *app)
{
  NotesTagList *tags_list;
  GQueue *pages_list;

  g_print("Start page createsd\n");

  tags_list = g_object_get_data(app, "tags_list");
  pages_list = g_object_get_data(app, "pages_list");

  g_queue_push_tail(pages_list, page);
  notes_tag_list_add(tags_list, page);

  g_signal_connect(page, "switch-page", G_CALLBACK(set_page), app);

  g_signal_connect(page, "new-anchor", G_CALLBACK(single_anchor), app);

  update_css(page->pages);
  g_print("Page created: %s\n", page->heading);
}

static gboolean
prepare_folder(const gchar *base_path)
{
  if (base_path == NULL) {
    return FALSE;
  }

  /*ensure path exists */

  return TRUE;
}

static void
save(GtkApplication *app, const gchar *base_path)
{
  GQueue *pages_list;
  GError *lerr = NULL;
  const gchar *root;

  if (base_path == NULL) {
    root = get_current_ws();
  } else {
    root = base_path;
    save_current_ws(root);
  }

  if (!prepare_folder(root)) {
    g_warning("Can not save to %s", base_path);
    return;
  }

  pages_list = g_object_get_data(G_OBJECT(app), "pages_list");

  for (GList *iter = pages_list->head; iter != NULL; iter = iter->next) {
    EditorPage *page = EDITOR_PAGE(iter->data);
    gchar *file;
    gchar *full_path;
    GString *content;

    file = editor_page_name_to_filename(page->heading);
    full_path = g_build_filename(root, file, NULL);

    g_print("Saving file... %s  -> %s -> %s\n", root, file, full_path);

    content = editor_page_to_md(page);

    if (!g_file_set_contents(full_path, content->str, content->len, &lerr)) {
      g_warning("Could not save %s: %s", full_path, lerr->message);
      g_clear_error(&lerr);
    }

    g_free(file);
    g_free(full_path);
    g_string_free(content, TRUE);
  }
}

static void
pages_load_iter(G_GNUC_UNUSED gpointer key,
                gpointer value,
                G_GNUC_UNUSED gpointer user_data)
{
  g_assert(value);

  editor_page_fix_content(value);
}

static void
load_repo(GHashTable *pages, GtkApplication *app)
{
  GError *lerr = NULL;
  GDir *dir;
  gboolean page_set = FALSE;
  const gchar *filename;
  GFile *sync_script;
  const gchar *root_path;

  g_assert(pages);

  root_path = get_current_ws();

  g_message("Loading name: %s", root_path);

  dir = g_dir_open(root_path, 0, &lerr);

  if (dir == NULL) {
    g_warning("Error loading dir: %s",
              lerr ? lerr->message : "No error message");
    g_clear_error(&lerr);
    return;
  }

  while ((filename = g_dir_read_name(dir))) {
    printf("%s\n", filename);

    if (g_strcmp0(filename, "sync.sh") == 0) {
      sync_script = g_file_new_build_filename(root_path, filename, NULL);
      g_object_set_data_full(G_OBJECT(app), "sync_script", sync_script,
                             g_object_unref);
    }
    if (!g_str_has_suffix(filename, ".md")) {
      continue;
    }

    gchar *full_path;
    EditorPage *page;

    full_path = g_build_filename(root_path, filename, NULL);
    page = editor_page_load(pages, full_path, G_CALLBACK(page_created), app);

    if (!page_set) {
      set_page(page, app);
      page_set = TRUE;
    }

    g_free(full_path);
  }

  g_dir_close(dir);

  g_hash_table_foreach(pages, pages_load_iter, NULL);

  update_css(pages);
}

static void
open_file_cb(GFile *file, gpointer user_data)
{
  GtkApplication *app = GTK_APPLICATION(user_data);

  g_assert(file);
  g_assert(app);

  save_current_ws(g_file_peek_path(file));
  load_repo(g_hash_table_new(g_str_hash, g_str_equal), app);
}

static void
save_file_cb(GFile *file, gpointer user_data)
{
  GtkApplication *app = GTK_APPLICATION(user_data);

  g_assert(file);
  g_assert(app);

  save(app, g_file_peek_path(file));
}

static void
open_menu_cb(GSimpleAction *simple_action, GVariant *parameter, gpointer *data)
{
  dialog_folder_select(app_window, "Open", "_Open", open_file_cb, data);
}

static void
save_menu_cb(GSimpleAction *simple_action, GVariant *parameter, gpointer *data)
{
  dialog_folder_select(app_window, "Save", "_Save", save_file_cb, data);
}

static void
sync_done_cb(GObject *source_object, GAsyncResult *res, gpointer data)
{
  GSubprocess *subprocess = G_SUBPROCESS(source_object);
  GtkApplication *app = (GtkApplication *) data;
  GError *err = NULL;

  AdwToastOverlay *toast_overlay = g_object_get_data(G_OBJECT(app),
                                                     "toast_overlay");

  if (!g_subprocess_wait_check_finish(subprocess, res, &err)) {
    adw_toast_overlay_add_toast(toast_overlay,
                                adw_toast_new(err != NULL ? err->message :
                                                            "Sync failed"));
    g_clear_error(&err);
  } else {
    adw_toast_overlay_add_toast(toast_overlay, adw_toast_new("Sync completed"));
  }
}

static void
sync_menu_cb(GSimpleAction *simple_action, GVariant *parameter, gpointer *data)
{
  GtkApplication *app = (GtkApplication *) data;
  GError *err = NULL;
  const gchar *root_path;
  GSubprocessLauncher *launcher;
  GSubprocess *sync_process;

  AdwToastOverlay *toast_overlay = g_object_get_data(G_OBJECT(app),
                                                     "toast_overlay");
  GFile *sync_script = g_object_get_data(G_OBJECT(app), "sync_script");

  if (sync_script == NULL) {
    adw_toast_overlay_add_toast(toast_overlay,
                                adw_toast_new("No sync script present"));
    return;
  }
  g_message("Running sync script!");

  root_path = get_current_ws();

  launcher = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_set_cwd(launcher, root_path);

  sync_process = g_subprocess_launcher_spawn(launcher, &err,
                                             g_file_peek_path(sync_script),
                                             NULL);
  g_subprocess_wait_check_async(sync_process, NULL, sync_done_cb, app);
}

static void
new_menu_cb(GSimpleAction *simple_action, GVariant *parameter, gpointer *data)
{
  GtkApplication *app = GTK_APPLICATION(data);
  EditorPage *page;

  g_print("Setting new page!");

  page = editor_page_new("Overview", NULL,
                         g_hash_table_new(g_str_hash, g_str_equal),
                         G_CALLBACK(page_created), app);

  set_page(page, app);
}

static void
event_key_released(G_GNUC_UNUSED GtkEventController *self,
                   guint keyval,
                   G_GNUC_UNUSED guint keycode,
                   GdkModifierType state,
                   gpointer user_data)

{
  GtkApplication *app = GTK_APPLICATION(user_data);

  if (keyval == 115 && (state & GDK_CONTROL_MASK)) {
    /* ctrl+s */

    const gchar *save_path = get_current_ws();
    if (save_path == NULL) {
      save_menu_cb(NULL, NULL, user_data);
    } else {
      save(app, NULL);
    }
  } else if (keyval == 98 && (state & GDK_CONTROL_MASK)) {
    /* ctrl + b*/
    set_heading(NULL, NULL, G_OBJECT(app));
  }
}

static void
build_menu(GtkWidget *header, GtkApplication *app)
{
  GtkWidget *menu_button = gtk_menu_button_new();
  GMenu *menubar = g_menu_new();
  GMenuItem *menu_item_menu;

  menu_item_menu = g_menu_item_new("New", "app.new");
  g_menu_append_item(menubar, menu_item_menu);
  g_object_unref(menu_item_menu);

  menu_item_menu = g_menu_item_new("Save", "app.save");
  g_menu_append_item(menubar, menu_item_menu);
  g_object_unref(menu_item_menu);

  menu_item_menu = g_menu_item_new("Open", "app.open");
  g_menu_append_item(menubar, menu_item_menu);
  g_object_unref(menu_item_menu);

  menu_item_menu = g_menu_item_new("Sync", "app.sync");
  g_menu_append_item(menubar, menu_item_menu);
  g_object_unref(menu_item_menu);

  GSimpleAction *act_open = g_simple_action_new("open", NULL);
  g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_open));
  g_signal_connect(act_open, "activate", G_CALLBACK(open_menu_cb), app);

  GSimpleAction *act_save = g_simple_action_new("save", NULL);
  g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_save));
  g_signal_connect(act_save, "activate", G_CALLBACK(save_menu_cb), app);

  GSimpleAction *act_sync = g_simple_action_new("sync", NULL);
  g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_sync));
  g_signal_connect(act_sync, "activate", G_CALLBACK(sync_menu_cb), app);

  GSimpleAction *act_new = g_simple_action_new("new", NULL);
  g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_new));
  g_signal_connect(act_new, "activate", G_CALLBACK(new_menu_cb), app);

  gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button),
                                 G_MENU_MODEL(menubar));
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), menu_button);
}

static void
set_icon(void)
{
  GdkDisplay *display;
  GtkIconTheme *icon_theme;

  display = gdk_display_get_default();
  g_assert(display);

  icon_theme = gtk_icon_theme_get_for_display(display);

  if (gtk_icon_theme_has_icon(icon_theme, APPLICATION_ID) != 1) {
    // manage error
    g_warning("Could not find icon");
  }
}

static void
activate(GtkApplication *app, gpointer user_data)
{
  GtkWidget *window;
  GtkWidget *textarea;

  GtkWidget *box;
  GtkWidget *content_box;
  GtkWidget *content_header_box;
  GtkWidget *content_header;
  GtkWidget *styles_drop_down;
  GtkWidget *remove_button;
  GtkWidget *tag_button;
  GtkWidget *scroll;
  GtkEventController *event_controller;
  // EditorPage *page;
  GtkWidget *tags_list;
  GtkWidget *toast_overlay;

  set_icon();

  textarea = gtk_text_view_new();

  scroll = gtk_scrolled_window_new();
  tags_list = notes_tag_list_new();
  GQueue *pages_list = g_queue_new();

  content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  content_header = gtk_editable_label_new("");
  gtk_widget_add_css_class(content_header, "title-1");
  gtk_widget_set_margin_start(content_box, 20);

  styles_drop_down = gtk_drop_down_new_from_strings(editor_page_get_styles());
  gtk_widget_set_halign(styles_drop_down, GTK_ALIGN_END);

  tag_button = gtk_button_new_with_label("Tags");

  gtk_widget_set_halign(tag_button, GTK_ALIGN_END);

  remove_button = gtk_button_new_from_icon_name("edit-delete");
  gtk_widget_set_halign(remove_button, GTK_ALIGN_END);

  content_header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_widget_set_hexpand(content_header_box, TRUE);
  gtk_widget_set_hexpand(content_header, TRUE);

  toast_overlay = adw_toast_overlay_new();

  gtk_box_append(GTK_BOX(content_header_box), content_header);

  gtk_box_append(GTK_BOX(content_header_box), styles_drop_down);
  gtk_box_append(GTK_BOX(content_header_box), tag_button);
  gtk_box_append(GTK_BOX(content_header_box), remove_button);

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), textarea);
  gtk_box_append(GTK_BOX(content_box), content_header_box);
  gtk_box_append(GTK_BOX(content_box), scroll);

  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textarea), 20);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textarea), GTK_WRAP_WORD);

  window = adw_application_window_new(app);
  GtkWidget *title = adw_window_title_new("Editor", NULL);
  GtkWidget *header = adw_header_bar_new();
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title);
  build_menu(header, app);

  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

  gtk_box_append(GTK_BOX(box), header);
  gtk_box_append(GTK_BOX(box), get_framed_content(tags_list, content_box));

  g_object_set_data(G_OBJECT(app), "content_header", content_header);
  g_object_set_data(G_OBJECT(app), "toast_overlay", toast_overlay);
  g_object_set_data(G_OBJECT(app), "textarea", textarea);
  g_object_set_data(G_OBJECT(app), "tags_list", tags_list);
  g_object_set_data(G_OBJECT(app), "pages_list", pages_list);
  g_object_set_data(G_OBJECT(app), "remove_button", remove_button);
  g_object_set_data(G_OBJECT(textarea), "app", app);

  // page = editor_page_new("Overview", g_hash_table_new(g_str_hash,
  // g_str_equal),
  //                      G_CALLBACK(page_created), app);

  /* set_page(page, app); */

  const gchar *saved_path = get_current_ws();

  if (saved_path != NULL && strlen(saved_path) > 3) {
    g_message("Loading pages from %s", saved_path);
    load_repo(g_hash_table_new(g_str_hash, g_str_equal), app);
  }

  g_signal_connect(styles_drop_down, "notify::selected-item",
                   G_CALLBACK(set_heading), app);
  g_signal_connect(tag_button, "clicked", G_CALLBACK(set_tags), app);

  event_controller = gtk_event_controller_key_new();
  g_signal_connect(event_controller, "key-released",
                   G_CALLBACK(event_key_released), app);
  gtk_widget_add_controller(window, event_controller);

  adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(toast_overlay), box);
  adw_application_window_set_content(ADW_APPLICATION_WINDOW(window),
                                     toast_overlay);

  gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll),
                                                   TRUE);
  gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scroll), 300);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll), 400);

  gtk_window_set_default_size(GTK_WINDOW(window), 1200, 720);
  gtk_window_present(GTK_WINDOW(window));
  app_window = GTK_WINDOW(window);

  gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window), TRUE);
}

static void
open(GApplication *self,
     gpointer files_pointer,
     gint n_files,
     G_GNUC_UNUSED gchar *hint,
     G_GNUC_UNUSED gpointer user_data)
{
  GFile **files = (GFile **) files_pointer;

  if (n_files >= 1) {
    save_current_ws(g_file_peek_path(files[0]));
  }

  g_application_activate(self);
}

int
main(int argc, char *argv[])
{
  AdwApplication *app;

  app = adw_application_new(APPLICATION_ID, G_APPLICATION_HANDLES_OPEN);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  g_signal_connect(app, "open", G_CALLBACK(open), NULL);
  g_application_run(G_APPLICATION(app), argc, argv);

  g_object_unref(app);

  return 0;
}
