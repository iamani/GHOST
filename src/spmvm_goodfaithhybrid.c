#include "ghost/config.h"
#include "ghost/types.h"
#include "ghost/locality.h"
#include "ghost/vec.h"
#include "ghost/util.h"
#include "ghost/constants.h"
#include "ghost/instr.h"
#include "ghost/mat.h"

#include <mpi.h>
#include <sys/types.h>
#include <string.h>

#if GHOST_HAVE_OPENMP
#include <omp.h>
#endif

ghost_error_t ghost_spmv_goodfaith(ghost_context_t *context, ghost_vec_t* res, ghost_mat_t* mat, ghost_vec_t* invec, int spmvmOptions)
{
    ghost_mnnz_t max_dues;
    char *work = NULL;
    ghost_error_t ret = GHOST_SUCCESS;
    int nprocs;

    int localopts = spmvmOptions;
    localopts &= ~GHOST_SPMVM_COMPUTE_LOCAL_DOTPRODUCT;

    int remoteopts = spmvmOptions;
    remoteopts &= ~GHOST_SPMVM_AXPBY;
    remoteopts &= ~GHOST_SPMVM_APPLY_SHIFT;
    remoteopts |= GHOST_SPMVM_AXPY;

    int me; 
    int i, from_PE, to_PE;
    int msgcount;
    ghost_vidx_t c;


    GHOST_CALL_RETURN(ghost_getRank(context->mpicomm,&me));
    GHOST_CALL_RETURN(ghost_getNumberOfRanks(context->mpicomm,&nprocs));
    MPI_Request request[invec->traits->nvecs*2*nprocs];
    MPI_Status  status[invec->traits->nvecs*2*nprocs];

    max_dues = 0;
    for (i=0;i<nprocs;i++)
        if (context->dues[i]>max_dues) 
            max_dues = context->dues[i];

    GHOST_CALL_RETURN(ghost_malloc((void **)&work,invec->traits->nvecs*max_dues*nprocs * invec->traits->elSize));

#ifdef __INTEL_COMPILER
 //   kmp_set_blocktime(1);
#endif

    invec->downloadNonHalo(invec);

    msgcount = 0;
    for (i=0;i<invec->traits->nvecs*2*nprocs;i++) {
        request[i] = MPI_REQUEST_NULL;
    }

    for (from_PE=0; from_PE<nprocs; from_PE++){
        if (context->wishes[from_PE]>0){
            for (c=0; c<invec->traits->nvecs; c++) {
                MPI_CALL_GOTO(MPI_Irecv(VECVAL(invec,invec->val,c,context->hput_pos[from_PE]), context->wishes[from_PE]*invec->traits->elSize,MPI_CHAR, from_PE, from_PE, context->mpicomm,&request[msgcount]),err,ret);
                msgcount++;
            }
        }
    }

    if (mat->traits->flags & GHOST_SPM_SORTED) {
#pragma omp parallel private(to_PE,i,c)
        for (to_PE=0 ; to_PE<nprocs ; to_PE++){
            for (c=0; c<invec->traits->nvecs; c++) {
#pragma omp for 
                for (i=0; i<context->dues[to_PE]; i++){
                    memcpy(work + c*nprocs*max_dues*invec->traits->elSize + (to_PE*max_dues+i)*invec->traits->elSize,VECVAL(invec,invec->val,c,invec->context->rowPerm[context->duelist[to_PE][i]]),invec->traits->elSize);
                }
            }
        }
    } else {
#pragma omp parallel private(to_PE,i,c)
        for (to_PE=0 ; to_PE<nprocs ; to_PE++){
            for (c=0; c<invec->traits->nvecs; c++) {
#pragma omp for 
                for (i=0; i<context->dues[to_PE]; i++){
                    memcpy(work + c*nprocs*max_dues*invec->traits->elSize + (to_PE*max_dues+i)*invec->traits->elSize,VECVAL(invec,invec->val,c,context->duelist[to_PE][i]),invec->traits->elSize);
                }
            }
        }
    }
    
    for (to_PE=0 ; to_PE<nprocs ; to_PE++){
        if (context->dues[to_PE]>0){
            for (c=0; c<invec->traits->nvecs; c++) {
                MPI_CALL_GOTO(MPI_Isend( work + c*nprocs*max_dues*invec->traits->elSize + to_PE*max_dues*invec->traits->elSize, context->dues[to_PE]*invec->traits->elSize, MPI_CHAR, to_PE, me, context->mpicomm, &request[msgcount]),err,ret);
                msgcount++;
            }
        }
    }

    GHOST_INSTR_START(spmvm_gf_local);
    GHOST_CALL_GOTO(mat->localPart->spmv(mat->localPart,res,invec,localopts),err,ret);
    GHOST_INSTR_STOP(spmvm_gf_local);

    GHOST_INSTR_START(spmvm_gf_waitall);
    MPI_CALL_GOTO(MPI_Waitall(msgcount, request, status),err,ret);
    GHOST_INSTR_STOP(spmvm_gf_waitall);

    GHOST_CALL_GOTO(invec->uploadHalo(invec),err,ret);

    GHOST_INSTR_START(spmvm_gf_remote);
    GHOST_CALL_GOTO(mat->remotePart->spmv(mat->remotePart,res,invec,remoteopts),err,ret);
    GHOST_INSTR_STOP(spmvm_gf_remote);

    goto out;
err:

out:
    free(work);

    return ret;
}
