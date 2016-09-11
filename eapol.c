#include "eapol.h"
#include "type.h"
#include "md5.h"

#include <netinet/if_ether.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#define BUFF_LEN	(1024)

static uchar client_mac[ETH_ALEN];

static uchar sendbuff[BUFF_LEN];
static uchar recvbuff[BUFF_LEN];
static char ifname[IFNAMSIZ] = "eth0";
static ethII_t *sendethii, *recvethii;
static eapol_t *sendeapol, *recveapol;
static eap_t *sendeap, *recveap;
static eapbody_t *sendeapbody, *recveapbody;

static char _uname[UNAME_LEN];
static char _pwd[PWD_LEN];
static int pwdlen;

static int mac_equal(uchar *mac1, uchar *mac2);
static int eap_keep_alive(int skfd, struct sockaddr const *skaddr);
static int eap_md5_clg(int skfd, struct sockaddr const *skaddr);
static int eap_res_identity(int skfd, struct sockaddr const *skaddr);
static int eapol_init(int *skfd, struct sockaddr *skaddr);
static int eapol_start(int skfd, struct sockaddr const *skaddr);
static int eapol_logoff(int skfd, struct sockaddr const *skaddr);
static int filte_req_identity(int skfd, struct sockaddr const *skaddr);
static int filte_req_md5clg(int skfd, struct sockaddr const *skaddr);
static int filte_success(int skfd, struct sockaddr const *skaddr);

/* 比较两个mac是否相等 */
static int mac_equal(uchar *mac1, uchar *mac2)
{
	uint32 a1 = *(uint32*)mac1;
	uint32 a2 = *(uint32*)mac2;
	uint32 b1 = *(uint32*)(mac1+4);
	uint32 b2 = *(uint32*)(mac2+4);
	if (a1 == a2 && b1 == b2)
		return 0;

	return 1;
}

/*
 * 初始化缓存区，生产套接字和地址接口信息
 * skfd: 被初始化的socket
 * skaddr: 被初始化地址接口信息
 * @return: 0: 成功
 *          -1: 初始化套接字失败
 *          -2: 初始化地址信息失败
 */
static int eapol_init(int *skfd, struct sockaddr *skaddr)
{
	struct ifreq ifr;
	struct sockaddr_ll *skllp = (struct sockaddr_ll*)skaddr;
	sendethii = (ethII_t*)sendbuff;
	sendeapol = (eapol_t*)((uchar*)sendethii+sizeof(ethII_t));
	sendeap = (eap_t*)((uchar*)sendeapol+sizeof(eapol_t));
	sendeapbody = (eapbody_t*)((uchar*)sendeap+sizeof(eap_t));
	recvethii = (ethII_t*)recvbuff;
	recveapol = (eapol_t*)((uchar*)recvethii+sizeof(ethII_t));
	recveap = (eap_t*)((uchar*)recveapol+sizeof(eapol_t));
	recveapbody = (eapbody_t*)((uchar*)recveap+sizeof(eap_t));

	if (-1 == (*skfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL)))) {
		perror("Socket");
		return -1;
	}
	/* 先假定就是eth0接口 */
	memset(skaddr, 0, sizeof(struct sockaddr_ll));
	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (-1 == ioctl(*skfd, SIOCGIFINDEX, &ifr)) {
		perror("Get index");
		goto addr_err;
	}
	skllp->sll_ifindex = ifr.ifr_ifindex;
#ifdef DEBUG
	printf("%s's index: %d\n", ifname, skllp->sll_ifindex);
#endif
	if (-1 == ioctl(*skfd, SIOCGIFHWADDR, &ifr)) {
		perror("Get MAC");
		goto addr_err;
	}
	memcpy(client_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
#ifdef DEBUG
	printf("%s's MAC: %02X-%02X-%02X-%02X-%02X-%02X\n", ifname,
			client_mac[0],client_mac[1],client_mac[2],
			client_mac[3],client_mac[4],client_mac[5]);
#endif
	/* TODO 这里每个字段的含义 */
	skllp->sll_family = PF_PACKET;	
	/*skllp->sll_protocol = ETH_P_ARP;*/
	/*skllp->sll_ifindex = ? 已给出 */
	skllp->sll_hatype = ARPHRD_ETHER;
	skllp->sll_pkttype = PACKET_HOST;
	skllp->sll_halen = ETH_ALEN;
	return 0;

addr_err:
	close(*skfd);
	return -2;
}

/*
 * 过滤得到eap-request-identity包
 * @return: 0: 成功获取
 *          -1: 超时
 */
