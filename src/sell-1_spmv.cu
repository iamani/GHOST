#include "ghost/config.h"
#include "ghost/types.h"
#include "ghost/log.h"
#include "ghost/cu_util.h"
#include "ghost/cu_complex.h"
#include "ghost/util.h"
#include "ghost/sparsemat.h"
#include "ghost/math.h"
#include "ghost/locality.h"

#include <complex>
#include <cuda_runtime.h>
#include <cusparse_v2.h>

typedef cusparseStatus_t (*cusparse_sell1_spmv_kernel_t) (cusparseHandle_t handle, cusparseOperation_t transA, 
        int m, int n, int nnz, const void           *alpha, 
        const cusparseMatDescr_t descrA, 
        const void           *csrValA, 
        const void *csrRowPtrA, const void *csrColIndA,
        const void           *x, const void           *beta, 
        void           *y);

typedef cusparseStatus_t (*cusparse_sell1_spmmv_cm_kernel_t) (cusparseHandle_t handle, cusparseOperation_t transA, 
        int m, int n, int k, int nnz, const void           *alpha, 
        const cusparseMatDescr_t descrA, 
        const void           *csrValA, 
        const void *csrRowPtrA, const void *csrColIndA,
        const void           *x, int ldx, const void           *beta, 
        void           *y, int ldy);

typedef cusparseStatus_t (*cusparse_sell1_spmmv_rm_kernel_t) (cusparseHandle_t handle, cusparseOperation_t transA,
        cusparseOperation_t transB,
        int m, int n, int k, int nnz, const void           *alpha, 
        const cusparseMatDescr_t descrA, 
        const void           *csrValA, 
        const void *csrRowPtrA, const void *csrColIndA,
        const void           *x, int ldx, const void           *beta, 
        void           *y, int ldy);

    template<typename dt1, typename dt2>
static ghost_error ghost_cu_sell1spmv_tmpl(ghost_sparsemat *mat, ghost_densemat * lhs, ghost_densemat * rhs, ghost_spmv_opts traits, cusparse_sell1_spmv_kernel_t sell1kernel)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH);
    cusparseHandle_t cusparse_handle;
    cusparseMatDescr_t descr;

    cusparseCreateMatDescr(&descr);
    GHOST_CALL_RETURN(ghost_cu_cusparse_handle(&cusparse_handle));

    dt2 * __attribute__((unused)) localdot = NULL;
    dt1 * __attribute__((unused)) shift = NULL, scale, beta, __attribute__((unused)) sdelta, __attribute__((unused)) seta;
    ghost_densemat * __attribute__((unused)) z = NULL;

    one<dt1>(scale);

    GHOST_SPMV_PARSE_TRAITS(traits,scale,beta,shift,localdot,z,sdelta,seta,dt2,dt1);

    if (traits.flags & GHOST_SPMV_AXPY) {
        one<dt1>(beta);
    } else if (!(traits.flags & GHOST_SPMV_AXPBY)) {
        zero<dt1>(beta);
    }
    
    int matncols,nrank;
    ghost_nrank(&nrank,mat->context->mpicomm);
    if (nrank > 1) {
        matncols = mat->context->col_map->dimhalo;
    } else {
        matncols = mat->context->col_map->dim;
    }
    CUSPARSE_CALL_RETURN(sell1kernel(cusparse_handle,CUSPARSE_OPERATION_NON_TRANSPOSE,SPM_NROWS(mat),matncols,mat->nEnts,&scale,descr,(dt1 *)mat->cu_val, mat->cu_chunkStart, mat->cu_col, (dt1 *)rhs->cu_val, &beta, (dt1 *)lhs->cu_val));
   
    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH);
    return GHOST_SUCCESS;
}
    
    template<typename dt1, typename dt2>
