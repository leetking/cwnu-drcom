#include <stdlib.h>
#include "wrap_eapol.h"
#include "common.h"
#include "eapol.h"

/**
 * 自动选择网卡登录
 * 尝试所有网卡进行登录，直到一个可正常使用网卡。
 * 并且在环境变量(DRCOM_IFNAME)里记录此次使用的网卡
 */
extern int try_smart_login(char const *username, char const *password)
{
    ifname_t ifs[IFS_MAX];
    int ifs_max = IFS_MAX;
    int ret = -3;
    if (0 >= getall_ifs(ifs, &ifs_max)) return -3;
    int i;
    for (i = 0; i < ifs_max; ++i) {
        _M("%d. try interface (%s) to login...\n", i, ifs[i].name);
        setifname(ifs[i].name);
        int state;
        switch (state = eaplogin(username, password)) {
        default:
            break;
        case 0:
            /*
             * 登录成功！
             * 设置环境变量
             */
            setenv("DRCOM_IFNAME", ifs[i].name, 1);
        case 1:
        case 2:
            return state;
        }
    }
    return ret;
}


/**
 * 自动选择网卡离线
 * TODO 优先从环境变量(DRCOM_IFNAME)里选择出网卡进行尝试，
 * 失败再遍历所有网卡
 */
extern void try_smart_logoff(void)
{
    ifname_t ifs[IFS_MAX];
    int ifs_max = IFS_MAX;
    if (0 >= getall_ifs(ifs, &ifs_max)) return;
    int i;
    for (i = 0; i < ifs_max; ++i) {
        _M("%d. try interface (%s) to logoff...\n", i, ifs[i].name);
        setifname(ifs[i].name);
        eaplogoff();
    }
}
