#include "seccomp-filters.h"
#include <stdio.h>

#ifdef WITH_SECCOMP
#include <girara/log.h>
#include <seccomp.h> /* libseccomp */
#include <sys/prctl.h> /* prctl */
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <girara/utils.h>

#define DENY_RULE(call) { if (seccomp_rule_add (ctx, SCMP_ACT_KILL, SCMP_SYS(call), 0) < 0) goto out; }
#define ALLOW_RULE(call) { if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(call), 0) < 0) goto out; }

int seccomp_enable_basic_filter(void){
    
    scmp_filter_ctx ctx;
 
    /* prevent child processes from getting more priv e.g. via setuid, capabilities, ... */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
        girara_error("prctl SET_NO_NEW_PRIVS");
        return -1;
    }

    /* prevent escape via ptrace */
    if(prctl (PR_SET_DUMPABLE, 0, 0, 0, 0)){
        girara_error("prctl PR_SET_DUMPABLE");
        return -1;
    }

    /* initialize the filter */
    ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (ctx == NULL){
        girara_error("seccomp_init failed");
        return -1;
    }
    
    DENY_RULE (_sysctl);
    DENY_RULE (acct);
    DENY_RULE (add_key);
    DENY_RULE (adjtimex);
    DENY_RULE (chroot);
    DENY_RULE (clock_adjtime);
    DENY_RULE (create_module);
    DENY_RULE (delete_module);
    DENY_RULE (fanotify_init);
    DENY_RULE (finit_module);
    DENY_RULE (get_kernel_syms);
    DENY_RULE (get_mempolicy);
    DENY_RULE (init_module);
    DENY_RULE (io_cancel);
    DENY_RULE (io_destroy);
    DENY_RULE (io_getevents);
    DENY_RULE (io_setup);
    DENY_RULE (io_submit);
    DENY_RULE (ioperm);
    DENY_RULE (iopl);
    DENY_RULE (ioprio_set);
    DENY_RULE (kcmp);
    DENY_RULE (kexec_file_load);
    DENY_RULE (kexec_load);
    DENY_RULE (keyctl);
    DENY_RULE (lookup_dcookie);
    DENY_RULE (mbind);
    DENY_RULE (nfsservctl);
    DENY_RULE (migrate_pages);
    DENY_RULE (modify_ldt);
    DENY_RULE (mount);
    DENY_RULE (move_pages);
    DENY_RULE (name_to_handle_at);
    DENY_RULE (open_by_handle_at);
    DENY_RULE (perf_event_open);
    DENY_RULE (pivot_root);
    DENY_RULE (process_vm_readv);
    DENY_RULE (process_vm_writev);
    DENY_RULE (ptrace);
    DENY_RULE (reboot);
    DENY_RULE (remap_file_pages);
    DENY_RULE (request_key);
    DENY_RULE (set_mempolicy);
    DENY_RULE (swapoff);
    DENY_RULE (swapon);
    DENY_RULE (sysfs);
    DENY_RULE (syslog);
    DENY_RULE (tuxcall);
    DENY_RULE (umount2);
    DENY_RULE (uselib);
    DENY_RULE (vmsplice);
    
    /* TODO: check for additional syscalls to blacklist */
    /* DENY_RULE (execve); */
   
    /* applying filter... */
    if (seccomp_load (ctx) >= 0){
	/* free ctx after the filter has been loaded into the kernel */
	seccomp_release(ctx);
        return 0;
    }
    
  out:
    /* something went wrong */
    seccomp_release(ctx);
    return -1;
}


