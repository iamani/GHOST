#include "ghost/config.h"
#include "ghost/types.h"
#include "ghost/util.h"
#include "ghost/math.h"
#include "ghost/omp.h"
#include "ghost/sell_kacz_rb.h"
#include <omp.h>

ghost_error ghost_kacz_rb_v1(ghost_densemat *x, ghost_sparsemat *mat, ghost_densemat *b, ghost_kacz_opts opts)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);

    //currently only implementation for SELL-1-1
    //const int CHUNKHEIGHT = 1;  
    const int NVECS = 1;

    //TODO check for RCM and give a Warning
    if (mat->context->nzones == 0 || mat->context->zone_ptr == NULL){
        ERROR_LOG("Splitting of matrix to Red and Black ( odd and even) have not be done!");
    }

    if (NVECS > 1) {
        ERROR_LOG("Multi-vec not implemented!");
        return GHOST_ERR_NOT_IMPLEMENTED;
    }

    double *bval = (double *)(b->val);
    double *xval = (double *)(x->val);
    double *mval = (double *)mat->val;
    //double omega = *(double *)opts.omega;
    ghost_lidx *zone_ptr = mat->context->zone_ptr;
    ghost_lidx nzones    = mat->context->nzones;
    ghost_lidx nthreads  = nzones/2;

    // disables dynamic thread adjustments 
    ghost_omp_set_dynamic(0);
    ghost_omp_nthread_set(nthreads);
    //printf("Setting number of threads to %d for KACZ sweep\n",nthreads);


#ifdef GHOST_HAVE_OPENMP  
#pragma omp parallel
    {
#endif
        ghost_lidx tid = ghost_omp_threadnum();
        ghost_lidx even_start = zone_ptr[2*tid];
        ghost_lidx even_end   = zone_ptr[2*tid+1];
        ghost_lidx odd_start  = zone_ptr[2*tid+1];
        ghost_lidx odd_end    = zone_ptr[2*tid+2];
        ghost_lidx stride     = 0;

        if (opts.direction == GHOST_KACZ_DIRECTION_BACKWARD) {
            even_start   = zone_ptr[2*tid+2]-1; 
            even_end     = zone_ptr[2*tid+1]-1;
            odd_start    = zone_ptr[2*tid+1]-1;
            odd_end      = zone_ptr[2*tid]-1;
            stride       = -1;
        } else {
            even_start = zone_ptr[2*tid];
            even_end   = zone_ptr[2*tid+1];
            odd_start  = zone_ptr[2*tid+1];
            odd_end    = zone_ptr[2*tid+2];
            stride     = 1;
        }

        //only for SELL-1-1 currently
        //even sweep

        double rownorm = 0.;

        for (ghost_lidx row=even_start; row!=even_end; row+=stride){
            //printf("projecting to row ........ %d\n",row); 
            rownorm = 0.;
            ghost_lidx  idx = mat->chunkStart[row];

            double scal  = -bval[row]; 
            for (ghost_lidx j=0; j<mat->rowLen[row]; ++j) {
                scal += (double)mval[idx] * xval[mat->col[idx]];
                if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                    rownorm += mval[idx]*mval[idx]; 
                }
                idx += 1;
            }

            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) { 
                scal /= (double)rownorm;
            }
            //scal *= omega;

            idx -= mat->rowLen[row];

            for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
                xval[mat->col[idx]] = xval[mat->col[idx]] - scal * (double)mval[idx];
                idx += 1;
            }

        }


#ifdef GHOST_HAVE_OPENMP     
#pragma omp barrier //does not affect much
#endif


        //odd sweep
        for (ghost_lidx row=odd_start; row!=odd_end; row+=stride){
            //printf("projecting to row ........ %d\n",row);
            rownorm = 0.; 
            ghost_lidx idx = mat->chunkStart[row];

            double scal = -bval[row];
            for (ghost_lidx j=0; j<mat->rowLen[row]; ++j) {
                scal += (double)mval[idx] * xval[mat->col[idx]];
                if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                    rownorm += mval[idx]*mval[idx];
                }
                idx += 1;
            }

            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) 
                scal /= (double)rownorm;
            //scal *= omega;

            idx -= mat->rowLen[row];

            for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
                xval[mat->col[idx]] = xval[mat->col[idx]]  - scal * (double)mval[idx];
                idx += 1;
            }      

        }


