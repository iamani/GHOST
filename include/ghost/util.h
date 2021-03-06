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
#include "func_util.h"
#include "cu_util.h"

#include <stdio.h>
#include <math.h>
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

#define CEILDIV(a,b) ((int)(ceil(((double)(a))/((double)(b)))))

#define GHOST_PAD_MAX 1024

#define IS_ALIGNED(PTR,BYTES) (((uintptr_t)(const void *)(PTR)) % (BYTES) == 0)

#define ISPOWEROFTWO(x) (((x) & ((x)-1)) == 0)

/**
 * @brief Avoid unused variable/function warnings.
 */
#define UNUSED(x) (void)(x)

#define GHOST_SINGLETHREAD(code) {\
    int nthread;\
_Pragma("omp parallel") {\
_Pragma("omp single") {\
            nthread = ghost_omp_nthread(); \
        }\
    }\
    ghost_omp_nthread_set(1);\
    code;\
    ghost_omp_nthread_set(nthread);\
}

#ifdef __cplusplus
extern "C" {
#endif

    ghost_error ghost_header_string(char **str, const char *fmt, ...);
    ghost_error ghost_footer_string(char **str); 
    ghost_error ghost_line_string(char **str, const char *label, const char *unit, const char *format, ...);


    /**
     * @brief Allocate memory.
     *
     * @param mem Where to store the allocated memory.
     * @param size The size (in bytes) to allocate.
     *
     * @return GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error ghost_malloc(void **mem, const size_t size);
    /**
     * @brief Allocate aligned memory.
     *
     * @param mem Where to store the allocated memory.
     * @param size The size (in bytes) to allocate.
     * @param align The alignment size.
     *
     * @return GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error ghost_malloc_align(void **mem, const size_t size, const size_t align);

    /**
     * @brief Allocate pinned memory to enable fast CPU-GPU transfers.
     *
     * @param mem Where to store the allocated memory.
     * @param size The size (in bytes) to allocate.
     *
     * @return GHOST_SUCCESS on success or an error indicator.
     *
     * If CUDA is not enabled, a normal malloc is done.
     */
    ghost_error ghost_malloc_pinned(void **mem, const size_t size);
    
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
    inline int ghost_hash(int a, int b, int c)
    {
        //GHOST_FUNC_ENTER(GHOST_FUNCTYPE_UTIL);
        
        a -= b; a -= c; a ^= (c>>13);
        b -= c; b -= a; b ^= (a<<8);
        c -= a; c -= b; c ^= (b>>13);
        a -= b; a -= c; a ^= (c>>12);
        b -= c; b -= a; b ^= (a<<16);
        c -= a; c -= b; c ^= (b>>5);
        a -= b; a -= c; a ^= (c>>3);
        b -= c; b -= a; b ^= (a<<10);
        c -= a; c -= b; c ^= (b>>15);

        //GHOST_FUNC_EXIT(GHOST_FUNCTYPE_UTIL);
        return c;
    }

#ifdef __cplusplus
}
#endif
#endif
