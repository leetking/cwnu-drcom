#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include "common.h"
#include "drcom.h"
#include "dhcp.h"

#define BUFF_LEN (512)

/* 一般超时3s */
#define GENERAL_TIMEOUT	(3*1000)

#ifdef DEBUG
# define _DUMP(d, len)	format_data(d, len)
#else
# define _DUMP(d, len)	((void)0)
#endif

/* 过滤函数类型 */
typedef int (*filter_t)(uchar const *buf, size_t len);

static ipv4_t srcip;
static ipv4_t dstip;
static char ifname[IFNAMSIZ];
static uchar srcmac[ETH_ALEN];
static int skfd;
static struct sockaddr_in skaddr;

/* 由于filter的需要，把客户端id(kpid)和服务器计数(sercnt)提出来了 */
static uint32 kpid;			/* 表征一次心跳会话 */
static uint32 sercnt;
static uchar nrmlcnt = 107;	/* 心跳会话没进行一次应答就加1 */

static uint32 drcom_id_checksum(uchar *in, int usrlen);
static uint32 drcom_kp2_checksum(uchar *in);
/* timeout: 单位毫秒1ms = 10^-3s */
static int wrap_send(uchar const *buf, size_t len, int timeout);
static int wrap_recv(uchar *buf, size_t len, int timeout);
static int recvfilter(uchar *buf, size_t len, int timeout, filter_t filter);
static int drcom_daemon(void);
static int drcom_keepalive(void);
static int drcom_init(void);
/* 一系列过滤函数 */
static int filter_is_702(uchar const *buf, size_t len);
static int filter_is_704(uchar const *buf, size_t len);
static int filter_is_70b1(uchar const *buf, size_t len);
static int filter_is_70b3(uchar const *buf, size_t len);

/*
 * 设定用于登录的接口
 * @return: 0: 成功
 *         -1: 失败，也许ifname过长
 */
extern int drcom_setifname(char const *_ifname)
{
	strncpy(ifname, _ifname, IFNAMSIZ);
	_D("ifname: %s\n", ifname);
	return 0;
}
extern int drcom_setserip(char const *_ip)
{
	if (!_ip) return 1;
	inet_pton(AF_INET, _ip, &dstip);
	return 0;
}

/*
 * 登录
 */
