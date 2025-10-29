/* SPDX-License-Identifier: Zlib */

#include "seccomp-filters.h"

#include <girara/log.h>
#include <seccomp.h>   /* libseccomp */
#include <sys/prctl.h> /* prctl */
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <girara/utils.h>
#include <linux/sched.h> /* for clone filter */
#include <unistd.h>      /* for fstat */
#include <sys/mman.h>    /* for mmap/mprotect arguments */

#ifdef GDK_WINDOWING_X11
#include <gtk/gtkx.h>
#endif

#define ADD_RULE(str_action, action, call, ...)                                                                        \
  do {                                                                                                                 \
    girara_debug("adding rule " str_action " to " G_STRINGIFY(call));                                                  \
    const int err = seccomp_rule_add(ctx, action, SCMP_SYS(call), __VA_ARGS__);                                        \
    if (err < 0) {                                                                                                     \
      girara_error("failed: %s", g_strerror(-err));                                                                    \
      goto out;                                                                                                        \
    }                                                                                                                  \
  } while (0)

#define DENY_RULE(call) ADD_RULE("kill", SCMP_ACT_KILL, call, 0)
#define ALLOW_RULE(call) ADD_RULE("allow", SCMP_ACT_ALLOW, call, 0)
#define ERRNO_RULE(call) ADD_RULE("errno", SCMP_ACT_ERRNO(ENOSYS), call, 0)