#ifdef GHOST_HAVE_OPENMP
    }
#endif

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);
    return GHOST_SUCCESS;
}

ghost_error ghost_kacz_rb_v2(ghost_densemat *x, ghost_sparsemat *mat, ghost_densemat *b, ghost_kacz_opts opts)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);

    //currently only implementation for SELL-1-1
    //const int CHUNKHEIGHT = 1;  
    const int NVECS = 1;

    //TODO check for RCM and give a Warning
    if (mat->context->nzones == 0 || mat->context->zone_ptr == NULL){
        ERROR_LOG("Splitting of matrix to Red and Black ( odd and even) have not be done!");
    }

    if (NVECS > 1) {
        ERROR_LOG("Multi-vec not implemented!");
        return GHOST_ERR_NOT_IMPLEMENTED;
    }

    double *bval = (double *)(b->val);
    double *xval = (double *)(x->val);
    double *mval = (double *)mat->val;
    //double omega = *(double *)opts.omega;
    ghost_lidx *zone_ptr = mat->context->zone_ptr;
    ghost_lidx nzones    = mat->context->nzones;
    ghost_lidx nthreads  = nzones/2;

    // disables dynamic thread adjustments 
    ghost_omp_set_dynamic(0);
    ghost_omp_nthread_set(nthreads);
    //printf("Setting number of threads to %d for KACZ sweep\n",nthreads);

#ifdef GHOST_HAVE_OPENMP  
#pragma omp parallel
    {
#endif
        ghost_lidx tid = ghost_omp_threadnum();
        ghost_lidx even_start = zone_ptr[2*tid];
        ghost_lidx even_end   = zone_ptr[2*tid+1];
        ghost_lidx odd_start  = zone_ptr[2*tid+1];
        ghost_lidx odd_end    = zone_ptr[2*tid+2];
        ghost_lidx stride     = 0;

        if (opts.direction == GHOST_KACZ_DIRECTION_BACKWARD) {
            even_start   = zone_ptr[2*tid+2]-1; 
            even_end     = zone_ptr[2*tid+1]-1;
            odd_start    = zone_ptr[2*tid+1]-1;
            odd_end      = zone_ptr[2*tid]-1;
            stride       = -1;
        } else {
            even_start = zone_ptr[2*tid];
            even_end   = zone_ptr[2*tid+1];
            odd_start  = zone_ptr[2*tid+1];
            odd_end    = zone_ptr[2*tid+2];
            stride     = 1;
        }

        //only for SELL-1-1 currently
        //even sweep
        double rownorm = 0.;

        for (ghost_lidx row=even_start; row!=even_end; row+=stride){
            //printf("projecting to row ........ %d\n",row); 
            rownorm = 0.;
            ghost_lidx  idx = mat->chunkStart[row];

            double scal  = -bval[row]; 
            for (ghost_lidx j=0; j<mat->rowLen[row]; ++j) {
                scal += (double)mval[idx] * xval[mat->col[idx]];
                if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                    rownorm += mval[idx]*mval[idx]; 
                }
                idx += 1;
            }

            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                scal /= (double)rownorm;
            }
            //scal *= omega;

            idx -= mat->rowLen[row];

#pragma simd vectorlength(4)
            for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
                xval[mat->col[idx]] = xval[mat->col[idx]] - scal * (double)mval[idx];
                idx += 1;
            }

        }



#ifdef GHOST_HAVE_OPENMP     
#pragma omp barrier //does not affect much
#endif


        //odd sweep
        for (ghost_lidx row=odd_start; row!=odd_end; row+=stride){
            //printf("projecting to row ........ %d\n",row);
            rownorm = 0.; 
            ghost_lidx idx = mat->chunkStart[row];

            double scal = -bval[row];
            for (ghost_lidx j=0; j<mat->rowLen[row]; ++j) {
                scal += (double)mval[idx] * xval[mat->col[idx]];
                if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                    rownorm += mval[idx]*mval[idx];
                }
                idx += 1;
            }

            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                scal /= (double)rownorm;
            }
            //scal *= omega;

            idx -= mat->rowLen[row];

