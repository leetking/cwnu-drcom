#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

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

#include "eapol.h"
#include "common.h"
#include "md5.h"


#define BUFFER_LEN      512

static u8 client_mac[ETH_ALEN];

static u8 sendbuff[BUFFER_LEN];
static u8 recvbuff[BUFFER_LEN];
static char ifname[IF_NAMESIZE] = "eth0";
static ethII_t *sendethii, *recvethii;
static eapol_t *sendeapol, *recveapol;
static eap_t *sendeap, *recveap;
static eapbody_t *sendeapbody, *recveapbody;

static char username[USERNAME_LEN+1];
static char password[PASSWORD_LEN+1];
static int password_len;

static int eap_keep_alive(int skfd, struct sockaddr_ll const *skaddr);
static int eap_md5_clg(int skfd, struct sockaddr_ll const *skaddr);
static int eap_res_identity(int skfd, struct sockaddr_ll const *skaddr);
static int eapol_init(int *skfd, struct sockaddr_ll *skaddr);
static int eapol_start(int skfd, struct sockaddr_ll const *skaddr);
static int eapol_logoff(int skfd, struct sockaddr_ll const *skaddr);
static int filter_req_identity(int skfd, struct sockaddr_ll const *skaddr);
static int filter_req_md5clg(int skfd, struct sockaddr_ll const *skaddr);
static int filter_success(int skfd, struct sockaddr_ll const *skaddr);
static int eap_daemon(int skfd, struct sockaddr_ll const *skaddr);


/**
 * 初始化缓存区，生产套接字和地址接口信息
 * skfd: 被初始化的socket
 * skaddr: 被初始化地址接口信息
 * @return: 0: 成功
 *          -1: 初始化套接字失败
 *          -2: 初始化地址信息失败
 */
static int eapol_init(int *skfd, struct sockaddr_ll *skaddr)
{
    struct ifreq ifr;
    sendethii = (ethII_t*)sendbuff;
    sendeapol = (eapol_t*)((u8*)sendethii+sizeof(ethII_t));
    sendeap = (eap_t*)((u8*)sendeapol+sizeof(eapol_t));
    sendeapbody = (eapbody_t*)((u8*)sendeap+sizeof(eap_t));
    recvethii = (ethII_t*)recvbuff;
    recveapol = (eapol_t*)((u8*)recvethii+sizeof(ethII_t));
    recveap = (eap_t*)((u8*)recveapol+sizeof(eapol_t));
    recveapbody = (eapbody_t*)((u8*)recveap+sizeof(eap_t));

    if (-1 == (*skfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL)))) {
        perror("Socket");
        return -1;
    }
    /* 先假定就是eth0接口 */
    memset(skaddr, 0, sizeof(struct sockaddr_ll));
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ifname, IF_NAMESIZE);
    if (-1 == ioctl(*skfd, SIOCGIFINDEX, &ifr)) {
        perror("Get index");
        goto addr_err;
    }
    skaddr->sll_ifindex = ifr.ifr_ifindex;
    _D("%s's index: %d\n", ifname, skaddr->sll_ifindex);
    if (-1 == ioctl(*skfd, SIOCGIFHWADDR, &ifr)) {
        perror("Get MAC");
        goto addr_err;
    }
    memcpy(client_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    _D("%s's MAC: %02X-%02X-%02X-%02X-%02X-%02X\n", ifname,
            client_mac[0],client_mac[1],client_mac[2],
            client_mac[3],client_mac[4],client_mac[5]);
    skaddr->sll_family = PF_PACKET;
    /*skllp->sll_protocol = ETH_P_ARP;*/
    /*skllp->sll_ifindex = ? 已给出 */
    skaddr->sll_hatype = ARPHRD_ETHER;
    skaddr->sll_pkttype = PACKET_HOST;
    skaddr->sll_halen = ETH_ALEN;
    return 0;

addr_err:
    close(*skfd);
    return -2;
}


/**
 * 过滤得到eap-request-identity包
 * @return: 0: 成功获取
 *          -1: 超时
 */
static int filter_req_identity(int skfd, struct sockaddr_ll const *skaddr)
{
    (void)skaddr;
    int stime = time((time_t*)NULL);
    for (; difftime(time((time_t*)NULL), stime) <= TIMEOUT;) {
        /* TODO 看下能不能只接受某类包，包过滤 */
        recvfrom(skfd, recvbuff, BUFFER_LEN, 0, NULL, NULL);
        /* eap包且是request */
        if (recvethii->type == htons(ETHII_8021X)
                && (memcmp(recvethii->dst_mac, client_mac, ETH_ALEN) == 0)
                && recveapol->type == EAPOL_PACKET
                && recveap->code == EAP_CODE_REQ
                && recveap->type == EAP_TYPE_IDEN) {
            return 0;
        }
    }
    return -1;
}


