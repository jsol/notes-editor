#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>

GtkWidget *
get_framed_content(GtkWidget *menu, GtkWidget *content)
{
  GtkWidget *flap;

  g_return_val_if_fail(menu != NULL, NULL);
  g_return_val_if_fail(content != NULL, NULL);

  flap = adw_flap_new();

  adw_flap_set_flap(ADW_FLAP(flap), menu);
  adw_flap_set_content(ADW_FLAP(flap), content);

  return flap;
}