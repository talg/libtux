#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>

#include <dirent.h>

#include "host.h"
#include "print.h"
#include "cwalk.h"
#include "file.h"
#include "proc.h"
#include "syscalls/syscalls.h"

static bool
validflags(int flags)
{
    // TODO: validate open flags
    return true;
}

struct FDFile*
filenew(struct Tux* tux, struct HostFile* dir, const char* path, int flags, int mode)
{
    if (!validflags(flags))
        return NULL;

    int fullflags = flags;
    if ((flags & TUX_O_CLOEXEC) != 0)
        flags &= ~TUX_O_CLOEXEC;

    struct HostFile* f = host_openat(dir, path, flags, mode);
    if (!f)
        return NULL;
    return filefnew(f, fullflags);
}

static struct HostFile*
filef(void* dev)
{
    return ((struct File*) dev)->file;
}

static ssize_t
fileread(void* dev, struct TuxProc* p, uint8_t* buf, size_t buflen)
{
    (void) p;
    return host_read(filef(dev), buf, buflen);
}

static ssize_t
filewrite(void* dev, struct TuxProc* p, uint8_t* buf, size_t buflen)
{
    (void) p;
    return host_write(filef(dev), buf, buflen);
}

static ssize_t
filelseek(void* dev, struct TuxProc* p, off_t off, int whence)
{
    return host_seek(filef(dev), off, whence);
}

static int
filetruncate(void* dev, struct TuxProc* p, off_t length)
{
    return host_ftruncate(filef(dev), length);
}

static int
filechown(void* dev, struct TuxProc* p, tux_uid_t owner, tux_gid_t group)
{
    return host_fchown(filef(dev), owner, group);
}

static int
filechmod(void* dev, struct TuxProc* p, tux_mode_t mode)
{
    return host_fchmod(filef(dev), mode);
}

static int
fileclose(void* dev, struct TuxProc* p)
{
    int x = host_close(filef(dev));
    return x;
}

static int
filestat(void* dev, struct TuxProc* p, struct Stat* stat)
{
    return host_fstat(filef(dev), stat);
}

static int
filesync(void* dev, struct TuxProc* p)
{
    return host_fsync(filef(dev));
}

static ssize_t
filegetdents(void* dev, struct TuxProc* p, void* dirp, size_t count)
{
    return host_getdents64(filef(dev), (struct Dirent*) dirp, count);
}

static struct HostFile*
filefile(void* dev)
{
    return filef(dev);
}

struct FDFile*
filefnew(struct HostFile* kfile, int flags)
{
    struct File* f = malloc(sizeof(struct File));
    if (!f)
        goto err1;
    *f = (struct File) {
        .file = kfile,
        .flags = flags,
    };
    struct FDFile* ff = malloc(sizeof(struct FDFile));
    if (!ff)
        goto err2;
    *ff = (struct FDFile) {
        .dev = f,
        .refs = 0,
        .read = fileread,
        .write = filewrite,
        .lseek = filelseek,
        .close = fileclose,
        .stat_ = filestat,
        .truncate = filetruncate,
        .chown = filechown,
        .chmod = filechmod,
        .sync = filesync,
        .getdents = filegetdents,
        .file = filefile,
    };
    pthread_mutex_init(&ff->lk_refs, NULL);
    return ff;
err2:
    free(f);
err1:
    return NULL;
}
