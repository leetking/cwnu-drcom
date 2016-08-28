#ifndef _GUI_H
#define _GUI_H

#include <gtk/gtk.h>

/* 初始化主界面win */
int drcom_gui_init(GtkWindow *win);
/* 绑定信号，对win上的一些控件 */
int bind_all(GtkWidget *win);

#endif
