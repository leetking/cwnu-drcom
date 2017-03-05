#include <time.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "dhcp.h"
#include "common.h"

/*
 * TODO 这里没有实现dhcp协议，只是把接口留在这里了。
 * 使用了一种替代方案来使程序工作
 */

/* TODO 暂时为我们学校固定 */
#define SERIP	"10.255.0.195"

/*
 * 为s设置接口名字
 * 使用s进行dhcp获取ip时，**必须**先指明接口
 * @return: 0: 成功
 *         -1: 失败
 */
int dhcp_setif(dhcp_t *s, char const *ifname)
{
	strncpy(s->ifname, ifname, IF_NAMESIZE);
	_D("ifname: %s\n", s->ifname);
	return 0;
}
/*
 * 从s里获取dhcp服务器ip信息
 * s: dhcp_t结构体
 * type: AF_INET: ipv4; AF_INET6: ipv6
 * serip: 如果是AF_INET，这是一个struct in_addr指针
 *        如果是AF_INET6, 这是一个struct in6_addr指针
 * @return: 0: 成功
 *         -1: s没有需要获取的信息
 */
int dhcp_getsip(dhcp_t *s, int type, void *serip)
{
	memcpy(serip, &s->_data._v4._serip, sizeof(struct in_addr));
	return 0;
}
/* 获取到的ip, 和上述类似 */
int dhcp_getcip(dhcp_t *s, int type, void *clip)
{
	memcpy(clip, &s->_data._v4._clip, sizeof(struct in_addr));
	return 0;
}
/* 获取获取到ip的subnet mask, 和上述类似 */
int dhcp_getmask(dhcp_t *s, int type, void *mask)
{
	memcpy(mask, &s->_data._v4._mask, sizeof(struct in_addr));
	return 0;
}
/*
 * 执行dhcp协议获取可用ip和mask信息
 * tip需要给出接口信息
 * 结果存储在tip里
 * @return: 0: 成功
 *          1: 没有这个接口
 *          2: 网络并不通，没有服务器响应
 *         -1: 超时
 */
int dhcp_run(dhcp_t *tip)
{
	struct in_addr ip;
	inet_pton(AF_INET, SERIP, &ip);
	memcpy(&tip->_data._v4._serip, &ip, sizeof(ip));
	struct timespec stime, etime;
	/*
	 * 每3秒检测网络是否通
	 * 直到一个超过DHCP_TIMEOUT时间
	 */
	clock_gettime(CLOCK_MONOTONIC, &stime);
	clock_gettime(CLOCK_MONOTONIC, &etime);
	while (!isnetok(tip->ifname) && difftimespec(etime, stime) <= 1000*DHCP_TIMEOUT) {
		sleep(3);
		clock_gettime(CLOCK_MONOTONIC, &etime);
	}
	if (difftimespec(etime, stime) > 1000*DHCP_TIMEOUT) {
		return -1;
	}

	return 0;
}

