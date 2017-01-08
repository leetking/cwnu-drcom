/*
 * 生成合法随机mac地址
 *
 * 使用clock_gettime来初始化random，
 * 使得这个程序同时并行时不会出现随机数初始化相同
 * 由于嵌入式环境的特殊原因，靠shell没有获取到高精度时间作为随机函数种子，
 * 故此实现
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
	struct timespec nowtime;
	clock_gettime(CLOCK_MONOTONIC, &nowtime);
	srand((unsigned int)(nowtime.tv_sec+nowtime.tv_nsec));
	printf("ec:17:2f:%02x:%02x:%02x",
			//rand()%0x100, rand()%0x100, rand()%0x100,
			rand()%0x100, rand()%0x100, rand()%0x100);
	return 0;
}


