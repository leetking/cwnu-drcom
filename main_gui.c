#include "gui.h"

#include <gtk/gtk.h>

int main(int argc, char **argv)
{
	GtkWidget *window;

	gtk_init(&argc, &argv);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	//getconf();
	drcom_gui_init(GTK_WINDOW(window));

	bind_all(window);

	gtk_main();
	return 0;
}
