#include "eapol.h"
#include "common.h"
#include "gui.h"
#include "config.h"

#include <gtk/gtk.h>
#include <unistd.h>
#include <sys/types.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define DRCOM_TITLE     "drcom"
#define MENU_TEXT_MAX   48
#define NO_SUBMENU      ((void*)1)
#define IFDESCSIZ       126

static gboolean chose_if = FALSE;
/* 全局消息编号 */
static gint msg = 0;
static gboolean is_confed = FALSE;

static void add_to_linearstore(GtkWidget *list, const char *str);
static void init_list(GtkWidget *list);
static void _set_ifname(GtkTreeSelection *selection);

static void set_icon(GtkWindow *win, gchar const *icon);


/* 对相应widget设置动作 */
static void choose_if_cbk(GtkWidget *widget, gpointer father);
/*
static void open_index_cbk(GtkWidget *widget, gpointer data);
static void open_log_cbk(GtkWidget *widget, gpointer data);
static void exit_daemon_cbk(GtkWidget *widget, gpointer data);
static void manual_cbk(GtkWidget *widget, gpointer data);
static void change_password_cbk(GtkWidget *widget, gpointer data);
static void version_cbk(GtkWidget *widget, gpointer data);
static void author_cbk(GtkWidget *widget, gpointer data);
*/
static void login_cbk(GtkWidget *widget, gpointer statubar);
static void logoff_cbk(GtkWidget *widget, gpointer statubar);

static GtkWidget *main_ui;
/* 一些需要bind信号的widget */
static GtkWidget *choose_if;
static GtkWidget *open_index;
static GtkWidget *open_log;
static GtkWidget *exit_daemon;
static GtkWidget *password_ent;

static GtkWidget *uinput, *pinput;
static GtkWidget *statubar;
static GtkWidget *login;
static GtkWidget *logoff;

static GtkWidget* drcom_menubar_new(void);
static GtkWidget* drcom_inputs_new(void);
static GtkWidget* drcom_toggles_new(void);
static GtkWidget* drcom_buttons_new(void);
static GtkWidget* drcom_statusbar_new(void);

static int try_smart_login(char const *username, char const *password);


/* 初始化主界面win */
int drcom_gui_init(GtkWindow *win)
{
    gtk_window_set_title(win, DRCOM_TITLE);
    gtk_widget_set_size_request(GTK_WIDGET(win), 300, 340);
    gtk_window_set_position(win, GTK_WIN_POS_CENTER);
    //gtk_window_set_resizable(win, FALSE);
    set_icon(win, ICON_PATH);
    gtk_container_set_border_width(GTK_CONTAINER(win), 0);

    main_ui = GTK_WIDGET(win);

    /* 主布局 分 上、中、下 */
    GtkWidget *box = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(win), box);

    /* 上： 添加菜单栏 */
    gtk_box_pack_start(GTK_BOX(box), drcom_menubar_new(), FALSE, FALSE, 0);
    /* 账户和密码输入框 */
    gtk_box_pack_start(GTK_BOX(box), drcom_inputs_new(), FALSE, FALSE, 0);
    /* 确认框 */
    gtk_box_pack_start(GTK_BOX(box), drcom_toggles_new(), FALSE, FALSE, 0);
    /* 登录，注销按钮 */
    gtk_box_pack_start(GTK_BOX(box), drcom_buttons_new(), FALSE, FALSE, 0);

    /* 下： 状态栏 */
    gtk_box_pack_end(GTK_BOX(box), drcom_statusbar_new(), FALSE, FALSE, 0);

    /* add gtkrc */
    //gtk_rc_parse("gtkrc");
    gtk_widget_show_all(GTK_WIDGET(win));

    return 0;
}


int bind_all(GtkWidget *win)
{
    g_signal_connect(G_OBJECT(main_ui), "delete_event", G_CALLBACK(gtk_main_quit), NULL);
    //g_signal_connect(G_OBJECT(choose_if), "activate", G_CALLBACK(choose_if_cbk), main_ui);

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
    ifname_t ifs[IFS_MAX];
    int ifs_max = IFS_MAX;
    if (0 >= getall_ifs(ifs, &ifs_max)) {
        _M("Get interfaces error\n");
        /* TODO 显示一个错误 */
        gtk_widget_destroy(dialog);
        return;
    }
    for (int i = 0; i < ifs_max; ++i) {
        _D("ifs[%d].name: %s\n", i, ifs[i].name);
        add_to_linearstore(list, ifs[i].name);
    }
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_OK || result == GTK_RESPONSE_ACCEPT)
        _set_ifname(gtk_tree_view_get_selection(GTK_TREE_VIEW(list)));
    gtk_widget_destroy(dialog);
}


