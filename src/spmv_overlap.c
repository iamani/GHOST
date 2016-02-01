#include "ghost/config.h"
#include "ghost/types.h"
#include "ghost/locality.h"
#include "ghost/densemat.h"
#include "ghost/util.h"
#include "ghost/instr.h"
#include "ghost/sparsemat.h"
#include "ghost/spmv_solvers.h"

#ifdef GHOST_HAVE_MPI
#include <mpi.h>
#endif
#include <sys/types.h>
#include <string.h>

#ifdef GHOST_HAVE_OPENMP
#include <omp.h>
#endif

ghost_error ghost_spmv_goodfaith(ghost_densemat* res, ghost_sparsemat* mat, ghost_densemat* invec, ghost_spmv_flags flags, va_list argp)
{
#ifndef GHOST_HAVE_MPI
    UNUSED(res);
    UNUSED(mat);
    UNUSED(invec);
    UNUSED(flags);
    UNUSED(argp);
    ERROR_LOG("Cannot execute this spMV solver without MPI");
    return GHOST_ERR_UNKNOWN;
#else
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH);
    ghost_error ret = GHOST_SUCCESS;

    ghost_spmv_flags localopts = (ghost_spmv_flags)(flags|(ghost_spmv_flags)GHOST_SPMV_LOCAL);
    ghost_spmv_flags remoteopts = (ghost_spmv_flags)(flags|(ghost_spmv_flags)GHOST_SPMV_REMOTE);
    ghost_densemat_halo_comm comm = GHOST_DENSEMAT_HALO_COMM_INITIALIZER;

    va_list remote_argp;
    va_copy(remote_argp,argp);

    GHOST_CALL_RETURN(invec->halocommInit(invec,&comm));

    GHOST_INSTR_START("comm+localcomp");
    GHOST_CALL_RETURN(invec->halocommStart(invec,&comm));
    
    GHOST_INSTR_START("local");
    GHOST_CALL_GOTO(mat->localPart->spmv(mat->localPart,res,invec,localopts,argp),err,ret);
    GHOST_INSTR_STOP("local");

    GHOST_CALL_RETURN(invec->halocommFinalize(invec,&comm));
    GHOST_INSTR_STOP("comm+localcomp");
    
    GHOST_INSTR_START("remote");
    GHOST_CALL_GOTO(mat->remotePart->spmv(mat->remotePart,res,invec,remoteopts,remote_argp),err,ret);
    GHOST_INSTR_STOP("remote");

    goto out;
err:

out:
    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH);

    return ret;
#endif
}
