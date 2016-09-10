#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include "eapol.h"
#include "config.h"
#include "wrap_eapol.h"
#ifdef LINUX
# define _PATH_MAX  PATH_MAX
# define PATH_SEP   '/'
#elif defined(WINDOWS)
# include <windows.h>
# define _PATH_MAX  MAX_PATH
# define PATH_SEP   '\\'
#endif

/*
 * 读取配置文件，路径在CONF_PATH里，默认在当前目录下
 * @return: 0: 成功
 *          -1: 没有配置文件
 *          -2: 配置文件格式错误
 */
static int getconf(char *uname, char *pwd);
static void help(int argc, char **argv);
/*
 * 获取程序所在的目录
 * exedir: 返回目录
 * @return: 0: 成功
 *         !0: 失败
 */
static int getexedir(char *exedir);

int main(int argc, char **argv)
{
	char uname[UNAME_LEN];
	char pwd[PWD_LEN];

	char islogoff = 0;

	if (argc == 2) {
		if (0 == strcmp("-l", argv[1]) || 0 == (strcmp("-L", argv[1])))
			islogoff = 1;
		else if (0 == strcmp("-h", argv[1])) {
			help(argc, argv);
			return 0;
		}
	}
	if (0 != getconf(uname, pwd)) {
		fprintf(stderr, "Not configure.\n");
		return 1;
	}
	if (islogoff) {
		eaplogoff();
		return 0;
	}

    switch (try_smart_login(uname, pwd)) {
        case 0: printf("Login success!!\n"); break;
        case 1: fprintf(stderr, "No this user.\n"); break;
        case 2: fprintf(stderr, "Password error.\n"); break;
        case 3: fprintf(stderr, "Timeout.\n"); break;
        case 4: fprintf(stderr, "Server refuse to login.\n"); break;
        case -1: fprintf(stderr, "This ethernet can't be used.\n"); break;
        case -2: fprintf(stderr, "Server isn't in range.\n"); break;
        case -3: fprintf(stderr, "Can't found ethernet.\n"); break;
        default: fprintf(stderr, "Other error.\n");
    }
    return 0;
}

static int getconf(char *_uname, char *_pwd)
{
    char uname[MAX(RECORD_LEN, UNAME_LEN)];
    char pwd[MAX(RECORD_LEN, PWD_LEN)];
    char configpath[_PATH_MAX+1];
    if (getexedir(configpath)) return -1;
    strcat(configpath, CONF_PATH);
#ifdef DEBUG
    printf("configfile path: %s\n", configpath);
#endif
    FILE *conf = fopen(configpath, "r");
    if (NULL == conf) {
        perror("drcomrc");
        return -1;
    }
    if (0 != getvalue(conf, "uname", uname)) return -2;
    if (0 != getvalue(conf, "pwd", pwd)) return -2;
    strncpy(_uname, uname, UNAME_LEN);
    strncpy(_pwd, pwd, PWD_LEN);
#ifdef DEBUG
    printf("uname: %s\n", _uname);
    printf("pwd: %s\n", _pwd);
#endif
    fclose(conf);

    return 0;
}

static void help(int argc, char **argv)
{
    (void)argc;
    printf("Usage: %s -r|h|l\n", argv[0]);
    printf("      -r: relogin.\n");
    printf("      -l: logoff.\n");
    printf("      -h: show this help page.\n");
    printf("NOTE: you must need root to login.\n");
}
static int getexedir(char *exedir)
{
#ifdef LINUX
    int cnt = readlink("/proc/self/exe", exedir, _PATH_MAX+1);
#elif defined(WINDOWS)
    int cnt = GetModuleFileName(NULL, exedir, _PATH_MAX+1);
#endif
    if (cnt < 0 || cnt > _PATH_MAX)
        return -1;
#ifdef DEBUG
    printf("exedir: %s\n", exedir);
#endif
    char *end = strrchr(exedir, PATH_SEP);
    if (!end) return -1;
    *(end+1) = '\0';
#ifdef DEBUG
    printf("exedir: %s\n", exedir);
#endif
    return 0;
}
