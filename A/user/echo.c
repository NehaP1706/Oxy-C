#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void print_sp() {
  register unsigned long sp asm("sp");
  printf("Userland SP at entry: 0x%lx\n", sp);
}

int main(int argc, char *argv[])
{
  // printf("argv = %p\n", argv);
  // printf("[echo] argc=%d\n", argc);
  // for(int i = 0; i < argc; i++) {
  //   printf("[echo] argv[%d]=%p, string='%s'\n", i, argv[i], argv[i] ? argv[i] : "(null)");
  // }

  // print_sp();

  for(int i = 1; i < argc; i++){
    write(1, argv[i], strlen(argv[i]));
    if(i + 1 < argc){
      write(1, " ", 1);
    } else {
      write(1, "\n", 1);
    }
  }
  exit(0);
}
