#ifndef COMMON_H__
#define COMMON_H__

#include <stdint.h>

#include <linux/limits.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <net/if.h>

typedef uint8_t u8;     /* 一个字节 */
typedef uint16_t u16;   /* 两个字节 */
typedef uint32_t u32;   /* 四个字节 */

typedef struct {
    char name[IF_NAMESIZE]; /* linux下是eth0, windows采用的是注册表类似的(\Device\NPF_{xxxx-xxx-xx-xx-xxx}) */
} ifname_t;


/* 用户名和密码长度 */
#define USERNAME_LEN    32
#define PASSWORD_LEN    32

#define FORMAT_TIME_MAX 64


#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

#define MAX(x, y)       ((x)>(y)? (x): (y))
#define MIN(x, y)       ((x)>(y)? (y): (x))

#undef PRINT
#ifdef GUI
# include <glib.h>
# define PRINT(...) g_print(__VA_ARGS__)
#else
# include <stdio.h>
# define PRINT(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifdef DEBUG
# define _D(...) \
    do { \
        PRINT("%s:%s:%d:", format_time(), __FILE__, __LINE__); \
        PRINT(__VA_ARGS__); \
    } while(0)
#else
# define _D(...)    ((void)0)
#endif

#define _M(...)    PRINT(__VA_ARGS__);

/**
 * 获取程序所在的实际绝对路径的目录
 * exedir: 返回目录，加上\0一起长度是EXE_PATH_MAX，
 *         如果本上长度达到了EXE_PATH_MAX(不包括\0)，那么也会返回失败
 * @return: 0: 成功
 *         !0: 失败
 */
extern int getexedir(char *exedir);

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
extern int getall_ifs(ifname_t *ifs, int *cnt);

/**
 * 获取当前时间按照
 * yyyy-MM-dd HH:mm:ss
 * 格式返回
 * NOTE 不要去修改返回结果，并且不是线程安全的
 * @return: NULL: 失败
 *         !NULL: 存储的结果
 */
extern char const *format_time(void);

/**
 * 简单的复制文件，暂时不进行细致错误检查
 * NOTE 是绝对路径
 * scr: 源文件
 * dst: 目标文件
 * @return: 0: 成功
 *         -1: 失败
 */
extern int copy_file(char const *src, char const *dst);

/**
 * 字节序转换相关函数
 * host to lsb/msb short/long (host->l/m)
 * lsb/msb to host short/long (l/m->host)
 */
extern u16 htols(u16 n);
extern u16 htoms(u16 n);
extern u16 ltohs(u16 n);
extern u16 mtohs(u16 n);

extern u32 htoll(u32 n);
extern u32 htoml(u32 n);
extern u32 ltohl(u32 n);
extern u32 mtohl(u32 n);

extern u8 const *format_mac(u8 const *mac);

/**
 * 判断网络是否连通
 * ifname: 接口名字
 * @return: !0: 连通
 *           0: 没有连通
 */
extern int isnetok(char const *ifname);

/**
 * 返回t1-t0的时间差
 * 由于这里精度没必要达到ns，故返回相差微秒ms
 * @return: 时间差，单位微秒(1s == 1000ms)
 */
extern long difftimespec(struct timespec t1, struct timespec t0);

/**
 * 休眠ms微秒
 */
extern void msleep(long ms);

/**
 * 以16进制打印数据
 */
extern void format_data(u8 const *d, size_t len);

#endif /* COMMON_H__ */
