#include "ghost/config.h"
#include "ghost/types.h"
#include "ghost/sell.h"
#include "ghost/util.h"
#include "ghost/instr.h"
#include "ghost/machine.h"
#include "ghost/omp.h"
#include <immintrin.h>

#GHOST_FUNC_BEGIN#CHUNKHEIGHT=16,32,64,128
ghost_error_t dd_SELL_kernel_MIC_CHUNKHEIGHT_multivec_x_cm(ghost_sparsemat_t *mat, ghost_densemat_t* res, ghost_densemat_t* invec, ghost_spmv_flags_t spmvmOptions,va_list argp)
{
    UNUSED(argp);
#ifdef GHOST_HAVE_MIC
    INFO_LOG("in MIC kernel with chunkheight %d",CHUNKHEIGHT);
    ghost_idx_t c,j,v,i;
    ghost_nnz_t offs;
    double *mval = (double *)SELL(mat)->val;
    __m512d val;
    __m512d rhs;
    __m512i idx;
    double *local_dot_product = NULL;
    double *partsums = NULL;
    double sscale = 1., sbeta = 1.;
    double *sshift = NULL;
    __m512d shift, scale, beta;

    GHOST_SPMV_PARSE_ARGS(spmvmOptions,argp,sscale,sbeta,sshift,local_dot_product,double);
    scale = _mm512_set1_pd(sscale);
    beta = _mm512_set1_pd(sbeta);

    int nthreads = 1;
    unsigned clsize;
    ghost_machine_cacheline_size(&clsize);
    int padding = (int)clsize/sizeof(double);
    if (spmvmOptions & GHOST_SPMV_DOT) {

#pragma omp parallel 
        {
#pragma omp single
            nthreads = ghost_omp_nthread();
        }

        GHOST_CALL_RETURN(ghost_malloc((void **)&partsums,(3*invec->traits.ncols+padding)*nthreads*sizeof(double))); 
        for (i=0; i<(3*invec->traits.ncols+padding)*nthreads; i++) {
            partsums[i] = 0.;
        }
    }

#pragma omp parallel private(j,idx,val,rhs,offs,v)
    {
        
        #GHOST_UNROLL#__m512d tmp@;#CHUNKHEIGHT/8
        int tid = ghost_omp_threadnum();
        __m512d dot1[invec->traits.ncols],dot2[invec->traits.ncols],dot3[invec->traits.ncols];
        for (v=0; v<invec->traits.ncols; v++) {
            dot1[v] = _mm512_setzero_pd();
            dot2[v] = _mm512_setzero_pd();
            dot3[v] = _mm512_setzero_pd();
        }
#pragma omp for schedule(runtime) 
        for (c=0; c<mat->nrowsPadded/CHUNKHEIGHT; c++) {
            for (v=0; v<invec->traits.ncols; v++) {
                #GHOST_UNROLL#tmp@ = _mm512_setzero_pd();#CHUNKHEIGHT/8
                double *lval = (double *)res->val[v];
                double *rval = (double *)invec->val[v];
                offs = SELL(mat)->chunkStart[c];

                for (j=0; j<(SELL(mat)->chunkStart[c+1]-SELL(mat)->chunkStart[c])/CHUNKHEIGHT; j++) 
                { // loop inside chunk
                    #GHOST_UNROLL#val = _mm512_load_pd(&mval[offs]);idx = _mm512_load_epi32(&SELL(mat)->col[offs]);rhs = _mm512_i32logather_pd(idx,rval,8);tmp2*@ = _mm512_add_pd(tmp2*@,_mm512_mul_pd(val,rhs));offs += 8;val = _mm512_load_pd(&mval[offs]);idx = _mm512_permute4f128_epi32(idx,_MM_PERM_BADC);rhs = _mm512_i32logather_pd(idx,rval,8);tmp2*@+1 = _mm512_add_pd(tmp2*@+1,_mm512_mul_pd(val,rhs));offs += 8;#CHUNKHEIGHT/16
                }
                if (spmvmOptions & (GHOST_SPMV_SHIFT | GHOST_SPMV_VSHIFT)) {
                    if (spmvmOptions & GHOST_SPMV_SHIFT) {
                        shift = _mm512_set1_pd(sshift[0]);
                    } else {
                        shift = _mm512_set1_pd(sshift[v]);
                    }
                    #GHOST_UNROLL#tmp@ = _mm512_sub_pd(tmp@,_mm512_mul_pd(shift,_mm512_load_pd(&rval[c*CHUNKHEIGHT+8*@])));#CHUNKHEIGHT/8
                }
                if (spmvmOptions & GHOST_SPMV_SCALE) {
                    #GHOST_UNROLL#tmp@ = _mm512_mul_pd(scale,tmp@);#CHUNKHEIGHT/8
                }
                if (spmvmOptions & GHOST_SPMV_AXPY) {
                    #GHOST_UNROLL#_mm512_store_pd(&lval[c*SELL(mat)->chunkHeight+8*@],_mm512_add_pd(tmp@,_mm512_load_pd(&lval[c*SELL(mat)->chunkHeight])));#CHUNKHEIGHT/8
                } else if (spmvmOptions & GHOST_SPMV_AXPBY) {
                    #GHOST_UNROLL#_mm512_store_pd(&lval[c*SELL(mat)->chunkHeight+8*@],_mm512_add_pd(tmp@,_mm512_mul_pd(beta,_mm512_load_pd(&lval[c*SELL(mat)->chunkHeight]))));#CHUNKHEIGHT/8
                } else {
                    #GHOST_UNROLL#_mm512_storenrngo_pd(&lval[c*SELL(mat)->chunkHeight+8*@],tmp@);#CHUNKHEIGHT/8
                }
                if (spmvmOptions & GHOST_SPMV_DOT) {
                    if ((c+1)*CHUNKHEIGHT <= mat->nrows) {
                        #GHOST_UNROLL#dot1[v] = _mm512_add_pd(dot1[v],_mm512_mul_pd(_mm512_load_pd(&lval[c*CHUNKHEIGHT+8*@]),_mm512_load_pd(&lval[c*CHUNKHEIGHT+8*@])));#CHUNKHEIGHT/8
                        #GHOST_UNROLL#dot2[v] = _mm512_add_pd(dot2[v],_mm512_mul_pd(_mm512_load_pd(&rval[c*CHUNKHEIGHT+8*@]),_mm512_load_pd(&lval[c*CHUNKHEIGHT+8*@])));#CHUNKHEIGHT/8
                        #GHOST_UNROLL#dot3[v] = _mm512_add_pd(dot3[v],_mm512_mul_pd(_mm512_load_pd(&rval[c*CHUNKHEIGHT+8*@]),_mm512_load_pd(&rval[c*CHUNKHEIGHT+8*@])));#CHUNKHEIGHT/8
                    } else {
                        ghost_idx_t rem;
                        for (rem=0; rem<mat->nrows-c*CHUNKHEIGHT; rem++) {
                            partsums[((padding+3*invec->traits.ncols)*tid)+3*v+0] += lval[c*CHUNKHEIGHT+rem]*lval[c*CHUNKHEIGHT+rem];
                            partsums[((padding+3*invec->traits.ncols)*tid)+3*v+1] += lval[c*CHUNKHEIGHT+rem]*rval[c*CHUNKHEIGHT+rem];
                            partsums[((padding+3*invec->traits.ncols)*tid)+3*v+2] += rval[c*CHUNKHEIGHT+rem]*rval[c*CHUNKHEIGHT+rem];
                        }
                    }
                }
            }
        }
        if (spmvmOptions & GHOST_SPMV_DOT) {
            for (v=0; v<invec->traits.ncols; v++) {
                partsums[((padding+3*invec->traits.ncols)*tid)+3*v+0] += _mm512_reduce_add_pd(dot1[v]);
                partsums[((padding+3*invec->traits.ncols)*tid)+3*v+1] += _mm512_reduce_add_pd(dot2[v]);
                partsums[((padding+3*invec->traits.ncols)*tid)+3*v+2] += _mm512_reduce_add_pd(dot3[v]);
            }
        }
    }
    if (spmvmOptions & GHOST_SPMV_DOT) {
        for (v=0; v<invec->traits.ncols; v++) {
            local_dot_product[v                       ] = 0.; 
            local_dot_product[v  +   invec->traits.ncols] = 0.;
            local_dot_product[v  + 2*invec->traits.ncols] = 0.;
            for (i=0; i<nthreads; i++) {
                local_dot_product[v                       ] += partsums[(padding+3*invec->traits.ncols)*i + 3*v + 0];
                local_dot_product[v  +   invec->traits.ncols] += partsums[(padding+3*invec->traits.ncols)*i + 3*v + 1];
                local_dot_product[v  + 2*invec->traits.ncols] += partsums[(padding+3*invec->traits.ncols)*i + 3*v + 2];
            }
        }
        free(partsums);
    }
#else 
    UNUSED(mat);
    UNUSED(res);
    UNUSED(invec);
    UNUSED(spmvmOptions);
#endif
    return GHOST_SUCCESS;
}
#GHOST_FUNC_END

