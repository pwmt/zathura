/* SPDX-License-Identifier: Zlib */

#include "seccomp-filters.h"

#ifdef WITH_SECCOMP
#include <girara/log.h>
#include <seccomp.h> /* libseccomp */
#include <sys/prctl.h> /* prctl */
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <girara/utils.h>
#include <linux/sched.h> /* for clone filter */

#ifdef GDK_WINDOWING_X11
#include <gtk/gtkx.h>
#endif


#define ADD_RULE(str_action, action, call, ...)                                \
  do {                                                                         \
    girara_debug("adding rule " str_action " to " G_STRINGIFY(call));          \
    const int err =                                                            \
      seccomp_rule_add(ctx, action, SCMP_SYS(call), __VA_ARGS__);              \
    if (err < 0) {                                                             \
      girara_error("failed: %s", g_strerror(-err));                            \
      goto out;                                                                \
    }                                                                          \
  } while (0)

#define DENY_RULE(call) ADD_RULE("kill", SCMP_ACT_KILL, call, 0)
#define ALLOW_RULE(call) ADD_RULE("allow", SCMP_ACT_ALLOW, call, 0)

int
seccomp_enable_basic_filter(void)
{
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
  scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
  if (ctx == NULL) {
    girara_error("seccomp_init failed");
    return -1;
  }

  DENY_RULE(_sysctl);
  DENY_RULE(acct);
  DENY_RULE(add_key);
  DENY_RULE(adjtimex);
  DENY_RULE(chroot);
  DENY_RULE(clock_adjtime);
  DENY_RULE(create_module);
  DENY_RULE(delete_module);
  DENY_RULE(fanotify_init);
  DENY_RULE(finit_module);
  DENY_RULE(get_kernel_syms);
  DENY_RULE(get_mempolicy);
  DENY_RULE(init_module);
  DENY_RULE(io_cancel);
  DENY_RULE(io_destroy);
  DENY_RULE(io_getevents);
  DENY_RULE(io_setup);
  DENY_RULE(io_submit);
  DENY_RULE(ioperm);
  DENY_RULE(iopl);
  DENY_RULE(ioprio_set);
  DENY_RULE(kcmp);
  DENY_RULE(kexec_file_load);
  DENY_RULE(kexec_load);
  DENY_RULE(keyctl);
  DENY_RULE(lookup_dcookie);
  DENY_RULE(mbind);
  DENY_RULE(nfsservctl);
  DENY_RULE(migrate_pages);
  DENY_RULE(modify_ldt);
  DENY_RULE(mount);
  DENY_RULE(move_pages);
  DENY_RULE(name_to_handle_at);
  DENY_RULE(open_by_handle_at);
  DENY_RULE(perf_event_open);
  DENY_RULE(pivot_root);
  DENY_RULE(process_vm_readv);
  DENY_RULE(process_vm_writev);
  DENY_RULE(ptrace);
  DENY_RULE(reboot);
  DENY_RULE(remap_file_pages);
  DENY_RULE(request_key);
  DENY_RULE(set_mempolicy);
  DENY_RULE(swapoff);
  DENY_RULE(swapon);
  DENY_RULE(sysfs);
  DENY_RULE(syslog);
  DENY_RULE(tuxcall);
  DENY_RULE(umount2);
  DENY_RULE(uselib);
  DENY_RULE(vmsplice);

  /*TODO
   *
   * In case this basic filter is actually triggered, print a clear error message to report this
   *   The syscalls here should never be executed by an unprivileged process
   *
   * */

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

int
seccomp_enable_strict_filter(zathura_t* zathura)
{
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
  if (ctx == NULL){
    girara_error("seccomp_init failed");
    return -1;
  }

  ALLOW_RULE(access);
  ALLOW_RULE(bind);
  ALLOW_RULE(brk);
  ALLOW_RULE(clock_getres);
  /* ALLOW_RULE(clone); specified below */
  ALLOW_RULE(close);
  ALLOW_RULE(eventfd2);
  ALLOW_RULE(exit);
  ALLOW_RULE(exit_group);
  ALLOW_RULE(epoll_create);
  ALLOW_RULE(epoll_ctl);
  ALLOW_RULE(fadvise64);
  ALLOW_RULE(fallocate);
  ALLOW_RULE(fcntl);  /* TODO: build detailed filter */
  ALLOW_RULE(fstat);
  ALLOW_RULE(fstatfs);
  ALLOW_RULE(ftruncate);
  ALLOW_RULE(futex);
  ALLOW_RULE(getdents);
  ALLOW_RULE(getdents64);
  ALLOW_RULE(getegid);
  ALLOW_RULE(geteuid);
  ALLOW_RULE(getgid);
  ALLOW_RULE(getuid);
  ALLOW_RULE(getpid);
  ALLOW_RULE(getppid);
  ALLOW_RULE(gettid);
  ALLOW_RULE(getrandom);
  ALLOW_RULE(getresgid);
  ALLOW_RULE(getresuid);
  ALLOW_RULE(getrlimit);
  ALLOW_RULE(getpeername);
  ALLOW_RULE(inotify_add_watch);
  ALLOW_RULE(inotify_init1);
  ALLOW_RULE(inotify_rm_watch);
  /* ALLOW_RULE (ioctl); specified below  */
  ALLOW_RULE(lseek);
  ALLOW_RULE(lstat);
  ALLOW_RULE(madvise);
  ALLOW_RULE(memfd_create);
  ALLOW_RULE(mmap);
  ALLOW_RULE(mprotect);
  ALLOW_RULE(mremap);
  ALLOW_RULE(munmap);
  ALLOW_RULE(newfstatat);
  /* ALLOW_RULE (open); specified below */
  /* ALLOW_RULE (openat); specified below */
  ALLOW_RULE(pipe);
  ALLOW_RULE(pipe2);
  ALLOW_RULE(poll);
  ALLOW_RULE(pwrite64); 
  ALLOW_RULE(pread64);
  /* ALLOW_RULE (prctl); specified below  */
  ALLOW_RULE(read);
  ALLOW_RULE(readlink);
  ALLOW_RULE(recvfrom);
  ALLOW_RULE(recvmsg);
  ALLOW_RULE(restart_syscall);
  ALLOW_RULE(rseq);
  ALLOW_RULE(rt_sigaction);
  ALLOW_RULE(rt_sigprocmask);
  ALLOW_RULE(sched_setattr);
  ALLOW_RULE(sched_getattr);
  ALLOW_RULE(sendmsg);
  ALLOW_RULE(sendto);
  ALLOW_RULE(select);
  ALLOW_RULE(set_robust_list);
  ALLOW_RULE(shmat);
  ALLOW_RULE(shmctl);
  ALLOW_RULE(shmdt);
  ALLOW_RULE(shmget);
  ALLOW_RULE(shutdown);
  ALLOW_RULE(stat);
  ALLOW_RULE(statx);
  ALLOW_RULE(statfs);
  ALLOW_RULE(sysinfo);
  ALLOW_RULE(umask); /* required by X11 */
  ALLOW_RULE(uname);
  ALLOW_RULE(unlink);
  ALLOW_RULE(write);  
  ALLOW_RULE(writev);
  ALLOW_RULE(wait4);  

  /* required for testing only */
  ALLOW_RULE(timer_create);
  ALLOW_RULE(timer_delete);


/* Permit X11 specific syscalls */
#ifdef GDK_WINDOWING_X11
  GdkDisplay* display = gtk_widget_get_display(zathura->ui.session->gtk.view);

  if (GDK_IS_X11_DISPLAY (display)) {
  
    girara_debug("On X11, supporting X11 syscalls");

    /* permit the socket syscall for local UNIX domain sockets (required by X11) */
    ADD_RULE("allow", SCMP_ACT_ALLOW, socket, 1, SCMP_CMP(0, SCMP_CMP_EQ, AF_UNIX));

    ALLOW_RULE(mkdir);
    ALLOW_RULE(setsockopt);
    ALLOW_RULE(connect);
  }
  else {
    girara_debug("On Wayland, blocking X11 syscalls");
  }
#endif


  /* filter clone arguments */
  ADD_RULE("allow", SCMP_ACT_ALLOW, clone, 1, SCMP_CMP(0, SCMP_CMP_EQ, \
              CLONE_VM | \
              CLONE_FS | \
              CLONE_FILES | \
              CLONE_SIGHAND | \
              CLONE_THREAD | \
              CLONE_SYSVSEM | \
              CLONE_SETTLS | \
              CLONE_PARENT_SETTID | \
              CLONE_CHILD_CLEARTID));


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

  /* special restrictions for open, prevent opening files for writing */
  ADD_RULE("allow", SCMP_ACT_ALLOW,         open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), open, 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR));

  /* special restrictions for openat, prevent opening files for writing */
  ADD_RULE("allow", SCMP_ACT_ALLOW,         openat, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), openat, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY));
  ADD_RULE("errno", SCMP_ACT_ERRNO(EACCES), openat, 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR));




  /* Sandbox Status Notes:
   *
   * write: no actual files on the filesystem are opened with write permissions 
   *    exception is /run/user/UID/dconf/user (file descriptor not available during runtime)
   *
   *
   * mkdir: needed for first run only to create /run/user/UID/dconf (before seccomp init)
   * wait4: required to attempt opening links (which is then blocked)
   *
   * X11 environments require umask and socket syscalls after sandbox setup 
   *   no longer supported since X11 cannot be easily secured anyway
   *
   * TODO: prevent dbus socket connection before sandbox init - by checking the sandbox settings in zathurarc 
   *
   * TODO: check requirement of pipe/pipe2 syscalls when dbus is disabled
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

#endif /* WITH_SECCOMP */
