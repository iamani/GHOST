#include "ghost/config.h"
#include "ghost/types.h"
#include "ghost/densemat.h"
#include "ghost/util.h"
#include "ghost/math.h"
#include "ghost/tsmm.h"
#include "ghost/tsmm_var2_plain_gen.h"
#include "ghost/tsmm_var2_sse_gen.h"
#include "ghost/tsmm_var2_cu_gen.h"
#include "ghost/tsmm_plain_gen.h"
#include "ghost/tsmm_var1_plain_gen.h"
#include "ghost/tsmm_var2_avx_gen.h"
#include "ghost/tsmm_avx_gen.h"
#include "ghost/tsmm_sse_gen.h"
#include "ghost/tsmm_cu_gen.h"
#include "ghost/timing.h"
#include "ghost/machine.h"
#include "ghost/constants.h"

#include <unordered_map>

using namespace std;

// Hash function for unordered_map
namespace std
{
    template<> struct hash<ghost_tsmm_parameters>
    {
        typedef ghost_tsmm_parameters argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& a) const
        {
            return ghost_hash(a.dt,a.xcols,ghost_hash(a.vcols,a.impl,ghost_hash(a.xstor,a.wstor,ghost_hash(a.alignment,a.unroll,0))));
        }
    };
}

static bool operator==(const ghost_tsmm_parameters& a, const ghost_tsmm_parameters& b)
{
    return a.dt == b.dt && a.xcols == b.xcols && a.vcols == b.vcols && a.impl == b.impl && a.xstor == b.xstor && a.wstor == b.wstor && a.alignment == b.alignment && a.unroll == b.unroll;
}

static unordered_map<ghost_tsmm_parameters, ghost_tsmm_kernel> ghost_tsmm_kernels;

ghost_error ghost_tsmm_valid(ghost_densemat *x, ghost_densemat *v,  const char * transv, 
ghost_densemat *w, const char *transw, void *alpha, void *beta, int reduce, int printerror)
{
    if (x->traits.datatype != v->traits.datatype || x->traits.datatype != w->traits.datatype) {
        if (printerror) {
            ERROR_LOG("Different data types!");
        }
        return GHOST_ERR_INVALID_ARG;
    }
    if (x == v) {
        if (printerror) {
           ERROR_LOG("x must not be equal to v!");
        }
        return GHOST_ERR_INVALID_ARG;
    }
    if ((x->traits.flags & GHOST_DENSEMAT_SCATTERED) || (v->traits.flags & GHOST_DENSEMAT_SCATTERED) || (w->traits.flags & GHOST_DENSEMAT_SCATTERED)) {
        if (printerror) {
            ERROR_LOG("Scattered views not supported!");
        }
        return GHOST_ERR_INVALID_ARG;
    }
    if (reduce != GHOST_GEMM_NO_REDUCE) { 
        if (printerror) {
            ERROR_LOG("Only NO_REDUCE valid!");
        }
        return GHOST_ERR_INVALID_ARG;
    }
    if (strncasecmp(transv,"N",1)) {
        if (printerror) {
            ERROR_LOG("v must not be transposed!");
        }
        return GHOST_ERR_INVALID_ARG;
    }
    if (strncasecmp(transw,"N",1)) {
        if (printerror) {
            ERROR_LOG("w must not be transposed!");
        }
        return GHOST_ERR_INVALID_ARG;
    }

    UNUSED(alpha);
    UNUSED(beta);

    return GHOST_SUCCESS;
} 


ghost_error ghost_tsmm(ghost_densemat *x, ghost_densemat *v, ghost_densemat *w, void *alpha, void *beta)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH);
    ghost_error ret;

    if ((ret = ghost_tsmm_valid(x,v,"N",w,"N",alpha,beta,GHOST_GEMM_NO_REDUCE,1)) != GHOST_SUCCESS) {
        INFO_LOG("TSMM cannot be applied. Checking whether GEMM is fine!");
        if ((ret = ghost_gemm_valid(x,v,"N",w,"N",alpha,beta,GHOST_GEMM_NO_REDUCE,GHOST_GEMM_DEFAULT,1)) != GHOST_SUCCESS) {
            ERROR_LOG("GEMM cannot be applied!");
            return ret;
        } else {
            return ghost_gemm(x,v,"N",w,"N",alpha,beta,GHOST_GEMM_NO_REDUCE,GHOST_GEMM_NOT_SPECIAL);
        }
    }
    
    if (ghost_tsmm_kernels.empty()) {
#include "tsmm_var2_plain.def"
#include "tsmm_avx.def"
#include "tsmm_var2_avx.def"
#include "tsmm_var1_plain.def"
#include "tsmm_sse.def"
#include "tsmm_var2_sse.def"
#ifdef GHOST_HAVE_CUDA
#include "tsmm_cu.def"
#include "tsmm_var2_cu.def"
#endif
    }

    ghost_tsmm_parameters p;
    ghost_implementation opt_impl;
    ghost_alignment opt_align;
    int opt_unroll;
    ghost_tsmm_kernel kernel = NULL;
    
    // fix properties    
    p.xstor = x->traits.storage;
    p.wstor = w->traits.storage;

    // initial implementation
#ifdef GHOST_BUILD_MIC
    opt_impl = GHOST_IMPLEMENTATION_MIC;
#elif defined(GHOST_BUILD_AVX2)
    opt_impl = GHOST_IMPLEMENTATION_AVX2;
