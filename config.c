#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "config.h"


#define BUFFER_LEN      (2*RECORD_LEN+20)

int getvalue(FILE *conf, char const *key, char *value)
{
    char readbuff[BUFFER_LEN];
    char keybuff[RECORD_LEN];
    char *pb, *pkey, *pval;
    fseek(conf, 0L, SEEK_SET);
    while (NULL != fgets(readbuff, BUFFER_LEN, conf)) {
        pb = readbuff;
        pkey = keybuff;
        pval = value;
        /* 是否是注释 */
        while (isspace(*pb))
            ++pb;
        if ('#' == *pb || '\0' == *pb)
            continue;
        /* 解析变量 */
        while ('\0' != *pb && ('_' == *pb || isalnum(*pb))) {
            if (pkey >= keybuff+RECORD_LEN)
                return -3;
            *pkey++ = *pb++;
        }
        *pkey = '\0';
        /* 等号 */
        while ('=' != *pb && '\0' != *pb)
            ++pb;
        if ('\0' == *pb)
            continue;
        while (isspace(*++pb))
            ;
        if ('\0' == *pb)
            continue;
        /* value */
        if ('"' == *pb)
            ++pb;
        while ('\0' != *pb && '"' != *pb) {
            if (pval >= value+RECORD_LEN)
                return -3;
            *pval++ = *pb++;
        }
        *pval = '\0';
        if (!strcmp(key, keybuff)) {
#ifdef DEBUG
            printf("getvalue: %s-%s\n", key, value);
#endif /* DEBUG */
            return 0;
        }
    }
    return -2;
}
