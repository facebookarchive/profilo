/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <stddef.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/sysconf.h>

#include <machine/posix_limits.h>

__BEGIN_DECLS

#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _PC_FILESIZEBITS 0
#define _PC_LINK_MAX 1
#define _PC_MAX_CANON 2
#define _PC_MAX_INPUT 3
#define _PC_NAME_MAX 4
#define _PC_PATH_MAX 5
#define _PC_PIPE_BUF 6
#define _PC_2_SYMLINKS 7
#define _PC_ALLOC_SIZE_MIN 8
#define _PC_REC_INCR_XFER_SIZE 9
#define _PC_REC_MAX_XFER_SIZE 10
#define _PC_REC_MIN_XFER_SIZE 11
#define _PC_REC_XFER_ALIGN 12
#define _PC_SYMLINK_MAX 13
#define _PC_CHOWN_RESTRICTED 14
#define _PC_NO_TRUNC 15
#define _PC_VDISABLE 16
#define _PC_ASYNC_IO 17
#define _PC_PRIO_IO 18
#define _PC_SYNC_IO 19

extern char** environ;

extern __noreturn void _exit(int);

extern pid_t  fork(void);
extern pid_t  vfork(void);
extern pid_t  getpid(void);
extern pid_t  gettid(void) __pure2;
extern pid_t  getpgid(pid_t);
extern int    setpgid(pid_t, pid_t);
extern pid_t  getppid(void);
extern pid_t  getpgrp(void);
extern int    setpgrp(void);
extern pid_t  getsid(pid_t);
extern pid_t  setsid(void);

extern int execv(const char *, char * const *);
extern int execvp(const char *, char * const *);
extern int execvpe(const char *, char * const *, char * const *);
extern int execve(const char *, char * const *, char * const *);
extern int execl(const char *, const char *, ...);
extern int execlp(const char *, const char *, ...);
extern int execle(const char *, const char *, ...);

extern int nice(int);

extern int setuid(uid_t);
extern uid_t getuid(void);
extern int seteuid(uid_t);
extern uid_t geteuid(void);
extern int setgid(gid_t);
extern gid_t getgid(void);
extern int setegid(gid_t);
extern gid_t getegid(void);
extern int getgroups(int, gid_t *);
extern int setgroups(size_t, const gid_t *);
extern int setreuid(uid_t, uid_t);
extern int setregid(gid_t, gid_t);
extern int setresuid(uid_t, uid_t, uid_t);
extern int setresgid(gid_t, gid_t, gid_t);
extern int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
extern int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
extern char* getlogin(void);

extern long fpathconf(int, int);
extern long pathconf(const char*, int);

extern int access(const char*, int);
extern int faccessat(int, const char*, int, int);
extern int link(const char*, const char*);
extern int linkat(int, const char*, int, const char*, int);
extern int unlink(const char*);
extern int unlinkat(int, const char*, int);
extern int chdir(const char *);
extern int fchdir(int);
extern int rmdir(const char *);
extern int pipe(int *);
#if defined(__USE_GNU)
extern int pipe2(int *, int);
#endif
extern int chroot(const char *);
extern int symlink(const char*, const char*);
extern int symlinkat(const char*, int, const char*);
extern ssize_t readlink(const char*, char*, size_t);
extern ssize_t readlinkat(int, const char*, char*, size_t);
extern int chown(const char *, uid_t, gid_t);
extern int fchown(int, uid_t, gid_t);
extern int fchownat(int, const char*, uid_t, gid_t, int);
extern int lchown(const char *, uid_t, gid_t);
extern char *getcwd(char *, size_t);

extern int sync(void);

extern int close(int);

extern ssize_t read(int, void *, size_t);
extern ssize_t write(int, const void *, size_t);

extern int dup(int);
extern int dup2(int, int);
extern int dup3(int, int, int);
extern int fcntl(int, int, ...);
extern int ioctl(int, int, ...);
extern int fsync(int);
extern int fdatasync(int);