extern int drcom_login(char const *usr, char const *pwd)
{
	int ret = 0;

	uchar sendbuf[BUFF_LEN];
	drcom_t *senddr = (drcom_t*)sendbuf;
	uchar recvbuf[BUFF_LEN];
	drcom_t *recvdr = (drcom_t*)recvbuf;
	/* 初始化skfd和skaddr */
	_M("[DRCOM:0] Init interface (%s)...\n", ifname);
	if (0 != drcom_init()) {
		_M("[DRCOM:0] Init interface (%s) error!\n", ifname);
		return -1;
	}

	int wrlen, rdlen, len;
	int get701cnt = 0, get704cnt = 0;
	int try1cnt = 0, try702cnt = 0;
	int state = 1;			/* 开始 */
	int isfinish = 0;
	uchar flux[4];
	while (!isfinish) {
		switch (state) {
		case 1:
			if (get701cnt >= 3 || try1cnt >= 3) {	/* 已经尝试3次了 */
				get701cnt = 0;
				try1cnt = 0;
				isfinish = 1;
				ret = -1;
				break;
			}
			_M("[DRCOM:1] Try %d.%dth: Request Keep Alive...\n", try1cnt, get701cnt);
			memcpy(sendbuf, "\x07\x01\x08\x00\x01\x00\x00\x00", 8);
			wrlen = wrap_send(sendbuf, 8, GENERAL_TIMEOUT);
			_D("wrlen == 8? %d %s\n", wrlen, wrlen==8?"true":"false");
			if (8 == wrlen) {
				try1cnt = 0;
				state = 0x0701;
			} else {
				_M("[DRCOM:1] %dtd request error. try again.\n", try1cnt);
				++try1cnt;
			}
			break;
		case 0x701:
			rdlen = recvfilter(recvbuf, BUFF_LEN, GENERAL_TIMEOUT, filter_is_702);
			if (rdlen <= 0) {
				++get701cnt;
				state = 1;
			} else {
				get701cnt = 0;
				memcpy(&flux, recvdr->drcom_nrml_02flux, 4);
				_D("recvflux: %02X%02X%02X%02X\n", flux[0], flux[1], flux[2], flux[3]);
				state = 0x702;
				_M("[DRCOM:1.1] Get server Response Keep Alive.\n");
				_DUMP(recvbuf, rdlen);
			}
			break;
		case 0x702:
			if (try702cnt >= 3 || get704cnt >= 3) {	/* 发送失败或认证失败，直接退出吧 */
				state = 1;
				get704cnt = 0;
				try702cnt = 0;
				isfinish = 1;
				ret = -1;
				break;
			}
			_M("[DRCOM:2] Try %d.%dth: Send Identity packet...\n", try702cnt, get704cnt);
			memset(senddr, 0, BUFF_LEN);
			senddr->type = 0x07;
			senddr->drcom_nrml_cnt = 0;
			senddr->drcom_nrml_type = 0x03;
			senddr->drcom_nrml_03usrlen = strlen(usr);
			memcpy(senddr->drcom_nrml_03cmac, srcmac, ETH_ALEN);
			memcpy(&senddr->drcom_nrml_03cip, &srcip, sizeof(srcip));
			memcpy(senddr->drcom_nrml_03fixed, "\x03\x22\x00\x08", 4);
			memcpy(senddr->drcom_nrml_03flux, flux, 4);
			memcpy(senddr->drcom_nrml_03user, usr, strlen(usr));
			memcpy(senddr->drcom_nrml_03host, "vmbox", 5);
			drcom_id_checksum(sendbuf, strlen(usr));
			len = ltohs(senddr->drcom_nrml_len);
			wrlen = wrap_send(sendbuf, len, GENERAL_TIMEOUT);
			_DUMP(sendbuf, wrlen);
			if (wrlen == len) {
				try702cnt = 0;
				state = 0x703;
			} else {
				_M("[DRCOM:1] %dth send identity packet error. try again.\n", try702cnt);
				++try702cnt;
			}
			break;
		case 0x703:
			rdlen = recvfilter(recvbuf, BUFF_LEN, GENERAL_TIMEOUT, filter_is_704);
			if (rdlen <= 0) {
				state = 0x702;
				++get704cnt;
			} else {
				get704cnt = 0;
				_M("[DRCOM:2.1] Authenticaty finish.\n");
				state = 0x704;
				_DUMP(recvbuf, rdlen);
			}
			break;
		case 0x704:
			_M("[DRCOM:3] Authenticaty identity SUCCESS!\n");
			state = 0x70b;		/* 空跳转到心跳 */
			break;
			/* msg也许可以处理下 */
		case 0x70b:			/* 开始心跳 */
			switch (fork()) {
			case 0:
				if (0 != drcom_daemon()) {
					close(skfd);
					_M("[DRCOM:KPALV:ERROR] Get a error, Keep alive quit!\n");
					exit(1);
				}
				_M("[DRCOM:KPALV] OK, Keep alive quit!\n");
				exit(0);
				break;
			case -1:
				_M("[DRCOM:ERROR] Cant create drcom daemon, maybe `OFFLINE` after soon.\n");
				break;
			}
			isfinish = 1;
			break;
		default:
			break;
		}
	}
	close(skfd);
	return ret;
}
extern int drcom_logoff(void)
{
	return 0;
}

/*
 * 03(identity包)的校验算法
 * 注意考虑大小端的变化！
 * 这里都是转换为小端计算
 * 存储数据按照小端序，计算按照本地序
 * @return: 校验值4bytes
 */
