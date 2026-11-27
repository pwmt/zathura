/* SPDX-License-Identifier: Zlib */

#define _GNU_SOURCE
#include "seccomp-filters.h"

#include <girara/log.h>
#include <seccomp.h>   /* libseccomp */
#include <sys/prctl.h> /* prctl */
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <girara/utils.h>
#include <linux/sched.h>  /* for clone filter */
#include <sys/mman.h>     /* for madvise arguments */
#include <sys/ioctl.h>    /* for TCGETS, TCGETS2 and TIOCGWINSZ */
#include <asm/termbits.h> /* for struct termios2 used by TCGETS2 */

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#define ADD_RULE(str_action, action, call, ...)                                                                        \
  do {                                                                                                                 \
    girara_debug("adding rule " str_action " to " G_STRINGIFY(call));                                                  \
    const int err = seccomp_rule_add(ctx, action, SCMP_SYS(call), __VA_ARGS__);                                        \
    if (err < 0) {                                                                                                     \
      girara_error("seccomp_rule_add for " G_STRINGIFY(call) " failed: %s", g_strerror(-err));                         \
      goto out;                                                                                                        \
    }                                                                                                                  \
  } while (0)

#define DENY_RULE(call) ADD_RULE("kill", SCMP_ACT_KILL, call, 0)
#define ALLOW_RULE(call) ADD_RULE("allow", SCMP_ACT_ALLOW, call, 0)
#define ERRNO_RULE(call) ADD_RULE("errno", SCMP_ACT_ERRNO(ENOSYS), call, 0)

int seccomp_enable_strict_filter(zathura_t* zathura) {
  /* prevent child processes from getting more priv e.g. via setuid, capabilities, ... */
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    girara_error("prctl SET_NO_NEW_PRIVS: %s", g_strerror(errno));
    return -1;
  }

  /* prevent escape via ptrace */
  if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0)) {
    girara_error("prctl PR_SET_DUMPABLE: %s", g_strerror(errno));
    return -1;
  }

  /* initialize the filter */
  /* ENOSYS tells the calling process that the syscall is not implemented,
   * allowing for a potential fallback function to execute
   * scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ERRNO(ENOSYS));*/
  scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
  if (ctx == NULL) {
    girara_error("seccomp_init failed");
    return -1;
  }

  ALLOW_RULE(access); /* faccessat, faccessat2 */
  ALLOW_RULE(brk);
  /* ALLOW_RULE(clone); specified below, clone3 see comment below */
  ALLOW_RULE(clock_gettime); /* used when vDSO function is unavailable */
  ALLOW_RULE(close);
  ALLOW_RULE(epoll_create1);
  ALLOW_RULE(epoll_ctl);
  ALLOW_RULE(eventfd2);
  ALLOW_RULE(exit);
  ALLOW_RULE(exit_group);
  ALLOW_RULE(faccessat);
  ALLOW_RULE(faccessat2);
  ALLOW_RULE(fallocate);
  /* ALLOW_RULE(fcntl); specified below */
#ifdef __NR_fstat
  ALLOW_RULE(fstat); /* stat, lstat, fstatat, newfstatat */
#endif
  ALLOW_RULE(fstatfs); /* statfs */
  ALLOW_RULE(ftruncate);
  ALLOW_RULE(futex);
  ALLOW_RULE(getcwd); /* used by the ps plugin */
  ALLOW_RULE(getdents64);
  ALLOW_RULE(getegid);
  ALLOW_RULE(geteuid);
  ALLOW_RULE(getgid);
  ALLOW_RULE(getpid);
  ALLOW_RULE(getppid); /* required inside containers */
  ALLOW_RULE(gettid);
  ALLOW_RULE(gettimeofday); /* used when vDSO function is unavailable */
  ALLOW_RULE(getuid);
  ALLOW_RULE(getrandom);         /* used by NSS for signature verification */
  ALLOW_RULE(inotify_add_watch); /* required by filemonitor feature */
  ALLOW_RULE(inotify_init1);     /* used by filemonitor, inotify_init (glibc<2.9) */
  ALLOW_RULE(inotify_rm_watch);  /* used by filemonitor */
  /* ALLOW_RULE (ioctl); specified below  */
  ALLOW_RULE(lseek);
