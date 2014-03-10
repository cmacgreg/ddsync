#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <nettle/sha2.h>
#include <arpa/inet.h>
#include "MurmurHash3.h"

#define PE_NAME "ddsync-sender"
#define PE_ERR  PE_NAME " ERROR"

#define USE_SHA256

void sender(){

  info_fprintf(stderr,"flags of concern: enable_progress=%d\n",enable_progress);
  // old flags
  int log_update=1;

  // fd and syscall err vars
  ssize_t given, got=1, in_total=0;
  
  // display vars
  uint64_t count=skip_blocks+1;
  int updated = 0, matched = 0;

  // buffer
  struct msg in;
  struct msg log; log.op=htonl(3);

//  close(2);
//  dup2(2,open("sender.out",O_WRONLY|O_CREAT|O_TRUNC));
  uint8_t *log_buf_curr=log.buf;
  uint8_t digest[DIGEST_SIZE], digest_zero[DIGEST_SIZE];
  bzero(in.buf,BLOCK_SIZE);

  #ifdef USE_SHA256
  struct sha256_ctx ctx;

  // a "zeroed" detection block for chart eyecandy
  sha256_init(&ctx);
  sha256_update(&ctx, BLOCK_SIZE, in.buf);
  sha256_digest(&ctx, SHA256_DIGEST_SIZE, digest_zero);  
  #else

  MurmurHash3_x86_32(in.buf, BLOCK_SIZE, 1337, digest_zero);

  #endif


  if(in_file){
    fd_in = open(in_file, O_RDONLY);
    if(fd_in == -1)
      perror(PE_ERR " open(in)"), exit(1);
  }

  #if 0
  struct stat in_stat;

  fstat(fd_in, &in_stat);

  fprintf(stderr,"ID of device containing file: %d\ninode number: %d\nprotection: %d\nnumber of hard links: %d\nuser ID of owner: %d\ngroup ID of owner: %d\ndevice ID (if special file): %d\ntotal size, in bytes: %d\nblocksize for filesystem I/O: %d\nnumber of 512B blocks allocated: %d\ntime of last access: %d\ntime of last modification: %d\ntime of last status change: %d\ntime of last status change: %d\ntime of last status change: %d\ntime of last status change: %d\n",
         in_stat.st_dev,in_stat.st_ino,in_stat.st_mode,in_stat.st_nlink,in_stat.st_uid,in_stat.st_gid,in_stat.st_rdev,in_stat.st_size,in_stat.st_blksize,in_stat.st_blocks,in_stat.st_atime,in_stat.st_mtime,in_stat.st_ctime);


           struct stat {
               dev_t     st_dev;     /* ID of device containing file */
               ino_t     st_ino;     /* inode number */
               mode_t    st_mode;    /* protection */
               nlink_t   st_nlink;   /* number of hard links */
               uid_t     st_uid;     /* user ID of owner */
               gid_t     st_gid;     /* group ID of owner */
               dev_t     st_rdev;    /* device ID (if special file) */
               off_t     st_size;    /* total size, in bytes */
               blksize_t st_blksize; /* blocksize for filesystem I/O */
               blkcnt_t  st_blocks;  /* number of 512B blocks allocated */
               time_t    st_atime;   /* time of last access */
               time_t    st_mtime;   /* time of last modification */
               time_t    st_ctime;   /* time of last status change */
           };

          //sleep 2; perl -ne '/(st_\w+);\s*\/\*\s(.*)\s\*\//; print "$2: %d\n";'
          //sleep 2; perl -ne '/(st_\w+);\s*\/\*\s(.*)\s\*\//; print "$1,";'
          

  #endif
  int skip_blocks_orig = skip_blocks;
  for(;skip_blocks;skip_blocks--){
    if(-1 == lseek(fd_in, BLOCK_SIZE, SEEK_CUR))
      break;
  }


  // timevals
  struct timeval time_now,time_last,time_start={0,0},one_msec={0,1};
  gettimeofday(&time_now, NULL);
  timersub(&time_now,&one_msec,&time_last);
  timersub(&time_last,&one_msec,&time_start);

  void stats(int is_tick){
    static int display_mod = 20;
    double display_total;
    // scale total size per kilo-unit
    while((display_total = ((double)count*BLOCK_SIZE)/pow(2,display_mod)) > 1024)
      display_mod+=10;
    
    const char *mod_byte_prefix="BKMGTPEZY";
    
    const int ticks = 64;
    if(!is_tick || count % ticks == 0){
      struct timeval time_diff_avg,time_diff_curr;
      time_last = time_now;
      gettimeofday(&time_now,NULL);
      timersub(&time_now,&time_start,&time_diff_avg);
      timersub(&time_now,&time_last,&time_diff_curr);
    
      fprintf(stderr,"%s%7uu %7u. %7llux%uk%8.4g%c %6lluk/s %6uk*s\n",
        is_tick ? "" : "\n", updated, matched,
        count, BLOCK_SIZE/1024, display_total, mod_byte_prefix[display_mod / 10], 
        (uint64_t)(((count-skip_blocks_orig) * BLOCK_SIZE/1024) / (time_diff_avg.tv_sec+time_diff_avg.tv_usec/1000000.0)),
        (int)((ticks * BLOCK_SIZE) / (1024*(time_diff_curr.tv_sec+time_diff_curr.tv_usec/1000000.0)))
      );
    }
  }

  re_read:
  for(;got != 0;++count,in_total=0){
    int match = 0;
    char tick_out = '?';
    got = read(fd_in, in.buf + in_total, BLOCK_SIZE - in_total);
  
    if(got == -1)
      if(errno == EINTR)
        goto re_read;
      else 
        perror(PE_ERR " read"), exit(1);
      
    info_fprintf(stderr,PE_NAME " in read(%d bytes)\n", got);
  
    // did we get BLOCK_SIZE bytes?       
    in_total += got;
    if(got != 0 && in_total < BLOCK_SIZE)
      goto re_read;
    
    if(skip_blocks){
      skip_blocks--;
      in_total=0;
      goto re_read;
    }
    
   #ifdef USE_SHA256
    // we got enough bytes, sha2 it up
    sha256_init(&ctx);
    sha256_update(&ctx, in_total, in.buf);
    sha256_digest(&ctx, SHA256_DIGEST_SIZE, digest);
   #else
  MurmurHash3_x86_32(in.buf, in_total, 1337, digest);
    
    //memcpy(digest, in.buf, DIGEST_SIZE);
    #endif
    
    //goto fake_it;
    
      
    if(log_update){
  
      static ssize_t log_total=0,log_got=1;
    
      log_reprocess:
      if(log_buf_curr < log.buf + ntohl(log.size)){  
        /*fputs("sender digest:",stderr);  
        for (i = 0; i < DIGEST_SIZE; i++) {
          fprintf(stderr,"%.2x", digest[i]);
        }
        fputs("\tlog_digest:",stderr);
      
        for (i = 0; i < DIGEST_SIZE; i++) {
          fprintf(stderr,"%.2x", log_buf_curr[i]);
        }
        fputs("\n",stderr);*/

        if(!strncmp(log_buf_curr, digest, DIGEST_SIZE))
          tick_out='.',
          match=1;
        else
          tick_out='u';
        
        log_buf_curr += DIGEST_SIZE;
        log_total -= DIGEST_SIZE;
      }
      else{
        log_buf_curr = log.buf;
        log_total = 0;
        log_re_read:
        log_got = read(fd_log, (void*)&log + log_total, sizeof(struct msg) - log_total);
        if(log_got == -1)
          perror(PE_NAME " read(log)"), exit(1);
        log_total += log_got;
        info_fprintf(stderr,PE_NAME " log op:%d %d bytes, %d total\n", ntohl(log.op),ntohl(log.size),log_total);
        if(log_total < sizeof(struct msg) - BLOCK_SIZE)
          goto log_re_read;

        if( !ntohl(log.size) || !log_got ){
          log_update = 0;
          close(fd_log);
          tick_out = 'L';
        }
        else if(log_total < sizeof(struct msg) - BLOCK_SIZE + ntohl(log.size))
          goto log_re_read;
        else
          goto log_reprocess;
        
      }

    }
    else
      tick_out='*';
        
    ssize_t out_total = 0;
    if(!match){
    
      // update the block
    
      force_data_msg:
      in.op = htonl(1);
      in.size = htonl(in_total);
    
      re_write:
      given = write(fd_out, (void*) &in + out_total, sizeof(in) - BLOCK_SIZE + in_total - out_total);
      if(given == -1)
        perror(PE_ERR " write(out)"), exit(1);
      info_fprintf(stderr,PE_NAME " write(%d bytes)\n", given);
      out_total+=given;
      if(out_total < sizeof(in) - BLOCK_SIZE + in_total)
        goto re_write;
      
      updated++;
    }
    else{
      fake_it:
      // just skip it on match
      in.op = htonl(2);
      in.size = 0;
      write(fd_out, &in, sizeof(in) - BLOCK_SIZE);
      matched++;
    }
  
    if(!in_total)
      tick_out='$';
    /*  
    uint8_t digest_str[2*DIGEST_SIZE+1];
    for (i = 0; i < DIGEST_SIZE; i++) {
      sprintf(digest_str + 2*i, "%.2x", digest[i]);
    }
    
    in.buf[2*DIGEST_SIZE+1]=0;
    
    char *interesting[] = {
      "0b9d60526c77fc8fb7e22817a4b17ed36ba30c634678cc19e2c7181e58ce7aa4",
      "0c449d5d5e3c918138b15dc2c5991735add9a8d506f4154b093dff4f42965ff5",
      "14ca214663496d4ab2e3940c82a5959a684f2f3c740d1f357f4de609099b20e3",
      "161a08e084c6130f8740e11752120c3d0b09bd6e89337c9c10912619a1d30706",
      "2c230730e0a3856444f3068f61dbf51fb7ba99d80e137ee9f5933a7b07468038",
      "3e23d8fc06c109290d9dadf4e2cee1cd23fcb08b639535fe43924a3bfc3d7ae1",
      "4071c0cb10867e9d95626be9c30c98badb5bbf04bab0d6c74b92f41ad47fd694",
      "42e62a23906ec34d09d3b2b9daf099c61674f99afff06507a9435118e88daf75",
      "4ec3a91c5a030da891ca4575b5af3482d1eaa477a28ef4f70780e1ff3a83ad2e",
      "578d9173cf8e700978a765ea69ee62e5ad6eee692ad68bf119ba2f11a43db920",
      "5b22cb205b77101ca7363da372232ee4efc170f407cf23d1ba557c15d1b8f1eb",
      "5fe7b9a7a38e7db7247aa8cab064383615c0fa3ae20321d5f0346778839f88f1",
      "652a324e47ecf11f62765241fcc515871a743ad4721214e63894b38746f2d258",
      "6fc2eeb8570e305b2f51f131c9c6d37f6fa29d3b6f180ede23d7c722720da14b",
      "afbe01d2f38434d7f91a5199484490cf2f9f285a08018be977b7a8e8dd28b092",
      "b5a41c3758763bbec72769fab4a2533bf2db0b6312d93d25a695f9e4b9e02260",
      "c7e575a8b4c03df09bf539a49984efaa3e894d022c5905a231cd5c5c25426c28",
      "d651f704eee1b3ebddd158931fc2806e89c62dac909dbd8388f853cb47bec56d",
      "ec80a15fccec80f36841d496ca3d8c9de31a3062263b29db49074f7fab705c3d",
      "ecf67ff951243cbc72c7c2f0e61cad9206b14b9a3aa464c834f8d9e1b8a4f921",
      //"fa43239bcee7b97ca62f007cc68487560a39e19f74f3dde7486db3f98df8e471",
    };
  
    for(i=0;i<sizeof(interesting);i++){
      if(!strncmp(interesting[i], digest_str, DIGEST_SIZE*2+1)){
        fprintf(stderr,"=\ninteresting - %s=\n%s...\n====\n",digest_str,in.buf);
        fprintf(stdout,"=\ninteresting - %s=\n%s\n====\n",digest_str,in.buf);
        tick_out='i';
      }
    }*/
    
    if(!strncmp(digest_zero, digest, DIGEST_SIZE))
      tick_out=tick_out=='*'?'_':tick_out=='u'?'Z':'z';
    
    if(enable_ticks)
      putc(tick_out, stderr);
  
    if(enable_progress)
      stats(1);
    
    if(count == skip_blocks_orig + count_blocks){
      break;
    }

  }
  struct msg msg_eof = {0,0,""};
  write(fd_out, &msg_eof, sizeof(struct msg) - BLOCK_SIZE);

  close(fd_in); close(fd_out); close(fd_log);

  if(enable_final_stats && !enable_progress)
    stats(0);
  
}
