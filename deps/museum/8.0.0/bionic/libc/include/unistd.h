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

#include <bits/fcntl.h>
#include <bits/getopt.h>
#include <bits/ioctl.h>
#include <bits/lockf.h>
#include <bits/posix_limits.h>
#include <bits/seek_constants.h>
#include <bits/sysconf.h>

__BEGIN_DECLS

#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

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

__noreturn void _exit(int __status);

pid_t  fork(void);
pid_t  vfork(void);
pid_t  getpid(void);
pid_t  gettid(void) __attribute_const__;
pid_t  getpgid(pid_t __pid);
int    setpgid(pid_t __pid, pid_t __pgid);
pid_t  getppid(void);
pid_t  getpgrp(void);
int    setpgrp(void);
pid_t  getsid(pid_t __pid) __INTRODUCED_IN(17);
pid_t  setsid(void);

int execv(const char* __path, char* const* __argv);
int execvp(const char* __file, char* const* __argv);
int execvpe(const char* __file, char* const* __argv, char* const* __envp) __INTRODUCED_IN(21);
int execve(const char* __file, char* const* __argv, char* const* __envp);
int execl(const char* __path, const char* __arg0, ...) __attribute__((__sentinel__));
int execlp(const char* __file, const char* __arg0, ...) __attribute__((__sentinel__));
int execle(const char* __path, const char* __arg0, ... /*,  char* const* __envp */)
    __attribute__((__sentinel__(1)));

int nice(int __incr);

int setuid(uid_t __uid);
uid_t getuid(void);
int seteuid(uid_t __uid);
uid_t geteuid(void);
int setgid(gid_t __gid);
gid_t getgid(void);
int setegid(gid_t __gid);
gid_t getegid(void);
int getgroups(int __size, gid_t* __list);
int setgroups(size_t __size, const gid_t* __list);
int setreuid(uid_t __ruid, uid_t __euid);
int setregid(gid_t __rgid, gid_t __egid);
int setresuid(uid_t __ruid, uid_t __euid, uid_t __suid);
int setresgid(gid_t __rgid, gid_t __egid, gid_t __sgid);
int getresuid(uid_t* __ruid, uid_t* __euid, uid_t* __suid);
int getresgid(gid_t* __rgid, gid_t* __egid, gid_t* __sgid);
char* getlogin(void);

long fpathconf(int __fd, int __name);
long pathconf(const char* __path, int __name);

int access(const char* __path, int __mode);
int faccessat(int __dirfd, const char* __path, int __mode, int __flags) __INTRODUCED_IN(16);
int link(const char* __oldpath, const char* __newpath);
int linkat(int __olddirfd, const char* __oldpath, int __newdirfd,
           const char* __newpath, int __flags) __INTRODUCED_IN(21);
int unlink(const char* __path);
int unlinkat(int __dirfd, const char* __path, int __flags);
int chdir(const char* __path);
int fchdir(int __fd);
int rmdir(const char* __path);
int pipe(int* __pipefd);
#if defined(__USE_GNU)
int pipe2(int* __pipefd, int __flags) __INTRODUCED_IN(9);
#endif
int chroot(const char* __path);
int symlink(const char* __oldpath, const char* __newpath);
int symlinkat(const char* __oldpath, int __newdirfd, const char* __newpath) __INTRODUCED_IN(21);
ssize_t readlink(const char* __path, char* __buf, size_t __bufsiz)
    __overloadable __RENAME_CLANG(readlink);
ssize_t readlinkat(int __dirfd, const char* __path, char* __buf,
                   size_t __bufsiz)
    __INTRODUCED_IN(21) __overloadable __RENAME_CLANG(readlinkat);
int chown(const char* __path, uid_t __owner, gid_t __group);
int fchown(int __fd, uid_t __owner, gid_t __group);
int fchownat(int __dirfd, const char* __path, uid_t __owner, gid_t __group, int __flags);
int lchown(const char* __path, uid_t __owner, gid_t __group);
char* getcwd(char* __buf, size_t __size) __overloadable __RENAME_CLANG(getcwd);

