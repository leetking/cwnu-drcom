#ifndef _DHCP_H
#define _DHCP_H

#include <arpa/inet.h>
#include <net/if.h>

/*
 * dhcp请求超时
 * 单位秒s
 */
#define DHCP_TIMEOUT	(120)

/*
 * 信息存储结构
 */
typedef struct dhcp_t {
	char ifname[IFNAMSIZ];
	union {
		struct {	/* ipv4 */
			struct in_addr _serip;
			struct in_addr _clip;
			struct in_addr _mask;
		}_v4;
		struct {	/* ipv6 */
			struct in6_addr _serip;
			struct in6_addr _clip;
			struct in6_addr _mask;
		}_v6;
	}_data;
}dhcp_t;

/*
 * 为s设置接口名字
 * NOTE 使用s进行dhcp获取ip时，**必须**先指明接口
 * @return: 0: 成功
 *         -1: 失败
 */
int dhcp_setif(dhcp_t *s, char const *ifname);
/*
 * 从s里获取dhcp服务器ip信息
 * s: dhcp_t结构体
 * type: AF_INET: ipv4; AF_INET6: ipv6
 * serip: 如果是AF_INET，这是一个struct in_addr指针
 *        如果是AF_INET6, 这是一个struct in6_addr指针
 * @return: 0: 成功
 *         -1: s没有需要获取的信息
 */
int dhcp_getsip(dhcp_t *s, int type, void *serip);
/* 获取到的ip, 和上述类似 */
int dhcp_getcip(dhcp_t *s, int type, void *clip);
/* 获取获取到ip的subnet mask, 和上述类似 */
int dhcp_getmask(dhcp_t *s, int type, void *mask);

/*
 * 执行dhcp协议获取可用ip和mask信息
 * tip需要给出接口信息
 * 结果存储在tip里
 * @return: 0: 成功
 *          1: 没有这个接口
 *          2: 网络并不通，没有服务器响应
 *         -1: 超时
 */
int dhcp_run(dhcp_t *tip);

#endif