static uint32 drcom_id_checksum(uchar *in, int usrlen)
{
	*(uint16*)(in+2) = htols((usrlen+232));
	*(uint32*)(in+24) = htoll(20000711u);
	*(uint32*)(in+28) = htoll(126u);
	uint16 v4 = htols(4*((ltohs(*(uint16*)(in+2))+2)/4));
	*(uint16*)(in+2) = v4;
	uint16 v5 = ltohs(v4) >> 2;	/* 这个v5始终本地使用 */
	uint32 v6 = 0;
	int i;
	for (i = 0; i < (int)v5; ++i) {
		v6 ^= *(uint32*)(in+4*i);	/* in里都是按照小端序数据理解，故v6也是小端的 */
	}
	v6 = ltohl(v6);
	*(uint32*)(in+24) = htoll(19680126*v6);
	*(uint32*)(in+28) = 0;
	return htoll(19680126*v6);
}
/*
 * 心跳包的校验
 * 存储按照小端序
 * @return: 校验值4bytes
 */
static uint32 drcom_kp2_checksum(uchar *in)
{
	uint32 sum = 0;
	*(uint32*)&in[24] = 0;
	int i;
	for (i = 0; i < 20; ++i) {
		sum ^= htols(*(uint16*)(in+2*i));
	}
	*(uint32*)&in[24] = htoll(711*sum);
	sum = htoll(sum*711);
	return sum;
}
/*
 * 一个设置超时的发送函数
 * timeout: 发送超时，最长timeout毫秒1s = 1000ms
 * @return: len: 正常发送成功，返回发送的字节数
 *            0: 在timeout时间内发送失败
 *           -n: 发送了n字节后，失败
 */
static int wrap_send(uchar const *buf, size_t len, int timeout)
{
	int wrlen = 0;
	struct timespec t0, t1;
	long used = 0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	while (wrlen < (long)len && used <= timeout) {
		struct timeval tv;
		socklen_t addrlen = sizeof(struct sockaddr_in);
		fd_set wrset;
		FD_ZERO(&wrset);
		FD_SET(skfd, &wrset);
		tv.tv_sec = (timeout-used)/1000;
		tv.tv_usec = (timeout-used)%1000*1000;
		int s;
		s = select(skfd+1, NULL, &wrset, NULL, &tv);
		if (-1 == s) {
			perror("Send Select");
			return 0;
		}
		if (0 == s) {
			_D("sendto timeout.\n");
			return -wrlen;
		}
		int wn;
		wn = sendto(skfd, buf+wrlen, len-wrlen, 0, (struct sockaddr*)&skaddr, addrlen);
		if (wn < 0) {
			_D("sendto %s\n", strerror(errno));
			return -wrlen;
		} else {
			wrlen += wn;
		}
		clock_gettime(CLOCK_MONOTONIC, &t1);
		used = difftimespec(t1, t0);
		_D("used: %ld wlen: %d\n", used, wrlen);
	}
	return wrlen;
}
/*
 * 一个设置超时的读取数据函数
 * timeout: 发送超时，最长timeout毫秒1s = 1000ms
 * @return: len: 正常读取成功，返回读取的字节数
 *            0: 在timeout时间内读取失败，读取了0字节
 *           -n: 读取了n字节后，超时了
 */
static int wrap_recv(uchar *buf, size_t len, int timeout)
{
	int rdlen = 0;
	struct timeval tv;
	struct timespec t1, t0;
	long used = 0;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (;;) {
		fd_set rdset;
		FD_ZERO(&rdset);
		FD_SET(skfd, &rdset);
		tv.tv_sec = (timeout-used)/1000;
		tv.tv_usec = (timeout-used)%1000*1000;
		int s;
		s = select(skfd+1, &rdset, NULL, NULL, &tv);
		if (-1 == s || 0 == s) {
			return -1;
		}
		struct sockaddr_in fromsk;
		socklen_t addrlen = sizeof(fromsk);
		memset(&fromsk, 0, addrlen);
		rdlen = recvfrom(skfd, buf, len, 0, (struct sockaddr*)&fromsk, &addrlen);
		_D("rdlen: %d\n", rdlen);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		used = difftimespec(t1, t0);
		if (ip_equal(AF_INET, &dstip, &fromsk.sin_addr) && rdlen >= 0) {
			return rdlen;
		}
		if (used >= timeout) {
			return -1;
		}
	}
	return rdlen;
}
/*
 * drcom心跳守护进程
 * 在/tmp下创建一个进程号文件，
 * 如果已经存在一个drcom心跳进程，那么干掉它
 * @return: 0: 成功
 *         -1: 失败
 */
