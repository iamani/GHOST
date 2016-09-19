#include "ghost/types.h"
#include "ghost/omp.h"

#include "ghost/complex.h"
#include "ghost/util.h"
#include "ghost/densemat.h"
#include "ghost/math.h"
#include "ghost/log.h"
#include "ghost/machine.h"
#include "ghost/timing.h"

#include "ghost/sell_spmv_mic_gen.h"
#include "ghost/sell_spmv_avx2_gen.h"
#include "ghost/sell_spmv_avx_gen.h"
#include "ghost/sell_spmv_sse_gen.h"
#include "ghost/sell_spmv_plain_gen.h"

#include "ghost/sell_spmv_varblock_mic_gen.h"
#include "ghost/sell_spmv_varblock_avx_gen.h"
#include "ghost/sell_spmv_varblock_sse_gen.h"
#include "ghost/sell_spmv_varblock_plain_gen.h"

#include <unordered_map>

using namespace std;
    
    template<typename m_t, typename v_t, bool scatteredvecs> 
static ghost_error ghost_sell_spmv_plain_rm(ghost_densemat *lhs, 
        ghost_sparsemat *mat, ghost_densemat *rhs, 
        ghost_spmv_opts traits)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);
    PERFWARNING_LOG("In plain row-major SEL SpMV with scatteredvecs=%d, blocksz=%d",scatteredvecs,rhs->traits.ncols);
    v_t *local_dot_product = NULL, *partsums = NULL;
    ghost_lidx i,j,c,rcol,lcol,zcol,cidx;
    ghost_lidx v;
    int nthreads = 1;
    int ch = mat->traits.C;

    unsigned clsize;
    ghost_machine_cacheline_size(&clsize);
    int pad = (int) clsize/sizeof(v_t);

    v_t scale = 1., beta = 1.;
    v_t *shift = NULL;
    v_t delta = 0., eta = 0.;
    ghost_densemat *z = NULL;
    GHOST_SPMV_PARSE_TRAITS(traits,scale,beta,shift,local_dot_product,z,delta,eta,v_t,
            v_t);

    if (traits.flags & GHOST_SPMV_DOT) {
#pragma omp parallel
        nthreads = ghost_omp_nthread();

        GHOST_CALL_RETURN(ghost_malloc((void **)&partsums,
                    (3*lhs->traits.ncols+pad)*nthreads*sizeof(v_t))); 
        for (i=0; i<(3*lhs->traits.ncols+pad)*nthreads; i++) {
            partsums[i] = 0.;
        }
    }
