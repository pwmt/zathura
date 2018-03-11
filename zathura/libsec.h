#ifndef SECCOMP_H
#define SECCOMP_H

/* basic filter */
/* this mode allows normal use */
/* only dangerous syscalls are blacklisted */
int seccomp_enable_basic_filter(void);

/* strict filter before document parsing */
/* this filter is to be enabled after most of the initialisation of zathura has finished */
int seccomp_enable_strict_filter(void);

#endif
