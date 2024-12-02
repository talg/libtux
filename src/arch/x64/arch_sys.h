#pragma once

#include "tux_pal.h"

enum {
    TUX_SYS_read               = 0,
    TUX_SYS_write              = 1,
    TUX_SYS_close              = 3,
    TUX_SYS_stat               = 4,
    TUX_SYS_fstat              = 5,
    TUX_SYS_lstat              = 6,
    TUX_SYS_poll               = 7,
    TUX_SYS_lseek              = 8,
    TUX_SYS_mmap               = 9,
    TUX_SYS_mprotect           = 10,
    TUX_SYS_munmap             = 11,
    TUX_SYS_brk                = 12,
    TUX_SYS_rt_sigaction       = 13,
    TUX_SYS_rt_sigprocmask     = 14,
    TUX_SYS_rt_sigreturn       = 15,
    TUX_SYS_ioctl              = 16,
    TUX_SYS_pread64            = 17,
    TUX_SYS_pwrite64           = 18,
    TUX_SYS_readv              = 19,
    TUX_SYS_writev             = 20,
    TUX_SYS_access             = 21,
    TUX_SYS_getpid             = 39,
    TUX_SYS_socket             = 41,
    TUX_SYS_clone              = 56,
    TUX_SYS_fork               = 57,
    TUX_SYS_vfork              = 58,
    TUX_SYS_execve             = 59,
    TUX_SYS_exit               = 60,
    TUX_SYS_uname              = 63,
    TUX_SYS_truncate           = 76,
    TUX_SYS_ftruncate          = 77,
    TUX_SYS_getcwd             = 79,
    TUX_SYS_chdir              = 80,
    TUX_SYS_fchdir             = 81,
    TUX_SYS_rename             = 82,
    TUX_SYS_mkdir              = 83,
    TUX_SYS_readlink           = 89,
    TUX_SYS_rt_sigpending      = 127,
    TUX_SYS_rt_sigtimedwait    = 128,
    TUX_SYS_rt_sigqueueinfo    = 129,
    TUX_SYS_rt_sigsuspend      = 130,
    TUX_SYS_sigaltstack        = 131,
    TUX_SYS_statfs             = 137,
    TUX_SYS_arch_prctl         = 158,
    TUX_SYS_gettid             = 186,
    TUX_SYS_getxattr           = 191,
    TUX_SYS_lgetxattr          = 192,
    TUX_SYS_futex              = 202,
    TUX_SYS_getdents64         = 217,
    TUX_SYS_set_tid_address    = 218,
    TUX_SYS_clock_gettime      = 228,
    TUX_SYS_exit_group         = 231,
    TUX_SYS_openat             = 257,
    TUX_SYS_newfstatat         = 262,
    TUX_SYS_unlinkat           = 263,
    TUX_SYS_renameat           = 264,
    TUX_SYS_readlinkat         = 267,
    TUX_SYS_faccessat          = 269,
    TUX_SYS_set_robust_list    = 273,
    TUX_SYS_prlimit64          = 302,
    TUX_SYS_getrandom          = 318,
    TUX_SYS_statx              = 332,
    TUX_SYS_rseq               = 334,

    // always last syscall+1
    TUX_SYS_ntotal             = 335,
};

void arch_syshandle(struct PlatContext* ctx);