static int filte_req_identity(int skfd, struct sockaddr const *skaddr)
{
	int stime = time((time_t*)NULL);
	for (; difftime(time((time_t*)NULL), stime) <= TIMEOUT;) {
		/* TODO 看下能不能只接受某类包，包过滤 */
		recvfrom(skfd, recvbuff, BUFF_LEN, 0, NULL, NULL);
		/* eap包且是request */
		if (recvethii->type == htons(ETHII_8021X)
				&& mac_equal(recvethii->dst_mac, client_mac)
				&& recveapol->type == EAPOL_PACKET
				&& recveap->code == EAP_CODE_REQ
				&& recveap->type == EAP_TYPE_IDEN) {
				return 0;
		}
	}
	return -1;
}
/*
 * 过滤得到eap-request-md5clg包
 * @return: 0: 成功获取
 *          -1: 超时
 *          -2: 服务器中止登录，用户名不存在
 */
static int filte_req_md5clg(int skfd, struct sockaddr const *skaddr)
{
	int stime = time((time_t*)NULL);
	for (; difftime(time((time_t*)NULL), stime) <= TIMEOUT;) {
		recvfrom(skfd, recvbuff, BUFF_LEN, 0, NULL, NULL);
		/* 是request且是eap-request-md5clg */
		if (recvethii->type == htons(ETHII_8021X)
				&& mac_equal(recvethii->dst_mac, client_mac)
				&& recveapol->type == EAPOL_PACKET ) {
			if (recveap->code == EAP_CODE_REQ
					&& recveap->type == EAP_TYPE_MD5) {
#ifdef DEBUG
				int i;
				printf("id: %d\n", sendeap->id);
				printf("md5: ");
				for (i = 0; i < recveapbody->md5size; ++i)
					printf("%.2x", recveapbody->md5value[i]);
				printf("\n");
				printf("ex-md5: ");
				for (i = 0; i < ntohs(recveap->len) - recveapbody->md5size - 2; ++i)
					printf("%.2x", recveapbody->md5exdata[i]);
				printf("\n");
#endif
				return 0;
			} else if (recveap->id == sendeap->id
					&& recveap->code == EAP_CODE_FAIL) {
#ifdef DEBUG
				printf("id: %d fail.\n", sendeap->id);
#endif
				return -2;
			}
		}
	}
	return -1;
}
/*
 * 过滤得到登录成功包
 * @return: 0: 成功获取
 *          -1: 超时
 *          -2: 服务器中止登录，密码错误吧
 */
static int filte_success(int skfd, struct sockaddr const *skaddr)
{
	int stime = time((time_t*)NULL);
	for (; difftime(time((time_t*)NULL), stime) <= TIMEOUT;) {
		recvfrom(skfd, recvbuff, BUFF_LEN, 0, NULL, NULL);
		if (recvethii->type == htons(ETHII_8021X)
				&& mac_equal(recvethii->dst_mac, client_mac)
				&& recveapol->type == EAPOL_PACKET ) {
			if (recveap->id == sendeap->id
					&& recveap->code == EAP_CODE_SUCS) {
#ifdef DEBUG
				printf("id: %d login success.\n", sendeap->id);
#endif
				return 0;
			} else if (recveap->id == sendeap->id
					&& recveap->code == EAP_CODE_FAIL) {
#ifdef DEBUG
				printf("id: %d fail.\n", sendeap->id);
#endif
				return -2;
			}
		}
	}
	return -1;
}
/*
 * 广播发送eapol-start
 */
