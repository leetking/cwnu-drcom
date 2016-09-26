#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "eapol.h"
#include "config.h"
#include "wrap_eapol.h"
#include "common.h"

/*
 * 读取配置文件，路径在CONF_PATH里，默认在当前目录下
 * @return: 0: 成功
 *          -1: 没有配置文件
 *          -2: 配置文件格式错误
 */
static int getconf(char *uname, char *pwd);
static void help(int argc, char **argv);

int main(int argc, char **argv)
{
	char uname[UNAME_LEN];
	char pwd[PWD_LEN];

	char islogoff = 0;
#ifdef WINDOWS
	char iskpalv = 0;
	char kpalv_if[IFNAMSIZ];
#endif

	int i;
	for (i = 1; i < argc; ++i) {
		if (0 == strncmp("-l", argv[i], 2) || 0 == (strncmp("-L", argv[i], 2))) {
			islogoff = 1;
		} else if (0 == strncmp("-h", argv[i], 2)) {
			help(argc, argv);
			return 0;
		} else if (0 == strncmp("-d", argv[i], 2)) {
			char msgfile[] = "./drcom-login.msg";
			char msg[EXE_PATH_MAX+sizeof(msgfile)];
			if (0 != getexedir(msg)) {
				_M("Cant get execute program directory.\n");
				return 1;
			}
			_M("Now, save msg to file `drcom-login.msg`\n");
			strncat(msg, msgfile, sizeof(msgfile));
			freopen(msg, "w", stderr);
			freopen(msg, "w", stdout);
		}
#ifdef WINDOWS
		if (0 == strcmp("-k", argv[i])) {
			/*
			 * -k选项让这个程序作为心跳程序运行。
			 * -k ifname
			 * e.g. -k \Device\NPF_{AEDD3BFA-33D3-4B29-B1FC-0B82C65E42D3}
			 * 对于外部使用这个命令行，不能让他们知道，
			 * 这个只有在windows下才有效
			 */
			if (NULL == argv[i+1])
				goto _cant_eap_daemon;
			strncpy(kpalv_if, argv[i+1], IFNAMSIZ);
			_D("kpalv_if: %s\n", kpalv_if);
			iskpalv = 1;
		}
#endif
	}

	if (0 != getconf(uname, pwd)) {
		fprintf(stderr, "Not configure.\n");
		return 1;
	}

#ifdef WINDOWS
	/* windows下作为心跳进程运行的代码 */
	if (iskpalv) {
		if (0 != eap_daemon(kpalv_if))
			goto _cant_eap_daemon;

		/* 正常退出 */
		return 0;
		/* 异常退出 */
_cant_eap_daemon:
		_M("[ERROR] Create daemon process to keep alive error!\n");
		return 1;
	}
#endif

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
	char configpath[EXE_PATH_MAX+sizeof(CONF_PATH)];
	if (getexedir(configpath)) return -1;
	strcat(configpath, CONF_PATH);
	_D("configfile path: %s\n", configpath);
	FILE *conf = fopen(configpath, "r");
	if (NULL == conf) {
		perror("drcomrc");
		return -1;
	}
	if (0 != getvalue(conf, "uname", uname)) return -2;
	if (0 != getvalue(conf, "pwd", pwd)) return -2;
	strncpy(_uname, uname, UNAME_LEN);
	strncpy(_pwd, pwd, PWD_LEN);
	_D("uname: %s\n", _uname);
	_D("pwd: %s\n", _pwd);
	fclose(conf);

	return 0;
}

static void help(int argc, char **argv)
{
	(void)argc;
	_M("Usage: %s -r|h|l\n", argv[0]);
	_M("      -r: relogin.\n");
	_M("      -l: logoff.\n");
	_M("      -h: show this help page.\n");
	_M("      -d: dump debug msg to `drcum-login.msg` at current directory.\n");
	_M("NOTE: you must need root to login.\n");
}

