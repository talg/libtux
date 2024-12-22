#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stdatomic.h>

#include "arch_regs.h"
#include "cwalk.h"
#include "print.h"
#include "fd.h"
#include "buf.h"
#include "proc.h"
#include "elfload.h"

#include "syscalls/syscalls.h"

static bool procsetup(struct TuxThread* p, uint8_t* prog, size_t progsz, uint8_t* interp, size_t interpsz, int argc, char** argv);
static bool procfile(struct TuxThread* p, uint8_t* prog, size_t progsz, int argc, char** argv);
static void procfree(struct TuxThread*);

static int
nexttid(void)
{
    // TODO: set a limit on the number of threads?
    static _Atomic(int) tid = 0;
    return atomic_fetch_add_explicit(&tid, 1, memory_order_relaxed);
}

static struct TuxThread*
procnewempty(void)
{
    struct TuxProc* proc = calloc(sizeof(struct TuxProc), 1);
    if (!proc)
        return NULL;
    struct TuxThread* p = calloc(sizeof(struct TuxThread), 1);
    if (!p)
        goto err;
    p->proc = proc;
    p->tid = nexttid();
    return p;

err:
    free(proc);
    return NULL;
}

struct TuxThread*
procnewthread(struct TuxThread* p)
{
    struct TuxThread* newp = calloc(sizeof(struct TuxThread), 1);
    if (!newp)
        goto err;

    struct PlatContext* ctx = pal_ctx_new(p->proc->tux->plat, p->proc->p_as, newp, false);
    if (!ctx)
        goto err1;
    newp->p_ctx = ctx;
    newp->proc = p->proc;
    newp->tid = nexttid();
    *pal_ctx_regs(newp->p_ctx) = *pal_ctx_regs(p->p_ctx);

    return newp;

err1:
    free(newp);
err:
    return NULL;
}

static struct TuxThread*
procnewfile(struct Tux* tux, uint8_t* prog, size_t size, int argc, char** argv)
{
    struct TuxThread* p = procnewempty();
    if (!p)
        return NULL;
    p->proc->tux = tux;
    struct PlatAddrSpace* as = pal_as_new(tux->plat);
    if (!as)
        goto err1;
    struct PlatContext* ctx = pal_ctx_new(tux->plat, as, p, true);
    if (!ctx)
        goto err2;
    p->proc->p_as = as;
    p->proc->p_info = pal_as_info(as);
    p->p_ctx = ctx;

    if (!procfile(p, prog, size, argc, argv))
        goto err3;

    fdinit(tux, &p->proc->fdtable);

    return p;
err3:
    pal_ctx_free(ctx);
err2:
    pal_as_free(as);
err1:
    procfree(p);
    return NULL;
}

static bool
procfile(struct TuxThread* p, uint8_t* prog, size_t progsz, int argc, char** argv)
{
    char* interppath = elfinterp(prog, progsz);
    buf_t interp = (buf_t){NULL, 0};
    if (interppath) {
        if (cwk_path_is_absolute(interppath)) {
            interp = bufreadfile(interppath);
            if (!interp.data) {
                WARN("error opening dynamic linker %s: %s", interppath, strerror(errno));
                free(interppath);
                return false;
            }
            VERBOSE(p->proc->tux, "dynamic linker: %s", interppath);
        } else {
            WARN("interpreter ignored because it is relative path: %s", interppath);
        }
        free(interppath);
    }

    bool success = true;
    if (!procsetup(p, prog, progsz, interp.data, interp.size, argc, argv))
        success = false;

    return success;
}

enum {
    ARGC_MAX = 1024,
    ARGV_MAX = 1024,

    ARG_BLOCK = 4096,
};