static int eapol_start(int skfd, struct sockaddr const *skaddr)
{
	/* 这里采用eap标记的组播mac地址，也许采用广播也可以吧 */
	uchar broadcast_mac[ETH_ALEN] = {
		0x01, 0x80, 0xc2, 0x00, 0x00, 0x03,
	};
	memcpy(sendethii->dst_mac, broadcast_mac, ETH_ALEN);
	memcpy(sendethii->src_mac, client_mac, ETH_ALEN);
	sendethii->type = htons(ETHII_8021X);
	sendeapol->ver = EAPOL_VER;
	sendeapol->type = EAPOL_START;
	sendeapol->len = 0x0;
	sendto(skfd, sendbuff, ETH_ALEN*2+6, 0, skaddr, sizeof(struct sockaddr_ll));
	return 0;
}
/* 退出登录 */
static int eapol_logoff(int skfd, struct sockaddr const *skaddr)
{
	uchar broadcast_mac[ETH_ALEN] = {
		0x01, 0x80, 0xc2, 0x00, 0x00, 0x03,
	};
	memcpy(sendethii->dst_mac, broadcast_mac, ETH_ALEN);
	memcpy(sendethii->src_mac, client_mac, ETH_ALEN);
	sendethii->type = htons(ETHII_8021X);
	sendeapol->ver = EAPOL_VER;
	sendeapol->type = EAPOL_LOGOFF;
	sendeapol->len = 0x0;
	sendeap->id = EAPOL_LOGOFF_ID;
	sendto(skfd, sendbuff, ETH_ALEN*2+6, 0, skaddr, sizeof(struct sockaddr_ll));
	return 0;
}
/* 回应request-identity */
static int eap_res_identity(int skfd, struct sockaddr const *skaddr)
{
	memcpy(sendethii->dst_mac, recvethii->src_mac, ETH_ALEN);
	sendeapol->type = EAPOL_PACKET;
	sendeapol->len = htons(sizeof(eap_t)+sizeof(eapbody_t));
	sendeap->code = EAP_CODE_RES;
	sendeap->id = recveap->id;
	sendeap->len = htons(sizeof(eapbody_t));
	sendeap->type = EAP_TYPE_IDEN;
	strncpy((char*)sendeapbody->identity, _uname, UNAME_LEN);
	sendto(skfd, sendbuff, ETH_ALEN*2+6+5+sizeof(eapbody_t),
			0, skaddr, sizeof(struct sockaddr_ll));
	return 0;
}
/* 回应md5clg */
static int eap_md5_clg(int skfd, struct sockaddr const *skaddr)
{
	uchar md5buff[BUFF_LEN];
	sendeap->id = recveap->id;
	sendeap->len = htons(sizeof(eapbody_t));
	sendeap->type = EAP_TYPE_MD5;
	sendeapbody->md5size = recveapbody->md5size;
	memcpy(md5buff, &sendeap->id, 1);
	memcpy(md5buff+1, _pwd, pwdlen);
	memcpy(md5buff+1+pwdlen, recveapbody->md5value, recveapbody->md5size);
	MD5(md5buff, 1+pwdlen+recveapbody->md5size, sendeapbody->md5value);
	memcpy((char*)sendeapbody->md5exdata, _uname, strlen(_uname));
	sendto(skfd, sendbuff, ETH_ALEN*2+6+5+sizeof(eapbody_t),
			0, skaddr, sizeof(struct sockaddr_ll));
	return 0;
}

/*
 * 保持在线
 * eap心跳包
 * 某些eap实现需要心跳或多次认证
 * 目前有些服务器会有如下特征
 * 每一分钟，服务端发送一个request-identity包来判断是否在线
 * 请求三次
 */
static int eap_keep_alive(int skfd, struct sockaddr const *skaddr)
{
#define EAP_KPALV_TIMEOUT	(420)	/* 7分钟 */
	int status;
	time_t stime = time((time_t*)NULL);
	/* EAP_KPALV_TIMEOUT时间内已经不再有心跳包，我们认为服务器不再需要心跳包了 */
	for (; difftime(time((time_t*)NULL), stime) <= EAP_KPALV_TIMEOUT; ) {
		status = filte_req_identity(skfd, skaddr);
#ifdef DEBUG
		printf("[KPALV] status: %d\n", status);
#endif
		if (0 == status) {
#ifdef DEBUG
			printf("[KPALV] get a request-identity\n");
#endif
			eap_res_identity(skfd, skaddr);
			stime = time((time_t*)NULL);
		}
	}
	return 0;
}

/*
 * eap认证
 * uname: 用户名
 * pwd: 密码
 * @return: 0: 成功
 *          1: 用户不存在
 *          2: 密码错误
 *          3: 其他超时
 *          4: 服务器拒绝请求登录
 *          -1: 没有找到合适网络接口
 *          -2: 没有找到服务器
 */