#GHOST_FUNC_BEGIN#CHUNKHEIGHT=1,2,4,8,16,32
ghost_error_t dd_SELL_kernel_MIC_CHUNKHEIGHT_multivec_x_rm(ghost_sparsemat_t *mat, ghost_densemat_t* res, ghost_densemat_t* invec, ghost_spmv_flags_t spmvmOptions,va_list argp)
{
#ifdef GHOST_HAVE_MIC
    INFO_LOG("rm MIC kernel with ch %d",CHUNKHEIGHT);
    ghost_idx_t j,c,col;
    ghost_nnz_t offs;
    double *mval = (double *)SELL(mat)->val;
    double *local_dot_product = NULL;
    double *partsums = NULL;
    __m512d rhs;
    int nthreads = 1, i;
    int ncolspadded = PAD(invec->traits.ncols,8);
    
    unsigned clsize;
    ghost_machine_cacheline_size(&clsize);
    int padding = (int)clsize/sizeof(double);

    UNUSED(argp);
    
    double sscale = 1., sbeta = 1.;
    double *sshift = NULL;
    __m512d shift, scale, beta;

    GHOST_SPMV_PARSE_ARGS(spmvmOptions,argp,sscale,sbeta,sshift,local_dot_product,double);
    scale = _mm512_set1_pd(sscale);
    beta = _mm512_set1_pd(sbeta);
    int axpy = spmvmOptions & (GHOST_SPMV_AXPY | GHOST_SPMV_AXPBY);
    
    if (spmvmOptions & GHOST_SPMV_DOT) {

#pragma omp parallel 
        {
#pragma omp single
            nthreads = ghost_omp_nthread();
        }

        GHOST_CALL_RETURN(ghost_malloc((void **)&partsums,(3*invec->traits.ncols+padding)*nthreads*sizeof(double))); 
        for (col=0; col<(3*invec->traits.ncols+padding)*nthreads; col++) {
            partsums[col] = 0.;
        }
    }

#pragma omp parallel private(c,j,offs,rhs,col) shared (partsums)
    {
        int tid = ghost_omp_threadnum();
        #GHOST_UNROLL#__m512d tmp@;#CHUNKHEIGHT
        double *tmpresult = NULL;
        if (!axpy) {
            ghost_malloc((void **)&tmpresult,sizeof(double)*CHUNKHEIGHT*ncolspadded);
        }

        ghost_idx_t donecols;

#pragma omp for schedule(runtime)
        for (c=0; c<mat->nrowsPadded/CHUNKHEIGHT; c++) 
        { // loop over chunks
            double *lval = (double *)res->val[c*CHUNKHEIGHT];
            double *rval = (double *)invec->val[c*CHUNKHEIGHT];

            for (donecols = 0; donecols < ncolspadded; donecols+=8) {
                #GHOST_UNROLL#tmp@ = _mm512_setzero_pd();#CHUNKHEIGHT
                offs = SELL(mat)->chunkStart[c];

                for (j=0; j<SELL(mat)->chunkLen[c]; j++) { // loop inside chunk
                    #GHOST_UNROLL#rhs = _mm512_load_pd((double *)invec->val[SELL(mat)->col[offs]]+donecols);tmp@ = _mm512_add_pd(tmp@,_mm512_mul_pd(_mm512_set1_pd(mval[offs++]),rhs));#CHUNKHEIGHT
                }
              
                if (spmvmOptions & (GHOST_SPMV_SHIFT | GHOST_SPMV_VSHIFT)) {
                    if (spmvmOptions & GHOST_SPMV_SHIFT) {
                        shift = _mm512_set1_pd(sshift[0]);
                    } else {
                        shift = _mm512_load_pd(&sshift[donecols]);
                    }
                    #GHOST_UNROLL#tmp@ = _mm512_sub_pd(tmp@,_mm512_mul_pd(shift,_mm512_load_pd((double *)invec->val[c*CHUNKHEIGHT+@]+donecols)));#CHUNKHEIGHT
                }
                if (spmvmOptions & GHOST_SPMV_SCALE) {
                    #GHOST_UNROLL#tmp@ = _mm512_mul_pd(scale,tmp@);#CHUNKHEIGHT
                }
                if (axpy || ncolspadded<=8 || CHUNKHEIGHT == 1) {
                    if (spmvmOptions & GHOST_SPMV_AXPY) {
                        #GHOST_UNROLL#_mm512_store_pd(&lval[invec->traits.ncolspadded*@+donecols],_mm512_add_pd(tmp@,_mm512_load_pd(&lval[invec->traits.ncolspadded*@+donecols])));#CHUNKHEIGHT
                    } else if (spmvmOptions & GHOST_SPMV_AXPBY) {
                        #GHOST_UNROLL#_mm512_store_pd(&lval[invec->traits.ncolspadded*@+donecols],_mm512_add_pd(tmp@,_mm512_mul_pd(_mm512_load_pd(&lval[invec->traits.ncolspadded*@+donecols]),beta)));#CHUNKHEIGHT
                    } else {
                        #GHOST_UNROLL#_mm512_store_pd(&lval[invec->traits.ncolspadded*@+donecols],tmp@);#CHUNKHEIGHT
                    }
                    if (spmvmOptions & GHOST_SPMV_DOT) {
                        for (col = donecols; col<donecols+8; col++) {
                            #GHOST_UNROLL#partsums[((padding+3*invec->traits.ncols)*tid)+3*col+0] += lval[col+@*invec->traits.ncolspadded]*lval[col+@*invec->traits.ncolspadded];#CHUNKHEIGHT
                            #GHOST_UNROLL#partsums[((padding+3*invec->traits.ncols)*tid)+3*col+1] += lval[col+@*invec->traits.ncolspadded]*rval[col+@*invec->traits.ncolspadded];#CHUNKHEIGHT
                            #GHOST_UNROLL#partsums[((padding+3*invec->traits.ncols)*tid)+3*col+2] += rval[col+@*invec->traits.ncolspadded]*rval[col+@*invec->traits.ncolspadded];#CHUNKHEIGHT
                        }
                    }
                } else { 
                    #GHOST_UNROLL#_mm512_store_pd(&tmpresult[@*ncolspadded+donecols],tmp@);#CHUNKHEIGHT
                    // TODO if non-AXPY cxompute DOT from tmp instead of lval
                }
            }
            if (!axpy && ncolspadded>8 && CHUNKHEIGHT != 1) {
                #GHOST_UNROLL#for (donecols = 0; donecols < ncolspadded; donecols+=8) {tmp@ = _mm512_load_pd(&tmpresult[@*ncolspadded+donecols]); _mm512_storenrngo_pd(&lval[@*ncolspadded+donecols],tmp@);}#CHUNKHEIGHT
            }
            
        }
    }
    if (spmvmOptions & GHOST_SPMV_DOT) {
        for (col=0; col<invec->traits.ncols; col++) {
            local_dot_product[col                       ] = 0.; 
            local_dot_product[col  +   invec->traits.ncols] = 0.;
            local_dot_product[col  + 2*invec->traits.ncols] = 0.;
            for (i=0; i<nthreads; i++) {
                local_dot_product[col                         ] += partsums[(padding+3*invec->traits.ncols)*i + 3*col + 0];
                local_dot_product[col  +   invec->traits.ncols] += partsums[(padding+3*invec->traits.ncols)*i + 3*col + 1];
                local_dot_product[col  + 2*invec->traits.ncols] += partsums[(padding+3*invec->traits.ncols)*i + 3*col + 2];
            }
        }
        free(partsums);
    }

#else
    UNUSED(mat);
    UNUSED(res);
    UNUSED(invec);
    UNUSED(spmvmOptions);
    UNUSED(argp);
#endif
    return GHOST_SUCCESS;
}
#GHOST_FUNC_END