#elif defined(GHOST_BUILD_AVX)
    opt_impl = GHOST_IMPLEMENTATION_AVX;
#elif defined(GHOST_BUILD_SSE)
    opt_impl = GHOST_IMPLEMENTATION_SSE;
#else
    opt_impl = GHOST_IMPLEMENTATION_PLAIN;
#endif
    
    
    // alignment of large input data
    // the alignment of the result array does not matter because we can easily re-allocate it accordingly
    int al = ghost_machine_alignment();
    if (IS_ALIGNED(x->val,al) && IS_ALIGNED(v->val,al) && !((x->stride*x->elSize) % al) && !((v->stride*v->elSize) % al)) {
        opt_align = GHOST_ALIGNED;
    } else {
        opt_align = GHOST_UNALIGNED;
    }
    
    ghost_lidx try_xcols[2] = {x->traits.ncols,-1};
    ghost_lidx try_vcols[2] = {v->traits.ncols,-1};
    ghost_datatype try_dt[2] = {v->traits.datatype,GHOST_DT_ANY};

    if (w->traits.flags & GHOST_DENSEMAT_VIEW || v->traits.flags & GHOST_DENSEMAT_VIEW) {
        opt_unroll = 1;
    } else {
        opt_unroll = GHOST_MAX_ROWS_UNROLL;
    }
    
    int n_xcols = sizeof(try_xcols)/sizeof(ghost_lidx); 
    int n_vcols = sizeof(try_vcols)/sizeof(ghost_lidx); 
    int n_dt = sizeof(try_dt)/sizeof(ghost_datatype); 
    int pos_xcols, pos_vcols, pos_dt;
    bool optimal = true; // if we find a kernel with highest specialization grade (regardless unrolling), this remains true and no performance warning gets printed

    for (pos_xcols = 0; pos_xcols < n_xcols; pos_xcols++) {  
        for (pos_vcols = 0; pos_vcols < n_vcols; pos_vcols++) {  
            for (p.impl = opt_impl; (int)p.impl >= GHOST_IMPLEMENTATION_PLAIN; p.impl  = (ghost_implementation)((int)p.impl-1)) {
                for (p.alignment = opt_align; (int)p.alignment >= GHOST_UNALIGNED; p.alignment = (ghost_alignment)((int)p.alignment-1)) {
                    for (p.unroll = opt_unroll; p.unroll > 0; p.unroll /= 2) {
                        for (pos_dt = 0; pos_dt < n_dt; pos_dt++) {
                            p.xcols = try_xcols[pos_xcols];
                            p.vcols = try_vcols[pos_vcols];
                            p.dt = try_dt[pos_dt];
                            INFO_LOG("Try xcols=%s, vcols=%s, impl=%s, %s, unroll=%d, dt=%s",
                                    p.xcols==-1?"arbitrary":to_string((long long)p.xcols).c_str(),p.vcols==-1?"arbitrary":to_string((long long)p.vcols).c_str(),
                                    ghost_implementation_string(p.impl),p.alignment==GHOST_UNALIGNED?"unaligned":"aligned",p.unroll,ghost_datatype_string(p.dt));
                            kernel = ghost_tsmm_kernels[p];
                            if (kernel) {
                                goto end_of_loop;
                            }
                        }
                    }
                    optimal = false;
                }
            }
        }
    }

end_of_loop:

    if (kernel) {
        if (optimal) {
            INFO_LOG("Found kernel with highest specialization grade: dt=%d xcols=%d vcols=%d xstor=%d wstor=%d align=%d unroll=%d impl=%s",p.dt,p.xcols,p.vcols,p.xstor,p.wstor,p.alignment,p.unroll,ghost_implementation_string(p.impl));
        } else {
            PERFWARNING_LOG("Using potentially non-optimal kernel: dt=%d xcols=%d vcols=%d xstor=%d wstor=%d align=%d unroll=%d impl=%s",p.dt,p.xcols,p.vcols,p.xstor,p.wstor,p.alignment,p.unroll,ghost_implementation_string(p.impl));
        }

        ret = kernel(x,v,w,alpha,beta);
    } else {
        PERFWARNING_LOG("Could not find TSMM kernel. Fallback to GEMM");
        ret = ghost_gemm(x,v,"N",w,"N",alpha,beta,GHOST_GEMM_NO_REDUCE,GHOST_GEMM_NOT_SPECIAL);
    }

#ifdef GHOST_HAVE_INSTR_TIMING
    ghost_gemm_perf_args tsmm_perfargs;
    tsmm_perfargs.n = p.xcols;
    tsmm_perfargs.k = p.vcols;
    if (v->context) {
        tsmm_perfargs.m = v->context->gnrows;
    } else {
        tsmm_perfargs.m = v->traits.nrows;
    }
    tsmm_perfargs.dt = x->traits.datatype;
    tsmm_perfargs.betaiszero = ghost_iszero(beta,p.dt);
    tsmm_perfargs.alphaisone = ghost_isone(alpha,p.dt);
    ghost_timing_set_perfFunc(NULL,__ghost_functag,ghost_gemm_perf_GBs,(void *)&tsmm_perfargs,sizeof(tsmm_perfargs),"GB/s");
    ghost_timing_set_perfFunc(NULL,__ghost_functag,ghost_gemm_perf_GFs,(void *)&tsmm_perfargs,sizeof(tsmm_perfargs),"GF/s");
#endif

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH);

    return ret;
}