int eaplogin(char const *uname, char const *pwd)
{
	int i;
	int state;
	int skfd;
	struct sockaddr_ll ll;

	printf("[0] Initilize interface...\n");
	strncpy(_uname, uname, UNAME_LEN);
	strncpy(_pwd, pwd, PWD_LEN);
	pwdlen = strlen(_pwd);
	if (0 != eapol_init(&skfd, (struct sockaddr*)&ll))
		return -1;
	/* 无论如何先请求一下下线 */
	eapol_logoff(skfd, (struct sockaddr*)&ll);
	/* eap-start */
	printf("[1] Send eap-start...\n");
	for (i = 0; i < TRY_TIMES; ++i) {
		eapol_start(skfd, (struct sockaddr*)&ll);
		if (0 == filte_req_identity(skfd, (struct sockaddr*)&ll))
			break;
		printf(" [1] %dth Try send eap-start...\n", i+1);
	}
	if (i >= TRY_TIMES) goto _timeout;

	/* response-identity */
	printf("[2] Send response-identity...\n");
	for (i = 0; i < TRY_TIMES; ++i) {
		eap_res_identity(skfd, (struct sockaddr*)&ll);
		state = filte_req_md5clg(skfd, (struct sockaddr*)&ll);
		if (0 == state) break;
		else if (-2 == state) goto _no_uname;
		printf(" [2] %dth Try send response-identity...\n", i+1);
	}
	if (i >= TRY_TIMES) goto _timeout;

	/* response-md5clg */
	printf("[3] Send response-md5clg...\n");
	for (i = 0; i < TRY_TIMES; ++i) {
		eap_md5_clg(skfd, (struct sockaddr*)&ll);
		state = filte_success(skfd, (struct sockaddr*)&ll);
		if (0 == state) break;	/* 登录成功 */
		else if (-2 == state) goto _pwd_err;
		printf(" [3] %dth Try send response-md5clg...\n", i+1);
	}
	if (i >= TRY_TIMES) goto _timeout;

	/* 登录成功，生成心跳进程 */
	switch (fork()) {
	case 0:
		{
			/* 放到后台 */
			/* 如果存在原来的keep alive进程，就干掉他 */
#define PID_FILE	"/tmp/cwnu-drcom.pid"
			FILE *kpalvpid = fopen(PID_FILE, "r+");
			if (NULL == kpalvpid) {
				perror(PID_FILE);	/* 不存在，创建 */
				kpalvpid = fopen(PID_FILE, "w+");
				if (NULL == kpalvpid) {
					perror(PID_FILE);
					printf("[KPALV] Detect pid file eror! quit!\n");
					exit(1);
				}
			}
			pid_t oldpid;
			fseek(kpalvpid, 0L, SEEK_SET);
			if ((1 == fscanf(kpalvpid, "%d", (int*)&oldpid))
					&& (oldpid != (pid_t)-1)) {
#ifdef DEBUG
				printf("oldkpalv pid: %d\n", oldpid);
#endif
				kill(oldpid, SIGKILL);
			}
			setsid();
			chdir("/");
			umask(0);
			/* 在/tmp下写入自己(keep alive)pid */
			pid_t curpid = getpid();
#ifdef DEBUG
			printf("kpalv curpid: %d\n", curpid);
#endif
			ftruncate(fileno(kpalvpid), 0);
			fprintf(kpalvpid, "%d", curpid);
			fflush(kpalvpid);
			if (0 == eap_keep_alive(skfd, (struct sockaddr*)&ll)) {
				printf("[KPALV] Server maybe not need keep alive paket.\n");
				printf("[KPALV] Now, keep alive process quit!\n");
			}
			ftruncate(fileno(kpalvpid), 0);
			fprintf(kpalvpid, "-1");	/* 写入-1表示已经离开 */
			fflush(kpalvpid);
			fclose(kpalvpid);
			close(skfd);
		}
		exit(0);
		break;
	case -1:
		printf("[WARN] Cant create daemon, maybe offline after soon.\n");
	}
	close(skfd);
	return 0;

_timeout:
	printf("[ERROR] Not server in range.\n");
	close(skfd);
	return -2;
_no_uname:
	printf("[ERROR] No this user(%s).\n", uname);
	close(skfd);
	return 1;
_pwd_err:
	printf("[ERROR] The server refuse to login. Password error.\n");
	close(skfd);
	return 4;
}

int eaplogoff(void)
{
	int skfd;
	struct sockaddr_ll ll;
	int state;
	int i;

	printf("[0] Initilize interface...\n");
	if (0 != eapol_init(&skfd, (struct sockaddr*)&ll))
		return -1;
	printf("[1] Requset logoff...\n");
	for (i = 0; i < TRY_TIMES; ++i) {
		eapol_logoff(skfd, (struct sockaddr*)&ll);
		state = filte_success(skfd, (struct sockaddr*)&ll);
		if (-2 == state) {
			printf("[2] Logoff!\n");
			return 0;
		}
		printf(" [1] %dth Try Requset logoff...\n", i+1);
	}
	printf("[ERROR] Not server in range. or You were logoff.\n");
	return -1;
}

int eaprefresh(char const *uname, char const *pwd)
{
	return eaplogin(uname, pwd);
}

/* 设置ifname */
void setifname(char *_ifname)
{
	strncpy(ifname, _ifname, IFNAMSIZ);
}
