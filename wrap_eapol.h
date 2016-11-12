#ifndef _WRAP_EAPOL_H
#define _WRAP_EAPOL_H
/*
 * 对eapol登录的一个包裹，实现自己选择网卡登录
 * 也会在环境变量(DRCOM_IFNAME)里设置此次成功登录使用的网卡名字
 * e.g. setenv("DRCOM_IFNAME", "eth0");
 */

/*
 * 自动选择网卡登录
 * 尝试所有网卡进行登录，直到一个可正常使用网卡。
 * 并且在环境变量(DRCOM_IFNAME)里记录此次使用的网卡
 */
extern int try_smart_login(char const *uname, char const *pwd);
/*
 * 自动选择网卡离线
 * 优先从环境变量(DRCOM_IFNAME)里选择出网卡进行尝试，
 * 失败再遍历所有网卡
 */
extern void try_smart_logoff(void);

#endif
