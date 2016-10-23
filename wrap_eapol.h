#ifndef _WRAP_EAPOL_H
#define _WRAP_EAPOL_H
/*
 * 对eapol登录的一个包裹，实现自己选择网卡登录
 */

/*
 * 自动选择网卡登录
 */
extern int try_smart_login(char const *uname, char const *pwd);
/*
 * 自动选择网卡离线
 */
extern void try_smart_logoff(void);

#endif