#pragma omp parallel private(c,j,rcol,lcol,zcol,cidx,i,v) shared(partsums)
    {
        v_t **tmp;
        ghost_malloc((void **)&tmp,sizeof(v_t *)*ch);
        ghost_malloc((void **)&tmp[0],
                sizeof(v_t)*ch*rhs->traits.ncols);
        for (i=1; i<ch; i++) {
            tmp[i] = tmp[i-1] + rhs->traits.ncols;
        }
        int tid = ghost_omp_threadnum();
        v_t * rhsrow, *lhsrow, *zrow = NULL;
        v_t matrixval;

#pragma omp for schedule(runtime) 
        for (c=0; c<mat->nrowsPadded/ch; c++) { // loop over chunks

            for (i=0; i<ch; i++) {
                for (rcol=0; rcol<rhs->traits.ncols; rcol++) {
                    tmp[i][rcol] = (v_t)0;
                }
            }

            for (j=0; j<mat->chunkLen[c]; j++) { // loop inside chunk
                for (i=0; i<ch; i++) {
                    matrixval = (v_t)(((m_t*)(mat->val))
                            [mat->chunkStart[c]+j*ch+i]);
                    rhsrow = ((v_t *)rhs->val)+rhs->stride*mat->col[mat->chunkStart[c]+j*ch+i];
                    rcol = 0;
                    for (cidx = 0; cidx<rhs->traits.ncols; cidx++) {
                        tmp[i][cidx] +=  matrixval * rhsrow[rcol];
                        if (scatteredvecs) {
                            rcol = ghost_bitmap_next(rhs->colmask,rcol);
                        } else {
                            rcol++;
                        }
                    }

                }
            }

            for (i=0; (i<ch) && (c*ch+i < mat->nrows); i++) {
                lhsrow = ((v_t *)lhs->val)+lhs->stride*(c*ch+i);
                rhsrow = ((v_t *)rhs->val)+rhs->stride*(c*ch+i);
                if (z) {
                    zrow = ((v_t *)z->val)+z->stride*(c*ch+i);
                }
                rcol = 0;
                lcol = 0;
                zcol = 0;
                for (cidx = 0; cidx<lhs->traits.ncols; cidx++) {
                    if ((traits.flags & GHOST_SPMV_SHIFT) && shift) {
                        tmp[i][cidx] = tmp[i][cidx]-shift[0]*rhsrow[rcol];
                    }
                    if ((traits.flags & GHOST_SPMV_VSHIFT) && shift) {
                        tmp[i][cidx] = tmp[i][cidx]-shift[cidx]*rhsrow[rcol];
                    }
                    if (traits.flags & GHOST_SPMV_SCALE) {
                        tmp[i][cidx] = tmp[i][cidx]*scale;
                    }
                    if (traits.flags & GHOST_SPMV_AXPY) {
                        lhsrow[lcol] += tmp[i][cidx];
                    } else if (traits.flags & GHOST_SPMV_AXPBY) {
                        lhsrow[lcol] = beta*lhsrow[lcol] + tmp[i][cidx];
                    } else {
                        lhsrow[lcol] = tmp[i][cidx];
                    }
                    if (traits.flags & GHOST_SPMV_CHAIN_AXPBY) {
                        zrow[zcol] = delta*zrow[zcol] + eta*lhsrow[lcol];
                    }

                    if (traits.flags & GHOST_SPMV_DOT) {
                        partsums[((pad+3*lhs->traits.ncols)*tid)+3*cidx+0] += 
                            conjugate(&lhsrow[lcol])*lhsrow[rcol];
                        partsums[((pad+3*lhs->traits.ncols)*tid)+3*cidx+1] += 
                            conjugate(&rhsrow[rcol])*lhsrow[lcol];
                        partsums[((pad+3*lhs->traits.ncols)*tid)+3*cidx+2] += 
                            conjugate(&rhsrow[rcol])*rhsrow[rcol];
                    }
                    if (scatteredvecs) {
                        rcol = ghost_bitmap_next(rhs->colmask,rcol);
                        lcol = ghost_bitmap_next(lhs->colmask,lcol);
                        if (z) {
                            zcol = ghost_bitmap_next(z->colmask,zcol);
                        }
                    } else {
                        rcol++;
                        lcol++;
                        zcol++;
                    }
                }
            }

        }
        free(tmp[0]);
        free(tmp);
    }
    if (traits.flags & GHOST_SPMV_DOT) {
        if (!local_dot_product) {
            ERROR_LOG("The location of the local dot products is NULL!");
            return GHOST_ERR_INVALID_ARG;
        }
        for (v=0; v<lhs->traits.ncols; v++) {
            local_dot_product[v                       ] = 0.; 
            local_dot_product[v  +   lhs->traits.ncols] = 0.;
            local_dot_product[v  + 2*lhs->traits.ncols] = 0.;
            for (i=0; i<nthreads; i++) {
                local_dot_product[v                      ] += 
                    partsums[(pad+3*lhs->traits.ncols)*i + 3*v + 0];
                local_dot_product[v +   lhs->traits.ncols] += 
                    partsums[(pad+3*lhs->traits.ncols)*i + 3*v + 1];
                local_dot_product[v + 2*lhs->traits.ncols] += 
                    partsums[(pad+3*lhs->traits.ncols)*i + 3*v + 2];
            }
        }
        free(partsums);
    }

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);
    return GHOST_SUCCESS;
}
    
    template<typename m_t, typename v_t> 