int seccomp_enable_strict_filter(void){

    scmp_filter_ctx ctx;

    /* prevent child processes from getting more priv e.g. via setuid, capabilities, ... */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
        perror("prctl SET_NO_NEW_PRIVS");
        exit(EXIT_FAILURE);
    }

    /* prevent escape via ptrace */
    if(prctl (PR_SET_DUMPABLE, 0, 0, 0, 0)){
        perror("prctl PR_SET_DUMPABLE");
        exit(EXIT_FAILURE);
    }
    
    /* initialize the filter */
    ctx = seccomp_init(SCMP_ACT_KILL);
    if (ctx == NULL){
	perror("seccomp_init failed");
        exit(EXIT_FAILURE);
    }

    ALLOW_RULE (access);
    /* ALLOW_RULE (arch_prctl); */
    ALLOW_RULE (bind);
    ALLOW_RULE (brk);
    ALLOW_RULE (clock_getres);
    ALLOW_RULE (clone); /* TODO: investigate */
    ALLOW_RULE (close);
    /* ALLOW_RULE (connect); */
    ALLOW_RULE (eventfd2);
    ALLOW_RULE (exit);
    ALLOW_RULE (exit_group);
    ALLOW_RULE (fadvise64);
    ALLOW_RULE (fallocate);
    ALLOW_RULE (fcntl);  /* TODO: build detailed filter */
    ALLOW_RULE (fstat);
    ALLOW_RULE (fstatfs);
    ALLOW_RULE (ftruncate);
    ALLOW_RULE (futex);
    ALLOW_RULE (getdents);
    ALLOW_RULE (getegid);
    ALLOW_RULE (geteuid);
    ALLOW_RULE (getgid);
    ALLOW_RULE (getuid);
    ALLOW_RULE (getpid);
    /* ALLOW_RULE (getpeername); */
    ALLOW_RULE (getresgid);
    ALLOW_RULE (getresuid);
    ALLOW_RULE (getrlimit);
    /* ALLOW_RULE (getsockname); */
    /* ALLOW_RULE (getsockopt);   needed for access to x11 socket in network namespace (without abstract sockets) */
    ALLOW_RULE (inotify_add_watch);
    ALLOW_RULE (inotify_init1);
    ALLOW_RULE (inotify_rm_watch);
    /* ALLOW_RULE (ioctl);  specified below  */
    ALLOW_RULE (lseek);
    ALLOW_RULE (lstat);
    ALLOW_RULE (madvise);
    ALLOW_RULE (memfd_create);
    ALLOW_RULE (mkdir); /* needed for first run only */
    ALLOW_RULE (mmap);
    ALLOW_RULE (mprotect);
    ALLOW_RULE (mremap);
    ALLOW_RULE (munmap);
    //ALLOW_RULE (open);  /* (zathura needs to open for writing) TODO: avoid needing this somehow */
    //ALLOW_RULE (openat);
    ALLOW_RULE (pipe);
    ALLOW_RULE (poll);
    ALLOW_RULE (pwrite64); /* TODO: build detailed filter */
    ALLOW_RULE (pread64);
    /* ALLOW_RULE (prlimit64); */
    /* ALLOW_RULE (prctl);   specified below  */
    ALLOW_RULE (read);
    ALLOW_RULE (readlink);
    ALLOW_RULE (recvfrom);
    ALLOW_RULE (recvmsg);
    ALLOW_RULE (restart_syscall);
    ALLOW_RULE (rt_sigaction);
    ALLOW_RULE (rt_sigprocmask);
    ALLOW_RULE (sendmsg);
    ALLOW_RULE (sendto);
    ALLOW_RULE (select);
    ALLOW_RULE (set_robust_list);
    /* ALLOW_RULE (set_tid_address); */
    /* ALLOW_RULE (setsockopt); */
    ALLOW_RULE (shmat);
    ALLOW_RULE (shmctl);
    ALLOW_RULE (shmdt);
    ALLOW_RULE (shmget);
    ALLOW_RULE (shutdown);
    ALLOW_RULE (stat);
    ALLOW_RULE (statfs);
    /* ALLOW_RULE (socket); */
    ALLOW_RULE (sysinfo);
    ALLOW_RULE (uname);
    ALLOW_RULE (unlink);
    ALLOW_RULE (write);  /* specified below (zathura needs to write files)*/
    ALLOW_RULE (writev); 
    ALLOW_RULE (wait4);  /* trying to open links should not crash the app */

    
    /* Special requirements for ioctl, allowed on stdout/stderr */
    if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 1,
    			  SCMP_CMP(0, SCMP_CMP_EQ, 1)) < 0)
    	goto out;
    if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 1,
    			  SCMP_CMP(0, SCMP_CMP_EQ, 2)) < 0)
    	goto out;

   
    /* needed by gtk??? (does not load content without) */

    /* special restrictions for prctl, only allow PR_SET_NAME/PR_SET_PDEATHSIG */
    if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(prctl), 1,
    			  SCMP_CMP(0, SCMP_CMP_EQ, PR_SET_NAME)) < 0)
    	goto out;

    if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(prctl), 1,
    			  SCMP_CMP(0, SCMP_CMP_EQ, PR_SET_PDEATHSIG)) < 0)
    	goto out;


    /* special restrictions for open, prevent opening files for writing */ 
    if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 1,
                          SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) < 0) 
     	goto out;

    if (seccomp_rule_add (ctx, SCMP_ACT_ERRNO (EACCES), SCMP_SYS(open), 1,
                          SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY)) < 0) 
    	goto out;

    if (seccomp_rule_add (ctx, SCMP_ACT_ERRNO (EACCES), SCMP_SYS(open), 1,
                          SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR)) < 0)
     	goto out;

    /* special restrictions for openat, prevent opening files for writing */ 
    if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 1,
                          SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) < 0) 
     	goto out;

    if (seccomp_rule_add (ctx, SCMP_ACT_ERRNO (EACCES), SCMP_SYS(openat), 1,
                          SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY, O_WRONLY)) < 0) 
    	goto out;

    if (seccomp_rule_add (ctx, SCMP_ACT_ERRNO (EACCES), SCMP_SYS(openat), 1,
                          SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_RDWR, O_RDWR)) < 0)
    	goto out;


    /* allowed for debugging: */

    /* ALLOW_RULE (prctl); */
    /* ALLOW_RULE (ioctl); */


    /* TODO: test fcntl rules */
    /* if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 1, */
    /* 			  SCMP_CMP(0, SCMP_CMP_EQ, F_GETFL)) < 0) */
    /* 	goto out; */
    
    /* if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 1, */
    /* 			  SCMP_CMP(0, SCMP_CMP_EQ, F_SETFL)) < 0) */
    /* 	goto out; */

    /* if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 1, */
    /* 			  SCMP_CMP(0, SCMP_CMP_EQ, F_SETFD)) < 0) */
    /* 	goto out; */

    /* if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 1, */
    /* 			  SCMP_CMP(0, SCMP_CMP_EQ, F_GETFD)) < 0) */
    /* 	goto out; */

    /* if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 1, */
    /* 			  SCMP_CMP(0, SCMP_CMP_EQ, F_SETLK)) < 0) */
    /* 	goto out; */


    /* TODO: build detailed filter for prctl */
    /*  needed by gtk??? (does not load content without) */
       
    /* /\* special restrictions for prctl, only allow PR_SET_NAME/PR_SET_PDEATHSIG *\/ */
    /*     if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(prctl), 1, */
    /* 			  SCMP_CMP(0, SCMP_CMP_EQ, PR_SET_NAME)) < 0) */
    /* 	goto out; */

    /* if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(prctl), 1, */
    /* 			  SCMP_CMP(0, SCMP_CMP_EQ, PR_SET_PDEATHSIG)) < 0) */
    /* 	goto out; */

    
    /* when zathura is run on wayland, with X11 server available but blocked, unset the DISPLAY variable */
    /* otherwise it will try to connect to X11 using inet socket protocol */
    

    /*  ------------ experimental filters --------------- */
    
    /* /\* this filter is susceptible to TOCTOU race conditions, providing limited use *\/ */
    /* /\* allow opening only specified files identified by their file descriptors*\/ */

    /*  this requires either a list of all files to open (A LOT!!!) */
    /*  or needs to be applied only after initialisation, right before parsing */
    /*  if(seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 1, */
    /* 			 SCMP_CMP(SCMP_CMP_EQ, fd)) < 0) /\* or < 1 ??? *\/ */
    /*      goto out; */


    /* /\* restricting write access *\/ */

    /* /\* allow stdin *\/ */
    /*  if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1, */
    /*                          SCMP_CMP(0, SCMP_CMP_EQ, 0)) < 0 ) */
    /*      goto out; */

    /* /\* allow stdout *\/ */
    /*  if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1, */
    /*                              SCMP_CMP(0, SCMP_CMP_EQ, 1)) < 0 ) */
    /*      goto out; */


    /* /\* allow stderr *\/ */
    /* if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1, */
    /*                              SCMP_CMP(0, SCMP_CMP_EQ, 2)) < 0 ) */
    /*     goto out; */
    

    /* /\* restrict writev (write a vector) access *\/ */
    /*  this does not seem reliable but it surprisingly is. investigate more */
    /* if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(writev), 1, */
    /*                             SCMP_CMP(0, SCMP_CMP_EQ, 3)) < 0 ) */
    /*     goto out; */

    /* test if repeating this after some time or denying it works */


    /*  first attempt to filter poll requests */
    /*  if (seccomp_rule_add (ctx, SCMP_ACT_ALLOW, SCMP_SYS(poll), 1, */
    /*  	                      SCMP_CMP(0, SCMP_CMP_MASKED_EQ, POLLIN | POLL, 0)) < 0) */
    /*  	goto out; */


    /* /\* restrict fcntl calls *\/ */
    /*  this syscall sets the file descriptor to read write */
    /* if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 1, */
    /*                             SCMP_CMP(0, SCMP_CMP_EQ, 3)) < 0 ) */
    /*     goto out; */
    /*  fcntl(3, F_GETFL)                       = 0x2 (flags O_RDWR) */
    /*  fcntl(3, F_SETFL, O_RDWR|O_NONBLOCK)    = 0 */
    /*  fcntl(3, F_SETFD, FD_CLOEXEC)           = 0 */


    /*  ------------------ end of experimental filters ------------------ */


    /* applying filter... */
    if (seccomp_load (ctx) >= 0){
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
