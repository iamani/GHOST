/**
 * @file util.h
 * @brief General utility functions.
 * @author Moritz Kreutzer <moritz.kreutzer@fau.de>
 */
#ifndef GHOST_UTIL_H
#define GHOST_UTIL_H

#include "config.h"
#include "types.h"
#include "instr.h"

#ifdef GHOST_HAVE_CUDA
#include "cu_util.h"
#endif

#include <stdio.h>
/*
#ifndef __cplusplus
#include <complex.h>
#include <string.h>
#include <float.h>
#else
#include <complex>
#include <cstring>
#include <cfloat>
#endif
*/

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif
#ifndef MAX
#define MAX(x,y) ((x)<(y)?(y):(x))
#endif

/**
 * @brief Pad a number to a multiple of a second number.
 *
 * @param n The number to be padded.
 * @param p The desired padding.
 *
 * @return n padded to a multiple of p or n if p or n smaller than 1.
 */
#define PAD(n,p) (((n)<1 || (p)<1)?(n):(((n) % (p)) ? ((n) + (p) - (n) % (p)) : (n)))

#define GHOST_PAD_MAX 1024

/**
 * @brief Avoid unused variable/function warnings.
 */
#define UNUSED(x) (void)(x)

typedef enum
{
    GHOST_FUNCTYPE_MATH = 1,
    GHOST_FUNCTYPE_UTIL = 2,
    GHOST_FUNCTYPE_COMMUNICATION = 4,
    GHOST_FUNCTYPE_PREPROCESS = 8,
    GHOST_FUNCTYPE_INITIALIZATION = 16
} ghost_functype_t;

#define GHOST_FUNC_ENTRY(functype)\
    int __funcnameoffset = strncmp(__FUNCTION__,"ghost_",6)?0:6;\
    if (functype == GHOST_FUNCTYPE_MATH) {\
        GHOST_INSTR_START(__FUNCTION__+__funcnameoffset);\
    }\
    char * __prefixbackup = ghost_instr_prefix_get();\
    char __prefix[256];\
    snprintf(__prefix,strlen(__prefixbackup)+strlen(__FUNCTION__+__funcnameoffset)+3,"%s%s->",__prefixbackup,__FUNCTION__+__funcnameoffset);\
    ghost_instr_prefix_set(__prefix);\

#define GHOST_FUNC_EXIT(functype)\
    ghost_instr_prefix_set(__prefixbackup);\
    if (functype == GHOST_FUNCTYPE_MATH) {\
        GHOST_INSTR_STOP(__FUNCTION__+__funcnameoffset);\
    }


#ifdef __cplusplus
extern "C" {
#endif

    ghost_error_t ghost_header_string(char **str, const char *fmt, ...);
    ghost_error_t ghost_footer_string(char **str); 
    ghost_error_t ghost_line_string(char **str, const char *label, const char *unit, const char *format, ...);


    /**
     * @brief Allocate memory.
     *
     * @param mem Where to store the allocated memory.
     * @param size The size (in bytes) to allocate.
     *
     * @return GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error_t ghost_malloc(void **mem, const size_t size);
    /**
     * @brief Allocate aligned memory.
     *
     * @param mem Where to store the allocated memory.
     * @param size The size (in bytes) to allocate.
     * @param align The alignment size.
     *
     * @return GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error_t ghost_malloc_align(void **mem, const size_t size, const size_t align);

    /**
     * @brief Computes a hash from three integral input values.
     *
     * @param a First parameter.
     * @param b Second parameter.
     * @param c Third parameter.
     *
     * @return Hash value.
     *
     * The function has been taken from http://burtleburtle.net/bob/hash/doobs.html
     */
    int ghost_hash(int a, int b, int c);

#ifdef __cplusplus
}
#endif
#endif