#if defined(__NR_lsm_get_self_attr) && defined(__SNR_lsm_get_self_attr)
  ALLOW_RULE(lsm_get_self_attr);
  ALLOW_RULE(lsm_list_modules);
#endif
  ALLOW_RULE(lstat); /* used by signature verification on musl */
  /* ALLOW_RULE(madvise); specified below */
  ALLOW_RULE(memfd_create);
  ALLOW_RULE(mmap);
  ALLOW_RULE(mprotect);
  ALLOW_RULE(mremap);
  ALLOW_RULE(munmap);
  ALLOW_RULE(newfstatat);
  /* ALLOW_RULE (open); specified below */
  /* ALLOW_RULE (openat); specified below */
  ALLOW_RULE(poll);
  ALLOW_RULE(ppoll); /* AArch64 requirement */
  /* ALLOW_RULE (prctl); specified below  */
  ALLOW_RULE(pread64); /* equals pread */
  ALLOW_RULE(read);
  /* musl libc (and some other non-glibc libc implementations) may use readv; */
#if !defined(__GLIBC__)
  ALLOW_RULE(readv);
#endif
  ALLOW_RULE(readlink);   /* readlinkat */
  ALLOW_RULE(readlinkat); /* AArch64 requirement */
  ALLOW_RULE(recvmsg);
  ALLOW_RULE(restart_syscall); /* required for wakeup after SIGSTOP or suspend */
  ALLOW_RULE(rseq);
  ALLOW_RULE(rt_sigaction);
  ALLOW_RULE(rt_sigprocmask);
  ALLOW_RULE(rt_sigreturn);
  ALLOW_RULE(sched_yield); /* used by the cb plugin */
  ALLOW_RULE(sendmsg);     /* ipc, used by wayland socket */
  ALLOW_RULE(set_robust_list);
  ALLOW_RULE(stat);
  ALLOW_RULE(statx);
  ALLOW_RULE(statfs); /* used by filemonitor, fstatfs */
  ALLOW_RULE(sysinfo);
  ALLOW_RULE(write);
  ALLOW_RULE(writev);
  ALLOW_RULE(wait4);

  /* required for testing only */
  ALLOW_RULE(timer_create);
  ALLOW_RULE(timer_delete);

  /* pipe2 is sometimes required for commands and search - condition appears to be triggered externally */
  ALLOW_RULE(pipe2);

/* Permit X11 specific syscalls */
#ifdef GDK_WINDOWING_X11
  GdkDisplay* display = gtk_widget_get_display(zathura->ui.session->gtk.view);

  if (GDK_IS_X11_DISPLAY(display)) {
    girara_debug("On X11, supporting X11 syscalls");
    girara_warning("Running strict sandbox mode on X11 provides only incomplete process isolation.");

    /* permit the socket syscall for local UNIX domain sockets (required by X11) */
    ADD_RULE("allow", SCMP_ACT_ALLOW, socket, 1, SCMP_CMP(0, SCMP_CMP_EQ, AF_UNIX));

    ALLOW_RULE(mkdir); /* mkdirat */
    ALLOW_RULE(setsockopt);
    ALLOW_RULE(getsockopt);
    ALLOW_RULE(getsockname);
    ALLOW_RULE(connect);
    ALLOW_RULE(fchmod);
    ALLOW_RULE(sendto);
    ALLOW_RULE(umask);
    ALLOW_RULE(uname);
    ALLOW_RULE(shmat);
    ALLOW_RULE(shmctl);
    ALLOW_RULE(shmdt);
    ALLOW_RULE(shmget);
    ALLOW_RULE(recvfrom);
  } else {
    girara_debug("On Wayland, blocking X11 syscalls");
  }
#endif

  /* block unsuccessful ipc attempt */
  ERRNO_RULE(getpeername);

  /* MUSL libc still uses CLONE_DETACHED */
#if !defined(__GLIBC__)
  /* filter clone arguments */
  ADD_RULE("allow", SCMP_ACT_ALLOW, clone, 1,
           SCMP_CMP(0, SCMP_CMP_EQ,
                    CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
                        CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID | CLONE_DETACHED));
#else
  /* filter clone arguments */
  ADD_RULE("allow", SCMP_ACT_ALLOW, clone, 1,
           SCMP_CMP(0, SCMP_CMP_EQ,
                    CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
                        CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID));
