#include "eapol.h"
#include "type.h"
#include "gui.h"
#include "config.h"

#include <gtk/gtk.h>
#include <unistd.h>
#include <sys/types.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define DRCOM_TITLE "drcom(黑)"

static char const *opts_text[] = {
	"选项", "选择网卡", "打开首页", "打开日志",
	"修改密码", "退出程序",
};
static char const *help_text[] = {
	"帮助", "使用说明",
};
static char const *about_text[] = {
	"关于", "版本说明", "作者",
};

/* 一些其他辅助函数 */
static int getall_ifs(char (*ifnames)[IFNAMSIZ], int *cnt);
static char *get_ifname_from_buff(char *buff);
static GdkPixbuf *create_pixbuf(gchar const *icon);
static void set_icon(GtkWindow *win, gchar const *icon);
static void add_to_linearstore(GtkWidget *list, const char *str);
static void init_list(GtkWidget *list);
static void _set_ifname(GtkTreeSelection *selection);

static gboolean is_choosed_if = FALSE;
/* 全局消息编号 */
static gint msg = 0;
static gboolean is_confed = FALSE;

/* 对相应widget设置动作 */
static void choose_if_cbk(GtkWidget *widget, gpointer father);
/*
   static void open_index_cbk(GtkWidget *widget, gpointer data);
   static void open_log_cbk(GtkWidget *widget, gpointer data);
   static void exit_daemon_cbk(GtkWidget *widget, gpointer data);
   static void manual_cbk(GtkWidget *widget, gpointer data);
   static void change_pwd_cbk(GtkWidget *widget, gpointer data);
   static void version_cbk(GtkWidget *widget, gpointer data);
   static void author_cbk(GtkWidget *widget, gpointer data);
   */
static void login_cbk(GtkWidget *widget, gpointer status_bar);
static void logoff_cbk(GtkWidget *widget, gpointer status_bar);

static GtkWidget *main_ui;
/* 一些需要bind信号的widget */
static GtkWidget *choose_if;
static GtkWidget *open_index;
static GtkWidget *open_log;
static GtkWidget *exit_daemon;
static GtkWidget *manual;
static GtkWidget *change_pwd;
static GtkWidget *version;
static GtkWidget *author;
static GtkWidget *login;
static GtkWidget *logoff;
static GtkWidget *status_bar;
static GtkWidget *unames_cmb;
static GtkWidget *rember_pwd;
static GtkWidget *ai_login;
static GtkWidget *pwd_ent;

static void drcom_add_menuber(GtkContainer *container);