#if defined(__USE_FILE_OFFSET64)
extern int truncate(const char *, off_t) __RENAME(truncate64);
extern off_t lseek(int, off_t, int) __RENAME(lseek64);
extern ssize_t pread(int, void *, size_t, off_t) __RENAME(pread64);
extern ssize_t pwrite(int, const void *, size_t, off_t) __RENAME(pwrite64);
extern int ftruncate(int, off_t) __RENAME(ftruncate64);
#else
extern int truncate(const char *, off_t);
extern off_t lseek(int, off_t, int);
extern ssize_t pread(int, void *, size_t, off_t);
extern ssize_t pwrite(int, const void *, size_t, off_t);
extern int ftruncate(int, off_t);
#endif
extern int truncate64(const char *, off64_t);
extern off64_t lseek64(int, off64_t, int);
extern ssize_t pread64(int, void *, size_t, off64_t);
extern ssize_t pwrite64(int, const void *, size_t, off64_t);
extern int ftruncate64(int, off64_t);

extern int pause(void);
extern unsigned int alarm(unsigned int);
extern unsigned int sleep(unsigned int);
extern int usleep(useconds_t);

int gethostname(char*, size_t);
int sethostname(const char*, size_t);

extern void *__brk(void *);
extern int brk(void *);
extern void *sbrk(ptrdiff_t);

extern int getopt(int, char * const *, const char *);
extern char *optarg;
extern int optind, opterr, optopt;

extern int isatty(int);
extern char* ttyname(int);
extern int ttyname_r(int, char*, size_t);

extern int  acct(const char*  filepath);

int getpagesize(void);

long sysconf(int);

long syscall(long number, ...);

extern int daemon(int, int);

#if defined(__arm__) || (defined(__mips__) && !defined(__LP64__))
extern int cacheflush(long, long, long);
    /* __attribute__((deprecated("use __builtin___clear_cache instead"))); */
#endif

extern pid_t tcgetpgrp(int fd);
extern int   tcsetpgrp(int fd, pid_t _pid);

/* Used to retry syscalls that can return EINTR. */
#define TEMP_FAILURE_RETRY(exp) ({         \
    __typeof__(exp) _rc;                   \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && errno == EINTR); \
    _rc; })

extern ssize_t __pread_chk(int, void*, size_t, off_t, size_t);
__errordecl(__pread_dest_size_error, "pread called with size bigger than destination");
__errordecl(__pread_count_toobig_error, "pread called with count > SSIZE_MAX");
extern ssize_t __pread_real(int, void*, size_t, off_t) __RENAME(pread);

extern ssize_t __pread64_chk(int, void*, size_t, off64_t, size_t);
__errordecl(__pread64_dest_size_error, "pread64 called with size bigger than destination");
__errordecl(__pread64_count_toobig_error, "pread64 called with count > SSIZE_MAX");
extern ssize_t __pread64_real(int, void*, size_t, off64_t) __RENAME(pread64);

extern ssize_t __read_chk(int, void*, size_t, size_t);
__errordecl(__read_dest_size_error, "read called with size bigger than destination");
__errordecl(__read_count_toobig_error, "read called with count > SSIZE_MAX");
extern ssize_t __read_real(int, void*, size_t) __RENAME(read);

extern ssize_t __readlink_chk(const char*, char*, size_t, size_t);
__errordecl(__readlink_dest_size_error, "readlink called with size bigger than destination");
__errordecl(__readlink_size_toobig_error, "readlink called with size > SSIZE_MAX");
extern ssize_t __readlink_real(const char*, char*, size_t) __RENAME(readlink);

extern ssize_t __readlinkat_chk(int dirfd, const char*, char*, size_t, size_t);
__errordecl(__readlinkat_dest_size_error, "readlinkat called with size bigger than destination");
__errordecl(__readlinkat_size_toobig_error, "readlinkat called with size > SSIZE_MAX");
extern ssize_t __readlinkat_real(int dirfd, const char*, char*, size_t) __RENAME(readlinkat);