#endif

  /* trigger fallback to clone */
  ERRNO_RULE(clone3);

  /* allowed for glycin runtime #928 */
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), clone, 1, SCMP_CMP(0, SCMP_CMP_MASKED_EQ, CLONE_THREAD, 0));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), kill, 0);
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), tgkill, 0);
  ALLOW_RULE(timerfd_create);
  ALLOW_RULE(timerfd_settime);
  ALLOW_RULE(timerfd_gettime);
  ALLOW_RULE(epoll_pwait);

  /* block F_SETOWN, F_SETSIG and F_SETOWN_EX, SIGIO could signal other processes */
  ADD_RULE("allow", SCMP_ACT_ALLOW, fcntl, 1, SCMP_CMP(1, SCMP_CMP_LT, F_SETOWN));
  ADD_RULE("allow", SCMP_ACT_ALLOW, fcntl, 1, SCMP_CMP(1, SCMP_CMP_EQ, F_GETOWN));
  ADD_RULE("allow", SCMP_ACT_ALLOW, fcntl, 1, SCMP_CMP(1, SCMP_CMP_GT, F_SETSIG));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), fcntl, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, 0xffffffff, F_SETOWN));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), fcntl, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, 0xffffffff, F_SETSIG));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), fcntl, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, 0xffffffff, F_SETOWN_EX));
  /* no file leases or directory notifications */
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), fcntl, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, 0xffffffff, F_SETLEASE));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), fcntl, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, 0xffffffff, F_NOTIFY));

  /* ioctl: only reading terminal settings reaches the kernel */
  ADD_RULE("allow", SCMP_ACT_ALLOW, ioctl, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, 0xffffffff, TCGETS));
#ifdef TCGETS2
  ADD_RULE("allow", SCMP_ACT_ALLOW, ioctl, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, 0xffffffff, TCGETS2));
#endif
  ADD_RULE("allow", SCMP_ACT_ALLOW, ioctl, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, 0xffffffff, TIOCGWINSZ));
  ADD_RULE("errno", SCMP_ACT_ERRNO(ENOTTY), ioctl, 1, SCMP_CMP(1, SCMP_CMP_LT, TCGETS));
  ADD_RULE("errno", SCMP_ACT_ERRNO(ENOTTY), ioctl, 1, SCMP_CMP(1, SCMP_CMP_GT, TCGETS));

  /* madvise: no page merging or forced page reclaim */
  ADD_RULE("allow", SCMP_ACT_ALLOW, madvise, 1, SCMP_CMP(2, SCMP_CMP_LT, MADV_MERGEABLE));
  ADD_RULE("allow", SCMP_ACT_ALLOW, madvise, 1, SCMP_CMP(2, SCMP_CMP_GT, MADV_MERGEABLE));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), madvise, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, 0xffffffff, MADV_MERGEABLE));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), madvise, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, 0xffffffff, MADV_PAGEOUT));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EPERM), madvise, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, 0xffffffff, MADV_COLD));

  /* special restrictions for prctl, only allow PR_SET_NAME/PR_SET_PDEATHSIG */
  ADD_RULE("allow", SCMP_ACT_ALLOW, prctl, 1, SCMP_CMP(0, SCMP_CMP_EQ, PR_SET_NAME));
  ADD_RULE("allow", SCMP_ACT_ALLOW, prctl, 1, SCMP_CMP(0, SCMP_CMP_EQ, PR_SET_PDEATHSIG));

  /* executable memory is not restricted, code written to a memfd can still be mapped executable */
  /* ADD_RULE("allow", SCMP_ACT_ALLOW, mmap, 1,  SCMP_CMP(2, SCMP_CMP_MASKED_EQ, PROT_READ | PROT_WRITE |
              PROT_NONE, PROT_READ | PROT_WRITE | PROT_NONE)); */

  /* executable memory is not restricted, code written to a memfd can still be mapped executable */
  /*ADD_RULE("allow", SCMP_ACT_ALLOW, mprotect, 1,
   *         SCMP_CMP(2, SCMP_CMP_MASKED_EQ, PROT_READ | PROT_WRITE | PROT_NONE, PROT_READ | PROT_WRITE | PROT_NONE));
   */

  /* open is used by musl */
  /* special restrictions for open, prevent opening files for writing */
  ADD_RULE("allow", SCMP_ACT_ALLOW, open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR | O_CREAT | O_TRUNC, 0));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_CREAT, O_CREAT));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_TRUNC, O_TRUNC));

  /* special restrictions for openat, prevent opening files for writing */
  ADD_RULE("allow", SCMP_ACT_ALLOW, openat, 1,
           SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR | O_CREAT | O_TRUNC, 0));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), openat, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), openat, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), openat, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_CREAT, O_CREAT));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), openat, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_TRUNC, O_TRUNC));

  /* Gracefully fail syscalls that may be used by dependencies in the future
     These rules will still block the syscalls but since there usually is fallback code
     for new syscalls, it will not shut down zathura and give us more time to
     analyse the newly required syscall before potentially allowing it.
  */

  ERRNO_RULE(openat2);
  ERRNO_RULE(fadvise64); /* optional for fontconfig but works without it */
  ERRNO_RULE(pwritev2);