void sync(void);

int close(int __fd);

ssize_t read(int __fd, void* __buf, size_t __count) __overloadable
    __RENAME_CLANG(read);
ssize_t write(int __fd, const void* __buf, size_t __count) __overloadable
    __RENAME_CLANG(write);

int dup(int __oldfd);
int dup2(int __oldfd, int __newfd);
int dup3(int __oldfd, int __newfd, int __flags) __INTRODUCED_IN(21);
int fsync(int __fd);
int fdatasync(int __fd) __INTRODUCED_IN(9);

#if defined(__USE_FILE_OFFSET64)
off_t lseek(int __fd, off_t __offset, int __whence) __RENAME(lseek64);
#else
off_t lseek(int __fd, off_t __offset, int __whence);
#endif

off64_t lseek64(int __fd, off64_t __offset, int __whence);

#if defined(__USE_FILE_OFFSET64) && __ANDROID_API__ >= __ANDROID_API_L__
int truncate(const char* __path, off_t __length) __RENAME(truncate64) __INTRODUCED_IN(21);
ssize_t pread(int __fd, void* __buf, size_t __count, off_t __offset)
  __overloadable __RENAME(pread64) __INTRODUCED_IN(12);
ssize_t pwrite(int __fd, const void* __buf, size_t __count, off_t __offset)
  __overloadable __RENAME(pwrite64) __INTRODUCED_IN(12);
int ftruncate(int __fd, off_t __length) __RENAME(ftruncate64) __INTRODUCED_IN(12);
#else
int truncate(const char* __path, off_t __length);
ssize_t pread(int __fd, void* __buf, size_t __count, off_t __offset)
    __overloadable __RENAME_CLANG(pread);
ssize_t pwrite(int __fd, const void* __buf, size_t __count, off_t __offset)
    __overloadable __RENAME_CLANG(pwrite);
int ftruncate(int __fd, off_t __length);
#endif

int truncate64(const char* __path, off64_t __length) __INTRODUCED_IN(21);
ssize_t pread64(int __fd, void* __buf, size_t __count, off64_t __offset)
    __INTRODUCED_IN(12) __overloadable __RENAME_CLANG(pread64);
ssize_t pwrite64(int __fd, const void* __buf, size_t __count, off64_t __offset)
    __INTRODUCED_IN(12) __overloadable __RENAME_CLANG(pwrite64);
int ftruncate64(int __fd, off64_t __length) __INTRODUCED_IN(12);

int pause(void);
unsigned int alarm(unsigned int __seconds);
unsigned int sleep(unsigned int __seconds);
int usleep(useconds_t __usec);

int gethostname(char* __name, size_t __len);
int sethostname(const char* __name, size_t __len) __INTRODUCED_IN(23);

int brk(void* __addr);
void* sbrk(ptrdiff_t __increment);

int isatty(int __fd);
char* ttyname(int __fd);
int ttyname_r(int __fd, char* __buf, size_t __buflen) __INTRODUCED_IN(8);

int acct(const char* __filepath);

#if __ANDROID_API__ >= __ANDROID_API_L__
int getpagesize(void) __INTRODUCED_IN(21);
#else
static __inline__ int getpagesize(void) {
  return sysconf(_SC_PAGESIZE);
}
#endif

long syscall(long __number, ...);

int daemon(int __nochdir, int __noclose);

#if defined(__arm__) || (defined(__mips__) && !defined(__LP64__))
int cacheflush(long __addr, long __nbytes, long __cache);
    /* __attribute__((deprecated("use __builtin___clear_cache instead"))); */
#endif

pid_t tcgetpgrp(int __fd);
int tcsetpgrp(int __fd, pid_t __pid);

