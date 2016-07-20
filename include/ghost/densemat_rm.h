/**
 * @file densemat_rm.h
 * @brief Types and functions related to row major dense matrices/vectors.
 * @author Moritz Kreutzer <moritz.kreutzer@fau.de>
 */
#ifndef GHOST_DENSEMAT_RM_H
#define GHOST_DENSEMAT_RM_H

#include "config.h"
#include "types.h"
#include "context.h"
#include "densemat.h"
#ifdef GHOST_HAVE_CUDA
#include "cu_densemat_rm.h"
#endif

#ifdef __cplusplus

extern "C" {
#endif

    /**
     * @brief Set the function pointers of a row-major densemat.
     *
     * @param[inout] vec The dense matrix.
     *
     * @return ::GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error ghost_densemat_rm_setfuncs(ghost_densemat *vec);
    ghost_error ghost_densemat_rm_malloc(ghost_densemat *vec, int* needInit);
    ghost_error ghost_densemat_rm_string_selector(ghost_densemat *vec, char **str);
    ghost_error ghost_densemat_rm_normalize_selector(ghost_densemat *vec);
    ghost_error ghost_densemat_rm_dotprod_selector(ghost_densemat *vec, void *, ghost_densemat *);
    ghost_error ghost_densemat_rm_vscale_selector(ghost_densemat *vec1, void *); 
    ghost_error ghost_densemat_rm_vaxpy_selector(ghost_densemat *vec1, ghost_densemat *vec2, void *); 
    ghost_error ghost_densemat_rm_vaxpby_selector(ghost_densemat *vec1, ghost_densemat *vec2, void *, void *); 
    ghost_error ghost_densemat_rm_fromRand_selector(ghost_densemat *vec);
    ghost_error ghost_densemat_rm_fromScalar_selector(ghost_densemat *vec, void *);
    ghost_error ghost_densemat_rm_fromVec_selector(ghost_densemat *vec1, ghost_densemat *vec2, ghost_lidx, ghost_lidx); 
    ghost_error ghost_densemat_rm_fromReal_selector(ghost_densemat *vec, ghost_densemat *re, ghost_densemat *im); 
    ghost_error ghost_densemat_rm_fromComplex_selector(ghost_densemat *re, ghost_densemat *im, ghost_densemat *c); 
    ghost_error ghost_densemat_rm_permute_selector(ghost_densemat *vec, ghost_permutation_direction dir);
    ghost_error ghost_densemat_rm_norm_selector(ghost_densemat *vec, void *res, void *p);
    ghost_error ghost_densemat_rm_averagehalo_selector(ghost_densemat *vec);
    ghost_error ghost_densemat_rm_conj_selector(ghost_densemat *vec);
    ghost_error ghost_densemat_rm_vaxpbypcz_selector(ghost_densemat *vec, ghost_densemat *vec2, void *scale, void *b, ghost_densemat *vec3, void *c);
#ifdef __cplusplus
}
#endif

#endif
