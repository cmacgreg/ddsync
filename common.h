#ifndef ddsync_main_h

#define _FILE_OFFSET_BITS 64
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#define info_fprintf(fp, ...) !fp
//#define info_fprintf(fp, format, ...)  fprintf (fp, "%d\t%s\t%u\t " format, time(NULL), __FILE__, __LINE__, ##__VA_ARGS__)

#define BLOCK_SIZE (128 * 1024)

#define DIGEST_SIZE SHA256_DIGEST_SIZE
//#define DIGEST_SIZE 16 //murmurhash3 x86-128
//#define DIGEST_SIZE 4 //murmurhash3 x86-32


struct __attribute__((__packed__)) msg{
  uint32_t op;
  uint32_t size;
  uint8_t buf[BLOCK_SIZE];
};

// getopt flags/vars
extern int enable_final_stats;
extern int enable_dryrun;
extern int enable_ticks;
extern int enable_progress;
extern int skip_blocks, count_blocks;
extern char *in_file, *out_file;
extern char *exec_cmd;

extern int fd_in, fd_log, fd_out, fd_log_pipe;

void sender();
void receiver();

#endif