static void login_cbk(GtkWidget *widget, gpointer statubar)
{
    /* TODO 采用异步登录 */
    char username[USERNAME_LEN+1];
    char password[PASSWORD_LEN+1];
    strncpy(username, (char*)gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(uinput)),
            USERNAME_LEN+1);
    strncpy(password, (char*)gtk_entry_get_text(GTK_ENTRY(pinput)), PASSWORD_LEN);
    _D("username: %s\n", username);
    _D("password: %s\n", password);
    if (!strcmp("", username) || !strcmp("", password)) {
        _M("No username or password\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "必须输入用户名和密码!!");
        return;
    }

    if (!chose_if) {
        /* 需要手动选择网卡 */
        if (0 != try_smart_login(username, password)) {
            /* TODO 显示网卡错误弹出窗口 */
            _M("can't choose net interface cleverly!\n");
        } else {
            goto _login_sucess;
        }
        return ;
    }
    int stat = eaplogin(username, password);
    /* 选择了网卡，但是登录失败 */
    switch (stat) {
    case 1:
        _M("No this user.\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "没有这个用户!!");
        break;
    case 2:
        _M("Password error.\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "密码错误!!");
        break;
    case 3:
        _M("Timeout.\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "登录超时，检查网卡!!");
        break;
    case 4:
        _M("Server refuse to login.\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "服务器拒绝登录!!");
        break;
    case -1:
        _M("This ethernet can't be used.\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "网卡不能使用!!");
        break;
    case -2:
        _M("Server isn't in range.\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "没在服务器服务范围!!");
        break;
    default:
        _M("Other error.\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "未知错误!!");
    case 0:
_login_sucess:
        _M("Login success!!\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "登录成功!!");
        break;
    }
}


static void logoff_cbk(GtkWidget *widget, gpointer statubar)
{
    if (0 == eaplogoff()) {
        _M("logoff sucess!\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "注销成功!!");
    } else {
        _M("you can't login!\n");
        gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "你没有登录!!");
    }
}


#ifdef LINUX
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
#endif /* LINUX */


static int is_filter(char const *ifname)
{
    /* 过滤掉无线，虚拟机接口等 */
    char const *filter[] = {
        /* windows */
        "Wireless", "Microsoft",
        "Virtual",
        /* linux */
        "lo", "wlan", "vboxnet",
    };
    for (int i = 0; i < ARRAY_SIZE(filter); ++i) {
        if (strstr(ifname, filter[i]))
            return 1;
    }
    return 0;
}


/* 对win设置图标 */
static void set_icon(GtkWindow *win, gchar const *icon)
{
    GError *err = NULL;
    /* 把png图片转换为pixbuf数据 */
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(icon, &err);
    if (!pixbuf) {
        _M("%s\n", err->message);
        g_error_free(err);
    }
    gtk_window_set_icon(win, pixbuf);
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
        _D("_set_ifname: %s\n", value);
        setifname(value);
        chose_if = TRUE;
        g_free(value);
    }
}


typedef void (*cbkfun_t)(GtkWidget *w, gpointer father);
typedef struct {
    void *icon;         /* TODO has icon? */
    char text[MENU_TEXT_MAX]; /* text name */
    GtkWidget *item;
    cbkfun_t action;    /* 对应回调函数, NULL 表示没有回调函数 */
    gpointer *data;     /* 回调函数操作的数据的二级指针, NULL表示没有数据 */
    GtkWidget *submenu; /* 子菜单, NULL: 没有, NO_SUBMENU: 有子菜单 */
    gboolean visual;    /* 是否可操作 */
    gboolean clicked;   /* 是否选中 */
} menu_item_t;

#define SEP_LINE "---"    /* --- 表示分割栏 */
/* itmes 是 menu_opt_t 类型的 */
#define INIT_MENU_ITEMS(items)   \
    do { \
        for (int i = 0; i < ARRAY_SIZE(items); ++i) { \
            if (!strncmp(SEP_LINE, items[i].text, ARRAY_SIZE(SEP_LINE)-1)) { \
                items[i].item = gtk_separator_menu_item_new(); \
            } else { \
                items[i].item = gtk_menu_item_new_with_label(items[i].text); \
                if (NULL != items[i].submenu) { \
                    items[i].submenu = gtk_menu_new(); \
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(items[i].item), items[i].submenu); \
                } \
                if (NULL != items[i].action) { \
                    g_signal_connect(G_OBJECT(items[i].item), "activate", \
                            G_CALLBACK(items[i].action), items[i].data?*items[i].data:NULL); \
                } \
            } \
        } \
    } while(0)
/* menu_shell 是menubar或submenu */
#define APPEND_ITEMS(menu_shell, items) \
    do { \
        for (int i = 0; i < ARRAY_SIZE(items); ++i) { \
            gtk_menu_shell_append(GTK_MENU_SHELL(menu_shell), items[i].item); \
        } \
    } while(0)


static GtkWidget* drcom_menubar_new(void)
{
    menu_item_t menus[] = {
        /*icon, text, widget,  fun, data, submenu, visual, clicked */
        {NULL, "选项", NO_SUBMENU, NULL, NULL, NO_SUBMENU, TRUE, FALSE},
        {NULL, "高级", NO_SUBMENU, NULL, NULL, NO_SUBMENU, TRUE, FALSE},
        {NULL, "帮助", NO_SUBMENU, NULL, NULL, NO_SUBMENU, TRUE, FALSE},
    };
    menu_item_t opts[] = {
        {NULL, "学校首页", NO_SUBMENU, NULL, NULL, NULL, TRUE, FALSE},
        {NULL, "打开日志", NO_SUBMENU, NULL, NULL, NULL, TRUE, FALSE},
        {NULL, "修改密码", NO_SUBMENU, NULL, NULL, NULL, TRUE, FALSE},
        {NULL, SEP_LINE,   NO_SUBMENU, NULL, NULL, NULL, TRUE, FALSE},
        {NULL, "退出程序", NO_SUBMENU, (cbkfun_t)gtk_main_quit, NULL, NULL, TRUE, FALSE},
    };
    menu_item_t advance[] = {
        {NULL, "选择网卡", NO_SUBMENU, choose_if_cbk, NULL, NULL, TRUE, FALSE},
        {NULL, "有皮肤?",  NO_SUBMENU, NULL, NULL, NULL, TRUE, FALSE},
    };
    menu_item_t helps[] = {
        {NULL, "使用说明", NO_SUBMENU, NULL, NULL, NULL, TRUE, FALSE},
        {NULL, "关于",     NO_SUBMENU, NULL, NULL, NULL, TRUE, FALSE},
    };

    GtkWidget *menubar = gtk_menu_bar_new();
    INIT_MENU_ITEMS(menus);
    APPEND_ITEMS(menubar, menus);
    INIT_MENU_ITEMS(opts);
    APPEND_ITEMS(menus[0].submenu, opts);
    INIT_MENU_ITEMS(advance);
    APPEND_ITEMS(menus[1].submenu, advance);
    INIT_MENU_ITEMS(helps);
    APPEND_ITEMS(menus[2].submenu, helps);
    return menubar;
}


static GtkWidget* drcom_inputs_new(void)
{
    GtkWidget *centeralign = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_container_set_border_width(GTK_CONTAINER(centeralign), 10);

    GtkWidget *box = gtk_table_new(2, 2, FALSE);
    gtk_container_add(GTK_CONTAINER(centeralign), box);
    gtk_table_set_row_spacings(GTK_TABLE(box), 5);

    GtkWidget *username = gtk_label_new("账  户:"); /* "账户" */
    uinput = gtk_combo_box_text_new_with_entry();
    gtk_widget_set_size_request(GTK_WIDGET(uinput), 200, -1);
    GtkWidget *password = gtk_label_new("密  码:");	/* "密码" */
    pinput = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(pinput),FALSE);
    gtk_widget_set_size_request(GTK_WIDGET(pinput), 200, -1);
    gtk_table_attach_defaults(GTK_TABLE(box), username, 0, 1, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(box), uinput, 1, 2, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(box), password, 0, 1, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(box), pinput, 1, 2, 1, 2);

    return centeralign;
}


static GtkWidget* drcom_toggles_new(void)
{
    GtkWidget *centeralign = gtk_alignment_new(0.5, 0.5, 0, 0);
    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(centeralign), vbox);

    GtkWidget *remem_password = gtk_check_button_new_with_label("记住密码");
    GtkWidget *autologin = gtk_check_button_new_with_label("自动登录");
    gtk_box_pack_start(GTK_BOX(vbox), remem_password, TRUE, TRUE, 10);
    gtk_box_pack_start(GTK_BOX(vbox), autologin, TRUE, TRUE, 0);

    return centeralign;
}


static GtkWidget* drcom_buttons_new(void)
{
    GtkWidget *centeralign = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_container_set_border_width(GTK_CONTAINER(centeralign), 10);
    GtkWidget *box = gtk_hbox_new(FALSE, 50);
    gtk_container_add(GTK_CONTAINER(centeralign), box);

    login = gtk_button_new_with_label("登录");
    logoff = gtk_button_new_with_label("注销");
    gtk_widget_set_size_request(GTK_WIDGET(login), 50, -1);
    gtk_widget_set_size_request(GTK_WIDGET(logoff), 50, -1);
    gtk_box_pack_start(GTK_BOX(box), login, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), logoff, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(login), "clicked", G_CALLBACK(login_cbk), statubar);
    g_signal_connect(G_OBJECT(logoff), "clicked", G_CALLBACK(logoff_cbk), statubar);

    return centeralign;
}


static GtkWidget* drcom_statusbar_new(void)
{
    GtkWidget *box = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), gtk_label_new("消息: "), FALSE, FALSE, 5);
    statubar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(box), statubar, TRUE, TRUE, 5);
    gtk_statusbar_push(GTK_STATUSBAR(statubar), ++msg, "-");

    return box;
}


static int try_smart_login(char const *username, char const *password)
{
    ifname_t ifs[IFS_MAX];
    int ifs_max = IFS_MAX;
    if (0 >= getall_ifs(ifs, &ifs_max)) return -1;
    for (int i = 0; i < ifs_max; ++i) {
        _M("%d. try interface (%s) to login\n", i, ifs[i].name);
        setifname(ifs[i].name);
        if (0 == eaplogin(username, password))
            return 0;
    }
    return 1;
}
