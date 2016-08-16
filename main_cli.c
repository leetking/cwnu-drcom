#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "eapol.h"
#include "config.h"

/*
 * 读取配置文件，路径在CONF_PATH里，默认在当前目录下
 * @return: 0: 成功
 *          -1: 没有配置文件
 *          -2: 配置文件格式错误
 */
static int getconf(char *uname, char *pwd, char *ifname);
static void help(int argc, char **argv);

int main(int argc, char **argv)
{
	char uname[UNAME_LEN];
	char pwd[PWD_LEN];
	char ifname[IFNAMSIZ];

	char islogoff = 0;

	if (argc == 2) {
		if (0 == strcmp("-l", argv[1]) || 0 == (strcmp("-L", argv[1])))
			islogoff = 1;
		else if (0 == strcmp("-h", argv[1])) {
			help(argc, argv);
			return 0;
		}
	}
	if (0 != getconf(uname, pwd, ifname)) {
		fprintf(stderr, "Not configure.\n");
		return 1;
	}
	setifname(ifname);
	if (islogoff) {
		eaplogoff();
		return 0;
	}

	switch (eaplogin(uname, pwd, NULL, NULL)) {
	case 0: printf("Login success!!\n"); break;
	case 1: fprintf(stderr, "No this user.\n"); break;
	case 2: fprintf(stderr, "Password error.\n"); break;
	case 3: fprintf(stderr, "Timeout.\n"); break;
	case 4: fprintf(stderr, "Server refuse to login.\n"); break;
	case -1: fprintf(stderr, "This ethernet can't be used.\n"); break;
	case -2: fprintf(stderr, "Server isn't in range.\n"); break;
	default: fprintf(stderr, "Other error.\n");
	}
	return 0;
}

static int getconf(char *_uname, char *_pwd, char *_ifname)
{
	char uname[MAX(RECORD_LEN, UNAME_LEN)];
	char pwd[MAX(RECORD_LEN, PWD_LEN)];
	char ifname[MAX(RECORD_LEN, IFNAMSIZ)];
#ifdef DEBUG
	printf("configfile path: %s\n", CONF_PATH);
#endif
	FILE *conf = fopen(CONF_PATH, "r");
	if (NULL == conf) {
		perror("drcomrc");
		return -1;
	}
	if (0 != getvalue(conf, "uname", uname)) return -2;
	if (0 != getvalue(conf, "pwd", pwd)) return -2;
	if (0 != getvalue(conf, "ethif", ifname)) return -2;
	strncpy(_uname, uname, UNAME_LEN);
	strncpy(_pwd, pwd, PWD_LEN);
	strncpy(_ifname, ifname, IFNAMSIZ);
#ifdef DEBUG
	printf("uname: %s\n", _uname);
	printf("pwd: %s\n", _pwd);
	printf("ifname: %s\n", _ifname);
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
