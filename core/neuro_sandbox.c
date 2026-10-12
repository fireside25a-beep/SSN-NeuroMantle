#define _GNU_SOURCE
#include "neuro_sandbox.h"
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>

#if defined(__x86_64__)
#define NM_AUDIT_ARCH AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
#define NM_AUDIT_ARCH AUDIT_ARCH_AARCH64
#else
#error "Sandbox syscall ABI requires a reviewed architecture"
#endif

#define DENY(n) BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (n), 0, 1), \
                BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM)
#define OPEN_FLAGS(n,arg) \
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (n), 0, 4), \
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data,args[arg])), \
    BPF_STMT(BPF_ALU | BPF_AND | BPF_K, O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND), \
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 1, 0), \
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM), \
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data,nr))

static int install_filter(void){
    struct sock_filter code[]={
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data,arch)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, NM_AUDIT_ARCH, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data,nr)),
#if defined(__x86_64__)
        BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, 0x40000000, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
#endif
#ifdef __NR_open
        OPEN_FLAGS(__NR_open,1),
#endif
        OPEN_FLAGS(__NR_openat,2),
#ifdef __NR_openat2
        DENY(__NR_openat2),
#endif
#ifdef __NR_creat
        DENY(__NR_creat),
#endif
#ifdef __NR_unlink
        DENY(__NR_unlink),
#endif
        DENY(__NR_unlinkat),
#ifdef __NR_rename
        DENY(__NR_rename),
#endif
        DENY(__NR_renameat),
#ifdef __NR_renameat2
        DENY(__NR_renameat2),
#endif
        DENY(__NR_mkdirat),
#ifdef __NR_mkdir
        DENY(__NR_mkdir),
#endif
#ifdef __NR_rmdir
        DENY(__NR_rmdir),
#endif
        DENY(__NR_linkat),DENY(__NR_symlinkat),DENY(__NR_mknodat),
#ifdef __NR_link
        DENY(__NR_link),
#endif
#ifdef __NR_symlink
        DENY(__NR_symlink),
#endif
#ifdef __NR_mknod
        DENY(__NR_mknod),
#endif
        DENY(__NR_truncate),DENY(__NR_ftruncate),DENY(__NR_fallocate),
        DENY(__NR_fchmod),DENY(__NR_fchmodat),
#ifdef __NR_fchmodat2
        DENY(__NR_fchmodat2),
#endif
#ifdef __NR_chmod
        DENY(__NR_chmod),
#endif
        DENY(__NR_fchown),DENY(__NR_fchownat),
#ifdef __NR_chown
        DENY(__NR_chown),
#endif
#ifdef __NR_lchown
        DENY(__NR_lchown),
#endif
        DENY(__NR_utimensat),
#ifdef __NR_utime
        DENY(__NR_utime),
#endif
#ifdef __NR_utimes
        DENY(__NR_utimes),
#endif
#ifdef __NR_futimesat
        DENY(__NR_futimesat),
#endif
        DENY(__NR_setxattr),DENY(__NR_lsetxattr),DENY(__NR_fsetxattr),
        DENY(__NR_removexattr),DENY(__NR_lremovexattr),DENY(__NR_fremovexattr),
        DENY(__NR_socket),DENY(__NR_socketpair),DENY(__NR_connect),
        DENY(__NR_bind),DENY(__NR_listen),DENY(__NR_accept),DENY(__NR_accept4),
        DENY(__NR_sendto),DENY(__NR_sendmsg),DENY(__NR_recvfrom),DENY(__NR_recvmsg),
        DENY(__NR_ptrace),DENY(__NR_kill),DENY(__NR_tkill),DENY(__NR_tgkill),
        DENY(__NR_clone),
#ifdef __NR_fork
        DENY(__NR_fork),
#endif
#ifdef __NR_vfork
        DENY(__NR_vfork),
#endif
#ifdef __NR_clone3
        DENY(__NR_clone3),
#endif
#ifdef __NR_process_vm_writev
        DENY(__NR_process_vm_writev),
#endif
#ifdef __NR_process_vm_readv
        DENY(__NR_process_vm_readv),
#endif
#ifdef __NR_pidfd_open
        DENY(__NR_pidfd_open),
#endif
#ifdef __NR_pidfd_send_signal
        DENY(__NR_pidfd_send_signal),
#endif
#ifdef __NR_pidfd_getfd
        DENY(__NR_pidfd_getfd),
#endif
#ifdef __NR_shmget
        DENY(__NR_shmget),
#endif
#ifdef __NR_shmat
        DENY(__NR_shmat),
#endif
#ifdef __NR_shmctl
        DENY(__NR_shmctl),
#endif
#ifdef __NR_msgget
        DENY(__NR_msgget),
#endif
#ifdef __NR_msgsnd
        DENY(__NR_msgsnd),
#endif
#ifdef __NR_msgrcv
        DENY(__NR_msgrcv),
#endif
#ifdef __NR_msgctl
        DENY(__NR_msgctl),
#endif
#ifdef __NR_semget
        DENY(__NR_semget),
#endif
#ifdef __NR_semop
        DENY(__NR_semop),
#endif
#ifdef __NR_semtimedop
        DENY(__NR_semtimedop),
#endif
#ifdef __NR_semctl
        DENY(__NR_semctl),
#endif
#ifdef __NR_keyctl
        DENY(__NR_keyctl),
#endif
#ifdef __NR_add_key
        DENY(__NR_add_key),
#endif
#ifdef __NR_request_key
        DENY(__NR_request_key),
#endif
#ifdef __NR_perf_event_open
        DENY(__NR_perf_event_open),
#endif
#ifdef __NR_io_uring_setup
        DENY(__NR_io_uring_setup),
#endif
#ifdef __NR_bpf
        DENY(__NR_bpf),
#endif
#ifdef __NR_userfaultfd
        DENY(__NR_userfaultfd),
#endif
        DENY(__NR_mount),DENY(__NR_umount2),DENY(__NR_pivot_root),
        DENY(__NR_unshare),DENY(__NR_setns),
#ifdef __NR_open_by_handle_at
        DENY(__NR_open_by_handle_at),
#endif
#ifdef __NR_ioctl
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_ioctl, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | ENOTTY),
#endif
#ifdef __NR_memfd_create
        DENY(__NR_memfd_create),
#endif
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)
    };
    struct sock_fprog program={(unsigned short)(sizeof(code)/sizeof(code[0])),code};
    if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))return -1;
    return prctl(PR_SET_SECCOMP,SECCOMP_MODE_FILTER,&program);
}
int nm_sandbox_child(int preserve_fd){
    if(preserve_fd!=-1&&preserve_fd!=3)return -1;
    int errors=open("/dev/null",O_WRONLY);
    if(errors<0||dup2(errors,STDERR_FILENO)<0)return -1;
    if(errors!=STDERR_FILENO)close(errors);
    struct rlimit zero={0,0};
    if(setrlimit(RLIMIT_FSIZE,&zero)||setrlimit(RLIMIT_CORE,&zero))return -1;
    if(syscall(__NR_close_range,preserve_fd==3?4u:3u,~0u,0))return -1;
    if(clearenv())return -1;
    if(setenv("PATH","/usr/bin:/bin",1)||setenv("PYTHONDONTWRITEBYTECODE","1",1))return -1;
    if(preserve_fd==3&&setenv("NM_MODEL_FD","3",1))return -1;
    if(chdir("/"))return -1;
    if(prctl(PR_SET_DUMPABLE,0,0,0,0))return -1;
    return install_filter();
}