static int drcom_daemon()
{
	/* 如果原来存在drcom的keep alive进程，就干掉它 */
#define PID_FILE	"/tmp/cwnu-drcom-drcom.pid"
	FILE *kpalvfd = fopen(PID_FILE, "r+");
	if (NULL == kpalvfd) {
		_M("[DRCOM:KPALV:WARN] No process pidfile. %s: %s\n", PID_FILE, strerror(errno));
		kpalvfd = fopen(PID_FILE, "w+"); /* 不存在，创建 */
		if (NULL == kpalvfd) {
			_M("[DRCOM:KPALV:ERROR] Detect pid file eror(%s)! quit!\n", strerror(errno));
			return -1;
		}
	}
	pid_t oldpid;
	fseek(kpalvfd, 0L, SEEK_SET);
	if ((1 == fscanf(kpalvfd, "%d", (int*)&oldpid)) && (oldpid != (pid_t)-1)) {
		_D("oldkpalv pid: %d\n", oldpid);
		kill(oldpid, SIGKILL);
	}
	setsid();
	if (0 != chdir("/")) {
		_M("[DRCOM:KPALV:WARN] %s\n", strerror(errno));
	}
	umask(0);
	/* 在/tmp下写入自己(keep alive)pid */
	pid_t curpid = getpid();
	_D("kpalv curpid: %d\n", curpid);
	if (NULL == (kpalvfd = freopen(PID_FILE, "w+", kpalvfd))) {
		_M("[DRCOM:KPALV:WARN] truncat pidfile '%s': %s\n", PID_FILE, strerror(errno));
	}
	fprintf(kpalvfd, "%d", curpid);
	fflush(kpalvfd);
	if (0 == drcom_keepalive()) {
		_M("%s: [DRCOM:KPALV:WARN] Server maybe not need keep alive paket.\n", format_time());
		_M("%s: [DRCOM:KPALV:WARN] Now, drcom keep alive process quit!\n", format_time());
	}
	if (NULL == (kpalvfd = freopen(PID_FILE, "w+", kpalvfd))) {
		_M("[DRCOM:KPALV:WARN] truncat pidfile '%s': %s\n", PID_FILE, strerror(errno));
	}
	fprintf(kpalvfd, "-1");	/* 写入-1表示已经离开 */
	fflush(kpalvfd);
	fclose(kpalvfd);
#undef PID_FILE
	return 0;
}
/* 心跳进程 */
static int drcom_keepalive(void)
{
	/* 把心跳日志记录下来, 可执行程序目录下 */
#ifdef DEBUG
	char logfile[] = "./drcom-keepalive.log";
	char logbackfile[] = "./drcom-keepalive.log.back";
	char log[EXE_PATH_MAX+sizeof(logfile)];
	char logback[EXE_PATH_MAX+sizeof(logbackfile)];
	if (0 != getexedir(log)) {
		_M("[DRCOM:WARN] Cant get execute program directory.\n");
		goto _keepalv_redirect_out_error;
	}
	strncat(log, logfile, sizeof(logfile));
	if (0 != getexedir(logback)) {
		_M("[DRCOM:WARN] Cant get execute program directory.\n");
		goto _keepalv_redirect_out_error;
	}
	strncat(logback, logbackfile, sizeof(logbackfile));
	if (0 != copy(log, logback)) {
		_M("WARN: Cant backup kpalvlogfile (%s), no such file.\n", log);
	}
	_M("Now, save kpalvlog to file `%s`\n", log);
	if (NULL == freopen(log, "w", stderr)) {
		_M("WARN: Redirect `stderr` error!\n");
	}
	if (NULL == freopen(log, "w", stdout)) {
		_M("WARN: Redirect `stdout` error!\n");
	}
_keepalv_redirect_out_error:
	(void)0;

#endif
	int ret = 0;
	uchar sendbuf[BUFF_LEN];
	drcom_t *senddr = (drcom_t*)sendbuf;
	uchar recvbuf[BUFF_LEN];
	drcom_t *recvdr = (drcom_t*)recvbuf;

	/* 一个心跳循环记时 */
	int try0cnt = 0;
	int try70b2cnt = 0;
	int rdlen, wrlen;

	int isquit = 0;
	int state = 0;
	for (;!isquit;) {
		switch (state) {
		case 0:
			//clock_gettime(CLOCK_MONOTONIC, &stime);
			if (try0cnt >= 3) {
				isquit = 1;
				return -1;
			}
			memset(sendbuf, 0, BUFF_LEN);
			senddr->type = 0x07;
			senddr->drcom_nrml_cnt = nrmlcnt;
			senddr->drcom_nrml_len = htols(40);
			senddr->drcom_nrml_type = 0x0b;
			senddr->drcom_nrml_kpstep = 0x01;
			senddr->drcom_nrml_kpfixed = htols(0x02d6);
			kpid = (uint32)rand();
			senddr->drcom_nrml_kpid = kpid;
			senddr->drcom_nrml_kpsercnt = htoll(0x0b1);
			wrlen = wrap_send(sendbuf, 40, GENERAL_TIMEOUT);	/* 心跳大小固定为40 */
			_DUMP(sendbuf, 40);
			if (wrlen == 40) {
				try0cnt = 0;
				state = 0x70b1;
				_D("[drcom:kpalv] send 0x70b1 0k.\n");
			} else {
				++try0cnt;
			}
			break;
		case 0x70b1:
			rdlen = recvfilter(recvbuf, BUFF_LEN, GENERAL_TIMEOUT, filter_is_70b1);
			if (rdlen > 0) {
				_DUMP(recvbuf, rdlen);
				_D("[drcom:kpalv] get sercnt: %0X\n", recvdr->drcom_nrml_kpsercnt);
				sercnt = recvdr->drcom_nrml_kpsercnt;
			}
			state = 0x70b2;
		case 0x70b2:
			if (try70b2cnt >= 3) {
				isquit = 1;
				return -1;
			}
			++nrmlcnt;		/* buf[1] 处的计数  */
			memset(sendbuf, 0, BUFF_LEN);
			senddr->type = 0x07;
			senddr->drcom_nrml_cnt = nrmlcnt;
			senddr->drcom_nrml_len = htols(40);
			senddr->drcom_nrml_type = 0x0b;
			senddr->drcom_nrml_kpstep = 0x03;
			senddr->drcom_nrml_kpfixed = htols(0x02d6);
			kpid = (uint32)rand();
			senddr->drcom_nrml_kpid = kpid;
			senddr->drcom_nrml_kpsercnt = sercnt;
			drcom_kp2_checksum(sendbuf);
			wrlen = wrap_send(sendbuf, 40, GENERAL_TIMEOUT);
			_DUMP(sendbuf, 40);
			if (wrlen == 40) {
				try70b2cnt = 0;
				state = 0x70b3;
				_D("[drcom:kpalv] send 0x70b3 ok.\n");
			} else {
				++try70b2cnt;
			}
			break;
		case 0x70b3:
			rdlen = recvfilter(recvbuf, BUFF_LEN, GENERAL_TIMEOUT, filter_is_70b3);
			if (rdlen > 0) {
				_DUMP(recvbuf, rdlen);
				/* TODO 修改为其他公寓的模式 */
				_M("%s [DRCOM:KPALV] finish A keep alive crycle.\n", format_time());
			}
			/* TODO fixed time */
			long ms = 20000;
			msleep(ms);
			state = 0;
			break;
		case 0x70b4:
			break;
		case 0xff:
			break;
		case 0x706:
			break;
		}
	}

	return ret;
}
/*
 * 初始化缓存区，生产套接字和地址接口信息
 * skfd: 被初始化的socket
 * skaddr: 被初始化地址接口信息
 * @return: 0: 成功
 *          -1: 初始化套接字失败
 *          -2: 初始化地址信息失败
 *          -3: 获取mac地址和ip失败
 */