#pragma simd vectorlength(4)
            for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
                xval[mat->col[idx]] = xval[mat->col[idx]]  - scal * (double)mval[idx];
                idx += 1;
            }      

        }


#ifdef GHOST_HAVE_OPENMP
    }
#endif

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);
    return GHOST_SUCCESS;
}

ghost_error ghost_kacz_rb_v3(ghost_densemat *x, ghost_sparsemat *mat, ghost_densemat *b, ghost_kacz_opts opts)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);

    //currently only implementation for SELL-1-1
    //const int CHUNKHEIGHT = 1;  
    const int NVECS = 1;

    //TODO check for RCM and give a Warning
    if (mat->context->nzones == 0 || mat->context->zone_ptr == NULL){
        ERROR_LOG("Splitting of matrix to Red and Black ( odd and even) have not be done!");
    }

    if (NVECS > 1) {
        ERROR_LOG("Multi-vec not implemented!");
        return GHOST_ERR_NOT_IMPLEMENTED;
    }

    double *bval = (double *)(b->val);
    double *xval = (double *)(x->val);
    double *mval = (double *)mat->val;
    //double omega = *(double *)opts.omega;
    ghost_lidx *zone_ptr = mat->context->zone_ptr;
    ghost_lidx nzones    = mat->context->nzones;
    ghost_lidx nthreads  = nzones/2;

    // disables dynamic thread adjustments 
    ghost_omp_set_dynamic(0);
    ghost_omp_nthread_set(nthreads);
    //printf("Setting number of threads to %d for KACZ sweep\n",nthreads);


#ifdef GHOST_HAVE_OPENMP  
#pragma omp parallel
    {
#endif
        ghost_lidx tid = ghost_omp_threadnum();
        ghost_lidx even_start = zone_ptr[2*tid];
        ghost_lidx even_end   = zone_ptr[2*tid+1];
        ghost_lidx odd_start  = zone_ptr[2*tid+1];
        ghost_lidx odd_end    = zone_ptr[2*tid+2];
        ghost_lidx stride     = 0;

        if (opts.direction == GHOST_KACZ_DIRECTION_BACKWARD) {
            even_start   = zone_ptr[2*tid+2]-1; 
            even_end     = zone_ptr[2*tid+1]-1;
            odd_start    = zone_ptr[2*tid+1]-1;
            odd_end      = zone_ptr[2*tid]-1;
            stride       = -1;
        } else {
            even_start = zone_ptr[2*tid];
            even_end   = zone_ptr[2*tid+1];
            odd_start  = zone_ptr[2*tid+1];
            odd_end    = zone_ptr[2*tid+2];
            stride     = 1;
        }

        //only for SELL-1-1 currently
        //even sweep
        double rownorm = 0.;
        ghost_lidx row = even_start;
        ghost_lidx  idx = mat->chunkStart[row];

        double scal    = -bval[row];
        for (ghost_lidx j=0; j<mat->rowLen[row]; ++j) {
            scal += (double)mval[idx] * xval[mat->col[idx]];
            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                rownorm += mval[idx]*mval[idx];
            }
            idx += 1;
        }

        for (row=even_start; row!=even_end-1; row+=stride){
            //printf("projecting to row ........ %d\n",row); 
            rownorm = 0.;
            idx = mat->chunkStart[row];

#pragma simd vectorlength(4)
            for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
                xval[mat->col[idx]] = xval[mat->col[idx]] - scal * (double)mval[idx];
                idx += 1;
            }

            idx = mat->chunkStart[row+stride];

            scal  = -bval[row+stride]; 
            for (ghost_lidx j=0; j<mat->rowLen[row+stride]; ++j) {
                scal += (double)mval[idx] * xval[mat->col[idx]];
                if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                    rownorm += mval[idx]*mval[idx]; 
                }
                idx += 1;
            }

            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) 
                scal /= (double)rownorm;
            //scal *= omega;

        }

        row = even_end-1;
        idx = mat->chunkStart[row];

#pragma simd vectorlength(4)
        for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
            xval[mat->col[idx]] = xval[mat->col[idx]] - scal * (double)mval[idx];
            idx += 1;
        }


