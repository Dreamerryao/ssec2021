#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LEN 0x20

unsigned int tmp = 0xAA;

void smile(void)
{
    char flag;
    puts("[?] Are you smiling right now? (y/n)");
    flag = getchar();
	getchar();
    printf("[ ] Your answer is: ");
    printf(&flag);
	printf("\n");

	if(flag == 'y' || flag == 'Y')
	{
		printf("\n\n[+] Fine, I just want you to be happy. :)\n");
		int  canary = 0x12345678;
		
		__asm__ __volatile__(
		"ldr %[canary], [sp, #0x4]	\n\t"
            : [canary] "=r" (canary)
            :
            :"memory");

		printf("[!] here is a gift for smiling: %x \n", canary);
	}

    if(tmp++ != 0xAA)
    {
	puts("bad Control FLOW!");
    	exit(0);
    }
    return;
}

void gift(void)
{
	printf("[ ] ...look at here");
	execve("/bin/ls\0", 0, 0);
	
	__asm__ __volatile__(
			"pop {r1, r2, r3}	\n\t"
			"pop {r0, r4, r5, pc}	\n\t"
			"pop {r0, r1, r2, pc}	\n\t"
			"sub sp, sp, #0x10  \n\t"
            		"pop {pc} \n\t"	
	    );

	return;
}

void hear(void)
{
    char buf[LEN];
    puts("[ ] Now Reading:");
    read(STDIN_FILENO, buf, LEN + 0x14);
	return;
}



void prepare(void)
{
    char buf[0x18];
    sleep(1);
    setbuf(stdin, NULL);    
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    puts("----------------------- [ ZJUSSEC final ] -----------------------");
    printf("The addr of hear is: %p \n", hear);	
    printf("The addr of gift is: %p \n", gift);
	printf("The addr of execve is: %p \n", execve);
	int stack=0x87654321;	
    __asm__ __volatile__(
        "mov %[stack], sp	\n\t"
        : [stack] "=r" (stack)
        :
        :"memory");
	printf("The addr of stack is %p \n", stack);	
    sleep(3);
    return;
}

int main(void)
{
    prepare();
    smile();
    hear();
    return 0;
}

