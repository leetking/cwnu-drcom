#include "eapol.h"

#include <net/if.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define RECORD_LEN	(16)
#define READ_BUFF	(82)

/*
 * 读取配置文件，路径在CONF_PATH里，默认在当前目录下
 * @return: 0: 成功
 *          -1: 没有配置文件
 *          -2: 配置文件格式错误
 */
static int getconf(char *uname, char *pwd, char *ifname);
/*
 * 读取配置文件里的值
 * key: 待读取的关键字
 * value: 读取的值存储在value里
 * @reutrn: 0: 成功
 *          -1: 配置文件格式错误
 *          -2: 没有这个key
 *          -3: key或value长度大于RECORD_LEN
 */
static int getvalue(FILE *conf, char const *key, char *value);

int main(int argc, char **argv)
{
	char uname[UNAME_LEN];
	char pwd[PWD_LEN];
	char ifname[IFNAMSIZ];

	if (0 != getconf(uname, pwd, ifname)) {
		fprintf(stderr, "Not configure.\n");
		return 1;
	}
	setifname(ifname);
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

static int getconf(char *uname, char *pwd, char *ifname)
{
#ifdef DEBUG
	printf("file path: %s\n", CONF_PATH);
#endif
	FILE *conf = fopen(CONF_PATH, "r");
	if (NULL == conf) {
		perror("drcomrc");
		return -1;
	}
	if (0 != getvalue(conf, "uname", uname)) return -2;
	if (0 != getvalue(conf, "pwd", pwd)) return -2;
	if (0 != getvalue(conf, "ethif", ifname)) return -2;
#ifdef DEBUG
	printf("uname: %s\n", uname);
	printf("pwd: %s\n", pwd);
	printf("ifname: %s\n", ifname);
#endif

	return 0;
}

static int getvalue(FILE *conf, char const *key, char *value)
{
	char readbuff[READ_BUFF];
	char keybuff[RECORD_LEN];
	char *pb, *pkey, *pval;
	fseek(conf, 0L, SEEK_SET);
	while (NULL != fgets(readbuff, READ_BUFF, conf)) {
		pb = readbuff;
		pkey = keybuff;
		pval = value;
		/* 是否是注释 */
		while (isspace(*pb))
			++pb;
		if ('#' == *pb || '\0' == *pb)
			continue;
		/* 解析变量 */
		while ('\0' != *pb && ('_' == *pb || isalnum(*pb))) {
			if (pkey >= keybuff+RECORD_LEN)
				return -3;
			*pkey++ = tolower(*pb++);
		}
		*pkey = '\0';
		/* 等号 */
		while ('=' != *pb && '\0' != *pb)
			++pb;
		if ('\0' == *pb)
			continue;
		while (isspace(*++pb))
			;
		if ('\0' == *pb)
			continue;
		/* value */
		if ('"' == *pb)
			++pb;
		while ('\0' != *pb && '"' != *pb) {
			if (pval >= value+RECORD_LEN)
				return -3;
			*pval++ = tolower(*pb++);
		}
		*pval = '\0';
		if (0 == strcmp(key, keybuff)) {
#ifdef DEBUG
			printf("getvalue: %s-%s\n", key, value);
#endif
			return 0;
		}
	}
	fclose(conf);
	return -2;
}