#ifdef GHOST_HAVE_OPENMP     
#pragma omp barrier //does not affect much
#endif

        //odd sweep
        rownorm = 0.;
        row =odd_start;
        idx = mat->chunkStart[row];

        scal    = -bval[row];
        for (ghost_lidx j=0; j<mat->rowLen[row]; ++j) {
            scal += (double)mval[idx] * xval[mat->col[idx]];
            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                rownorm += mval[idx]*mval[idx];
            }
            idx += 1;
        }

        for (row=odd_start; row!=odd_end-1; row+=stride){
            //printf("projecting to row ........ %d\n",row);
            rownorm = 0.; 
            idx = mat->chunkStart[row];

#pragma simd vectorlength(4)
            for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
                xval[mat->col[idx]] = xval[mat->col[idx]]  - scal * (double)mval[idx];
                idx += 1;
            }

            idx = mat->chunkStart[row+stride];

            scal = -bval[row+stride];
            for (ghost_lidx j=0; j<mat->rowLen[row+stride]; ++j) {
                scal += (double)mval[idx] * xval[mat->col[idx]];
                if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                    rownorm += mval[idx]*mval[idx];
                }
                idx += 1;
            }

            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) 
                scal /= (double)rownorm;
            //scal *= omega;	

        }

        row = odd_end-1;
        idx = mat->chunkStart[row];

#pragma simd vectorlength(4)
        for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
            xval[mat->col[idx]] = xval[mat->col[idx]] - scal * (double)mval[idx];
            idx += 1;
        }


#ifdef GHOST_HAVE_OPENMP
    }
#endif

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);
    return GHOST_SUCCESS;
}

//might not work for small matrices, see version 5 
ghost_error ghost_kacz_rb_v4(ghost_densemat *x, ghost_sparsemat *mat, ghost_densemat *b, ghost_kacz_opts opts)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);

    //currently only implementation for SELL-1-1
    //const int CHUNKHEIGHT = 1;  
    const int NVECS = 1;

    //TODO check for RCM and give a Warning
    if (mat->context->nzones == 0 || mat->context->zone_ptr == NULL){
        ERROR_LOG("Splitting of matrix to Red and Black ( odd and even) have not be done!");
    }

    if (NVECS > 1) {
        ERROR_LOG("Multi-vec not implemented!");
        return GHOST_ERR_NOT_IMPLEMENTED;
    }

    double *bval = (double *)(b->val);
    double *xval = (double *)(x->val);
    double *mval = (double *)mat->val;
    //double omega = *(double *)opts.omega;
    ghost_lidx *zone_ptr = mat->context->zone_ptr;
    ghost_lidx nzones    = mat->context->nzones;
    ghost_lidx nthreads  = nzones/2;

    // disables dynamic thread adjustments 
    ghost_omp_set_dynamic(0);
    ghost_omp_nthread_set(nthreads);
    //printf("Setting number of threads to %d for KACZ sweep\n",nthreads);

    omp_lock_t* execute;
    execute = (omp_lock_t*) malloc(nthreads*sizeof(omp_lock_t));

    for (int i=0; i<nthreads; i++) {
        omp_init_lock(&execute[i]);
    }


#ifdef GHOST_HAVE_OPENMP  
#pragma omp parallel
    {
#endif
        ghost_lidx tid = ghost_omp_threadnum();
        ghost_lidx even_start = zone_ptr[2*tid];
        ghost_lidx even_end   = zone_ptr[2*tid+1];
        ghost_lidx odd_start  = zone_ptr[2*tid+1];
        ghost_lidx odd_end    = zone_ptr[2*tid+2];
        ghost_lidx stride     = 0;

        if (opts.direction == GHOST_KACZ_DIRECTION_BACKWARD) {
            even_start   = zone_ptr[2*tid+2]-1; 
            even_end     = zone_ptr[2*tid+1]-1;
            odd_start    = zone_ptr[2*tid+1]-1;
            odd_end      = zone_ptr[2*tid]-1;
            stride       = -1;
        } else {
            even_start = zone_ptr[2*tid];
            even_end   = zone_ptr[2*tid+1];
            odd_start  = zone_ptr[2*tid+1];
            odd_end    = zone_ptr[2*tid+2];
            stride     = 1;
        }

        //only for SELL-1-1 currently
        //even sweep
        omp_set_lock(&execute[tid]);
        double rownorm = 0.;

        for (ghost_lidx row=even_start; row!=even_end; row+=stride){
            //printf("projecting to row ........ %d\n",row); 
            rownorm = 0.;
            ghost_lidx  idx = mat->chunkStart[row];

            double scal  = -bval[row]; 
            for (ghost_lidx j=0; j<mat->rowLen[row]; ++j) {
                scal += (double)mval[idx] * xval[mat->col[idx]];
                if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                    rownorm += mval[idx]*mval[idx]; 
                }
                idx += 1;
            }

            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                scal /= (double)rownorm;
            }
            //scal *= omega;

            idx -= mat->rowLen[row];

