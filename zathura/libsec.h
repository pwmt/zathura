#ifndef SECCOMP_H
#define SECCOMP_H

/* basic filter */
/* this mode allows normal use */
/* only dangerous syscalls are blacklisted */
int seccomp_enable_protected_mode(void);

/* secure whitelist filter */
/* whitelist minimal syscalls only */
/* this mode does not allow to open external links or to start applications */
/* network connections are prohibited as well */
int seccomp_enable_protected_view(void);

/* strict filter before document parsing */
/* this filter is to be enabled after most of the initialisation of zathura has finished */
int seccomp_enable_strict_filter(void);

#endif