/* 初始化主界面win */
int drcom_gui_init(GtkWindow *win)
{
	gtk_window_set_title(win, DRCOM_TITLE);
	gtk_widget_set_size_request(GTK_WIDGET(win), 300, 340);
	gtk_window_set_position(win, GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(win, FALSE);
	set_icon(win, ICON_PATH);
	main_ui = GTK_WIDGET(win);

	/* 主布局 分 上、中、下 */
	GtkWidget *box = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(win), box);

	/* 上： 添加菜单栏 */
	drcom_add_menuber(GTK_CONTAINER(box));

	/* 中： 主要部分 */
	GtkWidget *box_main = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), box_main, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(box_main), 20);

	GtkWidget *hbox_usr = gtk_hbox_new(FALSE, 5);	/* 账户输入框 */
	GtkWidget *uname = gtk_label_new("账  户:");
	/* TODO 实现记录账户，防止账户名过长 */
	unames_cmb = gtk_combo_box_new_with_entry();
	if (is_confed) {
		//GList* items = NULL;
		//gtk_combo_set_popdown_strings(GTK_COMBO(combo),items);
		//init_list(unames);
	}
	gtk_widget_set_size_request(GTK_WIDGET(unames_cmb), 200, -1);
	gtk_box_pack_start(GTK_BOX(box_main), hbox_usr, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_usr), uname, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox_usr), unames_cmb, FALSE, FALSE, 0);

	GtkWidget *pwd = gtk_label_new("密  码:");		/* 密码输入框 */
	pwd_ent = gtk_entry_new();
	GtkWidget *hbox_pwd = gtk_hbox_new(FALSE, 5);
	gtk_entry_set_visibility(GTK_ENTRY(pwd_ent),FALSE);
	gtk_widget_set_size_request(GTK_WIDGET(pwd_ent), 200, -1);
	gtk_box_pack_start(GTK_BOX(box_main), hbox_pwd, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_pwd), pwd, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox_pwd), pwd_ent, FALSE, FALSE, 0);
	/* 中间显示部分 */
	GtkWidget *fixed = gtk_fixed_new();
	gtk_box_pack_start(GTK_BOX(box_main), fixed, TRUE, TRUE, 0);
	rember_pwd = gtk_check_button_new_with_label("记住密码");
	gtk_fixed_put(GTK_FIXED(fixed), rember_pwd, 50, 20);
	ai_login = gtk_check_button_new_with_label("智能登录");
	gtk_fixed_put(GTK_FIXED(fixed), ai_login, 50, 50);
	GtkWidget *hbox_ip = gtk_hbox_new(FALSE, 30);
	gtk_fixed_put(GTK_FIXED(fixed), hbox_ip, 50, 80);
	GtkWidget *ip = gtk_label_new("IP:");
	GtkWidget *ipvlu = gtk_label_new("0.0.0.0");
	gtk_box_pack_start(GTK_BOX(hbox_ip), ip, FALSE, FALSE, 10);
	gtk_box_pack_start(GTK_BOX(hbox_ip), ipvlu, FALSE, FALSE, 0);
	GtkWidget *hbox_lgin = gtk_hbox_new(FALSE, 10);
	gtk_fixed_put(GTK_FIXED(fixed), hbox_lgin, 50, 110);
	GtkWidget *logtime = gtk_label_new("在线时长:");
	GtkWidget *logtimevlu = gtk_label_new("0:0:0");
	gtk_box_pack_start(GTK_BOX(hbox_lgin), logtime, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_lgin), logtimevlu, FALSE, FALSE, 0);
	/* 登录，注销 */
	GtkWidget *hbox_btns = gtk_hbox_new(FALSE, 0);
	login = gtk_button_new_with_label("登录");
	logoff = gtk_button_new_with_label("注销");
	gtk_widget_set_size_request(GTK_WIDGET(login), 50, -1);
	gtk_widget_set_size_request(GTK_WIDGET(logoff), 50, -1);
	gtk_box_pack_end(GTK_BOX(box_main), hbox_btns, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_btns), login, FALSE, FALSE, 55);
	gtk_box_pack_start(GTK_BOX(hbox_btns), logoff, FALSE, FALSE, 0);

	/* 下： 状态栏 */
	GtkWidget *hbox_status = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(box), hbox_status, FALSE, FALSE, 5);
	GtkWidget *_msg = gtk_label_new("消息: ");
	gtk_box_pack_start(GTK_BOX(hbox_status), _msg, FALSE, FALSE, 5);
	status_bar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(hbox_status), status_bar, TRUE, TRUE, 5);
	gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "-");

	gtk_widget_show_all(GTK_WIDGET(win));

	return 0;
}

int bind_all(GtkWidget *win)
{
	g_signal_connect(G_OBJECT(main_ui), "delete_event", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(choose_if), "activate", G_CALLBACK(choose_if_cbk), main_ui);
	g_signal_connect(G_OBJECT(login), "clicked", G_CALLBACK(login_cbk), status_bar);
	g_signal_connect(G_OBJECT(logoff), "clicked", G_CALLBACK(logoff_cbk), status_bar);

	return 0;
}