static int drcom_init(void)
{
	/* 获取用于通讯的udp套接字 */
	skfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == skfd) {
		perror("drcom_login create socket");
		return -1;
	}

	/* 初始化服务器地址描述结构 */
	memset(&skaddr, 0, sizeof(skaddr));
	skaddr.sin_family = AF_INET;
	skaddr.sin_port = htons(DRCOM_PORT);
	memcpy(&skaddr.sin_addr, &dstip, sizeof(dstip));

	/* 获取客户端mac地址和ip地址 */
	int rawskfd;
	if (-1 == (rawskfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL)))) {
		perror("Socket");
		return -1;
	}
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (-1 == ioctl(rawskfd, SIOCGIFHWADDR, &ifr)) {
		perror("Get Mac");
		close(rawskfd);
		return -3;
	}
	memcpy(srcmac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
	_D("srcmac: %s\n", format_mac(srcmac));
	if (-1 == ioctl(rawskfd, SIOCGIFADDR, &ifr)) {
		perror("Get Ip");
		close(rawskfd);
		return -3;
	}
	memcpy(&srcip, &((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr, 4);
#ifdef DEBUG
	uchar const *p = (uchar*)&srcip;
	_D("srcip: %d.%d.%d.%d\n", p[0], p[1], p[2], p[3]);
#endif

	close(rawskfd);

	return 0;
}
/*
 * 根据过滤函数filter在timeout时间内过滤得到需要的数据
 * buf: 需要的到数据
 * len: buf缓冲区最大长度
 * timeout: 超时毫秒1s == 1000ms
 * filter: 过滤函数
 * @return: =0: 没有超时且没有读取到数据
 *          >0: 没有超时且正常读取了返回值大小的数据
 *          -n: 在读取了n字节后超时了
 */
static int recvfilter(uchar *buf, size_t len, int timeout, filter_t filter)
{
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	long used = 0;
	int rdlen;
	for (;used <= timeout;) {
		rdlen = wrap_recv(buf, len, timeout-used);
		if (rdlen > 0 && filter(buf, rdlen))
			return rdlen;
		clock_gettime(CLOCK_MONOTONIC, &t1);
		used = difftimespec(t1, t0);
	}
	return rdlen>0?-rdlen:rdlen;
}
static int filter_is_702(uchar const *buf, size_t len)
{
	drcom_t *p = (drcom_t*)buf;
	if (0x07 == p->type && 0x02 == p->drcom_nrml_type) {
		return 1;
	}
	return 0;
}
static int filter_is_704(uchar const *buf, size_t len)
{
	drcom_t *p = (drcom_t*)buf;
	if (0x07 == p->type && 0x04 == p->drcom_nrml_type) {
		return 1;
	}
	return 0;
}
static int filter_is_70b1(uchar const *buf, size_t len)
{
	drcom_t *p = (drcom_t*)buf;
	if (0x07 == p->type && 0x0b == p->drcom_nrml_type
			&& nrmlcnt == p->drcom_nrml_cnt
			&& 0x02 == p->drcom_nrml_kpstep
			&& kpid == p->drcom_nrml_kpid) {
		return 1;
	}
	return 0;
}
static int filter_is_70b3(uchar const *buf, size_t len)
{
	drcom_t *p = (drcom_t*)buf;
	if (0x07 == p->type && 0x0b == p->drcom_nrml_type
			&& nrmlcnt == p->drcom_nrml_cnt
			&& 0x04 == p->drcom_nrml_kpstep
			&& kpid == p->drcom_nrml_kpid) {
		return 1;
	}
	return 0;
}
