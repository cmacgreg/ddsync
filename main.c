#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <strings.h>

#include "common.h"

#define PE_NAME "ddsync-main"
#define PE_ERR  PE_NAME " ERROR"
#define PE_INFO  PE_NAME " INFO"

int enable_final_stats = 1;
int enable_dryrun = 0;
int enable_ticks = 0;
int enable_progress = 0;
int skip_blocks = 0, count_blocks=0;
char *in_file = NULL, *out_file = NULL;
char *exec_cmd = NULL;

int fd_in = 0, fd_log, fd_log_pipe = 1, fd_out = 1;

int main(int argc, char **argv){

  int pipe_sender[2], pipe_cmd[2];
  pid_t child_pid;

  enum { NO, SENDER, RECEIVER } is_remote = NO;

  int remote_compress=0, is_sender = 1;
  char *remote_host=NULL, *remote_user=NULL, *remote_port=NULL, *t1, *t2;
  char *skip_blocks_arg=NULL, *count_blocks_arg=NULL;

  for(;;){
    static struct option long_options[] = {
      {"silent",   no_argument,       0, 's'},
      {"dryrun",   no_argument,       0, 'n'},
      {"ticks",    no_argument,       0, 't'},
      {"progress", no_argument,       0, 'g'},
      {"compress", no_argument,       0, 'C'},
      {"verbose",  no_argument,       0, 'v'},
      {"help",     no_argument,       0, 'h'},
      {"exec",     optional_argument, 0, 'e'},
      {"if",       required_argument, 0, 'f'},
      {"of",       required_argument, 0, 'o'},
      {"skip",     required_argument, 0, 'k'},
      {"count",    required_argument, 0, 'c'},
      {"user",     required_argument, 0, 'l'},
      {"port",     required_argument, 0, 'p'},
    
      {"remote-sender",   no_argument, 0, 1},
      {"remote-receiver", no_argument, 0, 2},
      {0, 0, 0, 0}
    };

    int option_index = 0;
    int c = getopt_long(argc, argv, "sntgCvh?e::f:o:k:c:l:p:", long_options, &option_index);

    if(c == -1)
      break;

    switch(c){
      case 0:
        printf("option %s", long_options[option_index].name);
        if (optarg)
            printf(" with arg %s", optarg);
        printf("\n");
        break;
      case 1:
        is_remote=SENDER;
        break;
      case 2:
        is_remote=RECEIVER;
        is_sender=0;
        break;
      case 's':
        enable_final_stats = 0;
        break;
      /*case 'n':
        enable_dryrun = 1;
        break;*/
      case 't':
        enable_ticks = 1;
        break;
      case 'v':
        enable_ticks = 1;
        //fall thru...
      case 'g':
        enable_progress = 1;
        break;
      case 'e':
        exec_cmd = optarg;
        break;
      case 'k':
        skip_blocks_arg = optarg;
        skip_blocks=atol(optarg);
        if(skip_blocks <= 0)
          puts("skip value must be a positive integer\n"), exit(2);
        break;
      case 'c':
        count_blocks_arg = optarg;
        count_blocks=atol(optarg);
        if(count_blocks <= 0)
          puts("count value must be a positive integer\n"), exit(2);
        break;
      case 'f':
        in_file = optarg;
        break;
      case 'o':
        out_file = optarg;
        break;
      case 'l':
        remote_user = optarg;
        break;
      case 'C':
        remote_compress = 1;
        break;
      case 'p':
        remote_port = optarg;
        break;
      case 'h':
      case '?':
      usage:
      default:
        fputs("usage: TBD", stderr);
        exit(2);
    }
  }

  if(optind == argc - 2){
    in_file  = argv[optind];
    out_file = argv[optind + 1];
  }
  else if(optind == argc - 1){
    out_file = argv[optind];
  }
  else if(optind > argc){
    goto usage;
  }

  if(isatty(fd_out) && !out_file){
    fputs(PE_ERR " i won't b0rk your terminal: no output file specifed\n", stderr), exit(3);
  }

  if(isatty(fd_in) && !in_file){
    fputs(PE_INFO " i accept your terminal input in protest; go ahead or ctrl-c to quit\n", stderr);
  }

  if(is_remote == NO){

    if(in_file && (t1 = strchr(in_file, ':'))){
      t2 = strchr(in_file, '/');
      if(!t2 || t2 > t1){
        remote_host = in_file;
        in_file = t1 + 1;
        *t1 = 0;
        is_sender = 0;
      }
    }
    if(out_file && (t1 = strchr(out_file, ':'))){
      t2 = strchr(out_file, '/');

      if(!t2 || t2 > t1){
        if(remote_host)
          fputs(PE_ERR " src and dest cannot both be remote\n", stderr), exit(3);
        remote_host = out_file;
        out_file = t1 + 1;
        *t1 = 0;
      }
    }

    if(remote_host && (t1 = strchr(remote_host, '@'))){
      if(remote_user)
        fputs(PE_ERR " remote username specified in both args and filename", stderr), exit(3);
      remote_user = remote_host;
      remote_host = t1 + 1;
      *t1 = 0;
    }
  }    

  exec_cmd="YESSSSSS//////////////S!";


  /* Print any remaining command line arguments (not options). */
  /*for(;optind < argc;optind++)
  {
    in_file=argv[optind++]);
  }*/

if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
  perror(0);
  exit(1);
}

  if(is_remote == NO){
    if(pipe(pipe_sender) == -1 || pipe(pipe_cmd) == -1)
      perror(PE_ERR " pipe()"), exit(1);
    if((child_pid = fork()) == -1)
      perror(PE_ERR " fork()"), exit(1);
    // child
    if(child_pid == 0){
      close(0); close(1);
      close(pipe_sender[1]);
      close(pipe_cmd[0]);
      dup2(pipe_sender[0],0);
      dup2(pipe_cmd[1],1);
    
      int argc_new_size = 0, argc_new = 0;
      char **argv_new = NULL;
      void argv_add(char *t){
        if(argc_new == argc_new_size){
          argc_new_size = argc_new_size ? argc_new_size << 1 : 16;
          argv_new = realloc(argv_new,  sizeof(char *) * argc_new_size);
        }
        argv_new[argc_new++] = t;
      }

      if(remote_host){
        argv_add("ssh");
        if(remote_compress)
          argv_add("-C");
        if(remote_port)
          argv_add("-p"), argv_add(remote_port);
        if(remote_user)
          argv_add("-l"), argv_add(remote_user);
        argv_add(remote_host);
        argv_add("/root/ddsync/ddsync");
       
      }
      else{
        argv_add("/usr/dev/ddsync/ddsync");
      }

      if(count_blocks)
        argv_add("-c"), argv_add(count_blocks_arg);
      if(skip_blocks)
        argv_add("-k"), argv_add(skip_blocks_arg);
      if(enable_dryrun)
        argv_add("-n");

      if(is_sender){
        argv_add("--remote-receiver");
        if(out_file)
          argv_add("-o"), argv_add(out_file);
      }
      else{
        argv_add("--remote-sender");
        if(in_file)
          argv_add("-f"), argv_add(in_file);
    
        char f[5], *t=f;
        *t++ = '-';
        if(!enable_final_stats)
          *t++ = 's';
        if(enable_ticks)
          *t++ = 't';
        if(enable_progress)
          *t++ = 'g';
        *t++ = 0;
        
        argv_add(f);
                
      }

      argv_add(NULL);
      
      int i=0;
      
      for(i=0; i<argc_new-1;i++){
        fprintf(stderr,"%s ", argv_new[i]);
      }
      fprintf(stderr,"\n");  
      
      if(-1 == execvp(argv_new[0], argv_new))
        perror(PE_ERR " execvp"), exit(1);
  
    }
    else{
      close(pipe_sender[0]);
      close(pipe_cmd[1]);
    }
  }

  if(is_sender){
    if(is_remote == NO){
      fd_log = pipe_cmd[0];
      fd_out = pipe_sender[1];
    }
    sender();
  }
  else{
    if(is_remote == NO){
      fd_in = pipe_cmd[0];
      fd_log_pipe = pipe_sender[1];
    }
    receiver();
  }
  if(is_remote == NO){
    wait(NULL);
  }


  exit(0);
}