/* Used to retry syscalls that can return EINTR. */
#define TEMP_FAILURE_RETRY(exp) ({         \
    __typeof__(exp) _rc;                   \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && errno == EINTR); \
    _rc; })

/* TODO(unified-headers): Factor out all the FORTIFY features. */
char* __getcwd_chk(char*, size_t, size_t) __INTRODUCED_IN(24);

ssize_t __pread_chk(int, void*, size_t, off_t, size_t) __INTRODUCED_IN(23);
ssize_t __pread_real(int, void*, size_t, off_t) __RENAME(pread);

ssize_t __pread64_chk(int, void*, size_t, off64_t, size_t) __INTRODUCED_IN(23);
ssize_t __pread64_real(int, void*, size_t, off64_t) __RENAME(pread64) __INTRODUCED_IN(12);

ssize_t __pwrite_chk(int, const void*, size_t, off_t, size_t) __INTRODUCED_IN(24);
ssize_t __pwrite_real(int, const void*, size_t, off_t) __RENAME(pwrite);

ssize_t __pwrite64_chk(int, const void*, size_t, off64_t, size_t) __INTRODUCED_IN(24);
ssize_t __pwrite64_real(int, const void*, size_t, off64_t) __RENAME(pwrite64)
  __INTRODUCED_IN(12);

ssize_t __read_chk(int, void*, size_t, size_t) __INTRODUCED_IN(21);
ssize_t __write_chk(int, const void*, size_t, size_t) __INTRODUCED_IN(24);
ssize_t __readlink_chk(const char*, char*, size_t, size_t) __INTRODUCED_IN(23);
ssize_t __readlinkat_chk(int dirfd, const char*, char*, size_t, size_t) __INTRODUCED_IN(23);

int getdomainname(char*, size_t) __INTRODUCED_IN(26);
int setdomainname(const char*, size_t) __INTRODUCED_IN(26);

#if defined(__BIONIC_FORTIFY)

#if defined(__USE_FILE_OFFSET64)
#define __PREAD_PREFIX(x) __pread64_ ## x
#define __PWRITE_PREFIX(x) __pwrite64_ ## x
#else
#define __PREAD_PREFIX(x) __pread_ ## x
#define __PWRITE_PREFIX(x) __pwrite_ ## x
#endif