#GHOST_FUNC_BEGIN#NVECS=8,16,24#CHUNKHEIGHT=1,2,4,8,16,32
ghost_error_t dd_SELL_kernel_MIC_CHUNKHEIGHT_multivec_NVECS_rm(ghost_sparsemat_t *mat, ghost_densemat_t* res, ghost_densemat_t* invec, ghost_spmv_flags_t spmvmOptions,va_list argp)
{
#ifdef GHOST_HAVE_MIC
    INFO_LOG("in row-mjaor MIC kernel w nvecs %d chunkheight %d",NVECS,CHUNKHEIGHT);
    double *mval = (double *)SELL(mat)->val;
    double *local_dot_product = NULL;
    double *partsums = NULL;
    int nthreads = 1, i;
    
    unsigned clsize;
    ghost_machine_cacheline_size(&clsize);
    int padding = (int)clsize/sizeof(double);

    UNUSED(argp);
    
    double sscale = 1., sbeta = 1.;
    double *sshift = NULL;
    __m512d scale, beta;

    GHOST_SPMV_PARSE_ARGS(spmvmOptions,argp,sscale,sbeta,sshift,local_dot_product,double);
    scale = _mm512_set1_pd(sscale);
    beta = _mm512_set1_pd(sbeta);
    
    if (spmvmOptions & GHOST_SPMV_DOT) {

#pragma omp parallel 
        {
#pragma omp single
            nthreads = ghost_omp_nthread();
        }

        GHOST_CALL_RETURN(ghost_malloc((void **)&partsums,(3*invec->traits.ncols+padding)*nthreads*sizeof(double))); 
        ghost_idx_t col;
        for (col=0; col<(3*invec->traits.ncols+padding)*nthreads; col++) {
            partsums[col] = 0.;
        }
    }

#pragma omp parallel shared (partsums)
    {
        ghost_idx_t j,c,col;
        ghost_nnz_t offs;
        __m512d rhs;
        int tid = ghost_omp_threadnum();
        #GHOST_UNROLL#__m512d tmp@;#CHUNKHEIGHT*NVECS/8

#pragma omp for schedule(runtime)
        for (c=0; c<mat->nrowsPadded/CHUNKHEIGHT; c++) 
        { // loop over chunks
            double *lval = (double *)res->val[c*CHUNKHEIGHT];
            double *rval = (double *)invec->val[c*CHUNKHEIGHT];

            #GHOST_UNROLL#tmp@ = _mm512_setzero_pd();#CHUNKHEIGHT*NVECS/8
            offs = SELL(mat)->chunkStart[c];

            for (j=0; j<SELL(mat)->chunkLen[c]; j++) { // loop inside chunk
                
                #GHOST_UNROLL#rhs = _mm512_load_pd((double *)invec->val[SELL(mat)->col[offs]]+(@%(NVECS/8))*8);tmp@ = _mm512_add_pd(tmp@,_mm512_mul_pd(_mm512_set1_pd(mval[offs]),rhs));if(!((@+1)%(NVECS/8)))offs++;#CHUNKHEIGHT*NVECS/8
            }
            if (spmvmOptions & GHOST_SPMV_SHIFT) {
                #GHOST_UNROLL#tmp@ = _mm512_sub_pd(tmp@,_mm512_mul_pd(_mm512_set1_pd(sshift[0]),_mm512_load_pd(rval+@*8)));#CHUNKHEIGHT*NVECS/8
            } else if (spmvmOptions & GHOST_SPMV_VSHIFT) {
                #GHOST_UNROLL#tmp@ = _mm512_sub_pd(tmp@,_mm512_mul_pd(_mm512_load_pd(&sshift[(@%(NVECS/8))*8]),_mm512_load_pd(rval+@*8)));#CHUNKHEIGHT*NVECS/8
            }
            if (spmvmOptions & GHOST_SPMV_SCALE) {
                #GHOST_UNROLL#tmp@ = _mm512_mul_pd(scale,tmp@);#CHUNKHEIGHT*NVECS/8
            }
            if (spmvmOptions & GHOST_SPMV_AXPY) {
                #GHOST_UNROLL#_mm512_store_pd(lval+@*8,_mm512_add_pd(tmp@,_mm512_load_pd(lval+@*8)));#CHUNKHEIGHT*NVECS/8
            } else if (spmvmOptions & GHOST_SPMV_AXPBY) {
                #GHOST_UNROLL#_mm512_store_pd(lval+@*8,_mm512_add_pd(tmp@,_mm512_mul_pd(_mm512_load_pd(lval+@*8),beta)));#CHUNKHEIGHT*NVECS/8
            } else {
                #GHOST_UNROLL#_mm512_storenrngo_pd(lval+@*8,tmp@);#CHUNKHEIGHT*NVECS/8
            }
            if (spmvmOptions & GHOST_SPMV_DOT) {
                for (col = 0; col<invec->traits.ncols; col++) {
                    #GHOST_UNROLL#partsums[((padding+3*invec->traits.ncols)*tid)+3*col+0] += lval[col+@*invec->traits.ncolspadded]*lval[col+@*invec->traits.ncolspadded];#CHUNKHEIGHT
                    #GHOST_UNROLL#partsums[((padding+3*invec->traits.ncols)*tid)+3*col+1] += lval[col+@*invec->traits.ncolspadded]*rval[col+@*invec->traits.ncolspadded];#CHUNKHEIGHT
                    #GHOST_UNROLL#partsums[((padding+3*invec->traits.ncols)*tid)+3*col+2] += rval[col+@*invec->traits.ncolspadded]*rval[col+@*invec->traits.ncolspadded];#CHUNKHEIGHT
                }
            }
        }
    }
    if (spmvmOptions & GHOST_SPMV_DOT) {
        ghost_idx_t col;
        for (col=0; col<invec->traits.ncols; col++) {
            local_dot_product[col                       ] = 0.; 
            local_dot_product[col  +   invec->traits.ncols] = 0.;
            local_dot_product[col  + 2*invec->traits.ncols] = 0.;
            for (i=0; i<nthreads; i++) {
                local_dot_product[col                         ] += partsums[(padding+3*invec->traits.ncols)*i + 3*col + 0];
                local_dot_product[col  +   invec->traits.ncols] += partsums[(padding+3*invec->traits.ncols)*i + 3*col + 1];
                local_dot_product[col  + 2*invec->traits.ncols] += partsums[(padding+3*invec->traits.ncols)*i + 3*col + 2];
            }
        }
        free(partsums);
    }

#else
    UNUSED(mat);
    UNUSED(res);
    UNUSED(invec);
    UNUSED(spmvmOptions);
    UNUSED(argp);
#endif
    return GHOST_SUCCESS;
}
#GHOST_FUNC_END

