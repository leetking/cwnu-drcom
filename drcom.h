#ifndef _DRCOM_H
#define _DRCOM_H

#include "common.h"
#include <netinet/in.h>

/*
 * 西华师范大学的drcom认证协议
 * 端口： 61440
 * 基于UDP
 * 数据通常都是大端序，除了特殊注明外
 */

#define DRCOM_PORT	(61440)

typedef struct in_addr ipv4_t;

#pragma pack(1)
typedef struct {
	uchar zeros[3];   	/* 3 bytes 0 */
	uchar flux[4];		/* 4 bytes */
	ipv4_t cip;			/* cient IP */
}drcom_nrml_02_t;
typedef struct {
	uchar usrlen;		/* 本校长度是12,暂且定位12固定的 */
	uchar cmac[ETH_ALEN];
	ipv4_t cip;
	uchar fixed[4];
	uchar flux[4];		/* 回应服务器的flux */
	uchar verify[4];	/* 校验值 */
	uchar zeros[4];		/* 4 bytes 0 */
	uchar user[12];		/* user和hostname总长：28 bytes, 这里为了方便先固定为12 */
	uchar host[16];		/* host: 主机名 */
	uchar ign[16];		/* 后面貌似已经没有影响了长度是nrml_len-上述所占长度*/
}drcom_nrml_03_t;
typedef struct {
	uchar usrlen;
}drcom_nrml_04_t;
typedef struct {
	uchar  step;
	uint16 fixed;		/* 本校是固定值, 0x02d6 */
	uint32 id;
	uint32 zeros1;		/* 4bytes 0 */
	uint32 sercnt;		/* 服务端的计数 */
	uint32 zeros2;		/* 4bytes 0 */
	uint32 cksum;		/* 校验和 */
	ipv4_t clip;		/* 客户ip */
}drcom_nrml_kp_t;
/* 07: normal 类型包 */
typedef struct {
	uchar cnt;
	uint16 len;		/* 包长度，小端 */
	/*
	 * 认证过程（类型）:
	 * 0x01: 开始请求心跳认证
	 * 0x02: 同意心跳请求认证
	 * 0x03: 发送认证信息
	 * 0x04: 认证成功
	 * 0x0b: 心跳包
	 */
	uchar type;
	union {
		uchar zeros[3];	/* 0x01: 3 bytes 0 */
		drcom_nrml_02_t nrml02;
		drcom_nrml_03_t nrml03;
		drcom_nrml_04_t nrml04;
		drcom_nrml_kp_t kp;
	}nrml_body;
}drcom_nrml_t;
/* 4d: msg包 */
typedef struct {
	/*
	 * msg type
	 */
	uchar type;
}drcom_msg_t;
typedef struct {
	/*
	 * 包类型：
	 * 0x01: Start Request
	 * 0x02: Start Response
	 * 0x03: Login Auth
	 * 0x04: Success
	 * 0x05: Failure
	 * 0x06: Logout Auth
	 * 0x07: Normal，我们学校多数是这个包
	 * 0x08: Unknown, Attention!
	 * 0x09: New Password
	 * 0x4d: Message，通知消息
	 * 0xfe: Alive 4 client
	 * 0xff: Alive, Client to server per 20s
	 * 其余字段并没有在我们学校出现，暂且不管了
	 */
	uchar type;
	union {
		drcom_nrml_t   nrml;
		drcom_msg_t    msg;
	}drcom_body;
}drcom_t;
#define drcom_nrml_cnt	     drcom_body.nrml.cnt
#define drcom_nrml_len	     drcom_body.nrml.len
#define drcom_nrml_type      drcom_body.nrml.type
#define drcom_nrml_02flux    drcom_body.nrml.nrml_body.nrml02.flux
#define drcom_nrml_02cip     drcom_body.nrml.nrml_body.nrml03.cip
#define drcom_nrml_03usrlen  drcom_body.nrml.nrml_body.nrml03.usrlen
#define drcom_nrml_03cmac    drcom_body.nrml.nrml_body.nrml03.cmac
#define drcom_nrml_03cip     drcom_body.nrml.nrml_body.nrml03.cip
#define drcom_nrml_03fixed   drcom_body.nrml.nrml_body.nrml03.fixed
#define drcom_nrml_03flux    drcom_body.nrml.nrml_body.nrml03.flux
#define drcom_nrml_03verify  drcom_body.nrml.nrml_body.nrml03.verify
#define drcom_nrml_03zeros   drcom_body.nrml.nrml_body.nrml03.zeros
#define drcom_nrml_03user    drcom_body.nrml.nrml_body.nrml03.user
#define drcom_nrml_03host    drcom_body.nrml.nrml_body.nrml03.host
#define drcom_nrml_04usrlen  drcom_body.nrml.nrml_body.nrml04.usrlen
#define drcom_nrml_kpstep    drcom_body.nrml.nrml_body.kp.step
#define drcom_nrml_kpfixed   drcom_body.nrml.nrml_body.kp.fixed
#define drcom_nrml_kpid      drcom_body.nrml.nrml_body.kp.id
#define drcom_nrml_kpsercnt  drcom_body.nrml.nrml_body.kp.sercnt
#define drcom_nrml_kpcksum   drcom_body.nrml.nrml_body.kp.cksum
#define drcom_nrml_kpclip    drcom_body.nrml.nrml_body.kp.clip

#pragma pack()

/*
 * 设置Dr.com服务器ip
 * 目前支持ipv4 only
 * @return: 0: 成功
 *         !0: 失败
 */
extern int drcom_setserip(char const *ip);
/*
 * 设定用于登录的接口
 * @return: 0: 成功
 *         -1: 失败，也许ifname过长
 */
extern int drcom_setifname(char const *ifname);
/*
 * 登录drcom账户
 * @return: 0: 成功
 *          1: 用户不存在
 *          2: 密码错误
 *          3: 服务器拒绝通讯
 *         -1: 没找到服务器，也许网络不通，也许本网络没有这个服务器
 */
extern int drcom_login(char const *usr, char const *pwd);
/*
 * drcom认证断线
 * @return: 0: 成功
 *         -1: 失败服务器没在网络内
 */
extern int drcom_logoff(void);

#endif
