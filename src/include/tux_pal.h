#pragma once

#include <stdio.h>
#include <stdint.h>

#include "tux_host.h"
#include "tux_arch.h"

enum {
    TUX_MAP_SHARED    = 1,
    TUX_MAP_PRIVATE   = 2,
    TUX_MAP_FIXED     = 16,
    TUX_MAP_ANONYMOUS = 32,
    TUX_MAP_DENYWRITE = 2048,
    TUX_MAP_NORESERVE = 16384,
};

enum {
    TUX_PROT_NONE  = 0,
    TUX_PROT_READ  = 1,
    TUX_PROT_WRITE = 2,
    TUX_PROT_EXEC  = 4,
};

struct PlatAddrSpace;

struct PlatContext;

struct Platform;

typedef uintptr_t asptr_t;
typedef uintptr_t asuserptr_t;

struct TuxAddrSpaceInfo {
    asptr_t base;
    size_t size;

    asptr_t minaddr;
    asptr_t maxaddr;
};

typedef void (*SysHandlerFn)(struct PlatContext* ctx);

struct PlatAddrSpace*   pal_as_new(struct Platform* plat);
struct TuxAddrSpaceInfo pal_as_info(struct PlatAddrSpace* as);
struct PlatContext*     pal_ctx_new(struct Platform* plat, struct PlatAddrSpace* as, void* ctxp, bool main);

asptr_t                 pal_as_mapat(struct PlatAddrSpace* as, asptr_t addr, size_t size, int prot, int flags, struct HostFile* f, off_t off);
asptr_t                 pal_as_mapany(struct PlatAddrSpace* as, size_t size, int prot, int flags, struct HostFile* f, off_t off);
int                     pal_as_munmap(struct PlatAddrSpace* as, asptr_t addr, size_t size);
int                     pal_as_mprotect(struct PlatAddrSpace* as, asptr_t addr, size_t size, int prot);
void                    pal_as_copyfrom(struct PlatAddrSpace* as, void* dst, asuserptr_t src, size_t size);
void                    pal_as_copyto(struct PlatAddrSpace* as, asuserptr_t dst, void* src, size_t size);
void                    pal_as_free(struct PlatAddrSpace* as);
// Only used if PAL_EXTERNAL_ADDRSPACE is not defined
asuserptr_t             pal_as_p2user(struct PlatAddrSpace* as, void* p);
void*                   pal_as_user2p(struct PlatAddrSpace* as, asuserptr_t asup);
asuserptr_t             pal_as_ap2user(struct PlatAddrSpace* as, asptr_t asp);
asptr_t                 pal_as_user2ap(struct PlatAddrSpace* as, asuserptr_t asup);

uint64_t                pal_ctx_run(struct PlatContext* ctx, struct PlatAddrSpace* as);
void*                   pal_ctx_data(struct PlatContext* ctx);
void                    pal_ctx_free(struct PlatContext* ctx);
struct TuxRegs*         pal_ctx_regs(struct PlatContext* ctx);
void                    pal_ctx_exit(struct PlatContext* ctx, uint64_t val);
void                    pal_ctx_tpset(struct PlatContext* ctx, asptr_t tp);

void                    pal_sys_handler(struct Platform* plat, SysHandlerFn fn);
