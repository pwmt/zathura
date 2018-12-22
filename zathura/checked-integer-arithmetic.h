/* SPDX-License-Identifier: Zlib */
#ifndef ZATHURA_CHECKED_INTEGER_ARITHMETIC_H
#define ZATHURA_CHECKED_INTEGER_ARITHMETIC_H

#include "macros.h"
#include <stdbool.h>

#if __GNUC__ >= 5 || __has_builtin(__builtin_add_overflow)
#define HAVE_BUILTIN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_BUILTIN
#define checked_umul(lhs, rhs, res) __builtin_umul_overflow((lhs), (rhs), (res))
#else
/**
 * Helper function for multiplication with overflow detection. This function has
 * the same semantics as the __builtin_*mul_overflow functions.
 *
 * @param[in] lhs first operand
 * @param[in] rhs second operand
 * @param[out] res result
 * @return true if an overflow occurred, false otherwise
 */
GIRARA_HIDDEN bool checked_umul(unsigned int lhs, unsigned int rhs, unsigned int* res);
#endif

#ifdef __cplusplus
}
#endif

#endif