//ghost_error_t dd_SELL_kernel_MIC_32(ghost_sparsemat_t *mat, ghost_densemat_t* res, ghost_densemat_t* invec, ghost_spmv_flags_t spmvmOptions,va_list argp)
/*{
    UNUSED(argp);
#ifdef GHOST_HAVE_MIC
    ghost_idx_t c,j;
    ghost_nnz_t offs;
    double *mval = (double *)SELL(mat)->val;
    double *lval = (double *)res->val[0];
    double *rval = (double *)invec->val[0];
    __m512d tmp1;
    __m512d tmp2;
    __m512d tmp3;
    __m512d tmp4;
    __m512d val;
    __m512d rhs;
    __m512i idx;

#pragma omp parallel for schedule(runtime) private(j,tmp1,tmp2,tmp3,tmp4,idx,val,rhs,offs)
    for (c=0; c<mat->nrowsPadded>>5; c++) 
    { // loop over chunks
        tmp1 = _mm512_setzero_pd(); // tmp1 = 0
        tmp2 = _mm512_setzero_pd(); // tmp2 = 0
        tmp3 = _mm512_setzero_pd(); // tmp3 = 0
        tmp4 = _mm512_setzero_pd(); // tmp4 = 0
        offs = SELL(mat)->chunkStart[c];

        for (j=0; j<(SELL(mat)->chunkStart[c+1]-SELL(mat)->chunkStart[c])>>5; j++) 
        { // loop inside chunk

            val = _mm512_load_pd(&mval[offs]);
            idx = _mm512_load_epi32(&SELL(mat)->col[offs]);
            rhs = _mm512_i32logather_pd(idx,rval,8);
            tmp1 = _mm512_add_pd(tmp1,_mm512_mul_pd(val,rhs));

            offs += 8;

            val = _mm512_load_pd(&mval[offs]);
            idx = _mm512_permute4f128_epi32(idx,_MM_PERM_BADC);
            rhs = _mm512_i32logather_pd(idx,rval,8);
            tmp2 = _mm512_add_pd(tmp2,_mm512_mul_pd(val,rhs));

            offs += 8;

            val = _mm512_load_pd(&mval[offs]);
            idx = _mm512_load_epi32(&SELL(mat)->col[offs]);
            rhs = _mm512_i32logather_pd(idx,rval,8);
            tmp3 = _mm512_add_pd(tmp3,_mm512_mul_pd(val,rhs));

            offs += 8;

            val = _mm512_load_pd(&mval[offs]);
            idx = _mm512_permute4f128_epi32(idx,_MM_PERM_BADC);
            rhs = _mm512_i32logather_pd(idx,rval,8);
            tmp4 = _mm512_add_pd(tmp4,_mm512_mul_pd(val,rhs));

            offs += 8;
        }
        if (spmvmOptions & GHOST_SPMV_AXPY) {
            _mm512_store_pd(&lval[c*SELL(mat)->chunkHeight],_mm512_add_pd(tmp1,_mm512_load_pd(&lval[c*SELL(mat)->chunkHeight])));
            _mm512_store_pd(&lval[c*SELL(mat)->chunkHeight+8],_mm512_add_pd(tmp2,_mm512_load_pd(&lval[c*SELL(mat)->chunkHeight+8])));
            _mm512_store_pd(&lval[c*SELL(mat)->chunkHeight+16],_mm512_add_pd(tmp3,_mm512_load_pd(&lval[c*SELL(mat)->chunkHeight+16])));
            _mm512_store_pd(&lval[c*SELL(mat)->chunkHeight+24],_mm512_add_pd(tmp4,_mm512_load_pd(&lval[c*SELL(mat)->chunkHeight+24])));
        } else {
            _mm512_storenrngo_pd(&lval[c*SELL(mat)->chunkHeight],tmp1);
            _mm512_storenrngo_pd(&lval[c*SELL(mat)->chunkHeight+8],tmp2);
            _mm512_storenrngo_pd(&lval[c*SELL(mat)->chunkHeight+16],tmp3);
            _mm512_storenrngo_pd(&lval[c*SELL(mat)->chunkHeight+24],tmp4);
        }
    }
#else 
    UNUSED(mat);
    UNUSED(res);
    UNUSED(invec);
    UNUSED(spmvmOptions);
#endif
    return GHOST_SUCCESS;
}*/