#if defined(__BIONIC_FORTIFY)

#if defined(__USE_FILE_OFFSET64)
#define __PREAD_PREFIX(x) __pread64_ ## x
#else
#define __PREAD_PREFIX(x) __pread_ ## x
#endif

__BIONIC_FORTIFY_INLINE
ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
    size_t bos = __bos0(buf);

#if !defined(__clang__)
    if (__builtin_constant_p(count) && (count > SSIZE_MAX)) {
        __PREAD_PREFIX(count_toobig_error)();
    }

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __PREAD_PREFIX(real)(fd, buf, count, offset);
    }

    if (__builtin_constant_p(count) && (count > bos)) {
        __PREAD_PREFIX(dest_size_error)();
    }

    if (__builtin_constant_p(count) && (count <= bos)) {
        return __PREAD_PREFIX(real)(fd, buf, count, offset);
    }
#endif

    return __PREAD_PREFIX(chk)(fd, buf, count, offset, bos);
}

__BIONIC_FORTIFY_INLINE
ssize_t pread64(int fd, void* buf, size_t count, off64_t offset) {
    size_t bos = __bos0(buf);

#if !defined(__clang__)
    if (__builtin_constant_p(count) && (count > SSIZE_MAX)) {
        __pread64_count_toobig_error();
    }

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __pread64_real(fd, buf, count, offset);
    }

    if (__builtin_constant_p(count) && (count > bos)) {
        __pread64_dest_size_error();
    }

    if (__builtin_constant_p(count) && (count <= bos)) {
        return __pread64_real(fd, buf, count, offset);
    }
#endif

    return __pread64_chk(fd, buf, count, offset, bos);
}

__BIONIC_FORTIFY_INLINE
ssize_t read(int fd, void* buf, size_t count) {
    size_t bos = __bos0(buf);

#if !defined(__clang__)
    if (__builtin_constant_p(count) && (count > SSIZE_MAX)) {
        __read_count_toobig_error();
    }

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __read_real(fd, buf, count);
    }

    if (__builtin_constant_p(count) && (count > bos)) {
        __read_dest_size_error();
    }

    if (__builtin_constant_p(count) && (count <= bos)) {
        return __read_real(fd, buf, count);
    }
#endif

    return __read_chk(fd, buf, count, bos);
}

__BIONIC_FORTIFY_INLINE
ssize_t readlink(const char* path, char* buf, size_t size) {
    size_t bos = __bos(buf);

#if !defined(__clang__)
    if (__builtin_constant_p(size) && (size > SSIZE_MAX)) {
        __readlink_size_toobig_error();
    }

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __readlink_real(path, buf, size);
    }

    if (__builtin_constant_p(size) && (size > bos)) {
        __readlink_dest_size_error();
    }

    if (__builtin_constant_p(size) && (size <= bos)) {
        return __readlink_real(path, buf, size);
    }
#endif

    return __readlink_chk(path, buf, size, bos);
}

__BIONIC_FORTIFY_INLINE
ssize_t readlinkat(int dirfd, const char* path, char* buf, size_t size) {
    size_t bos = __bos(buf);

#if !defined(__clang__)
    if (__builtin_constant_p(size) && (size > SSIZE_MAX)) {
        __readlinkat_size_toobig_error();
    }

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __readlinkat_real(dirfd, path, buf, size);
    }

    if (__builtin_constant_p(size) && (size > bos)) {
        __readlinkat_dest_size_error();
    }

    if (__builtin_constant_p(size) && (size <= bos)) {
        return __readlinkat_real(dirfd, path, buf, size);
    }
#endif

    return __readlinkat_chk(dirfd, path, buf, size, bos);
}

#endif /* defined(__BIONIC_FORTIFY) */

__END_DECLS

#endif /* _UNISTD_H_ */