int seccomp_enable_strict_filter(zathura_t* zathura) {
  /* prevent child processes from getting more priv e.g. via setuid, capabilities, ... */
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    girara_error("prctl SET_NO_NEW_PRIVS");
    return -1;
  }

  /* prevent escape via ptrace */
  if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0)) {
    girara_error("prctl PR_SET_DUMPABLE");
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
  /* ALLOW_RULE(bind);  unused? */
  ALLOW_RULE(brk);
  /* ALLOW_RULE(clock_getres); unused? */
  /* ALLOW_RULE(clone); specified below, clone3 see comment below */
  ALLOW_RULE(clock_gettime); /* used when vDSO function is unavailable */
  ALLOW_RULE(close);
  ALLOW_RULE(epoll_create1);
  ALLOW_RULE(epoll_ctl);
  ALLOW_RULE(eventfd2);
  ALLOW_RULE(exit);
  ALLOW_RULE(exit_group);
  /* ALLOW_RULE(epoll_create); outdated, to be removed */
  /* ALLOW_RULE(fadvise64); */
  ALLOW_RULE(faccessat); /* AArch64 requirement */
  ALLOW_RULE(fallocate);
  ALLOW_RULE(fcntl); /* TODO: build detailed filter */
#ifdef __NR_fstat
  ALLOW_RULE(fstat); /* used by older libc, stat (below), lstat(below), fstatat, newfstatat(below) */
#endif
  ALLOW_RULE(fstatfs); /* statfs (below) */
  ALLOW_RULE(ftruncate);
  ALLOW_RULE(futex);
  /* ALLOW_RULE(getdents); 32 bit only */
  ALLOW_RULE(getdents64);
  ALLOW_RULE(getegid);
  ALLOW_RULE(geteuid);
  ALLOW_RULE(getgid);
  ALLOW_RULE(getpid);
  ALLOW_RULE(getppid); /* required inside containers */
  ALLOW_RULE(gettid);
  ALLOW_RULE(gettimeofday); /* used when vDSO function is unavailable */
  ALLOW_RULE(getuid);
  ALLOW_RULE(getrandom); /* occasionally required */
  /* ALLOW_RULE(getresgid); */
  /* ALLOW_RULE(getresuid); */
  /* ALLOW_RULE(getrlimit); unused? */
  /* ALLOW_RULE(getpeername); not required if database is initializes properly */
  ALLOW_RULE(inotify_add_watch); /* required by filemonitor feature */
  ALLOW_RULE(inotify_init1);     /* used by filemonitor, inotify_init (glib<2.9) */
  ALLOW_RULE(inotify_rm_watch);  /* used by filemonitor */
  /* ALLOW_RULE (ioctl); specified below  */
  ALLOW_RULE(lseek);
  /* ALLOW_RULE(lstat); unused? */
  ALLOW_RULE(madvise);
  ALLOW_RULE(memfd_create);
  ALLOW_RULE(mmap);
#ifdef __NR_mmap2
  ALLOW_RULE(mmap2); /* Only required on 32-bit ARM systems */
#endif
  ALLOW_RULE(mprotect);
  ALLOW_RULE(mremap); /* mupdf requirement */
  ALLOW_RULE(munmap);
  ALLOW_RULE(newfstatat);
  /* ALLOW_RULE (open); specified below */
  /* ALLOW_RULE (openat); specified below */
  /* ALLOW_RULE(pipe); unused? */
  /* ALLOW_RULE(pipe2); used by dbus feature - disabled in sandbox*/
  ALLOW_RULE(poll);
  ALLOW_RULE(ppoll); /* AArch64 requirement */
  /* ALLOW_RULE (prctl); specified below  */
  ALLOW_RULE(pread64); /* equals pread */
  /* ALLOW_RULE(pwrite64); equals pwrite */
  ALLOW_RULE(read);
  ALLOW_RULE(readlink);   /* readlinkat */
  ALLOW_RULE(readlinkat); /* AArch64 requirement */
  /* ALLOW_RULE(recvfrom); X11 only */
  ALLOW_RULE(recvmsg);
  ALLOW_RULE(restart_syscall); /* required for wakeup from suspense */
  ALLOW_RULE(rseq);
  ALLOW_RULE(rt_sigaction);
  ALLOW_RULE(rt_sigprocmask);
  ALLOW_RULE(rt_sigreturn);
  /* ALLOW_RULE(sched_setattr); */
  /* ALLOW_RULE(sched_getattr); */
  ALLOW_RULE(sendmsg); /* ipc, used by wayland socket */
  /* ALLOW_RULE(sendto); ipc, investigate */
  /* ALLOW_RULE(select); pselect (equals pselect6) */
  ALLOW_RULE(set_robust_list);
  /* ALLOW_RULE(shmat); X11 only */
  /* ALLOW_RULE(shmctl); X11 only */
  /* ALLOW_RULE(shmdt); X11 only */
  /* ALLOW_RULE(shmget); X11 only */
  /* ALLOW_RULE(shutdown); */
  ALLOW_RULE(stat); /* used by older libc - Debian 11 */
  ALLOW_RULE(statx);
  ALLOW_RULE(statfs); /* used by filemonitor, fstatfs above */
  ALLOW_RULE(sysinfo);
  /* ALLOW_RULE(tgkill); investigate - used when zathura is quickly restarted and dbus socket is closed */
  /* ALLOW_RULE(umask); X11 only */
  /* ALLOW_RULE(uname); X11 only */
  /* ALLOW_RULE(unlink); unused?, unlinkat */
  ALLOW_RULE(write); /* investigate further */
  /*  ALLOW_RULE(writev); X11 only */
  /*  ALLOW_RULE(wait4); unused? */

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
    girara_warning("Running strict sandbox mode on X11 provides only \
        incomplete process isolation.");

    /* permit the socket syscall for local UNIX domain sockets (required by X11) */
    ADD_RULE("allow", SCMP_ACT_ALLOW, socket, 1, SCMP_CMP(0, SCMP_CMP_EQ, AF_UNIX));

    ALLOW_RULE(mkdir); /* mkdirat */
    ALLOW_RULE(setsockopt);
    ALLOW_RULE(getsockopt);
    ALLOW_RULE(getsockname);
    ALLOW_RULE(connect);
    ALLOW_RULE(umask);
    ALLOW_RULE(uname);
    ALLOW_RULE(shmat);
    ALLOW_RULE(shmctl);
    ALLOW_RULE(shmdt);
    ALLOW_RULE(shmget);
    ALLOW_RULE(recvfrom);
    ALLOW_RULE(writev); /* pwritev, pwritev2 */
  } else {
    girara_debug("On Wayland, blocking X11 syscalls");
  }
