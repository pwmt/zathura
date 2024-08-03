/* SPDX-License-Identifier: Zlib */

#define _GNU_SOURCE
#include <linux/landlock.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <girara/utils.h>

#include "landlock.h"

#ifndef landlock_create_ruleset
static inline int landlock_create_ruleset(const struct landlock_ruleset_attr* const attr, const size_t size,
                                          const __u32 flags) {
  return syscall(__NR_landlock_create_ruleset, attr, size, flags);
}
#endif

#ifndef landlock_add_rule
static inline int landlock_add_rule(const int ruleset_fd, const enum landlock_rule_type rule_type,
                                    const void* const rule_attr, const __u32 flags) {
  return syscall(__NR_landlock_add_rule, ruleset_fd, rule_type, rule_attr, flags);
}
#endif

#ifndef landlock_restrict_self
static inline int landlock_restrict_self(const int ruleset_fd, const __u32 flags) {
  return syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}
#endif

static void landlock_drop(__u64 fs_access) {
  const struct landlock_ruleset_attr ruleset_attr = {
      .handled_access_fs = fs_access,
  };

  int ruleset_fd = landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
  if (ruleset_fd < 0) {
    return;
  }
  prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
  if (landlock_restrict_self(ruleset_fd, 0)) {
    perror("landlock_restrict_self");
  }
  close(ruleset_fd);
}

#define _LANDLOCK_ACCESS_FS_WRITE                                                                                      \
  (LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |                    \
   LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG |                          \
   LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |                       \
   LANDLOCK_ACCESS_FS_MAKE_SYM)

#define _LANDLOCK_ACCESS_FS_READ (LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR)

void landlock_drop_write(void) {
  landlock_drop(_LANDLOCK_ACCESS_FS_WRITE | LANDLOCK_ACCESS_FS_EXECUTE);
}

void landlock_drop_all(void) {
  landlock_drop(_LANDLOCK_ACCESS_FS_READ | _LANDLOCK_ACCESS_FS_WRITE | LANDLOCK_ACCESS_FS_EXECUTE);
}

void landlock_write_fd(const int dir_fd) {
  const struct landlock_ruleset_attr ruleset_attr = {
      .handled_access_fs = _LANDLOCK_ACCESS_FS_WRITE | LANDLOCK_ACCESS_FS_EXECUTE,
  };
  struct landlock_path_beneath_attr path_beneath = {
      .allowed_access = _LANDLOCK_ACCESS_FS_WRITE,
  };

  int ruleset_fd = landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
  if (ruleset_fd < 0) {
    return;
  }
  if (dir_fd == -1) {
    path_beneath.parent_fd = open(".", O_PATH | O_CLOEXEC | O_DIRECTORY);
  } else {
    path_beneath.parent_fd = dir_fd;
  }

  if (!landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &path_beneath, 0)) {
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    if (landlock_restrict_self(ruleset_fd, 0)) {
      perror("landlock_restrict_self");
    }
  } else {
    perror("landlock_add_rule");
  }

  if (dir_fd == -1) {
    close(path_beneath.parent_fd);
  }
  close(ruleset_fd);
}

void landlock_restrict_write(void) {
  char* data_path = girara_get_xdg_path(XDG_DATA);
  int fd          = open(data_path, O_PATH | O_CLOEXEC | O_DIRECTORY);
  g_free(data_path);
  landlock_write_fd(fd);
}
