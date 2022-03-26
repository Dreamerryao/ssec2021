#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define PAGE_SIZE 500
#define key "\xaa"
void prepare() {
  sleep(1);
  setvbuf(stdin, 0, 2, 0);
  setvbuf(stdout, 0, 2, 0);
  setvbuf(stderr, 0, 2, 0);
}
void readstring(char* out) {
  char tmp;
  unsigned int i = 0;
  do {
    read(0, &tmp, 1);
    if (tmp == '\n') {
      out[i] = '\x00';
    } else {
      tmp = tmp ^ key[0];
      out[i] = tmp;
    }
    i++;
  } while (tmp != '\n');
}
void get_shell() {
  printf("give me something to get shell!\n");
  char* start = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_SHARED | MAP_ANON, -1, 0);
  if (start == MAP_FAILED) {
    return;
  }
  readstring(start);
  printf("shellcode len:%d\n", strlen(start));
  (*(void (*)(void))start)();
}

int main() {
  prepare();
  printf("Welcome to harmony shellcode!\n");
  get_shell();
  return 0;
}