static ghost_error ghost_sell_spmv_plain_rm_selector(ghost_densemat *lhs, 
        ghost_sparsemat *mat, 
        ghost_densemat *rhs, 
        ghost_spmv_opts traits)
{
    if ((lhs->traits.flags & GHOST_DENSEMAT_SCATTERED) || (rhs->traits.flags & GHOST_DENSEMAT_SCATTERED)) {
        return ghost_sell_spmv_plain_rm<m_t,v_t,true>(lhs,mat,rhs,traits);
    } else {
        return ghost_sell_spmv_plain_rm<m_t,v_t,false>(lhs,mat,rhs,traits);
    }
}

    template<typename m_t, typename v_t, bool scatteredvecs> 
static ghost_error ghost_sell_spmv_plain_cm(ghost_densemat *lhs, 
        ghost_sparsemat *mat, 
        ghost_densemat *rhs, 
        ghost_spmv_opts traits)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);
    v_t *local_dot_product = NULL, *partsums = NULL;
    ghost_lidx i,j,c;
    ghost_lidx v;
    int nthreads = 1;
    int ch = mat->traits.C;

    unsigned clsize;
    ghost_machine_cacheline_size(&clsize);
    int pad = (int) clsize/sizeof(v_t);

    v_t scale = 1., beta = 1.;
    v_t *shift = NULL;
    v_t delta = 0., eta = 0.;
    ghost_densemat *z = NULL;
    GHOST_SPMV_PARSE_TRAITS(traits,scale,beta,shift,local_dot_product,z,delta,eta,v_t,
            v_t);

    if (traits.flags & GHOST_SPMV_DOT) {
#pragma omp parallel
        nthreads = ghost_omp_nthread();

        GHOST_CALL_RETURN(ghost_malloc((void **)&partsums,
                    (3*lhs->traits.ncols+pad)*nthreads*sizeof(v_t))); 
        for (i=0; i<(3*lhs->traits.ncols+pad)*nthreads; i++) {
            partsums[i] = 0.;
        }
    }
#pragma omp parallel private(c,j,i,v) shared(partsums)
    {
        v_t *tmp = NULL;
        ghost_malloc((void **)&tmp,ch*sizeof(v_t));
        v_t *lhsv = NULL;
        v_t *rhsv = NULL;
        v_t *zv = NULL;
        int tid = ghost_omp_threadnum();


#pragma omp for schedule(runtime) 
        for (c=0; c<mat->nrowsPadded/ch; c++) { // loop over chunks
                    
            ghost_lidx rcol = 0, lcol = 0, zcol = 0;

            for (v=0; v<lhs->traits.ncols; v++)
            {

                rhsv = (v_t *)rhs->val+rcol*rhs->stride;
                lhsv = (v_t *)lhs->val+lcol*lhs->stride;
                if (z) {
                    zv = (v_t *)z->val+zcol*z->stride;
                }

                for (i=0; i<ch; i++) {
                    tmp[i] = (v_t)0;
                }

                for (j=0; j<mat->chunkLen[c]; j++) { // loop inside chunk
                    for (i=0; i<ch; i++) {
                        tmp[i] += (v_t)(((m_t*)(mat->val))[mat->chunkStart[c]+
                                j*ch+i]) * rhsv[mat->col[mat->chunkStart[c]+
                            j*ch+i]];
                    }
                }
                for (i=0; i<ch; i++) {
                    if (c*ch+i < mat->nrows) {
                        if ((traits.flags & GHOST_SPMV_SHIFT) && shift) {
                            tmp[i] = tmp[i]-shift[0]*rhsv[c*ch+i];
                        }
                        if ((traits.flags & GHOST_SPMV_VSHIFT) && shift) {
                            tmp[i] = tmp[i]-shift[v]*rhsv[c*ch+i];
                        }
                        if (traits.flags & GHOST_SPMV_SCALE) {
                            tmp[i] = tmp[i]*scale;
                        }
                        if (traits.flags & GHOST_SPMV_AXPY) {
                            lhsv[c*ch+i] += tmp[i];
                        } else if (traits.flags & GHOST_SPMV_AXPBY) {
                            lhsv[c*ch+i] = beta*lhsv[c*ch+i] + tmp[i];
                        } else {
                            lhsv[c*ch+i] = tmp[i];
                        }
                        if (traits.flags & GHOST_SPMV_CHAIN_AXPBY) {
                            zv[c*ch+i] = delta*zv[c*ch+i] + eta*lhsv[c*ch+i];
                        }

                        if (traits.flags & GHOST_SPMV_DOT) {
                            partsums[((pad+3*lhs->traits.ncols)*tid)+3*v+0] += 
                                conjugate(&lhsv[c*ch+i])*
                                lhsv[c*ch+i];
                            partsums[((pad+3*lhs->traits.ncols)*tid)+3*v+1] += 
                                conjugate(&rhsv[c*ch+i])*
                                lhsv[c*ch+i];
                            partsums[((pad+3*lhs->traits.ncols)*tid)+3*v+2] += 
                                conjugate(&rhsv[c*ch+i])*
                                rhsv[c*ch+i];
                        }
                    }

                }

                if (scatteredvecs) {
                    rcol = ghost_bitmap_next(rhs->colmask,rcol);
                    lcol = ghost_bitmap_next(lhs->colmask,lcol);
                    if (z) {
                        zcol = ghost_bitmap_next(z->colmask,zcol);
                    }
                } else {
                    rcol++;
                    lcol++;
                    zcol++;
                }
            }
        }
        free(tmp);
    }
    if (traits.flags & GHOST_SPMV_DOT) {
        if (!local_dot_product) {
            ERROR_LOG("The location of the local dot products is NULL!");
            return GHOST_ERR_INVALID_ARG;
        }
        for (v=0; v<lhs->traits.ncols; v++) {
            local_dot_product[v                       ] = 0.; 
            local_dot_product[v  +   lhs->traits.ncols] = 0.;
            local_dot_product[v  + 2*lhs->traits.ncols] = 0.;
            for (i=0; i<nthreads; i++) {
                local_dot_product[v                      ] += partsums[(pad+
                        3*lhs->traits.ncols)*i + 3*v + 0];
                local_dot_product[v +   lhs->traits.ncols] += partsums[(pad+
                        3*lhs->traits.ncols)*i + 3*v + 1];
                local_dot_product[v + 2*lhs->traits.ncols] += partsums[(pad+
                        3*lhs->traits.ncols)*i + 3*v + 2];
            }
        }
        free(partsums);
    }

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH|GHOST_FUNCTYPE_KERNEL);
    return GHOST_SUCCESS;
}
    
    template<typename m_t, typename v_t> 
