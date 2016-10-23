#include "wrap_eapol.h"
#include "common.h"
#include "eapol.h"

extern int try_smart_login(char const *uname, char const *pwd)
{
    iflist_t ifs[IFS_MAX];
    int ifs_max = IFS_MAX;
	int ret = -3;
    if (0 >= getall_ifs(ifs, &ifs_max)) return -3;
	int i;
    for (i = 0; i < ifs_max; ++i) {
        _M("%d. try interface (%s) to login...\n", i,
#ifdef LINUX
                ifs[i].name
#elif defined(WINDOWS)
                ifs[i].desc
#endif
          );
        setifname(ifs[i].name);
        int statu;
        switch (statu = eaplogin(uname, pwd)) {
            default:
                break;
            case 0:
            case 1:
            case 2:
				ret = statu;
        }
    }
	return ret;
}

extern void try_smart_logoff(void)
{
    iflist_t ifs[IFS_MAX];
    int ifs_max = IFS_MAX;
	int ret = -3;
    if (0 >= getall_ifs(ifs, &ifs_max)) return;
	int i;
    for (i = 0; i < ifs_max; ++i) {
        _M("%d. try interface (%s) to logoff...\n", i,
#ifdef LINUX
                ifs[i].name
#elif defined(WINDOWS)
                ifs[i].desc
#endif
          );
        setifname(ifs[i].name);
		eaplogoff();
	}
}
