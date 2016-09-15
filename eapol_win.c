#include "common.h"
#include "eapol.h"
#include "md5.h"

#include <windows.h>
#include <pcap.h>
#include <packet32.h>		/* 需要连接-lpacket */

#ifdef _MINGW64   /* now, not used */
# include <ddk/ntddndis.h>	/* 用于获取mac */
#else
# include <ntddndis.h>
#endif

#include <stdio.h>
#include <string.h>

#define BUFF_LEN	(1024)

static uchar client_mac[ETH_ALEN];

static uchar sendbuff[BUFF_LEN];
static char ifname[IFNAMSIZ] = "\\Device\\NPF_{AEDD3BFA-33D3-4B29-B1FC-0B82C65E42D3}";
static ethII_t *sendethii, *recvethii;
static eapol_t *sendeapol, *recveapol;
static eap_t *sendeap, *recveap;
static eapbody_t *sendeapbody, *recveapbody;
#undef FORMAT_RECVPKT
#define FORMAT_RECVPKT(pkt)	\
    do{ \
        recvethii = (ethII_t*)(pkt); \
        recveapol = (eapol_t*)((uchar*)recvethii+sizeof(ethII_t)); \
        recveap = (eap_t*)((uchar*)recveapol+sizeof(eapol_t)); \
        recveapbody = (eapbody_t*)((uchar*)recveap+sizeof(eap_t)); \
    }while(0)

static char _uname[UNAME_LEN];
static char _pwd[PWD_LEN];
static int pwdlen;

static int fork_eap_daemon();
static int eap_keep_alive(pcap_t *skfd);
static int eap_md5_clg(pcap_t *skfd);
static int eap_res_identity(pcap_t *skfd);
static int eapol_init(pcap_t **skfd);
static int eapol_logoff(pcap_t *skfd);
static int eapol_start(pcap_t *skfd);
static int filte_req_identity(pcap_t *skfd);
static int filte_req_md5clg(pcap_t *skfd);
static int filte_success(pcap_t *skfd);

/*
 * 初始化缓存区，生成接口句柄
 * skfd: 被初始化的接口句柄
 * @return: 0: 成功
 *          -1: 初始化接口句柄失败
 */
static int eapol_init(pcap_t **skfd)
{
    pcap_if_t *alldevs, *d;
    char errbuf[PCAP_ERRBUF_SIZE];
    char ifbuff[8+IFNAMSIZ] = "rpcap://";

    sendethii = (ethII_t*)sendbuff;
    sendeapol = (eapol_t*)((uchar*)sendethii+sizeof(ethII_t));
    sendeap = (eap_t*)((uchar*)sendeapol+sizeof(eapol_t));
    sendeapbody = (eapbody_t*)((uchar*)sendeap+sizeof(eap_t));

    if (-1 == pcap_findalldevs(&alldevs, errbuf)) {
        _M("Get interface: %s\n", errbuf);
        return -1;
    }

    for (d = alldevs; NULL != d; d = d->next)
        if (0 == strcmp(ifname, d->name))
            break;
    if (NULL == d) return -1;
    /* 获取mac */
    LPADAPTER lpAdapter = PacketOpenAdapter(d->name);
    if (!lpAdapter || (lpAdapter->hFile == INVALID_HANDLE_VALUE))
        return -1;

    PPACKET_OID_DATA oidData = malloc(ETH_ALEN + sizeof(PACKET_OID_DATA));
    if (NULL == oidData) {
        PacketCloseAdapter(lpAdapter);
        return -1;
    }
    oidData->Oid = OID_802_3_CURRENT_ADDRESS;
    oidData->Length = ETH_ALEN;
    memset(oidData->Data, 0, ETH_ALEN);
    if (0 == PacketRequest(lpAdapter, FALSE, oidData)) {
        free(oidData);
        return -1;
    }
    memcpy(client_mac, oidData->Data, ETH_ALEN);
    PacketCloseAdapter(lpAdapter);
    _D("%s's MAC: %02X-%02X-%02X-%02X-%02X-%02X\n", ifname,
            client_mac[0],client_mac[1],client_mac[2],
            client_mac[3],client_mac[4],client_mac[5]);

    /* 获取网络接口句柄 */
    strncat(ifbuff, ifname, IFNAMSIZ);
    if (NULL == (*skfd = pcap_open(d->name, MTU_MAX,
                    PCAP_OPENFLAG_PROMISCUOUS, TIMEOUT*1000, NULL, errbuf))) {
        _M("Get interface handler:%s\n", errbuf);
        pcap_freealldevs(alldevs);
        return -1;
    }
    pcap_freealldevs(alldevs);

    return 0;
}