static ghost_error ghost_cu_sell1spmmv_cm_tmpl(ghost_sparsemat *mat, ghost_densemat * lhs, ghost_densemat * rhs, ghost_spmv_opts traits, cusparse_sell1_spmmv_cm_kernel_t sell1kernel)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH);
    cusparseHandle_t cusparse_handle;
    cusparseMatDescr_t descr;

    cusparseCreateMatDescr(&descr);
    GHOST_CALL_RETURN(ghost_cu_cusparse_handle(&cusparse_handle));

    dt2 * __attribute__((unused)) localdot = NULL;
    dt1 * __attribute__((unused)) shift = NULL, scale, beta, __attribute__((unused)) sdelta, __attribute__((unused)) seta;
    ghost_densemat * __attribute__((unused)) z = NULL;

    one<dt1>(scale);

    GHOST_SPMV_PARSE_TRAITS(traits,scale,beta,shift,localdot,z,sdelta,seta,dt2,dt1);
    
    if (traits.flags & GHOST_SPMV_AXPY) {
        one<dt1>(beta);
    } else if (!(traits.flags & GHOST_SPMV_AXPBY)) {
        zero<dt1>(beta);
    }
    int matncols,nrank;
    ghost_nrank(&nrank,mat->context->mpicomm);
    if (nrank > 1) {
        matncols = mat->context->col_map->dimhalo;
    } else {
        matncols = mat->context->col_map->dim;
    }
    CUSPARSE_CALL_RETURN(sell1kernel(cusparse_handle,CUSPARSE_OPERATION_NON_TRANSPOSE,SPM_NROWS(mat),rhs->traits.ncols,matncols,mat->nEnts,&scale,descr,(dt1 *)mat->cu_val, mat->cu_chunkStart, mat->cu_col, (dt1 *)rhs->cu_val, rhs->stride, &beta, (dt1 *)lhs->cu_val, lhs->stride));

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH);
    return GHOST_SUCCESS;
}

    template<typename dt1, typename dt2>
static ghost_error ghost_cu_sell1spmmv_rm_tmpl(ghost_sparsemat *mat, ghost_densemat * lhs, ghost_densemat * rhs, ghost_spmv_opts traits, cusparse_sell1_spmmv_rm_kernel_t sell1kernel)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH);
    cusparseHandle_t cusparse_handle;
    cusparseMatDescr_t descr;

    cusparseCreateMatDescr(&descr);
    GHOST_CALL_RETURN(ghost_cu_cusparse_handle(&cusparse_handle));

    dt2 * __attribute__((unused)) localdot = NULL;
    dt1 * __attribute__((unused)) shift = NULL, scale, beta, __attribute__((unused)) sdelta, __attribute__((unused)) seta;
    ghost_densemat * __attribute__((unused)) z = NULL;

    one<dt1>(scale);

    GHOST_SPMV_PARSE_TRAITS(traits,scale,beta,shift,localdot,z,sdelta,seta,dt2,dt1);

    if (traits.flags & GHOST_SPMV_AXPY) {
        one<dt1>(beta);
    } else if (!(traits.flags & GHOST_SPMV_AXPBY)) {
        zero<dt1>(beta);
    }
    
    int matncols,nrank;
    ghost_nrank(&nrank,mat->context->mpicomm);
    if (nrank > 1) {
        matncols = mat->context->col_map->dimhalo;
    } else {
        matncols = mat->context->col_map->dim;
    }
    CUSPARSE_CALL_RETURN(sell1kernel(cusparse_handle,CUSPARSE_OPERATION_NON_TRANSPOSE,CUSPARSE_OPERATION_TRANSPOSE,SPM_NROWS(mat),rhs->traits.ncols,matncols,mat->nEnts,&scale,descr,(dt1 *)mat->cu_val, mat->cu_chunkStart, mat->cu_col, (dt1 *)rhs->cu_val, rhs->stride, &beta, (dt1 *)lhs->cu_val,lhs->stride));
    
    
    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH);
    return GHOST_SUCCESS;
}
    
    template<typename dt>
