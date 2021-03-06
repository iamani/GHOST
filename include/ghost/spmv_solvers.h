/**
 * @file spmv_solvers.h
 * @brief SpMV solver functions.
 * @author Moritz Kreutzer <moritz.kreutzer@fau.de>
 */
#ifndef GHOST_SPMV_SOLVERS_H
#define GHOST_SPMV_SOLVERS_H

#ifdef __cpluscplus
extern "C" {
#endif

    ghost_error ghost_spmv_vectormode(ghost_densemat* res, ghost_sparsemat* mat, ghost_densemat* invec, ghost_spmv_opts traits);
    ghost_error ghost_spmv_goodfaith(ghost_densemat* res, ghost_sparsemat* mat, ghost_densemat* invec, ghost_spmv_opts traits);
    ghost_error ghost_spmv_taskmode(ghost_densemat* res, ghost_sparsemat* mat, ghost_densemat* invec, ghost_spmv_opts traits);
    ghost_error ghost_spmv_pipelined(ghost_densemat* res, ghost_sparsemat* mat, ghost_densemat* invec, ghost_spmv_opts traits);

#ifdef __cpluscplus
}
#endif
#endif
