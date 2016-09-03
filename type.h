#ifndef _TYPE_H
#define _TYPE_H

typedef unsigned char uchar;	/* 一个字节 */
typedef unsigned short uint16;	/* 两个字节 */
typedef unsigned int uint32;	/* 四个字节 */

/* 用户名和密码长度 */
#define UNAME_LEN	(32)
#define PWD_LEN		(32)

#undef ARRAY_SIZE
#define ARRAY_SIZE(arr)	(sizeof(arr)/sizeof(arr[0]))

#define MAX(x, y)	((x)>(y)?(x):(y))
#define MIN(x, y)	((x)>(y)?(y):(x))

#ifdef DEBUG
# define _D(...) fprintf(stderr, __VA_ARGS__)
#else
# define _D(...) ((void)0)
#endif

#define _M(...) fprintf(stderr, __VA_ARGS__)

#endif