static ghost_error ghost_cu_sell1spmv_augfunc_tmpl(ghost_densemat * lhs, ghost_densemat * rhs, ghost_spmv_opts traits)
{
    dt *localdot = NULL;
    dt *shift = NULL, scale, __attribute__((unused)) beta, sdelta, seta;
    ghost_densemat *z = NULL;

    scale = 1.; //required because we need scale for SHIFT (i.e., even if we don't SCALE)
    GHOST_SPMV_PARSE_TRAITS(traits,scale,beta,shift,localdot,z,sdelta,seta,dt,dt);

    if (traits.flags & (GHOST_SPMV_SHIFT|GHOST_SPMV_VSHIFT)) {
        PERFWARNING_LOG("Shift will not be applied on-the-fly!");
        dt minusshift[rhs->traits.ncols];
        ghost_lidx col;
        if (traits.flags & GHOST_SPMV_SHIFT) {
            for (col=0; col<rhs->traits.ncols; col++) {
                minusshift[col] = (dt)-1.*(*(dt *)&scale)*(*(dt *)shift);
            }
        } else {
            for (col=0; col<rhs->traits.ncols; col++) {
                minusshift[col] = (dt)-1.*(*(dt *)&scale)*(((dt *)shift)[col]);
            }
        }
        ghost_vaxpy(lhs,rhs,minusshift);
    }
    
    if (traits.flags & GHOST_SPMV_DOT) {
        PERFWARNING_LOG("Dot product computation will be not be done on-the-fly!");
        memset(localdot,0,lhs->traits.ncols*3*sizeof(dt));
        if (traits.flags & GHOST_SPMV_DOT_YY) {
            ghost_localdot(&localdot[0],lhs,lhs);
        }
        if (traits.flags & GHOST_SPMV_DOT_XY) {
            ghost_localdot(&localdot[lhs->traits.ncols],rhs,lhs);
        }
        if (traits.flags & GHOST_SPMV_DOT_XX) {
            ghost_localdot(&localdot[2*lhs->traits.ncols],rhs,rhs);
        }
            
    }
    if (traits.flags & GHOST_SPMV_CHAIN_AXPBY) {
        PERFWARNING_LOG("AXPBY will not be done on-the-fly!");
        ghost_axpby(z,lhs,&seta,&sdelta);
    }
   
    return GHOST_SUCCESS; 
}

