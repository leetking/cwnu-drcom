/*
 * 一些通用的代码
 */
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include "common.h"

#ifdef LINUX
# include <unistd.h>
# define PATH_SEP   '/'
#elif defined(WINDOWS)
# include <windows.h>
# include <pcap.h>
# define PATH_SEP   '\\'
#endif


extern int getexedir(char *exedir)
{
#ifdef LINUX
    int cnt = readlink("/proc/self/exe", exedir, EXE_PATH_MAX);
#elif defined(WINDOWS)
    int cnt = GetModuleFileName(NULL, exedir, EXE_PATH_MAX);
#endif
    if (cnt < 0 || cnt >= EXE_PATH_MAX)
        return -1;
    _D("exedir: %s\n", exedir);
    char *end = strrchr(exedir, PATH_SEP);
    if (!end) return -1;
    *(end+1) = '\0';
    _D("exedir: %s\n", exedir);
    return 0;
}

extern int mac_equal(uchar const *mac1, uchar const *mac2)
{
	uint32 a1 = *(uint32*)mac1;
	uint32 a2 = *(uint32*)mac2;
	uint32 b1 = *(uint32*)(mac1+4);
	uint32 b2 = *(uint32*)(mac2+4);
	if (a1 == a2 && b1 == b2)
		return 0;

	return 1;
}

static int is_filter(char const *ifname)
{
    /* 过滤掉无线，虚拟机接口等 */
    char const *filter[] = {
		/* windows */
        "Wireless", "Microsoft",
        "Virtual",
		/* linux */
		"lo", "wlan", "vboxnet",
		"ifb", "gre", "teql",
		"br", "imq"
    };
	unsigned int i;
    for (i = 0; i < ARRAY_SIZE(filter); ++i) {
        if (strstr(ifname, filter[i]))
            return 1;
    }
    return 0;
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
#endif
/*
 * 获取所有网络接口
 * ifnames 实际获取的接口
 * cnt     两个作用，1：传入表示ifnames最多可以存储的接口个数
 *                   2：返回表示实际获取了的接口个数
 * 返回接口个数在cnt里
 * @return: >=0  成功，实际获取的接口个数
 *          -1 获取失败
 *          -2 cnt过小
 */
extern int getall_ifs(iflist_t *ifs, int *cnt)
{
    int i = 0;
    if (!ifs || *cnt <= 0) return -2;

#ifdef LINUX    /* linux (unix osx?) */
#define _PATH_PROCNET_DEV "/proc/net/dev"
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
    while (NULL != fgets(buff, BUFF_LINE_MAX, fd)) {
        name = get_ifname_from_buff(buff);
        _D("%s\n", name);
        /* 过滤无关网络接口 */
		if (is_filter(name)) {
			_D("filtered %s.\n", name);
			continue;
		}
        strncpy(ifs[i].name, name, IFNAMSIZ);
        _D("ifs[%d].name: %s\n", i, ifs[i].name);
        ++i;
        if (i >= *cnt) {
            fclose(fd);
            return -2;
        }
    }
    fclose(fd);

#elif defined(WINDOWS)
    pcap_if_t *alldevs;
    char errbuf[PCAP_ERRBUF_SIZE];
    if (-1 == pcap_findalldevs(&alldevs, errbuf)) {
        _M("Get interfaces handler error: %s\n", errbuf);
        return -1;
    }
    for (pcap_if_t *d = alldevs; d; d = d->next) {
        if (is_filter(d->description)) {
            _D("filtered %s.\n", d->description);
            continue;
        }
        if (i >= *cnt) return -2;
        strncpy(ifs[i].name, d->name, IFNAMSIZ);
        strncpy(ifs[i].desc, d->description, IFDESCSIZ);
        ++i;
    }
    pcap_freealldevs(alldevs);
#endif

    *cnt = i;
    return i;
}