#if defined(__clang__)
#define __error_if_overflows_ssizet(what) \
    __enable_if(what > SSIZE_MAX, #what " must be <= SSIZE_MAX") \
    __errorattr(#what " must be <= SSIZE_MAX")

#define __enable_if_no_overflow_ssizet(what) \
    __enable_if((what) <= SSIZE_MAX, "enabled if " #what " <= SSIZE_MAX")

#define __error_if_overflows_objectsize(what, objsize) \
    __enable_if((objsize) != __BIONIC_FORTIFY_UNKNOWN_SIZE && \
                    (what) > (objsize), \
                "'" #what "' bytes overflows the given object") \
    __errorattr("'" #what "' bytes overflows the given object")

__BIONIC_ERROR_FUNCTION_VISIBILITY
char* getcwd(char* buf, size_t size) __overloadable
        __error_if_overflows_objectsize(size, __bos(buf));

#if __ANDROID_API__ >= __ANDROID_API_N__
__BIONIC_FORTIFY_INLINE
char* getcwd(char* const __pass_object_size buf, size_t size) __overloadable {
    size_t bos = __bos(buf);

    /*
     * Clang responds bos==0 if buf==NULL
     * (https://llvm.org/bugs/show_bug.cgi?id=23277). Given that NULL is a valid
     * value, we need to handle that.
     */
    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE || buf == NULL) {
        return __call_bypassing_fortify(getcwd)(buf, size);
    }

    return __getcwd_chk(buf, size, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_N__ */

#if __ANDROID_API__ >= __ANDROID_API_M__
__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t pread(int fd, void* buf, size_t count, off_t offset) __overloadable
        __error_if_overflows_ssizet(count);

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t pread(int fd, void* buf, size_t count, off_t offset) __overloadable
        __enable_if_no_overflow_ssizet(count)
        __error_if_overflows_objectsize(count, __bos0(buf));

__BIONIC_FORTIFY_INLINE
ssize_t pread(int fd, void* const __pass_object_size0 buf, size_t count,
              off_t offset) __overloadable {
    size_t bos = __bos0(buf);

    if (count == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __PREAD_PREFIX(real)(fd, buf, count, offset);
    }

    return __PREAD_PREFIX(chk)(fd, buf, count, offset, bos);
}

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t pread64(int fd, void* buf, size_t count, off64_t offset) __overloadable
        __error_if_overflows_ssizet(count);

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t pread64(int fd, void* buf, size_t count, off64_t offset) __overloadable
        __enable_if_no_overflow_ssizet(count)
        __error_if_overflows_objectsize(count, __bos0(buf));

__BIONIC_FORTIFY_INLINE
ssize_t pread64(int fd, void* const __pass_object_size0 buf, size_t count,
                off64_t offset) __overloadable {
    size_t bos = __bos0(buf);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __pread64_real(fd, buf, count, offset);
    }

    return __pread64_chk(fd, buf, count, offset, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_M__ */

#if __ANDROID_API__ >= __ANDROID_API_N__
__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset)
        __overloadable
        __error_if_overflows_ssizet(count);

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset)
        __overloadable
        __enable_if_no_overflow_ssizet(count)
        __error_if_overflows_objectsize(count, __bos0(buf));

__BIONIC_FORTIFY_INLINE
ssize_t pwrite(int fd, const void* const __pass_object_size0 buf, size_t count,
               off_t offset) __overloadable {
    size_t bos = __bos0(buf);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __PWRITE_PREFIX(real)(fd, buf, count, offset);
    }

    return __PWRITE_PREFIX(chk)(fd, buf, count, offset, bos);
}

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t pwrite64(int fd, const void* buf, size_t count, off64_t offset)
        __overloadable
        __error_if_overflows_ssizet(count);

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t pwrite64(int fd, const void* buf, size_t count, off64_t offset)
        __overloadable
        __enable_if_no_overflow_ssizet(count)
        __error_if_overflows_objectsize(count, __bos0(buf));

__BIONIC_FORTIFY_INLINE
ssize_t pwrite64(int fd, const void* const __pass_object_size0 buf,
                 size_t count, off64_t offset) __overloadable {
    size_t bos = __bos0(buf);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __pwrite64_real(fd, buf, count, offset);
    }

    return __pwrite64_chk(fd, buf, count, offset, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_N__ */

#if __ANDROID_API__ >= __ANDROID_API_L__
__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t read(int fd, void* buf, size_t count) __overloadable
        __error_if_overflows_ssizet(count);

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t read(int fd, void* buf, size_t count) __overloadable
        __enable_if_no_overflow_ssizet(count)
        __error_if_overflows_objectsize(count, __bos0(buf));

__BIONIC_FORTIFY_INLINE
ssize_t read(int fd, void* const __pass_object_size0 buf, size_t count)
        __overloadable {
    size_t bos = __bos0(buf);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __call_bypassing_fortify(read)(fd, buf, count);
    }

    return __read_chk(fd, buf, count, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_L__ */

#if __ANDROID_API__ >= __ANDROID_API_N__
__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t write(int fd, const void* buf, size_t count) __overloadable
        __error_if_overflows_ssizet(count);

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t write(int fd, const void* buf, size_t count) __overloadable
        __enable_if_no_overflow_ssizet(count)
        __error_if_overflows_objectsize(count, __bos0(buf));

__BIONIC_FORTIFY_INLINE
ssize_t write(int fd, const void* const __pass_object_size0 buf, size_t count)
        __overloadable {
    size_t bos = __bos0(buf);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __call_bypassing_fortify(write)(fd, buf, count);
    }

    return __write_chk(fd, buf, count, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_N__ */

#if __ANDROID_API__ >= __ANDROID_API_M__
__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t readlink(const char* path, char* buf, size_t size) __overloadable
        __error_if_overflows_ssizet(size);

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t readlink(const char* path, char* buf, size_t size) __overloadable
        __enable_if_no_overflow_ssizet(size)
        __error_if_overflows_objectsize(size, __bos(buf));

__BIONIC_FORTIFY_INLINE
ssize_t readlink(const char* path, char* const __pass_object_size buf,
                 size_t size) __overloadable {
    size_t bos = __bos(buf);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __call_bypassing_fortify(readlink)(path, buf, size);
    }

    return __readlink_chk(path, buf, size, bos);
}

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t readlinkat(int dirfd, const char* path, char* buf, size_t size)
        __overloadable
        __error_if_overflows_ssizet(size);

__BIONIC_ERROR_FUNCTION_VISIBILITY
ssize_t readlinkat(int dirfd, const char* path, char* buf, size_t size)
        __overloadable
        __enable_if_no_overflow_ssizet(size)
        __error_if_overflows_objectsize(size, __bos(buf));

__BIONIC_FORTIFY_INLINE
ssize_t readlinkat(int dirfd, const char* path,
                   char* const __pass_object_size buf, size_t size)
        __overloadable {
    size_t bos = __bos(buf);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __call_bypassing_fortify(readlinkat)(dirfd, path, buf, size);
    }

    return __readlinkat_chk(dirfd, path, buf, size, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_M__ */

#undef __enable_if_no_overflow_ssizet
#undef __error_if_overflows_objectsize
#undef __error_if_overflows_ssizet
#else /* defined(__clang__) */

char* __getcwd_real(char*, size_t) __RENAME(getcwd);
ssize_t __read_real(int, void*, size_t) __RENAME(read);
ssize_t __write_real(int, const void*, size_t) __RENAME(write);
ssize_t __readlink_real(const char*, char*, size_t) __RENAME(readlink);
ssize_t __readlinkat_real(int dirfd, const char*, char*, size_t) __RENAME(readlinkat);

__errordecl(__getcwd_dest_size_error, "getcwd called with size bigger than destination");
__errordecl(__pread_dest_size_error, "pread called with size bigger than destination");
__errordecl(__pread_count_toobig_error, "pread called with count > SSIZE_MAX");
__errordecl(__pread64_dest_size_error, "pread64 called with size bigger than destination");
__errordecl(__pread64_count_toobig_error, "pread64 called with count > SSIZE_MAX");
__errordecl(__pwrite_dest_size_error, "pwrite called with size bigger than destination");
__errordecl(__pwrite_count_toobig_error, "pwrite called with count > SSIZE_MAX");
__errordecl(__pwrite64_dest_size_error, "pwrite64 called with size bigger than destination");
__errordecl(__pwrite64_count_toobig_error, "pwrite64 called with count > SSIZE_MAX");
__errordecl(__read_dest_size_error, "read called with size bigger than destination");
__errordecl(__read_count_toobig_error, "read called with count > SSIZE_MAX");
__errordecl(__write_dest_size_error, "write called with size bigger than destination");
__errordecl(__write_count_toobig_error, "write called with count > SSIZE_MAX");
__errordecl(__readlink_dest_size_error, "readlink called with size bigger than destination");
__errordecl(__readlink_size_toobig_error, "readlink called with size > SSIZE_MAX");
__errordecl(__readlinkat_dest_size_error, "readlinkat called with size bigger than destination");
__errordecl(__readlinkat_size_toobig_error, "readlinkat called with size > SSIZE_MAX");

#if __ANDROID_API__ >= __ANDROID_API_N__
__BIONIC_FORTIFY_INLINE
char* getcwd(char* buf, size_t size) __overloadable {
    size_t bos = __bos(buf);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __getcwd_real(buf, size);
    }

    if (__builtin_constant_p(size) && (size > bos)) {
        __getcwd_dest_size_error();
    }

    if (__builtin_constant_p(size) && (size <= bos)) {
        return __getcwd_real(buf, size);
    }

    return __getcwd_chk(buf, size, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_N__ */

#if __ANDROID_API__ >= __ANDROID_API_M__
__BIONIC_FORTIFY_INLINE
ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
    size_t bos = __bos0(buf);

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

    return __PREAD_PREFIX(chk)(fd, buf, count, offset, bos);
}

__BIONIC_FORTIFY_INLINE
ssize_t pread64(int fd, void* buf, size_t count, off64_t offset) {
    size_t bos = __bos0(buf);

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

    return __pread64_chk(fd, buf, count, offset, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_M__ */

#if __ANDROID_API__ >= __ANDROID_API_N__
__BIONIC_FORTIFY_INLINE
ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset) {
    size_t bos = __bos0(buf);

    if (__builtin_constant_p(count) && (count > SSIZE_MAX)) {
        __PWRITE_PREFIX(count_toobig_error)();
    }

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __PWRITE_PREFIX(real)(fd, buf, count, offset);
    }

    if (__builtin_constant_p(count) && (count > bos)) {
        __PWRITE_PREFIX(dest_size_error)();
    }

    if (__builtin_constant_p(count) && (count <= bos)) {
        return __PWRITE_PREFIX(real)(fd, buf, count, offset);
    }

    return __PWRITE_PREFIX(chk)(fd, buf, count, offset, bos);
}

__BIONIC_FORTIFY_INLINE
ssize_t pwrite64(int fd, const void* buf, size_t count, off64_t offset) {
    size_t bos = __bos0(buf);

    if (__builtin_constant_p(count) && (count > SSIZE_MAX)) {
        __pwrite64_count_toobig_error();
    }

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __pwrite64_real(fd, buf, count, offset);
    }

    if (__builtin_constant_p(count) && (count > bos)) {
        __pwrite64_dest_size_error();
    }

    if (__builtin_constant_p(count) && (count <= bos)) {
        return __pwrite64_real(fd, buf, count, offset);
    }

    return __pwrite64_chk(fd, buf, count, offset, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_N__ */

#if __ANDROID_API__ >= __ANDROID_API_L__
__BIONIC_FORTIFY_INLINE
ssize_t read(int fd, void* buf, size_t count) {
    size_t bos = __bos0(buf);

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

    return __read_chk(fd, buf, count, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_L__ */

#if __ANDROID_API__ >= __ANDROID_API_N__
__BIONIC_FORTIFY_INLINE
ssize_t write(int fd, const void* buf, size_t count) {
    size_t bos = __bos0(buf);

    if (bos == __BIONIC_FORTIFY_UNKNOWN_SIZE) {
        return __write_real(fd, buf, count);
    }

    if (__builtin_constant_p(count) && (count > bos)) {
        __write_dest_size_error();
    }

    if (__builtin_constant_p(count) && (count <= bos)) {
        return __write_real(fd, buf, count);
    }

    return __write_chk(fd, buf, count, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_N__ */

#if __ANDROID_API__ >= __ANDROID_API_M__
__BIONIC_FORTIFY_INLINE
ssize_t readlink(const char* path, char* buf, size_t size) {
    size_t bos = __bos(buf);

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

    return __readlink_chk(path, buf, size, bos);
}

__BIONIC_FORTIFY_INLINE
ssize_t readlinkat(int dirfd, const char* path, char* buf, size_t size) {
    size_t bos = __bos(buf);

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

    return __readlinkat_chk(dirfd, path, buf, size, bos);
}
#endif /* __ANDROID_API__ >= __ANDROID_API_M__ */
#endif /* defined(__clang__) */
#undef __PREAD_PREFIX
#undef __PWRITE_PREFIX
#endif /* defined(__BIONIC_FORTIFY) */

__END_DECLS

#endif /* _UNISTD_H_ */