static ghost_error ghost_sell_spmv_plain_cm_selector(ghost_densemat *lhs, 
        ghost_sparsemat *mat, 
        ghost_densemat *rhs, 
        ghost_spmv_opts traits)
{
    if ((lhs->traits.flags & GHOST_DENSEMAT_SCATTERED) || (rhs->traits.flags & GHOST_DENSEMAT_SCATTERED)) {
        return ghost_sell_spmv_plain_cm<m_t,v_t,true>(lhs,mat,rhs,traits);
    } else {
        return ghost_sell_spmv_plain_cm<m_t,v_t,false>(lhs,mat,rhs,traits);
    }
}


// Hash function for unordered_map
namespace std
{
    template<> struct hash<ghost_sellspmv_parameters>
    {
        typedef ghost_sellspmv_parameters argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& a) const
        {
            return ghost_hash(ghost_hash(a.mdt,a.blocksz,a.storage),
                    ghost_hash(a.vdt,a.impl,a.chunkheight),a.alignment);
        }
    };
}

static bool operator==(const ghost_sellspmv_parameters& a, const ghost_sellspmv_parameters& b)
{
    return a.mdt == b.mdt && a.blocksz == b.blocksz && a.storage == b.storage && 
           a.vdt == b.vdt && a.impl == b.impl && a.chunkheight == b.chunkheight &&
           a.alignment == b.alignment;
}

static unordered_map<ghost_sellspmv_parameters, ghost_spmv_kernel> 
ghost_sellspmv_kernels = unordered_map<ghost_sellspmv_parameters,ghost_spmv_kernel>();