static bool
stacksetup(struct TuxProc* p, int argc, char** argv, struct ELFLoadInfo* info, asptr_t* newsp)
{
#ifdef PAL_EXTERNAL_ADDRSPACE
#error "TODO: platforms that require external address spaces are currently unsupported"
#endif

    char* argv_ptrs[ARGC_MAX];
    char* stack_top = (char*) info->stack + info->stacksize;
    char* p_argv = (char*) stack_top - ARG_BLOCK;

    // Write argv string values to the stack.
    for (int i = 0; i < argc; i++) {
        size_t len = strnlen(argv[i], ARGV_MAX) + 1;

        if (p_argv + len >= stack_top) {
            return false;
        }

        memcpy(p_argv, argv[i], len);
        p_argv[len - 1] = 0;
        argv_ptrs[i] = p_argv;
        p_argv += len;
    }

    // Write argc and argv pointers to the stack.
    long* p_argc = (long*) (stack_top - 2 * ARG_BLOCK);
    *newsp = (asptr_t) p_argc;
    *p_argc++ = argc;
    char** p_argvp = (char**) p_argc;
    char** p_argvp_start = p_argvp;
    for (int i = 0; i < argc; i++) {
        if ((uintptr_t) p_argvp >= (uintptr_t) stack_top - ARG_BLOCK) {
            return false;
        }
        p_argvp[i] = (char*) pal_as_p2user(p->p_as, argv_ptrs[i]);
    }
    p_argvp[argc] = NULL;
    // Empty envp.
    char** p_envp = (char**) &p_argvp[argc + 1];
    *p_envp++ = NULL;

    struct Auxv* av = (struct Auxv*) p_envp;
    *av++ = (struct Auxv) { AT_SECURE, 0 };
    *av++ = (struct Auxv) { AT_BASE, info->ldbase };
    *av++ = (struct Auxv) { AT_PHDR, info->elfbase + info->elfphoff };
    *av++ = (struct Auxv) { AT_PHNUM, info->elfphnum };
    *av++ = (struct Auxv) { AT_PHENT, info->elfphentsize };
    *av++ = (struct Auxv) { AT_ENTRY, info->elfentry };
    *av++ = (struct Auxv) { AT_EXECFN, pal_as_p2user(p->p_as, p_argvp_start[0]) };
    *av++ = (struct Auxv) { AT_PAGESZ, p->tux->opts.pagesize };
    *av++ = (struct Auxv) { AT_HWCAP, 0 };
    *av++ = (struct Auxv) { AT_HWCAP2, 0 };
    *av++ = (struct Auxv) { AT_RANDOM, pal_as_p2user(p->p_as, p_argvp_start[0]) }; // TODO
    *av++ = (struct Auxv) { AT_FLAGS, 0 };
    *av++ = (struct Auxv) { AT_UID, 1000 };
    *av++ = (struct Auxv) { AT_EUID, 1000 };
    *av++ = (struct Auxv) { AT_GID, 1000 };
    *av++ = (struct Auxv) { AT_EGID, 1000 };
    *av++ = (struct Auxv) { AT_NULL, 0 };

    return true;
}

static bool
procsetup(struct TuxThread* p, uint8_t* prog, size_t progsz, uint8_t* interp, size_t interpsz, int argc, char** argv)
{
    struct ELFLoadInfo info = {0};
    bool b = elfload(p, prog, progsz, interp, interpsz, &info);
    if (!b)
        return false;

    asptr_t sp;
    if (!stacksetup(p->proc, argc, argv, &info, &sp))
        return false;

    asptr_t entry = info.elfentry;
    if (interp != NULL)
        entry = info.ldentry;

    struct TuxRegs* regs = pal_ctx_regs(p->p_ctx);
    regs_init(regs, entry, sp);

    p->proc->brkbase = info.lastva;
    p->proc->brksize = 0;

    // Reserve the brk region.
    const int mapflags = TUX_MAP_PRIVATE | TUX_MAP_ANONYMOUS;
    asptr_t brkregion = pal_as_mapat(p->proc->p_as, p->proc->brkbase, TUX_BRKMAXSIZE, TUX_PROT_NONE, mapflags, NULL, 0);
    if (brkregion == (asptr_t) -1)
        return false;

    return true;
}

int
procmapany(struct TuxProc* p, size_t size, int prot, int flags, int fd,
        off_t offset, asptr_t* o_mapstart)
{
    struct HostFile* hf = NULL;
    if (fd >= 0) {
        struct FDFile* f = fdget(&p->fdtable, fd);
        if (!f)
            return -TUX_EBADF;
        if (!f->file)
            return -TUX_EACCES;
        hf = f->file(f->dev);
    }
    LOCK_WITH_DEFER(&p->lk_as, lk_as);
    asptr_t addr = pal_as_mapany(p->p_as, size, prot, flags, hf, offset);
    if (addr == (asptr_t) -1)
        return -TUX_EINVAL;
    *o_mapstart = (uintptr_t) addr;
    return 0;
}

int
procmapat(struct TuxProc* p, asptr_t start, size_t size, int prot, int flags,
        int fd, off_t offset)
{
    struct HostFile* hf = NULL;
    if (fd >= 0) {
        struct FDFile* f = fdget(&p->fdtable, fd);
        if (!f)
            return -TUX_EBADF;
        if (!f->file)
            return -TUX_EACCES;
        hf = f->file(f->dev);
    }
    LOCK_WITH_DEFER(&p->lk_as, lk_as);
    asptr_t addr = pal_as_mapat(p->p_as, start, size, prot, flags, hf, offset);
    if (addr == (asptr_t) -1)
        return -TUX_EINVAL;
    return 0;
}

int
procunmap(struct TuxProc* p, asptr_t start, size_t size)
{
    LOCK_WITH_DEFER(&p->lk_as, lk_as);
    return pal_as_munmap(p->p_as, start, size);
}

static void
procfree(struct TuxThread* p)
{
    (void) p;
    // TODO: free p
}

EXPORT struct TuxThread*
tux_proc_newfile(struct Tux* tux, uint8_t* prog, size_t progsz, int argc, char** argv)
{
    return procnewfile(tux, prog, progsz, argc, argv);
}
