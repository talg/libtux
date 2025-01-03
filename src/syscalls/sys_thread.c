#include <assert.h>
#include <stdatomic.h>
#include <stdalign.h>

#include <unistd.h>
#include <sys/syscall.h>

#include "arch_regs.h"
#include "host.h"
#include "futex.h"
#include "syscalls/syscalls.h"

static long
futexwait(struct TuxThread* p, uint32_t* uaddr, int op, uint32_t val, uintptr_t timeoutp)
{
    struct TimeSpec* timeout = (struct TimeSpec*) procbufalign(p->proc, timeoutp, sizeof(struct TimeSpec), alignof(struct TimeSpec));
    if (!timeout)
        return -TUX_EFAULT;

    return host_futexwait(p, uaddr, op, val, timeout);
}

static long
futexwake(struct TuxThread* p, uint32_t* uaddr, int op, uint32_t val)
{
    return host_futexwake(p, uaddr, op, val);
}

long
sys_futex(struct TuxThread* p, asuserptr_t uaddrp, int op, uint32_t val,
        uint64_t timeoutp, asuserptr_t uaddr2p, uint32_t val3)
{
    if (uaddrp & 3)
        return -TUX_EFAULT;

    uint32_t* uaddr = procbufalign(p->proc, uaddrp, sizeof(uint32_t), alignof(uint32_t));
    if (!uaddr)
        return -TUX_EFAULT;

    switch (op) {
    case TUX_FUTEX_WAIT:
        return futexwait(p, uaddr, op, val, timeoutp);
    case TUX_FUTEX_WAKE:
        return futexwake(p, uaddr, op, val);
    default:
        return -TUX_EINVAL;
    }
}

static bool
isfork(uint64_t flags)
{
    uint64_t allowed = TUX_CLONE_CHILD_SETTID | TUX_CLONE_CHILD_CLEARTID;
    return (flags & ~allowed) == TUX_SIGCHLD ||
        (flags & ~allowed) == (TUX_CLONE_VM | TUX_CLONE_VFORK | TUX_SIGCHLD);
}

static void*
threadspawn(void* arg)
{
    struct TuxThread* p = (struct TuxThread*) arg;
    tux_proc_start(p->proc->tux, p);
    VERBOSE(p->proc->tux, "thread %d exited", p->tid);
    return NULL;
}

static int
spawn(struct TuxThread* p, uint64_t flags, uint64_t stack, uint64_t ptidp, uint64_t ctidp, uint64_t tls, uint64_t func)
{
    if ((flags & 0xff) != 0 && (flags & 0xff) != TUX_SIGCHLD) {
        WARN("unsupported clone signal: %x", (unsigned) flags & 0xff);
        return -TUX_EINVAL;
    }
    flags &= ~0xff;
    unsigned allowed = TUX_CLONE_THREAD | TUX_CLONE_VM | TUX_CLONE_FS | TUX_CLONE_FILES |
        TUX_CLONE_SIGHAND | TUX_CLONE_SETTLS | TUX_CLONE_PARENT_SETTID |
        TUX_CLONE_CHILD_CLEARTID | TUX_CLONE_CHILD_SETTID | TUX_CLONE_SYSVSEM;
    unsigned required = TUX_CLONE_THREAD | TUX_CLONE_VM | TUX_CLONE_FS | TUX_CLONE_FILES |
        TUX_CLONE_SIGHAND;
    unsigned ignored = TUX_CLONE_DETACHED | TUX_CLONE_IO;
    flags &= ~ignored;

    if (flags & ~allowed) {
        WARN("disallowed clone flags: %lx", (unsigned long) (flags & ~allowed));
        return -TUX_EINVAL;
    }
    if ((flags & required) != required) {
        WARN("missing required clone flags: %lx", (unsigned long) required);
        return -TUX_EINVAL;
    }

    // TODO: validate ctid and ptid for real
    _Atomic(int)* ctid = (_Atomic(int)*) procaddr(p->proc, ctidp);
    _Atomic(int)* ptid = (_Atomic(int)*) procaddr(p->proc, ptidp);

    struct TuxThread* p2 = procnewthread(p);
    if (!p2) {
        return -TUX_EAGAIN;
    }

    if (flags & TUX_CLONE_SETTLS) {
        VERBOSE(p->proc->tux, "CLONE_SETTLS: %lx", tls);
        pal_ctx_tpset(p2->p_ctx, tls);
    }
    if (flags & TUX_CLONE_CHILD_CLEARTID) {
        p2->ctid = ctidp;
    }
    if (flags & TUX_CLONE_CHILD_SETTID) {
        atomic_store_explicit(ctid, p2->tid, memory_order_release);
    }

    VERBOSE(p->proc->tux, "spawning thread %d", p2->tid);

    struct TuxRegs* regs = pal_ctx_regs(p2->p_ctx);
    *regs_return(regs) = 0;
    *regs_sp(regs) = procaddr(p->proc, stack);

    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int err = pthread_create(&thread, &attr, threadspawn, p2);
    pthread_attr_destroy(&attr);
    if (err) {
        assert(!"unimplemented: free machine");
        return -TUX_EAGAIN;
    }
    if (flags & TUX_CLONE_PARENT_SETTID) {
        atomic_store_explicit(ptid, p2->tid, memory_order_release);
    }
    return p2->tid;
}

int
sys_clone(struct TuxThread* p, uint64_t flags, uint64_t stack, uint64_t ptid, uint64_t ctid, uint64_t tls, uint64_t func)
{
    if (isfork(flags)) {
        assert(!"unimplemented: fork or vfork");
    }
    return spawn(p, flags, stack, ptid, ctid, tls, func);
}

int
sys_sched_getaffinity(struct TuxProc* p, int32_t pid, uint64_t cpusetsize, int64_t maskaddr)
{
    WARN("unimplemented: sched_getaffinity");
    return 0;
}

int
sys_sched_setaffinity(struct TuxProc* p, int32_t pid, uint64_t cpusetsize, int64_t maskaddr)
{
    WARN("unimplemented: sched_setaffinity");
    return 0;
}

int
sys_sched_yield(struct TuxProc* p)
{
    return host_sched_yield();
}

int
sys_set_tid_address(struct TuxThread* p, uintptr_t ctid)
{
    p->ctid = procaddr(p->proc, ctid);
    return p->tid;
}

int
sys_gettid(struct TuxThread* p)
{
    return p->tid;
}
