/* SPDX-License-Identifier: Zlib */

#include "checked-integer-arithmetic.h"
#include <stdint.h>
#include <limits.h>

#ifndef HAVE_BUILTIN
bool checked_umul(unsigned int lhs, unsigned int rhs, unsigned int* res) {
  const uint64_t r = (uint64_t)lhs * (uint64_t)rhs;
  *res             = (unsigned int)r;

  return r > UINT_MAX;
}
#endif
