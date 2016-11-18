#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include "eapol.h"
#include "config.h"
#include "wrap_eapol.h"
#include "common.h"
#include "drcom.h"
#include "dhcp.h"

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
	char ifname[IFNAMSIZ];

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
			char msgfileback[] = "./drcom-login.msg.back";
			char msg[EXE_PATH_MAX+sizeof(msgfile)];
			char msgback[EXE_PATH_MAX+sizeof(msgfileback)];
			if (0 != getexedir(msg)) {
				_M("Cant get execute program directory.\n");
				return 1;
			}
			strncat(msg, msgfile, sizeof(msgfile));
			if (0 != getexedir(msgback)) {
				_M("Cant get execute program directory.\n");
				return 1;
			}
			strncat(msgback, msgfileback, sizeof(msgfileback));
			if (0 != copy(msg, msgback)) {
				_M("WARN: Cant backup msgfile.\n");
			}
			_M("Now, save msg to file `drcom-login.msg`\n");
			if (NULL == freopen(msg, "w", stderr)) {
				_M("WARN: Redirect `stderr` error!\n");
			}
			if (NULL == freopen(msg, "w", stdout)) {
				_M("WARN: Redirect `stdout` error!\n");
			}
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
		_M("Not configure.\n");
		return 1;
	}

#ifdef WINDOWS
	/* windows下作为心跳进程运行的代码 */
	if (iskpalv) {
		if (0 != eap_daemon(kpalv_if)) {
			/* 异常退出 */
			_M("[EAP:ERROR] Create daemon process to keep alive error!\n");
			return 1;
		}
		/* 正常退出 */
		return 0;
	}
#endif

	if (islogoff) {
		try_smart_logoff();
		return 0;
	}

	/* eap 认证 */
	switch (try_smart_login(uname, pwd)) {
	case 0: _M("[EAP] Login success!!\n"); break;
	case 1: _M("[EAP] No this user.\n"); return 1;
	case 2: _M("[EAP] Password error.\n"); return 2;
	case 3: _M("[EAP] Timeout.\n"); return 3;
	case 4: _M("[EAP] Server refuse to login.\n"); return 4;
	case -1: _M("[EAP] This ethernet can't be used.\n"); return 5;
	case -2: _M("[EAP] Server isn't in range.\n"); return 6;
	case -3: _M("[EAP] Can't found useable ethernet.\n"); return 7;
	default: _M("[EAP] Other error.\n"); return 8;
	}

	/* TODO 使用 dhcp 获取ip, 并绑定到ifname这个接口上 */
	dhcp_t dhcp;
	char const *_ifname = getenv("DRCOM_IFNAME");
	if (NULL == _ifname) {
		_M("[ERROR] Cant get ifname from environment.\n");
		return 9;
	}
	strncpy(ifname, _ifname, IFNAMSIZ);
	_D("getenv: %s\n", ifname);
	dhcp_setif(&dhcp, ifname);

	_M("[DHCP] Get host ip from dhcp server...\n");
	if (0 != dhcp_run(&dhcp)) {
		_M("[ERROR] Get locale ip error. Can't connect to server! Network is offline.\n");
		return 11;
	}

	/* drcom 心跳 */
	ipv4_t ip;
	dhcp_getsip(&dhcp, AF_INET, &ip);
	char tmp[30];
	inet_ntop(AF_INET, &ip, tmp, 30);
	_D("serip: %s\n", tmp);
	drcom_setifname(ifname);
	drcom_setserip(tmp);
	switch (drcom_login(uname, pwd)) {
	case 0: _M("[DRCOM] Identity success!\n"); break;
	default: _M("[DRCOM] Other error.\n"); return 10;
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
	_M("Usage: %s -r|h|l\n", argv[0]);
	_M("      if want to login, it without argument. e.g. %s\n", argv[0]);
	_M("      -r: relogin.\n");
	_M("      -l: logoff.\n");
	_M("      -h: show this help page.\n");
	_M("      -d: dump debug msg to `drcum-login.msg` at current directory.\n");
	_M("NOTE: you must need root to login.\n");
	_M("version: %s.\n", VERSION);
	_M("(C) leetking <li_Tking@163.com>\n");
}

