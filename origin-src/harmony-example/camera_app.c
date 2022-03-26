#include <stdio.h>

int main(int argc, char* argv[])
{
	char name[16];
	printf("Input your name: ");
	scanf("%s", name);

	printf("Hello World! %s!", name);

	return 0;
}
