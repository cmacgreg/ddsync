#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"

int main(int argc, char **argv){
  int fd = argc > 1 ? open(argv[1], O_RDONLY) : 0;
  if(fd == -1)
    perror("open"), fprintf(stderr,"usage: %s logfile\n", argv[0]), exit(1);
  
  uint8_t str[2*DIGEST_SIZE+2];
  
  uint8_t buf[DIGEST_SIZE];
  ssize_t got=0, total=0;

  re_read:
  while(got = read(fd, buf, DIGEST_SIZE)){
    if(got == -1)
      perror("read"), exit(1);
    total += got;
    
    int i;
    for (i = 0; i < DIGEST_SIZE; i++) {
      sprintf(str + 2*i, "%.2x", buf[i]);
    }
    
    str[2*DIGEST_SIZE]='\n';
    str[2*DIGEST_SIZE+1]=0;
  
    if(-1 == write(1, str, 2*DIGEST_SIZE+1))
      perror("write"), exit(1);
    
  }

  exit(0);
}
