#ifndef EAPOL_H__
#define EAPOL_H__

#include "common.h"

#define IDEN_LEN        USERNAME_LEN

#define TRY_TIMES       3
/* 每次请求超过TIMEOUT秒，就重新请求一次 */
#define TIMEOUT         3
/* eap 在EAP_KPALV_TIMEOUT秒内没有回应，认为不需要心跳 */
#define EAP_KPALV_TIMEOUT 420   /* 7分钟 */

/* 最多32个接口 */
#define IFS_MAX         32

/* ethii层取0x888e表示上层是8021.x */
#define ETHII_8021X     0x888e

#define EAPOL_VER       0x01u
#define EAPOL_PACKET    0x00u
#define EAPOL_START     0x01u
#define EAPOL_LOGOFF    0x02u
/* 貌似请求下线的id都是这个 */
#define EAPOL_LOGOFF_ID 255u

#define EAP_CODE_REQ    0x01u
#define EAP_CODE_RES    0x02u
#define EAP_CODE_SUCS   0x03u
#define EAP_CODE_FAIL   0x04u
#define EAP_TYPE_IDEN   0x01u
#define EAP_TYPE_MD5    0x04u


#pragma pack(1)
/* ethii 帧 */
/* 其实这个和struct ether_header是一样的结构 */
typedef struct {
    u8 dst_mac[ETH_ALEN];
    u8 src_mac[ETH_ALEN];
    u16 type;               /* 取值0x888e，表明是8021.x */
} ethII_t;

/* eapol 帧 */
typedef struct {
    u8 ver;                 /* 取值0x01 */
    /*
     * 0x00: eapol-packet
     * 0x01: eapol-start
     * 0x02: eapol-logoff
     */
    u8 type;
    u16 len;
} eapol_t;

/* eap报文头 */
typedef struct {
    /*
     * 0x01: request
     * 0x02: response
     * 0x03: success
     * 0x04: failure
     */
    u8 code;
    u8 id;
    u16 len;
    /*
     * 0x01: identity
     * 0x04: md5-challenge
     */
    u8 type;
} eap_t;

/* 报文体 */
#define MD5_SIZE        16
#define STUFF_LEN       64
typedef union {
    u8 identity[IDEN_LEN];
    struct {
        u8 _size;
        u8 _md5value[MD5_SIZE];
        u8 _exdata[STUFF_LEN];
    } md5clg;
} eapbody_t;
#define md5size         md5clg._size
#define md5value        md5clg._md5value
#define md5exdata       md5clg._exdata

#pragma pack() /* end of pack(1) */

/**
 * eap认证
 * username: 用户名
 * password: 密码
 * @return: 0: 成功
 *          1: 用户不存在
 *          2: 密码错误
 *          3: 其他超时
 *          4: 服务器拒绝请求登录
 *          -1: 没有找到合适网络接口
 *          -2: 没有找到服务器
 */
extern int eaplogin(char const *username, char const *password);

/**
 * eap下线
 */
extern int eaplogoff(void);

/**
 * eap重新登录
 */
extern int eaprefresh(char const *username, char const *password);

/**
 * 用来设置ifname
 */
extern void setifname(char *ifname);

#undef IDEN_LEN

#endif /* EAPOL_H__ */
