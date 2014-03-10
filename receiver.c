#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <nettle/sha2.h>
#include <sys/wait.h>
#include <signal.h>
//#include "MurmurHash3.h"

#define PE_NAME "ddsync-receiver"
#define PE_ERR  PE_NAME " ERROR"

void receiver(int argc, char **argv){
  // old flags
  int log_update=1;

  // fd and syscall err vars
  ssize_t given, got=1, in_total=0;

  // buffer
  struct msg in;
  struct msg log; log.op=htonl(3);

  //close(2);
  //dup2(2,open("receiver.out",O_WRONLY|O_CREAT|O_TRUNC));

  uint8_t digest[DIGEST_SIZE];

  // sha256 stuff
  struct sha256_ctx ctx;
 
  // creat/open a block-size-specific log
  char log_digest[98];
  snprintf(log_digest,128,"ddsync.%u.log",BLOCK_SIZE >> 10);
  fd_log = open(log_digest, O_RDWR);
  if(fd_log == -1){
    log_update = 0;
    fd_log = open(log_digest, O_WRONLY | O_CREAT, 0640);
    if(fd_log == -1)
      perror(PE_ERR " creat(log)"), exit(1);
  }

  fd_out = open(out_file, O_WRONLY | O_CREAT | O_LARGEFILE, 0640);
  if(fd_out == -1)
    perror(PE_ERR " open(out)"), exit(1);

  int i=0;
  for(i=0;i<skip_blocks;i++){
    if(-1 == lseek(fd_out, BLOCK_SIZE, SEEK_CUR))
      perror(PE_ERR " lseek(out)"), exit(1);
    if(-1 == lseek(fd_log, DIGEST_SIZE, SEEK_CUR))
      perror(PE_ERR " lseek(log)"), exit(1);
  }

if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
  perror(0);
  exit(1);
}

  int logreader_pid = fork();
  if(-1 == logreader_pid)
    perror(PE_NAME " fork(logreader)"), exit(1);
  if(!logreader_pid){
    close(fd_log);
    fd_log = open(log_digest, O_RDONLY);
    if(fd_log == -1)
      perror(PE_ERR " logreader open()"), exit(1);
    for(i=0;i<skip_blocks;i++)
      if(-1 == lseek(fd_log, DIGEST_SIZE, SEEK_CUR))
        perror(PE_ERR " lseek(log)"), exit(1);
    
    ssize_t log_got=1;
    while(log_got!=0){
      log_got = read(fd_log, log.buf, BLOCK_SIZE);
      if(log_got == -1)
        perror(PE_ERR " logreader read()"),exit(1);
      if(!log_got){
        log_update = 0;
      }
      log.size=htonl(log_got);

      int log_pipe_total=0;
      logreader_re_write:
      given = write(fd_log_pipe, (void*)&log + log_pipe_total, sizeof(struct msg) - BLOCK_SIZE + log_got - log_pipe_total);
      if(given == -1)
        perror(PE_ERR " logreader write(log-stdout)"), exit(1);
      log_pipe_total += given;
      if(log_pipe_total < sizeof(struct msg) - BLOCK_SIZE + log_got)
        goto logreader_re_write;
      
      info_fprintf(stderr,PE_NAME " out write(%d bytes)\n", given);
      
    }
    exit(0);
  }

  while(1){
    re_read:
    got = read(fd_in, (void*) &in + in_total, sizeof(in) - in_total);
    info_fprintf(stderr,PE_NAME " in read(%d bytes), %d total\n", got,in_total);

    if(got == -1)
      if(errno == EINTR)
        goto re_read;
      else 
        perror(PE_ERR " read"), exit(1);
  
    // did we get BLOCK_SIZE bytes?       
    in_total += got;

    if(!in_total)
     goto cleanup;
    
    if(in_total < sizeof(uint32_t))
      goto re_read;
    
    info_fprintf(stderr,PE_NAME " data op:%d %d bytes, %d total\n", ntohl(in.op),ntohl(in.size),in_total);
  
    switch(ntohl(in.op)){
      /*case 0:
        // terminal block...
        abort();
        break;*/
      case 2:
        // skip block
        lseek(fd_log, DIGEST_SIZE, SEEK_CUR);
        lseek(fd_out, BLOCK_SIZE, SEEK_CUR);
        in_total-=2*sizeof(uint32_t);
        bcopy((void*)&in+2*sizeof(uint32_t), &in, in_total);
        info_fprintf(stderr,PE_NAME "(next) data op:%d %d bytes, %d total\n", ntohl(in.op),ntohl(in.size),in_total);
  
        goto re_read;
      case 1:
        if(ntohl(in.size) == 0)
          goto cleanup;
        if(in_total < sizeof(in) - BLOCK_SIZE + ntohl(in.size))
          goto re_read;

        // normal block
        break;
    }

    // we got enough bytes, sha2 it up
    sha256_init(&ctx);
    sha256_update(&ctx, ntohl(in.size), in.buf);
    sha256_digest(&ctx, SHA256_DIGEST_SIZE, digest);
    
    
    //MurmurHash3_x86_32(in.buf, ntohl(in.size), 1337, digest);
  
    /*fputs("receiver digest: ",stderr);
    for (i = 0; i < DIGEST_SIZE; i++) {
      fprintf(stderr,"%.2x", digest[i]);
    }
    fprintf(stderr,"\n");*/


    // update the block
    ssize_t out_total = 0;
  
    re_write:
    given = write(fd_out, in.buf + out_total, ntohl(in.size) - out_total);
    if(given == -1)
      perror(PE_ERR " write(out)"), exit(1);
    out_total+=given;
    if(out_total < ntohl(in.size))
      goto re_write;
  
    
    given = write(fd_log, digest, DIGEST_SIZE);
    if(given == -1)
      perror(PE_ERR " write(log)"), exit(1);
    
    in_total=0;
  }
  cleanup:
    wait(NULL);

}