#pragma simd vectorlength(4)
            for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
                xval[mat->col[idx]] = xval[mat->col[idx]] - scal * (double)mval[idx];
                idx += 1;
            }

        }

        omp_unset_lock(&execute[tid]);

        //odd sweep
        if(tid+1 < nthreads) {
            omp_set_lock(&execute[tid+1]);
        }

        for (ghost_lidx row=odd_start; row!=odd_end; row+=stride){
            //printf("projecting to row ........ %d\n",row);
            rownorm = 0.; 
            ghost_lidx idx = mat->chunkStart[row];

            double scal = -bval[row];
            for (ghost_lidx j=0; j<mat->rowLen[row]; ++j) {
                scal += (double)mval[idx] * xval[mat->col[idx]];
                if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                    rownorm += mval[idx]*mval[idx];
                }
                idx += 1;
            }

            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                scal /= (double)rownorm;
            }
            //scal *= omega;

            idx -= mat->rowLen[row];

#pragma simd vectorlength(4)
            for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
                xval[mat->col[idx]] = xval[mat->col[idx]]  - scal * (double)mval[idx];
                idx += 1;
            }      

        }

        if(tid+1 < nthreads) {
            omp_unset_lock(&execute[tid+1]);
        }

#ifdef GHOST_HAVE_OPENMP
    }
#endif
    for (int i=0; i<nthreads; i++) {
        omp_destroy_lock(&execute[i]);
    }

    free(execute);

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);
    return GHOST_SUCCESS;
}



ghost_error ghost_kacz_rb_v5(ghost_densemat *x, ghost_sparsemat *mat, ghost_densemat *b, ghost_kacz_opts opts)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);
    //int flag_err = 0;
    //currently only implementation for SELL-1-1
    //const int CHUNKHEIGHT = 1;  
    const int NVECS = 1;

    //TODO check for RCM and give a Warning
    if (mat->context->nzones == 0 || mat->context->zone_ptr == NULL){
        ERROR_LOG("Splitting of matrix to Red and Black ( odd and even) have not be done!");
    }

    if (NVECS > 1) {
        ERROR_LOG("Multi-vec not implemented!");
        return GHOST_ERR_NOT_IMPLEMENTED;
    }

    double *bval = NULL;

    if(b!= NULL)
        bval = (double *)(b->val);

    double *xval = (double *)(x->val);
    double *mval = (double *)mat->val;
    double omega = *(double *)opts.omega;
    ghost_lidx *zone_ptr = mat->context->zone_ptr;
    ghost_lidx nzones    = mat->context->nzones;
    ghost_lidx nthreads  = nzones/2;

    // disables dynamic thread adjustments 
    ghost_omp_set_dynamic(0);
    ghost_omp_nthread_set(nthreads);
    //printf("Setting number of threads to %d for KACZ sweep\n",nthreads);

    //omp_lock_t* execute;
    //execute = (omp_lock_t*) malloc((nthreads+2)*sizeof(omp_lock_t));//added 2 more locks for convenience
    ghost_lidx *flag;
    flag = (ghost_lidx*) malloc((nthreads+2)*sizeof(ghost_lidx));

    for (int i=0; i<nthreads+2; i++) {
        // omp_init_lock(&execute[i]);
        flag[i] = 0;
    }

    //always execute first and last
    flag[0] = 1;         
    flag[nthreads+1] = 1;