/*
 * 过滤得到eap-request-identity包
 * @return: 0: 成功获取
 *          -1: 超时
 */
static int filte_req_identity(pcap_t *skfd)
{
    int stime = time((time_t*)NULL);
    struct pcap_pkthdr *pkt_hd;
    const uchar *recvbuff;
    int timeout;

    for (; time((time_t*)NULL)-stime <= TIMEOUT;) {
        timeout = pcap_next_ex(skfd, &pkt_hd, &recvbuff);
        if (0 >= timeout) return -1;
        FORMAT_RECVPKT(recvbuff);
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
static int filte_req_md5clg(pcap_t *skfd)
{
    int stime = time((time_t*)NULL);
    struct pcap_pkthdr *pkt_hd;
    const uchar *recvbuff;
    int timeout;
    for (; time((time_t*)NULL)-stime <= TIMEOUT;) {
        timeout = pcap_next_ex(skfd, &pkt_hd, &recvbuff);
        if (0 >= timeout) return -1;
        FORMAT_RECVPKT(recvbuff);
        /* 是request且是eap-request-md5clg */
        if (recvethii->type == htons(ETHII_8021X)
                && mac_equal(recvethii->dst_mac, client_mac)
                && recveapol->type == EAPOL_PACKET ) {
            if (recveap->code == EAP_CODE_REQ
                    && recveap->type == EAP_TYPE_MD5) {
#ifdef DEBUG
                _M("id: %d\n", sendeap->id);
                _M("md5: ");
                for (int i = 0; i < recveapbody->md5size; ++i)
                    _M("%.2x", recveapbody->md5value[i]);
                _M("\n");
                _M("ex-md5: ");
                for (int i = 0; i < ntohs(recveap->len) - recveapbody->md5size - 2; ++i)
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
static int filte_success(pcap_t *skfd)
{
    int stime = time((time_t*)NULL);
    struct pcap_pkthdr *pkt_hd;
    const uchar *recvbuff;
    int timeout;
    for (; time((time_t*)NULL)-stime <= TIMEOUT;) {
        timeout = pcap_next_ex(skfd, &pkt_hd, &recvbuff);
        if (0 >= timeout) return -1;
        FORMAT_RECVPKT(recvbuff);
        if (recvethii->type == htons(ETHII_8021X)
                && mac_equal(recvethii->dst_mac, client_mac)
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
static int eapol_start(pcap_t *skfd)
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
    pcap_sendpacket(skfd, sendbuff, ETH_ALEN*2+6);
    return 0;
}
/* 退出登录 */
static int eapol_logoff(pcap_t *skfd)
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
    pcap_sendpacket(skfd, sendbuff, ETH_ALEN*2+6);
    return 0;
}
/* 回应request-identity */
static int eap_res_identity(pcap_t *skfd)
{
    memcpy(sendethii->dst_mac, recvethii->src_mac, ETH_ALEN);
    sendeapol->type = EAPOL_PACKET;
    sendeapol->len = htons(sizeof(eap_t)+sizeof(eapbody_t));
    sendeap->code = EAP_CODE_RES;
    sendeap->id = recveap->id;
    sendeap->len = htons(sizeof(eapbody_t));
    sendeap->type = EAP_TYPE_IDEN;
    strncpy((char*)sendeapbody->identity, _uname, UNAME_LEN);
    pcap_sendpacket(skfd, sendbuff, ETH_ALEN*2+6+5+sizeof(eapbody_t));
    return 0;
}
/* 回应md5clg */
static int eap_md5_clg(pcap_t *skfd)
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
    pcap_sendpacket(skfd, sendbuff, ETH_ALEN*2+6+5+sizeof(eapbody_t));
    return 0;
}
/* 保持在线，实现心跳逻辑 */
static int eap_keep_alive(pcap_t *skfd)
{
    int status;
    time_t stime = time((time_t*)NULL);
    for (; difftime(time((time_t*)NULL), stime) <= EAP_KPALV_TIMEOUT; ) {
        status = filte_req_identity(skfd);
        _D("[KPALV] status: %d\n", status);
        if (0 == status) {
            _D("[KPALV] get a request-identity\n");
            eap_res_identity(skfd);
            stime = time((time_t*)NULL);
        }
    }
    return 0;
}
/* 负责创建心跳进程，并启动它 */
static int fork_eap_daemon(void)
{
    char exe[EXE_PATH_MAX];
    int cnt = GetModuleFileName(NULL, exe, EXE_PATH_MAX);
    if (cnt < 0 || cnt >= EXE_PATH_MAX)
        return -1;
    _D("exename: %s\n", exe);
    int rest = EXE_PATH_MAX-strlen(exe);
    strncat(exe, " -k ", rest);
    rest -= 4;  /* strlen(" -k "); */
    _D("exename: %s\n", exe);
    strncat(exe, ifname, rest);
    _D("exename: %s\n", exe);
    STARTUPINFO sinfo;
    PROCESS_INFORMATION pinfo;
    ZeroMemory(&sinfo, sizeof(sinfo));
    ZeroMemory(&pinfo, sizeof(pinfo));
    if (!CreateProcess(NULL, exe, NULL, NULL, 0, CREATE_NO_WINDOW, NULL, NULL, &sinfo, &pinfo)) {
        _D("create child error!\n");
        return -1;
    }
    CloseHandle(pinfo.hProcess);
    CloseHandle(pinfo.hThread);
    _D("create child process success!\n");

    return 0;
}
extern int eap_daemon(char const *ifname)
{
    pcap_t *skfd;
    setifname((char*)ifname);
    if (0 != eapol_init(&skfd))
        return -1;
    char pid_file[EXE_PATH_MAX];
    if (0 != getexedir(pid_file)) {
        _M("[KPALV] get currently path error!\n");
        return -1;
    }
    int rest = EXE_PATH_MAX-strlen(pid_file);
    strncat(pid_file, "cwnu-drcom.pid", rest);
    _D("pid_file: %s\n", pid_file);
    FILE *kpalvfd = fopen(pid_file, "r+");
    if (NULL == kpalvfd) {
        _M("[KPALV] No process pidfile. %s: %s\n", pid_file, strerror(errno));
        kpalvfd = fopen(pid_file, "w+"); /* 不存在，创建 */
        if (NULL == kpalvfd) {
            _M("[KPALV] Detect pid file eror(%s)! quit!\n", strerror(errno));
            return -1;
        }
    }

#define pid_t DWORD
    pid_t oldpid;
    fseek(kpalvfd, 0L, SEEK_SET);
    if ((1 == fscanf(kpalvfd, "%lu", (unsigned long*)&oldpid)) && (oldpid != (pid_t)-1)) {
        _D("oldkpalv pid: %lu\n", oldpid);
        HANDLE hp = OpenProcess(PROCESS_TERMINATE, 0, oldpid);
        TerminateProcess(hp, 0);
    }
	/* 在可执行程序所在目录下写入自己(keep alive)pid */
    pid_t curpid = GetCurrentProcessId();
    _D("kpalv curpid: %lu\n", curpid);
    /* 使用ansi c的方式使pid_file里的内容被截断为0 */
    if (NULL == (kpalvfd = freopen(pid_file, "w+", kpalvfd)))
        _M("[KPALV:WARN] truncat pidfile '%s': %s\n", pid_file, strerror(errno));
    fprintf(kpalvfd, "%lu", curpid);
    fflush(kpalvfd);
    if (0 == eap_keep_alive(skfd)) {
        _M("[KPALV] Server maybe not need keep alive paket.\n");
        _M("[KPALV] Now, keep alive process quit!\n");
    }
    if (NULL == (kpalvfd = freopen(pid_file, "w+", kpalvfd)))
        _M("[KPALV:WARN] truncat pidfile '%s': %s\n", pid_file, strerror(errno));
    fprintf(kpalvfd, "0");	/* 写入0表示已经离开 */
    fflush(kpalvfd);
    fclose(kpalvfd);
    pcap_close(skfd);

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
    pcap_t *skfd;

    _M("[0] Initilize interface...\n");
    strncpy(_uname, uname, UNAME_LEN);
    strncpy(_pwd, pwd, PWD_LEN);
    pwdlen = strlen(_pwd);
    if (0 != eapol_init(&skfd))
        return -1;
    /* 无论如何先请求一下下线 */
    eapol_logoff(skfd);
    /* eap-start */
    _M("[1] Send eap-start...\n");
    for (i = 0; i < TRY_TIMES; ++i) {
        eapol_start(skfd);
        if (0 == filte_req_identity(skfd))
            break;
        _M(" [1] %dth Try send eap-start...\n", i+1);
    }
    if (i >= TRY_TIMES) goto _timeout;

    /* response-identity */
    _M("[2] Send response-identity...\n");
    for (i = 0; i < TRY_TIMES; ++i) {
        eap_res_identity(skfd);
        state = filte_req_md5clg(skfd);
        if (0 == state) break;
        else if (-2 == state) goto _no_uname;
        _M(" [2] %dth Try send response-identity...\n", i+1);
    }
    if (i >= TRY_TIMES) goto _timeout;

    /* response-md5clg */
    _M("[3] Send response-md5clg...\n");
    for (i = 0; i < TRY_TIMES; ++i) {
        eap_md5_clg(skfd);
        state = filte_success(skfd);
        if (0 == state) break;	/* 登录成功 */
        else if (-2 == state) goto _pwd_err;
        _M(" [3] %dth Try send response-md5clg...\n", i+1);
    }
    if (i >= TRY_TIMES) goto _timeout;

    /* 登录成功创建后台心跳进程 */
    if (0 != fork_eap_daemon()) {
        _M("[ERROR] Create daemon process to keep alive error!\n");
        exit(1);
    }
    pcap_close(skfd);
    return 0;

_timeout:
    _M("[ERROR] Not server in range.\n");
    pcap_close(skfd);
    return -2;
_no_uname:
    _M("[ERROR] No this user(%s).\n", uname);
    pcap_close(skfd);
    return 1;
_pwd_err:
    _M("[ERROR] The server refuse to login. Password error.\n");
    pcap_close(skfd);
    return 4;
}
int eaprefresh(char const *uname, char const *pwd)
{
    return eaplogin(uname, pwd);
}
int eaplogoff(void)
{
    pcap_t *skfd;
    int state;
    int i;

    _M("[0] Initilize interface...\n");
    if (0 != eapol_init(&skfd))
        return -1;
    _M("[1] Requset logoff...\n");
    for (i = 0; i < TRY_TIMES; ++i) {
        eapol_logoff(skfd);
        state = filte_success(skfd);
        if (-2 == state) {
            _M("[2] Logoff!\n");
            return 0;
        }
        _M(" [1] %dth Try Requset logoff...\n", i+1);
    }
    _M("[ERROR] Not server in range. or You were logoff.\n");
    pcap_close(skfd);
    return -1;
}
void setifname(char *_ifname)
{
    strncpy(ifname, _ifname, IFNAMSIZ);
}