extern "C" ghost_error ghost_sell_spmv_selector(ghost_densemat *lhs, 
        ghost_sparsemat *mat, 
        ghost_densemat *rhs, 
        ghost_spmv_opts traits)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH);
    if (rhs->traits.storage != lhs->traits.storage) {
        ERROR_LOG("Different storage layout for in- and output densemats!");
        return GHOST_ERR_INVALID_ARG;
    }
    if (rhs->traits.ncols != lhs->traits.ncols) {
        ERROR_LOG("The number of columns for the densemats does not match!");
        return GHOST_ERR_INVALID_ARG;
    }
    if (!(mat->context->flags & GHOST_PERM_NO_DISTINCTION) && rhs->traits.nrows != lhs->traits.nrows) { //if No distinction is set it can be rectangular matrix
        ERROR_LOG("The number of rows for the densemats does not match!");
        return GHOST_ERR_INVALID_ARG;
    }
    if (!(mat->context->flags & GHOST_PERM_NO_DISTINCTION) && lhs->traits.nrows != mat->nrows) {
        ERROR_LOG("Different number of rows for the densemats and matrix!");
        return GHOST_ERR_INVALID_ARG;
    }
    if (((rhs->traits.storage == GHOST_DENSEMAT_COLMAJOR) && 
                (rhs->traits.nrowsorig != rhs->traits.nrows)) || 
            ((lhs->traits.storage == GHOST_DENSEMAT_COLMAJOR) && 
            (lhs->traits.nrowsorig != lhs->traits.nrows))) {
        ERROR_LOG("Col-major densemats with masked out rows currently not "
                "supported!");
        return GHOST_ERR_NOT_IMPLEMENTED;
    }


    // if map is empty include generated code for map construction
    if (ghost_sellspmv_kernels.empty()) {
#include "sell_spmv_mic.def"
#include "sell_spmv_avx2.def"
#include "sell_spmv_avx.def"
#include "sell_spmv_sse.def"
#include "sell_spmv_plain.def"
#include "sell_spmv_varblock_mic.def"
#include "sell_spmv_varblock_avx.def"
#include "sell_spmv_varblock_sse.def"
#include "sell_spmv_varblock_plain.def"
    }

    ghost_error ret = GHOST_SUCCESS;

    ghost_spmv_kernel kernel = NULL;
    ghost_sellspmv_parameters p;
    ghost_implementation opt_impl;
    ghost_alignment opt_align;
    
    
    p.vdt = rhs->traits.datatype;
    p.mdt = mat->traits.datatype;
    p.storage = rhs->traits.storage;
    if (p.storage == GHOST_DENSEMAT_ROWMAJOR && rhs->stride == 1 && lhs->stride == 1) {
        INFO_LOG("Chose col-major kernel for row-major densemat with 1 column");
        p.storage = GHOST_DENSEMAT_COLMAJOR;
    }

    if ((lhs->traits.flags & GHOST_DENSEMAT_SCATTERED) || 
            (rhs->traits.flags & GHOST_DENSEMAT_SCATTERED)) {
        PERFWARNING_LOG("Use plain implementation for scattered views");
        opt_impl = GHOST_IMPLEMENTATION_PLAIN;
    } else {
        if (rhs->stride > 1 && rhs->traits.storage == GHOST_DENSEMAT_ROWMAJOR) {
            opt_impl = ghost_get_best_implementation_for_bytesize(rhs->traits.ncols*rhs->elSize);
            if (opt_impl == GHOST_IMPLEMENTATION_PLAIN) {
                // this branch is taken for odd numbers
                // choose a version with remainder loops in this case!
                opt_impl = ghost_get_best_implementation_for_bytesize(PAD(rhs->traits.ncols*rhs->elSize,ghost_machine_simd_width()));
            }
        } else {
            opt_impl = ghost_get_best_implementation_for_bytesize(mat->traits.C*mat->elSize);
        }
    }
    
    int try_chunkheight[2] = {mat->traits.C,-1}; 
    int try_blocksz[2] = {rhs->traits.ncols,-1}; 

    int n_chunkheight = sizeof(try_chunkheight)/sizeof(int);
    int n_blocksz = sizeof(try_blocksz)/sizeof(int);
    int pos_chunkheight, pos_blocksz;

    bool optimal = true;

    for (pos_chunkheight = 0; pos_chunkheight < n_chunkheight; pos_chunkheight++) {  
        for (pos_blocksz = 0; pos_blocksz < n_blocksz; pos_blocksz++) {  
            for (p.impl = opt_impl; (int)p.impl >= GHOST_IMPLEMENTATION_PLAIN; p.impl  = (ghost_implementation)((int)p.impl-1)) {
                /*if (p.impl == GHOST_IMPLEMENTATION_SSE && p.storage == GHOST_DENSEMAT_ROWMAJOR && try_blocksz[pos_blocksz] % 2) {
                    PERFWARNING_LOG("Remainder loops not yet implemented for SSE, fallback to plain");
                    p.impl  = (ghost_implementation)((int)p.impl-1);
                }*/

                int al = ghost_implementation_alignment(p.impl);
                if (IS_ALIGNED(lhs->val,al) && IS_ALIGNED(rhs->val,al) && ((lhs->traits.ncols == 1 && lhs->stride == 1) || (!((lhs->stride*lhs->elSize) % al) && !((rhs->stride*rhs->elSize) % al)))) {
                    opt_align = GHOST_ALIGNED;
                } else {
                    if (!IS_ALIGNED(lhs->val,al)) {
                        PERFWARNING_LOG("Using unaligned kernel because base address of result vector is not aligned");
                    }
                    if (!IS_ALIGNED(rhs->val,al)) {
                        PERFWARNING_LOG("Using unaligned kernel because base address of input vector is not aligned");
                    }
                    if (lhs->stride*lhs->elSize % al) {
                        PERFWARNING_LOG("Using unaligned kernel because stride of result vector does not yield aligned addresses");
                    }
                    if (rhs->stride*lhs->elSize % al) {
                        PERFWARNING_LOG("Using unaligned kernel because stride of input vector does not yield aligned addresses");
                    }
                    opt_align = GHOST_UNALIGNED;
                }

                for (p.alignment = opt_align; (int)p.alignment >= GHOST_UNALIGNED; p.alignment = (ghost_alignment)((int)p.alignment-1)) {
                    p.chunkheight = try_chunkheight[pos_chunkheight];
                    p.blocksz = try_blocksz[pos_blocksz];

                    DEBUG_LOG(1,"Try chunkheight=%s, blocksz=%s, impl=%s, %s",
                            p.chunkheight==-1?"arbitrary":std::to_string((long long)p.chunkheight).c_str(),
                            p.blocksz==-1?"arbitrary":std::to_string((long long)p.blocksz).c_str(),
                            ghost_implementation_string(p.impl),p.alignment==GHOST_UNALIGNED?"unaligned":"aligned");
                    kernel = ghost_sellspmv_kernels[p];
                    if (kernel) {
                        goto end_of_loop;
                    }
                    optimal = false;
                }
            }
        }
    }

