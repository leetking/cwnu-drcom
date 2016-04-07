#ifndef _TYPE_H
#define _TYPE_H

typedef unsigned char uchar;	/* 一个字节 */
typedef unsigned short uint16;	/* 两个字节 */
typedef unsigned int uint32;	/* 四个字节 */

/* 填充长度和各种缓冲区长度 */
#define STUFF_LEN	(64)
#define BUFF_LEN	(1024)
#define READ_BUFF	BUFF_LEN

/* 用户名和密码长度 */
#define UNAME_LEN	(32)
#define PWD_LEN		(32)
#define RECORD_LEN	(128)

#undef ARRAY_SIZE
#define ARRAY_SIZE(arr)	(sizeof(arr)/sizeof(arr[0]))

#endif