/**
 * 过滤得到eap-request-md5clg包
 * @return: 0: 成功获取
 *          -1: 超时
 *          -2: 服务器中止登录，用户名不存在
 */
static int filter_req_md5clg(int skfd, struct sockaddr_ll const *skaddr)
{
    (void)skaddr;
    int stime = time((time_t*)NULL);
    for (; difftime(time((time_t*)NULL), stime) <= TIMEOUT;) {
        recvfrom(skfd, recvbuff, BUFFER_LEN, 0, NULL, NULL);
        /* 是request且是eap-request-md5clg */
        if (recvethii->type == htons(ETHII_8021X)
                && (memcmp(recvethii->dst_mac, client_mac, ETH_ALEN) == 0)
                && recveapol->type == EAPOL_PACKET ) {
            if (recveap->code == EAP_CODE_REQ
                    && recveap->type == EAP_TYPE_MD5) {
#ifdef DEBUG
                _M("id: %d\n", sendeap->id);
                _M("md5: ");
                int i;
                for (i = 0; i < recveapbody->md5size; ++i)
                    _M("%.2x", recveapbody->md5value[i]);
                _M("\n");
                _M("ex-md5: ");
                for (i = 0; i < ntohs(recveap->len) - recveapbody->md5size - 2; ++i)
                    _M("%.2x", recveapbody->md5exdata[i]);
                _M("\n");
#endif
                return 0;
            } else if (recveap->id == sendeap->id
                    && recveap->code == EAP_CODE_FAIL) {
                _D("id: %d fail.\n", sendeap->id);
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
static int filter_success(int skfd, struct sockaddr_ll const *skaddr)
{
    (void)skaddr;
    int stime = time((time_t*)NULL);
    for (; difftime(time((time_t*)NULL), stime) <= TIMEOUT;) {
        recvfrom(skfd, recvbuff, BUFFER_LEN, 0, NULL, NULL);
        if (recvethii->type == htons(ETHII_8021X)
                && (memcmp(recvethii->dst_mac, client_mac, ETH_ALEN) == 0)
                && recveapol->type == EAPOL_PACKET ) {
            if (recveap->id == sendeap->id
                    && recveap->code == EAP_CODE_SUCS) {
                _D("id: %d login success.\n", sendeap->id);
                return 0;
            } else if (recveap->id == sendeap->id
                    && recveap->code == EAP_CODE_FAIL) {
                _D("id: %d fail.\n", sendeap->id);
                return -2;
            }
        }
    }
    return -1;
}


/*
 * 广播发送eapol-start
 */
static int eapol_start(int skfd, struct sockaddr_ll const *skaddr)
{
    /* 这里采用eap标记的组播mac地址，也许采用广播也可以吧 */
    u8 broadcast_mac[ETH_ALEN] = {
        /* 0x01, 0x80, 0xc2, 0x00, 0x00, 0x03, */
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    memcpy(sendethii->dst_mac, broadcast_mac, ETH_ALEN);
    memcpy(sendethii->src_mac, client_mac, ETH_ALEN);
    sendethii->type = htons(ETHII_8021X);
    sendeapol->ver = EAPOL_VER;
    sendeapol->type = EAPOL_START;
    sendeapol->len = 0x0;
    sendto(skfd, sendbuff, ETH_ALEN*2+6, 0, (struct sockaddr*)skaddr, sizeof(*skaddr));
    return 0;
}


/* 退出登录 */
static int eapol_logoff(int skfd, struct sockaddr_ll const *skaddr)
{
    u8 broadcast_mac[ETH_ALEN] = {
        // 0x01, 0x80, 0xc2, 0x00, 0x00, 0x03,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    memcpy(sendethii->dst_mac, broadcast_mac, ETH_ALEN);
    memcpy(sendethii->src_mac, client_mac, ETH_ALEN);
    sendethii->type = htons(ETHII_8021X);
    sendeapol->ver = EAPOL_VER;
    sendeapol->type = EAPOL_LOGOFF;
    sendeapol->len = 0x0;
    sendeap->id = EAPOL_LOGOFF_ID;
    sendto(skfd, sendbuff, ETH_ALEN*2+6, 0,(struct sockaddr*)skaddr, sizeof(*skaddr));
    return 0;
}


/* 回应request-identity */
static int eap_res_identity(int skfd, struct sockaddr_ll const *skaddr)
{
    memcpy(sendethii->dst_mac, recvethii->src_mac, ETH_ALEN);
    sendeapol->type = EAPOL_PACKET;
    sendeapol->len = htons(sizeof(eap_t)+sizeof(eapbody_t));
    sendeap->code = EAP_CODE_RES;
    sendeap->id = recveap->id;
    sendeap->len = htons(sizeof(eapbody_t));
    sendeap->type = EAP_TYPE_IDEN;
    memcpy(sendeapbody->identity, username, USERNAME_LEN);
    sendto(skfd, sendbuff, ETH_ALEN*2+6+5+sizeof(eapbody_t),
            0, (struct sockaddr*)skaddr, sizeof(*skaddr));
    return 0;
}


/* 回应md5clg */
static int eap_md5_clg(int skfd, struct sockaddr_ll const *skaddr)
{
    u8 md5buff[BUFFER_LEN];
    sendeap->id = recveap->id;
    sendeap->len = htons(sizeof(eapbody_t));
    sendeap->type = EAP_TYPE_MD5;
    sendeapbody->md5size = recveapbody->md5size;
    memcpy(md5buff, &sendeap->id, 1);
    memcpy(md5buff+1, password, password_len);
    memcpy(md5buff+1+password_len, recveapbody->md5value, recveapbody->md5size);
    MD5(md5buff, 1+password_len+recveapbody->md5size, sendeapbody->md5value);
    memcpy((char*)sendeapbody->md5exdata, username, strlen(username));
    sendto(skfd, sendbuff, ETH_ALEN*2+6+5+sizeof(eapbody_t),
            0, (struct sockaddr*)skaddr, sizeof(*skaddr));
    return 0;
}

/**
 * 保持在线
 * eap心跳包
 * 某些eap实现需要心跳或多次认证
 * 目前有些服务器会有如下特征
 * 每一分钟，服务端发送一个request-identity包来判断是否在线
 */
static int eap_keep_alive(int skfd, struct sockaddr_ll const *skaddr)
{
    int status;
    time_t stime, etime;
    /* EAP_KPALV_TIMEOUT时间内已经不再有心跳包，我们认为服务器不再需要心跳包了 */
    /* for (; difftime(time((time_t*)NULL), stime) <= EAP_KPALV_TIMEOUT; ) { */
    stime = time((time_t*)NULL);
    for (;;) {
        status = filter_req_identity(skfd, skaddr);
        /* _D("%s: [EAP:KPALV] get status: %d\n", format_time(), status); */
        if (0 == status) {
            etime = time((time_t*)NULL);
            _D("dtime: %fs\n", difftime(etime, stime));
            if (difftime(etime, stime) <= 10) {
                stime = time((time_t*)NULL);
                continue;
            }
            stime = time((time_t*)NULL);
            _M("%s: [EAP:KPALV] get a request-identity\n", format_time());
            eap_res_identity(skfd, skaddr);
        }
        status = -1;
    }
    return 0;
}


/**
 * 后台心跳进程
 * @return: 0, 正常运行
 *         -1, 运行失败
 */
static int eap_daemon(int skfd, struct sockaddr_ll const *skaddr)
{
    /* 如果存在原来的keep alive进程，就干掉他 */
#define PID_FILE        "/tmp/cwnu-drcom-eap.pid"
    FILE *kpalvfd = fopen(PID_FILE, "r+");
    if (NULL == kpalvfd) {
        _M("[EAP:KPALV] No process pidfile. %s: %s\n", PID_FILE, strerror(errno));
        kpalvfd = fopen(PID_FILE, "w+"); /* 不存在，创建 */
        if (NULL == kpalvfd) {
            _M("[EAP:KPALV] Detect pid file eror(%s)! quit!\n", strerror(errno));
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
    if (0 != chdir("/"))
        _M("[EAP:KPALV:WARN] %s\n", strerror(errno));
    umask(0);
    /* 在/tmp下写入自己(keep alive)pid */
    pid_t curpid = getpid();
    _D("kpalv curpid: %d\n", curpid);
    /*
     * if (0 != ftruncate(fileno(kpalvfd), 0))
     * 这个写法有时不能正常截断文件，截断后前面有\0？
     */
    if (NULL == (kpalvfd = freopen(PID_FILE, "w+", kpalvfd)))
        _M("[EAP:KPALV:WARN] truncat pidfile '%s': %s\n", PID_FILE, strerror(errno));
    fprintf(kpalvfd, "%d", curpid);
    fflush(kpalvfd);
    if (0 == eap_keep_alive(skfd, skaddr)) {
        _M("%s: [EAP:KPALV] Server maybe not need keep alive paket.\n", format_time());
        _M("%s: [EAP:KPALV] Now, keep alive process quit!\n", format_time());
    }
    if (NULL == (kpalvfd = freopen(PID_FILE, "w+", kpalvfd)))
        _M("[EAP:KPALV:WARN] truncat pidfile '%s': %s\n", PID_FILE, strerror(errno));
    fprintf(kpalvfd, "-1"); /* 写入-1表示已经离开 */
    fflush(kpalvfd);
    fclose(kpalvfd);

    return 0;
}


/**
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

    _M("Use user '%s' to login...\n", uname);
    _M("[EAP:0] Initilize interface...\n");
    strncpy(username, uname, USERNAME_LEN+1);
    strncpy(password, pwd, PASSWORD_LEN+1);
    password_len = strlen(password);
    if (0 != eapol_init(&skfd, &ll))
        return -1;
    /* 无论如何先请求一下下线 */
    eapol_logoff(skfd, &ll);
    /* eap-start */
    _M("[EAP:1] Send eap-start...\n");
    for (i = 0; i < TRY_TIMES; ++i) {
        eapol_start(skfd, &ll);
        if (0 == filter_req_identity(skfd, &ll))
            break;
        _M(" [EAP:1] %dth Try send eap-start...\n", i+1);
    }
    if (i >= TRY_TIMES) goto timeout;

    /* response-identity */
    _M("[EAP:2] Send response-identity...\n");
    for (i = 0; i < TRY_TIMES; ++i) {
        eap_res_identity(skfd, &ll);
        state = filter_req_md5clg(skfd, &ll);
        if (0 == state) break;
        else if (-2 == state) goto no_such_user;
        _M(" [EAP:2] %dth Try send response-identity...\n", i+1);
    }
    if (i >= TRY_TIMES) goto timeout;

    /* response-md5clg */
    _M("[EAP:3] Send response-md5clg...\n");
    for (i = 0; i < TRY_TIMES; ++i) {
        eap_md5_clg(skfd, &ll);
        state = filter_success(skfd, &ll);
        if (0 == state) break;  /* 登录成功 */
        else if (-2 == state) goto incorrect_password;
        _M(" [EAP:3] %dth Try send response-md5clg...\n", i+1);
    }
    if (i >= TRY_TIMES) goto timeout;

    /* 登录成功，生成心跳进程 */
    switch (fork()) {
    case 0:
        if (0 != eap_daemon(skfd, &ll)) {
            _M("[EAP:ERROR] Create daemon process to keep alive error!\n");
            close(skfd);
            exit(1);
        }
        exit(0);
        break;
    case -1:
        _M("[EAP:WARN] Cant create daemon, maybe `OFFLINE` after soon.\n");
    }
    close(skfd);
    return 0;

timeout:
    _M("[EAP:ERROR] Not server in range.\n");
    close(skfd);
    return -2;
no_such_user:
    _M("[EAP:ERROR] No this user(%s).\n", uname);
    close(skfd);
    return 1;
incorrect_password:
    _M("[EAP:ERROR] The server refuse to login. Password error.\n");
    close(skfd);
    return 4;
}


int eaplogoff(void)
{
    int skfd;
    struct sockaddr_ll ll;
    int state;
    int i;

    _M("[EAP:0] Initilize interface...\n");
    if (0 != eapol_init(&skfd, &ll))
        return -1;
    _M("[EAP:1] Requset logoff...\n");
    for (i = 0; i < TRY_TIMES; ++i) {
        eapol_logoff(skfd, &ll);
        state = filter_success(skfd, &ll);
        if (-2 == state) {
            _M("[EAP:2] Logoff!\n");
            return 0;
        }
        _M(" [EAP:1] %dth Try Requset logoff...\n", i+1);
    }
    _M("[EAP:ERROR] Not server in range. or You were logoff.\n");
    return -1;
}


int eaprefresh(char const *uname, char const *pwd)
{
    return eaplogin(uname, pwd);
}


/* 设置ifname */
void setifname(char *_ifname)
{
    strncpy(ifname, _ifname, IF_NAMESIZE);
}
