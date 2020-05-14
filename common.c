#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <sys/select.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>

#include "common.h"

#define PATH_SEP   '/'

extern int getexedir(char *exedir)
{
    int cnt = readlink("/proc/self/exe", exedir, PATH_MAX);
    if (cnt < 0 || cnt > PATH_MAX)
        return -1;
    _D("exedir: %s\n", exedir);
    char *end = strrchr(exedir, PATH_SEP);
    if (!end) return -1;
    *(end+1) = '\0';
    _D("exedir: %s\n", exedir);
    return 0;
}


static int is_filtered(char const *ifname)
{
    /* 过滤掉无线，虚拟机接口等 */
    char const *filtered_ifs[] = {
        /* windows */
        "Wireless", "Microsoft",
        "Virtual",
        /* linux */
        "lo", "wlan", "vboxnet",
        "ifb", "gre", "teql",
        "br", "imq", "ra",
        "wds", "sit", "apcli",
        NULL,
    };
    for (char const **if_ = filtered_ifs; *if_; if_++) {
        if (strstr(ifname, *if_))
            return 1;
    }
    return 0;
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


/**
 * 获取所有网络接口
 * ifnames 实际获取的接口
 * cnt     两个作用，1：传入表示ifnames最多可以存储的接口个数
 *                   2：返回表示实际获取了的接口个数
 * 返回接口个数在cnt里
 * @return: >=0  成功，实际获取的接口个数
 *          -1 获取失败
 *          -2 cnt过小
 */
extern int getall_ifs(ifname_t *ifs, int *cnt)
{
    int i = 0;
    if (!ifs || *cnt <= 0) return -2;

#define NETDEV_PATH     "/proc/net/dev"
#define BUFF_LINE_MAX   1024
    char buff[BUFF_LINE_MAX];
    FILE *fd = fopen(NETDEV_PATH, "r");
    char *name;
    if (NULL == fd) {
        perror(NETDEV_PATH);
        return -1;
    }
    /* NETDEV_PATH 文件格式如下,...表示后面我们不关心
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
        if (is_filtered(name)) {
            _D("filtered %s.\n", name);
            continue;
        }
        strncpy(ifs[i].name, name, IF_NAMESIZE);
        _D("ifs[%d].name: %s\n", i, ifs[i].name);
        ++i;
        if (i >= *cnt) {
            fclose(fd);
            return -2;
        }
    }
    fclose(fd);
    *cnt = i;

    return i;
}


extern char const *format_time(void)
{
    static char buff[FORMAT_TIME_MAX];
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if (NULL == timeinfo) return NULL;
    strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", timeinfo);

    return buff;
}


extern int copy_file(char const *f1, char const *f2)
{
    if (NULL == f1 || NULL == f2) return -1;
    FILE *src, *dst;
    src = fopen(f1, "r");
    dst = fopen(f2, "w");
    if (NULL == src || NULL == dst) return -1;
    char buff[1024];
    int n;
    while (0 < (n = fread(buff, 1, 1024, src)))
        fwrite(buff, 1, n, dst);

    fclose(src);
    fclose(dst);

    return 0;
}


/**
 * 本地是否是小端序
 * @return: !0: 是
 *           0: 不是(大端序)
 */
static int islsb()
{
    static u16 a = 0x0001;
    return (int)(*(u8*)&a);
}


static u16 exorders(u16 n)
{
    return ((n>>8)|(n<<8));
}


static u32 exorderl(u32 n)
{
    return (n>>24)|((n&0x00ff0000)>>8)|((n&0x0000ff00)<<8)|(n<<24);
}


extern u16 htols(u16 n)
{
    return islsb()? n: exorders(n);
}


extern u16 htoms(u16 n)
{
    return islsb()? exorders(n): n;
}


extern u16 ltohs(u16 n)
{
    return islsb()? n: exorders(n);
}


extern u16 mtohs(u16 n)
{
    return islsb()? exorders(n): n;
}


extern u32 htoll(u32 n)
{
    return islsb()? n: exorderl(n);
}


extern u32 htoml(u32 n)
{
    return islsb()? exorderl(n): n;
}


extern u32 ltohl(u32 n)
{
    return islsb()? n: exorderl(n);
}


extern u32 mtohl(u32 n)
{
    return islsb()? exorderl(n): n;
}


extern u8 const *format_mac(u8 const *macarr)
{
    static u8 formatmac[] = "XX:XX:XX:XX:XX:XX";
    if (NULL == macarr)
        return NULL;
    sprintf((char*)formatmac, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
            macarr[0], macarr[1], macarr[2],
            macarr[3], macarr[4], macarr[5]);
    return formatmac;
}


/*
 * 以16进制打印数据
 */
extern void format_data(u8 const *d, size_t len)
{
    int i;
    for (i = 0; i < (long)len; ++i) {
        if (i != 0 && i%16 == 0)
            _M("\n");
        _M("%02x ", d[i]);
    }
    _M("\n");
}


/**
 * 返回t1-t0的时间差
 * 由于这里精度没必要达到ns，故返回相差微秒ms
 * @return: 时间差，单位微秒(1s == 1000ms)
 */
extern long difftimespec(struct timespec t1, struct timespec t0)
{
    long d = t1.tv_sec-t0.tv_sec;
    d *= 1000;
    d += (t1.tv_nsec-t0.tv_nsec)/(long)(1e6);
    return d;
}


/**
 * 判断网络是否连通
 * 最长延时3s，也就是说如果3s内没有检测到数据回应，那么认为网络不通
 * TODO 使用icmp协议判断
 * @return: !0: 连通
 *           0: 没有连通
 */
extern int isnetok(char const *ifname)
{
    static char baidu[] = "baidu.com";
    (void)ifname;
    (void)baidu;   /* repress warn */
    sleep(100);
    return 1;
}


/**
 * 休眠ms微秒
 */
extern void msleep(long ms)
{
    struct timeval tv;
    tv.tv_sec = ms/1000;
    tv.tv_usec = ms%1000*1000;
    select(0, 0, 0, 0, &tv);
}
