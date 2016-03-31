#include "eapol.h"

#include <stdio.h>

int main(int argc, char **argv)
{
	char uname[] = "uname";
	char pwd[] = "pwd";

	if (0 == eaplogin(uname, pwd, NULL, NULL))
		printf("Login success!!\n");

	return 0;
}