#endif

  /* block unsuccessful ipc attempt */
  ERRNO_RULE(getpeername);

  /* filter clone arguments */
  ADD_RULE("allow", SCMP_ACT_ALLOW, clone, 1,
           SCMP_CMP(0, SCMP_CMP_EQ,
                    CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
                        CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID));
  /* trigger fallback to clone */
  ERRNO_RULE(clone3);

  /* fcntl filter - not yet working */
  /*ADD_RULE("allow", SCMP_ACT_ALLOW, fcntl, 1, SCMP_CMP(0, SCMP_CMP_EQ, \
              F_GETFL | \
              F_SETFL | \
              F_ADD_SEALS | \
              F_SEAL_SEAL | \
              F_SEAL_SHRINK | \
              F_DUPFD_CLOEXEC | \
              F_SETFD | \
              FD_CLOEXEC )); */

  /* Special requirements for ioctl, allowed on stdout/stderr */
  ADD_RULE("allow", SCMP_ACT_ALLOW, ioctl, 1, SCMP_CMP(0, SCMP_CMP_EQ, 1));
  ADD_RULE("allow", SCMP_ACT_ALLOW, ioctl, 1, SCMP_CMP(0, SCMP_CMP_EQ, 2));

  /* special restrictions for prctl, only allow PR_SET_NAME/PR_SET_PDEATHSIG */
  ADD_RULE("allow", SCMP_ACT_ALLOW, prctl, 1, SCMP_CMP(0, SCMP_CMP_EQ, PR_SET_NAME));
  ADD_RULE("allow", SCMP_ACT_ALLOW, prctl, 1, SCMP_CMP(0, SCMP_CMP_EQ, PR_SET_PDEATHSIG));

  /* This wont work yet, because PROT_EXEC is used after seccomp is called - fix by second filter? */
  /* Prevent the creation of executeable memory */
  /* ADD_RULE("allow", SCMP_ACT_ALLOW, mmap, 1,  SCMP_CMP(2, SCMP_CMP_MASKED_EQ, PROT_READ | PROT_WRITE |
              PROT_NONE, PROT_READ | PROT_WRITE | PROT_NONE)); */

  /* Prevent the creation of executeable memory - required by some files */
  /*ADD_RULE("allow", SCMP_ACT_ALLOW, mprotect, 1,
  *         SCMP_CMP(2, SCMP_CMP_MASKED_EQ, PROT_READ | PROT_WRITE | PROT_NONE, PROT_READ | PROT_WRITE | PROT_NONE));
  */

  /* open syscall to be removed? openat is used instead */
  /* special restrictions for open, prevent opening files for writing */
  /*  ADD_RULE("allow", SCMP_ACT_ALLOW,         open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0));
   * ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY));
   * ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR));
   */

  /* special restrictions for openat, prevent opening files for writing */
  ADD_RULE("allow", SCMP_ACT_ALLOW, openat, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), openat, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), openat, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR));

  /* Gracefully fail syscalls that may be used by dependencies in the future
     These rules will still block the syscalls but since there usually is fallback code
     for new syscalls, it will not shut down zathura and give us more time to
     analyse the newly required syscall before potentionally allowing it.
  */

  ERRNO_RULE(openat2);
  /* ERRNO_RULE(faccessat2); permitted above for AArch64 */
  ERRNO_RULE(pwritev2);
#if defined(__NR_readfile) && defined(__SNR_readfile)
  ERRNO_RULE(readfile);
#endif
#if defined(__NR_fchmodat2) && defined(__SNR_fchmodat2)
  ERRNO_RULE(fchmodat2);
#endif
#if defined(__NR_map_shadow_stack) && defined(__SNR_map_shadow_stack)
  ERRNO_RULE(map_shadow_stack);
#endif
#if defined(__NR_mseal) && defined(__SNR_mseal)
  ERRNO_RULE(mseal);
#endif

  /* Sandbox Status Notes:
   *
   * write: no actual files on the filesystem are opened with write permissions
   *    exception is /run/user/UID/dconf/user (file descriptor not available during runtime)
   *
   *
   * mkdir: needed for first run only to create /run/user/UID/dconf (before seccomp init)
   * wait4: required to attempt opening links (which is then blocked)
   *
   *
   * Note about clone3():
   * Since the seccomp mechanism is unable to examine system-call arguments that are passed in separate structures
   * it will be unable to make decisions based on the flags given to clone3().
   * Code meant to be sandboxed with seccomp should not use clone3() at all until it is possible to inspect its
   * arguments.
   *
   *
   * Ideas for additional restrictions:
   * Apply second filter after the file has been opened but before it is parsed
   * This may allow the removal of additional syscalls
   * Additional restrictions of commands such as open may be required since it won't allow opening new files
   * Note: during rendering zathura still openes fonts, which makes this infeasible
   * - a second landlock filter would be an alternative solution to restrict read access to /usr/share/fonts only.
   *
   * Alternative sandbox approach:
   * Currently the entire sandbox is provided in a separate binary and prevents the use of
   * some features, including usability services.
   * An alternative approach would be the use of sandboxed-api in poppler and mupdf
   * This would allow the creation of a separate rendering process that can be restricted without removing features
   *
   */

  /* when zathura is run on wayland, with X11 server available but blocked, unset the DISPLAY variable */
  /* otherwise it will try to connect to X11 using inet socket protocol */

  /* applying filter... */
  if (seccomp_load(ctx) >= 0) {
    /* free ctx after the filter has been loaded into the kernel */
    seccomp_release(ctx);
    return 0;
  }

out:
  /* something went wrong */
  seccomp_release(ctx);
  return -1;
}