static void choose_if_cbk(GtkWidget *widget, gpointer father)
{
	/* 生成一个自定义对话选择框 */
	GtkWidget *dialog = gtk_dialog_new_with_buttons("选择网卡", GTK_WINDOW(father),
			GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			NULL);
	GtkWidget *list = gtk_tree_view_new();
	init_list(list);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);
	gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), list);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog),GTK_RESPONSE_OK);
	/* 根据网卡情况来填写这个对话框 */
	char ifnames[IFS_MAX][IFNAMSIZ];
	int i;
	int cnt;
	if (0 != getall_ifs(ifnames, &cnt)) {
		fprintf(stderr, "Get interfaces error\n");
		gtk_widget_destroy(dialog);
		return;
	}
	for (i = 0; i < cnt; ++i)
		add_to_linearstore(list, ifnames[i]);
	gtk_widget_show_all(dialog);
	gint result = gtk_dialog_run(GTK_DIALOG(dialog));
	if (result == GTK_RESPONSE_OK || result == GTK_RESPONSE_ACCEPT)
		_set_ifname(gtk_tree_view_get_selection(GTK_TREE_VIEW(list)));
	gtk_widget_destroy(dialog);
}

static void login_cbk(GtkWidget *widget, gpointer status_bar)
{
	char uname[UNAME_LEN];
	char pwd[PWD_LEN];
	gint stat;
	if (!is_choosed_if) {
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "请选择网卡!!");
		return;
	}
	//strncpy(uname,
	//(char*)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(unames_cmb)->entry)),
	//RECORD_LEN);
	strncpy(pwd, (char*)gtk_entry_get_text(GTK_ENTRY(pwd_ent)), RECORD_LEN);
#ifdef DEBUG
	g_print("uname: %s\n", uname);
	g_print("pwd: %s\n", pwd);
#endif
	stat = eaplogin(uname, pwd, NULL, NULL);
	switch (stat) {
	case 0:
		g_print("Login success!!\n");
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "登录成功!!");
		break;
	case 1:
		fprintf(stderr, "No this user.\n");
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "没有这个用户!!");
		break;
	case 2:
		fprintf(stderr, "Password error.\n");
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "密码错误!!");
		break;
	case 3:
		fprintf(stderr, "Timeout.\n");
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "登录超时，检查网卡!!");
		break;
	case 4:
		fprintf(stderr, "Server refuse to login.\n");
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "服务器拒绝登录!!");
		break;
	case -1:
		fprintf(stderr, "This ethernet can't be used.\n");
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "网卡不能使用!!");
		break;
	case -2:
		fprintf(stderr, "Server isn't in range.\n");
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "没在服务器服务范围!!");
		break;
	default:
		fprintf(stderr, "Other error.\n");
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "未知错误!!");
	}
}
static void logoff_cbk(GtkWidget *widget, gpointer status_bar)
{
	if (0 == eaplogoff())
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "注销成功!!");
	else
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), ++msg, "你没有登录!!");
}

/* @return: 0成功， -1 失败 */
static int getall_ifs(char (*ifnames)[IFNAMSIZ], int *cnt)
{
#ifndef _WINDOWS
#define _PATH_PROCNET_DEV "/proc/net/dev"
	int i = 0;
#define BUFF_LINE_MAX	(1024)
	char buff[BUFF_LINE_MAX];
	FILE *fd = fopen(_PATH_PROCNET_DEV, "r");
	char *name;
	if (NULL == fd) {
		perror("fopen");
		return -1;
	}
	/* _PATH_PROCNET_DEV文件格式如下,...表示后面我们不关心
	 * Inter-|   Receive ...
	 * face |bytes    packets ...
	 * eth0: 147125283  119599 ...
	 * wlan0:  229230    2635   ...
	 * lo: 10285509   38254  ...
	 */
	/* 略过开始两行 */
	fgets(buff, BUFF_LINE_MAX, fd);
	fgets(buff, BUFF_LINE_MAX, fd);
	while (0 < fgets(buff, BUFF_LINE_MAX, fd)) {
		name = get_ifname_from_buff(buff);
#ifdef DEBUG
		g_print("%s\n", name);
#endif
		if (!strncmp("lo", name, IFNAMSIZ)
				|| !strncmp("docker0", name, IFNAMSIZ))
			continue;
		strncpy(ifnames[i], name, IFNAMSIZ);
		++i;
	}
	*cnt = i;
	fclose(fd);
	return 0;
#else
#endif
}

