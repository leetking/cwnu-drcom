#ifndef CONFIG_H__
#define CONFIG_H__
/*
 * 一个简单的配置文件解析函数
 */
#include <stdio.h>

#define RECORD_LEN      216

/*
 * 读取配置文件里的值
 * key: 待读取的关键字
 * value: 读取的值存储在value里，value长度要长于RECORD_LEN
 * @reutrn: 0: 成功
 *          -1: 配置文件格式错误
 *          -2: 没有这个key
 *          -3: key或value长度大于RECORD_LEN
 *
 * [配置文件样例],以网络配置为例
 * # 这是注释
 * ethif = "eth0"           # 网络接口
 * username = "201473740838"   # 用户名
 * password = "password"              # 密码
 * [注意]
 * key必须是符合c标识符格式的字符串, value值全部用"包含
 */
extern int getvalue(FILE *conf, char const *key, char *value);

#endif
