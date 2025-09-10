#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: readcount <filename>\n");
    exit(1);
  }

  printf("Initial: %ld\n", getreadcount());

  int fd = open(argv[1], 0);
  if (fd < 0) {
    printf("Cannot open %s\n", argv[1]);
    exit(1);
  }

  char buf[100];
  read(fd, buf, sizeof(buf));
  close(fd);

  printf("After: %ld\n", getreadcount());
  exit(0);
}
