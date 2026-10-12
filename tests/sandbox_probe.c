#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef VICTIM_PATH
#error "Build with a test victim path"
#endif
int main(int argc,char **argv){
    (void)argv;
    int blocked=1;
    int fd=open(VICTIM_PATH,O_WRONLY|O_TRUNC);
    if(fd>=0){blocked=0;close(fd);}else if(errno!=EPERM)blocked=0;
    if(unlink(VICTIM_PATH)==0||errno!=EPERM)blocked=0;
    fd=open(VICTIM_PATH ".new",O_WRONLY|O_CREAT,0600);
    if(fd>=0){blocked=0;close(fd);}else if(errno!=EPERM)blocked=0;
    if(mkdir(VICTIM_PATH ".dir",0700)==0||errno!=EPERM)blocked=0;
    fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd>=0){blocked=0;close(fd);}else if(errno!=EPERM)blocked=0;
    fd=socket(AF_UNIX,SOCK_STREAM,0);
    if(fd>=0){blocked=0;close(fd);}else if(errno!=EPERM)blocked=0;
    fd=shmget(IPC_PRIVATE,4096,IPC_CREAT|0600);
    if(fd>=0){blocked=0;shmctl(fd,IPC_RMID,NULL);}else if(errno!=EPERM)blocked=0;
    pid_t child=fork();
    if(child>=0){blocked=0;if(child==0)_exit(2);}else if(errno!=EPERM)blocked=0;
    if(!blocked)return 4;
    if(argc==4)puts("0");
    else puts("NEUROMANTLE_AI_PROPOSAL_V1\nstim 0 0 10");
    return 0;
}