ghost_error ghost_cu_sell1_spmv_selector(ghost_densemat * lhs_in, ghost_sparsemat *mat, ghost_densemat * rhs_in, ghost_spmv_opts traits)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH);
    
    ghost_error ret = GHOST_SUCCESS;
    ghost_densemat *lhs, *rhs;
    
    if (mat->traits.datatype != lhs_in->traits.datatype) {
        ERROR_LOG("Mixed data types not implemented!");
        ret = GHOST_ERR_NOT_IMPLEMENTED;
        goto err;
    }
    if ((lhs_in->traits.flags & GHOST_DENSEMAT_SCATTERED) || ((lhs_in->traits.storage == GHOST_DENSEMAT_ROWMAJOR) && (lhs_in->stride != 1))) {
        PERFWARNING_LOG("Cloning lhs");
        if (lhs_in->traits.flags & GHOST_DENSEMAT_SCATTERED) {
            PERFWARNING_LOG("Cloning and compressing lhs before operation because it is scattered");
        }
        if ((lhs_in->traits.storage == GHOST_DENSEMAT_ROWMAJOR) && (lhs_in->stride != 1)) {
            PERFWARNING_LOG("Cloning and transposing lhs before operation because it is row-major");
        }
        ghost_densemat_traits lhstraits = lhs_in->traits;
        lhstraits.location = GHOST_LOCATION_DEVICE;
        lhstraits.storage = GHOST_DENSEMAT_COLMAJOR;
        lhstraits.flags &= (ghost_densemat_flags)(~GHOST_DENSEMAT_VIEW);
        GHOST_CALL_GOTO(ghost_densemat_create(&lhs,lhs_in->map,lhstraits),err,ret);
        GHOST_CALL_GOTO(ghost_densemat_init_densemat(lhs,lhs_in,0,0),err,ret);
    } else {
        lhs = lhs_in;
    }
    if ((rhs_in->traits.flags & GHOST_DENSEMAT_SCATTERED) || ((rhs_in->traits.ncols == 1) && (rhs_in->traits.storage == GHOST_DENSEMAT_ROWMAJOR) && (rhs_in->stride != 1))) {
        PERFWARNING_LOG("Cloning rhs");
        if (rhs_in->traits.flags & GHOST_DENSEMAT_SCATTERED) {
            PERFWARNING_LOG("Cloning and compressing rhs before operation because it is scattered");
        }
        if ((rhs_in->traits.ncols == 1) && (rhs_in->traits.storage == GHOST_DENSEMAT_ROWMAJOR) && (rhs_in->stride != 1)) {
            PERFWARNING_LOG("Cloning and transposing rhs before operation because it is row-major");
        }
        ghost_densemat_traits rhstraits = rhs_in->traits;
        rhstraits.location = GHOST_LOCATION_DEVICE;
        rhstraits.storage = GHOST_DENSEMAT_COLMAJOR;
        rhstraits.flags &= (ghost_densemat_flags)(~GHOST_DENSEMAT_VIEW);
        GHOST_CALL_GOTO(ghost_densemat_create(&rhs,rhs_in->map,rhstraits),err,ret);
        GHOST_CALL_GOTO(ghost_densemat_init_densemat(rhs,rhs_in,0,0),err,ret);
        GHOST_CALL_GOTO(ghost_cu_memtranspose(rhs->map->nhalo,rhs->traits.ncols,
                    &rhs->cu_val[(rhs->map->dimhalo-rhs->map->nhalo)*rhs->elSize],rhs->stride,
                    &rhs_in->cu_val[(rhs_in->map->dimhalo-rhs_in->map->nhalo)*rhs_in->stride*rhs_in->elSize],rhs_in->stride,
                    rhs->traits.datatype),err,ret);
    } else {
        rhs = rhs_in;
    }


    if (lhs->traits.ncols == 1) {
        if (mat->traits.datatype & GHOST_DT_DOUBLE) {
            if (mat->traits.datatype & GHOST_DT_REAL) {
                GHOST_CALL_GOTO((ghost_cu_sell1spmv_tmpl<double,double>(mat,lhs,rhs,traits,(cusparse_sell1_spmv_kernel_t)cusparseDcsrmv)),err,ret);
            } else {
                GHOST_CALL_GOTO((ghost_cu_sell1spmv_tmpl<cuDoubleComplex,std::complex<double> >(mat,lhs,rhs,traits,(cusparse_sell1_spmv_kernel_t)cusparseZcsrmv)),err,ret);
            }
        } else {
            if (mat->traits.datatype & GHOST_DT_REAL) {
                GHOST_CALL_GOTO((ghost_cu_sell1spmv_tmpl<float,float>(mat,lhs,rhs,traits,(cusparse_sell1_spmv_kernel_t)cusparseScsrmv)),err,ret);
            } else {
                GHOST_CALL_GOTO((ghost_cu_sell1spmv_tmpl<cuFloatComplex,std::complex<float> >(mat,lhs,rhs,traits,(cusparse_sell1_spmv_kernel_t)cusparseCcsrmv)),err,ret);
            }
        }
    } else if (rhs->traits.storage == GHOST_DENSEMAT_COLMAJOR) {
        INFO_LOG("Calling col-major cuSparse CRS SpMMV");
        if (mat->traits.datatype & GHOST_DT_DOUBLE) {
            if (mat->traits.datatype & GHOST_DT_REAL) {
                GHOST_CALL_GOTO((ghost_cu_sell1spmmv_cm_tmpl<double,double>(mat,lhs,rhs,traits,(cusparse_sell1_spmmv_cm_kernel_t)cusparseDcsrmm)),err,ret);
            } else {
                GHOST_CALL_GOTO((ghost_cu_sell1spmmv_cm_tmpl<cuDoubleComplex,std::complex<double> >(mat,lhs,rhs,traits,(cusparse_sell1_spmmv_cm_kernel_t)cusparseZcsrmm)),err,ret);
            }
        } else {
            if (mat->traits.datatype & GHOST_DT_REAL) {
                GHOST_CALL_GOTO((ghost_cu_sell1spmmv_cm_tmpl<float,float>(mat,lhs,rhs,traits,(cusparse_sell1_spmmv_cm_kernel_t)cusparseScsrmm)),err,ret);
            } else {
                GHOST_CALL_GOTO((ghost_cu_sell1spmmv_cm_tmpl<cuFloatComplex,std::complex<float> >(mat,lhs,rhs,traits,(cusparse_sell1_spmmv_cm_kernel_t)cusparseCcsrmm)),err,ret);
            }
        }
    } else {
        INFO_LOG("Calling row-major cuSparse CRS SpMMV");
        if (mat->traits.datatype & GHOST_DT_DOUBLE) {
            if (mat->traits.datatype & GHOST_DT_REAL) {
                GHOST_CALL_GOTO((ghost_cu_sell1spmmv_rm_tmpl<double,double>(mat,lhs,rhs,traits,(cusparse_sell1_spmmv_rm_kernel_t)cusparseDcsrmm2)),err,ret);
            } else {
                GHOST_CALL_GOTO((ghost_cu_sell1spmmv_rm_tmpl<cuDoubleComplex,std::complex<double> >(mat,lhs,rhs,traits,(cusparse_sell1_spmmv_rm_kernel_t)cusparseZcsrmm2)),err,ret);
            }
        } else {
            if (mat->traits.datatype & GHOST_DT_REAL) {
                GHOST_CALL_GOTO((ghost_cu_sell1spmmv_rm_tmpl<float,float>(mat,lhs,rhs,traits,(cusparse_sell1_spmmv_rm_kernel_t)cusparseScsrmm2)),err,ret);
            } else {
                GHOST_CALL_GOTO((ghost_cu_sell1spmmv_rm_tmpl<cuFloatComplex,std::complex<float> >(mat,lhs,rhs,traits,(cusparse_sell1_spmmv_rm_kernel_t)cusparseCcsrmm2)),err,ret);
            }
        }
    }
    
    if (lhs != lhs_in) {
        ghost_densemat_init_densemat(lhs_in,lhs,0,0);
        ghost_densemat_destroy(lhs);
    }
    if (rhs != rhs_in) {
        ghost_densemat_destroy(rhs);
    }

    if (mat->traits.datatype & GHOST_DT_DOUBLE) {
        if (mat->traits.datatype & GHOST_DT_REAL) {
            GHOST_CALL_GOTO((ghost_cu_sell1spmv_augfunc_tmpl<double>(lhs_in,rhs_in,traits)),err,ret);
        } else {
            GHOST_CALL_GOTO((ghost_cu_sell1spmv_augfunc_tmpl<std::complex<double> >(lhs_in,rhs_in,traits)),err,ret);
        }
    } else {
        if (mat->traits.datatype & GHOST_DT_REAL) {
            GHOST_CALL_GOTO((ghost_cu_sell1spmv_augfunc_tmpl<float>(lhs_in,rhs_in,traits)),err,ret);
        } else {
            GHOST_CALL_GOTO((ghost_cu_sell1spmv_augfunc_tmpl<std::complex<float> >(lhs_in,rhs_in,traits)),err,ret);
        }
    }
    
    goto out;
err:
    ERROR_LOG("Error in SELL-1 SpMV!");
out:

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH);
    return ret;
}