end_of_loop:


    if (kernel) {
        if (optimal) {
            INFO_LOG("Found kernel with highest specialization grade: C=%d blocksz=%d align=%d impl=%s",p.chunkheight,p.blocksz,p.alignment,ghost_implementation_string(p.impl));
        } else {
            PERFWARNING_LOG("Using potentially non-optimal kernel: C=%d blocksz=%d align=%d impl=%s",p.chunkheight,p.blocksz,p.alignment,ghost_implementation_string(p.impl));
        }
        ret = kernel(lhs,mat,rhs,traits);
    } else { // execute plain kernel as fallback
        PERFWARNING_LOG("Execute fallback SELL SpMV kernel which is potentially slow!");
        if (lhs->traits.storage == GHOST_DENSEMAT_COLMAJOR) {
            SELECT_TMPL_2DATATYPES(mat->traits.datatype,
                    rhs->traits.datatype,ghost_complex,ret,
                    ghost_sell_spmv_plain_cm_selector,lhs,mat,rhs,traits);

        } else {
            SELECT_TMPL_2DATATYPES(mat->traits.datatype,
                    rhs->traits.datatype,ghost_complex,ret,
                    ghost_sell_spmv_plain_rm_selector,lhs,mat,rhs,traits);
        }
    } 

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH);
    return ret;
}