#if defined(__NR_fchmodat2) && defined(__SNR_fchmodat2)
  ERRNO_RULE(fchmodat2);
#endif
#if defined(__NR_map_shadow_stack) && defined(__SNR_map_shadow_stack)
  ERRNO_RULE(map_shadow_stack);
#endif
#if defined(__NR_mseal) && defined(__SNR_mseal)
  ERRNO_RULE(mseal);
#endif
  ERRNO_RULE(close_range);
  ERRNO_RULE(epoll_pwait2);
#if defined(__NR_futex_wait) && defined(__SNR_futex_wait)
  ERRNO_RULE(futex_wait);
  ERRNO_RULE(futex_wake);
  ERRNO_RULE(futex_requeue);
#endif
#if defined(__NR_cachestat) && defined(__SNR_cachestat)
  ERRNO_RULE(cachestat);
#endif
#if defined(__NR_statmount) && defined(__SNR_statmount)
  ERRNO_RULE(statmount);
  ERRNO_RULE(listmount);
#endif
#if defined(__NR_getxattrat) && defined(__SNR_getxattrat)
  ERRNO_RULE(getxattrat);
  ERRNO_RULE(setxattrat);
  ERRNO_RULE(listxattrat);
  ERRNO_RULE(removexattrat);
#endif
#if defined(__NR_file_getattr) && defined(__SNR_file_getattr)
  ERRNO_RULE(file_getattr);
  ERRNO_RULE(file_setattr);
#endif
#if defined(__NR_rseq_slice_yield) && defined(__SNR_rseq_slice_yield)
  ERRNO_RULE(rseq_slice_yield);
#endif

  /* Sandbox Status Notes:
   *
   * write: no actual files on the filesystem are opened with write permissions
   *
   *
   * Note about clone3():
   * Since the seccomp mechanism is unable to examine system-call arguments that are passed in separate structures
   * it will be unable to make decisions based on the flags given to clone3().
   * Code meant to be sandboxed with seccomp should not use clone3() at all until it is possible to inspect its
   * arguments.
   *
   *
   * Alternative sandbox approach:
   * Currently the entire sandbox is provided in a separate binary and prevents the use of
   * some features, including usability services.
   * An alternative approach would be the use of sandboxed-api in poppler and mupdf
   * This would allow the creation of a separate rendering process that can be restricted without removing features
   *
   */

  /* applying filter... */
  /* synchronize the filter across all threads of the process; without this
   * the filter would only apply to the calling thread, leaving worker
   * threads created during zathura_init (e.g. the renderer pool) unsandboxed */
  const int tsync_err = seccomp_attr_set(ctx, SCMP_FLTATR_CTL_TSYNC, 1);
  if (tsync_err < 0) {
    girara_error("seccomp_attr_set(TSYNC) failed: %s", g_strerror(-tsync_err));
    goto out;
  }
  const int load_err = seccomp_load(ctx);
  if (load_err < 0) {
    girara_error("seccomp_load failed: %s", g_strerror(-load_err));
    goto out;
  }
  /* free ctx after the filter has been loaded into the kernel */
  seccomp_release(ctx);
  return 0;

out:
  /* something went wrong */
  seccomp_release(ctx);
  return -1;
}