#ifdef GHOST_HAVE_OPENMP  
#pragma omp parallel
    {
#endif
        ghost_lidx tid = ghost_omp_threadnum();
        ghost_lidx even_start = zone_ptr[2*tid];
        ghost_lidx even_end   = zone_ptr[2*tid+1];
        ghost_lidx odd_start  = zone_ptr[2*tid+1];
        ghost_lidx odd_end    = zone_ptr[2*tid+2];
        ghost_lidx stride     = 0;

        if (opts.direction == GHOST_KACZ_DIRECTION_BACKWARD) {
            even_start   = zone_ptr[2*tid+2]-1; 
            even_end     = zone_ptr[2*tid+1]-1;
            odd_start    = zone_ptr[2*tid+1]-1;
            odd_end      = zone_ptr[2*tid]-1;
            stride       = -1;
        } else {
            even_start = zone_ptr[2*tid];
            even_end   = zone_ptr[2*tid+1];
            odd_start  = zone_ptr[2*tid+1];
            odd_end    = zone_ptr[2*tid+2];
            stride     = 1;
        }

        //only for SELL-1-1 currently
        //even sweep
        //omp_set_lock(&execute[tid+1]);
        double rownorm = 0.;

        for (ghost_lidx row=even_start; row!=even_end; row+=stride){
            //printf("projecting to row ........ %d\n",row); 
            rownorm = 0.;
            ghost_lidx  idx = mat->chunkStart[row];

            double scal = 0;

            if(bval != NULL)
                scal  = -bval[row];

            for (ghost_lidx j=0; j<mat->rowLen[row]; ++j) {
                scal += (double)mval[idx] * xval[mat->col[idx]];
                if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                    rownorm += mval[idx]*mval[idx]; 
                }
                idx += 1;
            }
            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) 
                scal /= (double)rownorm;
            scal *= omega;

            idx -= mat->rowLen[row];

#pragma simd vectorlength(4)
            for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
                xval[mat->col[idx]] = xval[mat->col[idx]] - scal * (double)mval[idx];
                idx += 1;
            }

        }

        flag[tid+1] = 1;
        //printf("thread %d here \n",tid);

#pragma omp flush      
        //omp_unset_lock(&execute[tid+1]);
        //#pragma omp barrier 
        //odd sweep

        if(opts.direction == GHOST_KACZ_DIRECTION_FORWARD) {
            while(flag[tid+2]!=1){
#pragma omp flush
            }

            //omp_set_lock(&execute[tid+2]);
        }
        else {
            while(flag[tid]!=1){
#pragma omp flush
            }
            // omp_set_lock(&execute[tid]);
        }


        for (ghost_lidx row=odd_start; row!=odd_end; row+=stride){
            //printf("projecting to row ........ %d\n",row);
            rownorm = 0.; 
            ghost_lidx idx = mat->chunkStart[row];
            double scal = 0;

            if(bval != NULL)
                scal  = -bval[row];

            for (ghost_lidx j=0; j<mat->rowLen[row]; ++j) {
                scal += (double)mval[idx] * xval[mat->col[idx]];
                if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) {
                    rownorm += mval[idx]*mval[idx];
                }
                idx += 1;
            }

            if(opts.normalize==GHOST_KACZ_NORMALIZE_NO) 
                scal /= (double)rownorm;
            scal *= omega;

            idx -= mat->rowLen[row];

#pragma simd vectorlength(4)
            for (ghost_lidx j=0; j<mat->rowLen[row]; j++) {
                xval[mat->col[idx]] = xval[mat->col[idx]]  - scal * (double)mval[idx];
                idx += 1;
            }      

        }

        /*  if(opts.direction == GHOST_KACZ_DIRECTION_FORWARD) {
            omp_unset_lock(&execute[tid+2]);
            }
            else {
            omp_unset_lock(&execute[tid]);
            }*/


#ifdef GHOST_HAVE_OPENMP
    }
#endif
    /*    for (int i=0; i<nthreads+2; i++) {
          omp_destroy_lock(&execute[i]);
          }*/

    //free(execute);
    free(flag);	    
    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);
    return GHOST_SUCCESS;
}


