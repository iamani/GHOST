#include <complex>
    
template<typename T, int CFGK, int CFGM, int UNROLL>
static ghost_error_t ghost_tsmttsm__a_plain_cm_rm_tmpl(ghost_densemat_t *x, ghost_densemat_t *v, ghost_densemat_t *w, T *alpha, T *beta, int conjv)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH)
    ghost_error_t ret = GHOST_SUCCESS;

    int myrank=0;
    ghost_lidx_t n = v->traits.nrows;
    ghost_lidx_t i,j,k;
    ghost_lidx_t ldv, ldw, ldx;
    T * vval =  (T *)v->val, * wval = (T *)w->val, * xval = (T *)x->val;
    T mybeta = *beta; 

    if (v->context) {
        GHOST_CALL_GOTO(ghost_rank(&myrank,v->context->mpicomm),err,ret);
    }

    ldv = v->stride;
    ldw = w->stride;
    ldx = x->stride;
   
    // make sure that the initial x only gets added up once
    if (myrank) {
        mybeta = 0.;
    }
    
#if CFGK>1
#pragma simd
#endif
    for (k=0; k<CFGK; k++) {
        for (j=0; j<CFGM; j++) {
            xval[k*ldx+j] = mybeta*xval[k*ldx+j];
        }
    }
#pragma omp parallel private(j,k)
    {
        T *x_priv;
        ghost_malloc((void **)&x_priv,CFGM*CFGK*sizeof(T));
        memset(x_priv,0,CFGM*CFGK*sizeof(T));
        
        if (conjv) {
#pragma omp for schedule(runtime)
            for (i=0; i<n; i++) {
#if CFGK>1
#pragma simd
#pragma vector aligned
#pragma ivdep
#endif
                for (k=0; k<CFGK; k++) {
#pragma unroll_and_jam
                  for (j=0; j<CFGM; j++) {
                        x_priv[j*CFGK+k] += (*alpha)*std::conj(vval[i*ldv+j])*wval[i*ldw+k];
                    }
                }

            }
        } else {
#pragma omp for schedule(runtime)
            for (i=0; i<n; i++) {
#if CFGK>1
#pragma simd
#pragma vector aligned
#pragma ivdep
#endif
                for (k=0; k<CFGK; k++) {
#pragma unroll_and_jam
                  for (j=0; j<CFGM; j++) {
                        x_priv[j*CFGK+k] += (*alpha)*vval[i*ldv+j]*wval[i*ldw+k];
                    }
                }

            }
        }

#pragma omp critical
#if CFGK>1
#pragma simd
#pragma vector aligned
#pragma ivdep
#endif
        for (k=0; k<CFGK; k++) {
#pragma unroll_and_jam
            for (j=0; j<CFGM; j++) {
                xval[k*ldx+j] += x_priv[j*CFGK+k];
            }
        }

        free(x_priv);
    }
   
    goto out;
err:

out:
    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH)
    return ret;

}