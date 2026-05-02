/* SPDX-License-Identifier: Zlib */

#define _GNU_SOURCE
#include "landlock.h"

#include <linux/landlock.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <girara/log.h>
#include <girara/utils.h>

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

static int landlock_drop(__u64 fs_access) {
  const struct landlock_ruleset_attr ruleset_attr = {
      .handled_access_fs = fs_access,
  };

  int ruleset_fd = landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
  if (ruleset_fd < 0) {
    girara_error("landlock_create_ruleset failed: %s", g_strerror(errno));
    return -1;
  }
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
    girara_error("prctl(PR_SET_NO_NEW_PRIVS) failed: %s", g_strerror(errno));
    close(ruleset_fd);
    return -1;
  }
  if (landlock_restrict_self(ruleset_fd, 0)) {
    girara_error("landlock_restrict_self failed: %s", g_strerror(errno));
    close(ruleset_fd);
    return -1;
  }
  close(ruleset_fd);
  return 0;
}

#define _LANDLOCK_ACCESS_FS_WRITE                                                                                      \
  (LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |                    \
   LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG |                          \
   LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |                       \
   LANDLOCK_ACCESS_FS_MAKE_SYM)

#define _LANDLOCK_ACCESS_FS_READ (LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR)

/* returns 1 if landlock is supported, 0 if not, -1 on unexpected probe error */
static int landlock_check_kernel(void) {
  int abi = landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
  if (abi >= 1) {
    return 1;
  }
  if (abi == -1 && (errno == ENOSYS || errno == EOPNOTSUPP)) {
    /*
     * Kernel too old, not compiled with Landlock,
     * or Landlock was not enabled at boot time.
     */
    girara_warning("Unable to use Landlock: Kernel too old, not compiled with Landlock,\
            or Landlock was not enabled at boot time. Sandbox partly disabled.");
    return 0; /* Graceful fallback: Do nothing. */
  }
  girara_warning("Landlock probe failed: %s", g_strerror(errno));
  return -1;
}

int landlock_drop_write(void) {
  const int kernel = landlock_check_kernel();
  if (kernel == 0) {
    return 1; /* unsupported, graceful fallback */
  }
  if (kernel < 0) {
    return -1;
  }
  return landlock_drop(_LANDLOCK_ACCESS_FS_WRITE | LANDLOCK_ACCESS_FS_EXECUTE);
}

#if 0
static int landlock_drop_all(void) {
  return landlock_drop(_LANDLOCK_ACCESS_FS_READ | _LANDLOCK_ACCESS_FS_WRITE | LANDLOCK_ACCESS_FS_EXECUTE);
}
#endif

static int landlock_write_fd(const int dir_fd) {
  const struct landlock_ruleset_attr ruleset_attr = {
      .handled_access_fs = _LANDLOCK_ACCESS_FS_WRITE | LANDLOCK_ACCESS_FS_EXECUTE,
  };
  struct landlock_path_beneath_attr path_beneath = {
      .allowed_access = _LANDLOCK_ACCESS_FS_WRITE,
      .parent_fd      = -1,
  };
  int rc              = -1;
  int parent_fd_owned = 0;

  int ruleset_fd = landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
  if (ruleset_fd < 0) {
    girara_error("landlock_create_ruleset failed: %s", g_strerror(errno));
    return -1;
  }
  if (dir_fd == -1) {
    path_beneath.parent_fd = open(".", O_PATH | O_CLOEXEC | O_DIRECTORY);
    if (path_beneath.parent_fd < 0) {
      girara_error("open(\".\") for landlock path failed: %s", g_strerror(errno));
      goto out;
    }
    parent_fd_owned = 1;
  } else {
    path_beneath.parent_fd = dir_fd;
  }

  if (landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &path_beneath, 0) != 0) {
    girara_error("landlock_add_rule failed: %s", g_strerror(errno));
    goto out;
  }
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
    girara_error("prctl(PR_SET_NO_NEW_PRIVS) failed: %s", g_strerror(errno));
    goto out;
  }
  if (landlock_restrict_self(ruleset_fd, 0) != 0) {
    girara_error("landlock_restrict_self failed: %s", g_strerror(errno));
    goto out;
  }
  rc = 0;

out:
  if (parent_fd_owned && path_beneath.parent_fd >= 0) {
    close(path_beneath.parent_fd);
  }
  close(ruleset_fd);
  return rc;
}

int landlock_restrict_write(void) {
  const int kernel = landlock_check_kernel();
  if (kernel == 0) {
    return 1;
  }
  if (kernel < 0) {
    return -1;
  }
  g_autofree char* data_path = girara_get_xdg_path(XDG_DATA);
  if (data_path == NULL) {
    girara_error("girara_get_xdg_path(XDG_DATA) returned NULL");
    return -1;
  }
  int fd = open(data_path, O_PATH | O_CLOEXEC | O_DIRECTORY);
  if (fd < 0) {
    girara_error("open(\"%s\") for landlock failed: %s", data_path, g_strerror(errno));
    return -1;
  }
  const int rc = landlock_write_fd(fd);
  close(fd);
  return rc;
}