static char *get_ifname_from_buff(char *buff)
{
	char *s;
	while (isspace(*buff))
		++buff;
	s = buff;
	while (':' != *buff && '\0' != *buff)
		++buff;
	*buff = '\0';
	return s;
}

/* 把png图片转换为pixbuf数据 */
static GdkPixbuf *create_pixbuf(gchar const *icon)
{
	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(icon, &err);
	if (NULL == pixbuf) {
		fprintf(stderr, "%s\n", err->message);
		g_error_free(err);
	}

	return pixbuf;
}

/* 对win设置图标 */
static void set_icon(GtkWindow *win, gchar const *icon)
{
	gtk_window_set_icon(win, create_pixbuf(icon));
}

static void add_to_linearstore(GtkWidget *list, const char *str)
{
	GtkListStore *store;
	GtkTreeIter iter;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, str, -1);
}

static void init_list(GtkWidget *list)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("List Iterms",
			renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
	store = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));
	g_object_unref(store);
}

static void _set_ifname(GtkTreeSelection *selection)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	char *value;

	if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection),
				&model, &iter)) {
		gtk_tree_model_get(model, &iter, 0, &value, -1);
#ifdef DEBUG
		g_print("_set_ifname: %s\n", value);
#endif
		setifname(value);
		is_choosed_if = TRUE;
		g_free(value);
	}
}

static void drcom_add_menuber(GtkContainer *container)
{
	GtkWidget *opts[ARRAY_SIZE(opts_text)];
	GtkWidget *helps[ARRAY_SIZE(help_text)];
	GtkWidget *abouts[ARRAY_SIZE(about_text)];

	GtkWidget *menubar = gtk_menu_bar_new();
	gtk_box_pack_start(GTK_BOX(container), menubar, FALSE, FALSE, 0);
	GtkWidget *optmenu = gtk_menu_new();	/* "选项" */
	GtkWidget *sep = gtk_separator_menu_item_new();
	int i;
	for (i = 0; i < ARRAY_SIZE(opts_text); ++i)
		opts[i] = gtk_menu_item_new_with_label(opts_text[i]);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(opts[0]), optmenu);
	for (i = 1; i < ARRAY_SIZE(opts); ++i) {
		gtk_menu_shell_append(GTK_MENU_SHELL(optmenu), opts[i]);
		if (i == ARRAY_SIZE(opts_text)-2)
			gtk_menu_shell_append(GTK_MENU_SHELL(optmenu), sep);
	}
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), opts[0]);
	GtkWidget *helpmenu = gtk_menu_new();	/* "帮助" */
	for (i =0; i < ARRAY_SIZE(help_text); ++i)
		helps[i] = gtk_menu_item_new_with_label(help_text[i]);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(helps[0]), helpmenu);
	for (i = 1; i < ARRAY_SIZE(helps); ++i)
		gtk_menu_shell_append(GTK_MENU_SHELL(helpmenu), helps[i]);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), helps[0]);
	GtkWidget *aboutmenu = gtk_menu_new();	/* "关于" */
	for (i = 0; i < ARRAY_SIZE(about_text); ++i)
		abouts[i] = gtk_menu_item_new_with_label(about_text[i]);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(abouts[0]), aboutmenu);
	for (i = 1; i < ARRAY_SIZE(abouts); ++i)
		gtk_menu_shell_append(GTK_MENU_SHELL(aboutmenu), abouts[i]);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), abouts[0]);
	/* 指定bind的widget */
	choose_if = opts[1];
	open_index = opts[2];
	open_log = opts[3];
	change_pwd = opts[4];
	exit_daemon = opts[5];
	manual = helps[1];
	version = abouts[1];
	author = abouts[2];
}
