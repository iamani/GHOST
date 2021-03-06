#include "ghost/densemat_cm.h"
#include "ghost/util.h"
#include "ghost/locality.h"
#include "ghost/sparsemat.h"
#include <complex>

#define COLMAJOR
#include "ghost/densemat_iter_macros.h"

    template<typename T>
static ghost_error ghost_densemat_cm_averagehalo_tmpl(ghost_densemat *vec, ghost_context *ctx)
{
#ifdef GHOST_HAVE_MPI
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_COMMUNICATION);
    ghost_error ret = GHOST_SUCCESS;

    int rank, nrank, i, d, acc_dues = 0;
    T *work = NULL, *curwork = NULL;
    MPI_Request *req = NULL;
    T *sum = NULL;
    int *nrankspresent = NULL;


    if (vec->traits.ncols > 1) {
        ERROR_LOG("Multi-vec case not yet implemented");
        ret = GHOST_ERR_NOT_IMPLEMENTED;
        goto err;
    }

    if (ctx == NULL) {
        WARNING_LOG("Trying to average the halos of a densemat which has no context!");
        goto out;
    }

    GHOST_CALL_GOTO(ghost_rank(&rank,ctx->mpicomm),err,ret);
    GHOST_CALL_GOTO(ghost_nrank(&nrank,ctx->mpicomm),err,ret);

    if(nrank > 1) { 
        for (i=0; i<nrank; i++) {
            acc_dues += ctx->dues[i];
        }

        GHOST_CALL_GOTO(ghost_malloc((void **)&work, acc_dues*sizeof(T)),err,ret);
        GHOST_CALL_GOTO(ghost_malloc((void **)&req, 2*nrank*sizeof(MPI_Request)),err,ret);

        for (i=0; i<2*nrank; i++) {
            req[i] = MPI_REQUEST_NULL;
        }

        /* if(ctx->perm_local && ctx->flags & GHOST_PERM_NO_DISTINCTION){
           for (int to_PE=0; to_PE<nrank; to_PE++) {
           T* packed_data;
           GHOST_CALL_GOTO(ghost_malloc((void **)&packed_data, ctx->wishes[to_PE]*vec->traits.ncols*vec->elSize),err,ret);

        //   printf("packed data\n");	
        for (int i=0; i<ctx->wishes[to_PE]; i++){
        for (int c=0; c<vec->traits.ncols; c++) {
        memcpy(&packed_data[(c*ctx->wishes[to_PE]+i)],DENSEMAT_VALPTR(vec,ctx->perm_local->colPerm[ctx->hput_pos[to_PE]+i],c),vec->elSize);
        //			printf("%f\n",packed_data[(c*ctx->wishes[to_PE]+i)]);

        }
        }
        //TODO blocking and delete packed_data        
        MPI_CALL_GOTO(MPI_Isend(packed_data,ctx->wishes[to_PE]*vec->traits.ncols,vec->mpidt,to_PE,rank,ctx->mpicomm,&req[to_PE]),err,ret);

        }

        } else {*/
        for (i=0; i<nrank; i++) {
            MPI_CALL_GOTO(MPI_Isend(&((T *)vec->val)[ctx->hput_pos[i]],ctx->wishes[i]*vec->traits.ncols,vec->mpidt,i,rank,ctx->mpicomm,&req[i]),err,ret);
        }
        // }

        curwork = work;
        for (i=0; i<nrank; i++) {
            MPI_CALL_GOTO(MPI_Irecv(curwork,ctx->dues[i]*vec->traits.ncols,vec->mpidt,i,i,ctx->mpicomm,&req[nrank+i]),err,ret);
            curwork += ctx->dues[i];
        }


        MPI_CALL_GOTO(MPI_Waitall(2*nrank,req,MPI_STATUSES_IGNORE),err,ret);



        GHOST_CALL_GOTO(ghost_malloc((void **)&sum, DM_NROWS(vec)*sizeof(T)),err,ret);
        GHOST_CALL_GOTO(ghost_malloc((void **)&nrankspresent, DM_NROWS(vec)*sizeof(int)),err,ret);

#pragma omp parallel for schedule(static) private(i)
        for (i=0; i<DM_NROWS(vec); i++) {	
            if(ctx->col_map->loc_perm_inv) {
                // This check is important since entsInCol has only lnrows (NO_DISTINCTION
                // might give seg fault else) the rest are halo anyway, not needed for local sums
                if(ctx->col_map->loc_perm_inv[i]< ctx->row_map->ldim[rank] ) { 
                    nrankspresent[i] = ctx->entsInCol[ctx->col_map->loc_perm_inv[i]]?1:0; //this has also to be permuted since it was
                    //for unpermuted columns that we calculate
                    if(nrankspresent[i]==1)
                        sum[i] = ((T *)vec->val)[i];
                    else
                        sum[i] = 0;
                } else {
                    nrankspresent[i] = 0;
                    sum[i]=0;
                } 	
            } else {
                nrankspresent[i] = ctx->entsInCol[i]?1:0;		
                if(nrankspresent[i]==1)
                    sum[i] = ((T *)vec->val)[i];
                else
                    sum[i] = 0;
            }
        }

        ghost_lidx currow;
        curwork = work;

        for (i=0; i<nrank; i++) {
            if(vec->map->loc_perm) {
#pragma omp parallel for schedule(static) private(d)
                for (d=0 ;d < ctx->dues[i]; d++) {
                    sum[ctx->col_map->loc_perm[ctx->duelist[i][d]]] += curwork[d];
                    nrankspresent[ctx->col_map->loc_perm[ctx->duelist[i][d]]]++; 
                }
            } else {
#pragma omp parallel for schedule(static) private(d)
                for (d=0 ;d < ctx->dues[i]; d++) {
                    sum[ctx->duelist[i][d]] += curwork[d];
                    nrankspresent[ctx->duelist[i][d]]++; 
                }
            }
            curwork += ctx->dues[i];
        }

#pragma omp parallel for schedule(static) private(currow)
        for (currow=0; currow<DM_NROWS(vec); currow++) {
            if(nrankspresent[currow]!=0) {
                ((T *)vec->val)[currow] = sum[currow]/(T)nrankspresent[currow];
            }
        }

        /* } else { 
           GHOST_CALL_GOTO(ghost_malloc((void **)&sum, ctx->row_map->ldim[rank]*sizeof(T)),err,ret);
           GHOST_CALL_GOTO(ghost_malloc((void **)&nrankspresent, ctx->row_map->ldim[rank]*sizeof(int)),err,ret);


           for (i=0; i<ctx->row_map->ldim[rank]; i++) {
           sum[i] = ((T *)vec->val)[i];
           nrankspresent[i] = ctx->entsInCol[i]?1:0;	
           }

           ghost_lidx currow;
           curwork = work;
           for (i=0; i<nrank; i++) {
           for (d=0 ;d < ctx->dues[i]; d++) {
           if(ctx->perm_local) {
           sum[ctx->perm_local->colPerm[ctx->duelist[i][d]]] +=  curwork[d];
           nrankspresent[ctx->perm_local->colPerm[ctx->duelist[i][d]]]++;
           } else {
           sum[ctx->duelist[i][d]] += curwork[d];
           nrankspresent[ctx->duelist[i][d]]++;
           }        
           }
           curwork += ctx->dues[i];
           }

           for (i=0; i<ctx->row_map->ldim[rank]; i++) {
           printf("<%d> ranks of row[%d] = %d\n",rank,i,nrankspresent[i]);
           }


           for (currow=0; currow<ctx->row_map->ldim[rank]; currow++) { 
           ((T *)vec->val)[currow] = sum[currow]/(T)nrankspresent[currow];
           }
           }
           */

        goto out;
err:

out:
        free(sum);
        free(nrankspresent);
        free(work);
        free(req);
        GHOST_FUNC_EXIT(GHOST_FUNCTYPE_COMMUNICATION);
        return ret;

} else {
    return ret;
}

#else
UNUSED(vec);
UNUSED(ctx);
ERROR_LOG("MPI is required!");
return GHOST_ERR_MPI;
#endif
}

ghost_error ghost_densemat_cm_averagehalo_selector(ghost_densemat *vec, ghost_context *ctx)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_COMMUNICATION);
    ghost_error ret = GHOST_SUCCESS;

    SELECT_TMPL_1DATATYPE(vec->traits.datatype,std::complex,ret,ghost_densemat_cm_averagehalo_tmpl,vec,ctx);

    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_COMMUNICATION);
    return ret;
}
