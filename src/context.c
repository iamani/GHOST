#include "ghost/config.h"
#include "ghost/types.h"
#include "ghost/util.h"
#include "ghost/context.h"
#include "ghost/constants.h"
#include "ghost/affinity.h"
#include "ghost/crs.h"
#include "ghost/io.h"
#include "ghost/log.h"


ghost_error_t ghost_createContext(ghost_context_t **context, ghost_midx_t gnrows, ghost_midx_t gncols, ghost_context_flags_t context_flags, void *matrixSource, ghost_mpi_comm_t comm, double weight) 
{
    int nranks;
    int me;
    int i;
    GHOST_CALL_RETURN(ghost_getNumberOfRanks((*context)->mpicomm,&nranks));
    GHOST_CALL_RETURN(ghost_getRank((*context)->mpicomm,&me));

    (*context) = (ghost_context_t *)ghost_malloc(sizeof(ghost_context_t));
    (*context)->flags = context_flags;
    (*context)->rowPerm = NULL;
    (*context)->invRowPerm = NULL;
    (*context)->mpicomm = comm;
    (*context)->wishes   = NULL;
    (*context)->dues     = NULL;
    (*context)->hput_pos = NULL;
    
    (*context)->wishlist = (ghost_midx_t**) ghost_malloc(nranks*sizeof(ghost_midx_t *)); 
    (*context)->duelist  = (ghost_midx_t**) ghost_malloc(nranks*sizeof(ghost_midx_t *));
    for (i=0; i<nranks; i++){
        (*context)->wishlist[i] = NULL;
        (*context)->duelist[i] = NULL;
    }

    if (!((*context)->flags & GHOST_CONTEXT_DIST_NZ)) {
        (*context)->flags |= GHOST_CONTEXT_DIST_ROWS;
    }

    if ((gnrows < 1) || (gncols < 1)) {
        ghost_matfile_header_t fileheader;
        ghost_readMatFileHeader((char *)matrixSource,&fileheader);
#if !(GHOST_HAVE_LONGIDX)
        if ((fileheader.nrows >= (int64_t)INT_MAX) || (fileheader.ncols >= (int64_t)INT_MAX)) {
            ABORT("The matrix is too big for 32-bit indices. Recompile with LONGIDX!");
        }
#endif
        if (gnrows < 1)
            (*context)->gnrows = (ghost_midx_t)fileheader.nrows;
        if (gncols < 1)
            (*context)->gncols = (ghost_midx_t)fileheader.ncols;

    } else {
#if !(GHOST_HAVE_LONGIDX)
        if ((gnrows >= (int64_t)INT_MAX) || (gncols >= (int64_t)INT_MAX)) {
            ABORT("The matrix is too big for 32-bit indices. Recompile with LONGIDX!");
        }
#endif
        (*context)->gnrows = (ghost_midx_t)gnrows;
        (*context)->gncols = (ghost_midx_t)gncols;
    }
    DEBUG_LOG(1,"Creating context with dimension %"PRmatIDX"x%"PRmatIDX,(*context)->gnrows,(*context)->gncols);

#ifdef GHOST_HAVE_MPI
    if (!((*context)->flags & (GHOST_CONTEXT_DISTRIBUTED|GHOST_CONTEXT_REDUNDANT))) { // neither flag is set
        DEBUG_LOG(1,"Context is set to be distributed");
        (*context)->flags |= GHOST_CONTEXT_DISTRIBUTED;
    }
#else
    if ((*context)->flags & GHOST_CONTEXT_DISTRIBUTED) {
        WARNING_LOG("Creating a distributed matrix without MPI is not possible. Forcing redundant.");
        (*context)->flags &= ~GHOST_CONTEXT_DISTRIBUTED;
        (*context)->flags |= GHOST_CONTEXT_REDUNDANT;
    } else if (!((*context)->flags & GHOST_CONTEXT_REDUNDANT)) {
        DEBUG_LOG(1,"Context is set to be redundant");
        (*context)->flags |= GHOST_CONTEXT_REDUNDANT;
    }
#endif

 /*   (*context)->spmvsolvers = (ghost_spmvsolver_t *)ghost_malloc(sizeof(ghost_spmvsolver_t)*GHOST_NUM_MODES);
    for (i=0; i<GHOST_NUM_MODES; i++) (*context)->spmvsolvers[i] = NULL;
#ifdef GHOST_HAVE_MPI
    (*context)->spmvsolvers[GHOST_SPMVM_MODE_VECTORMODE_IDX] = &hybrid_kernel_I;
    (*context)->spmvsolvers[GHOST_SPMVM_MODE_GOODFAITH_IDX] = &hybrid_kernel_II;
    (*context)->spmvsolvers[GHOST_SPMVM_MODE_TASKMODE_IDX] = &hybrid_kernel_III;
#else
    (*context)->spmvsolvers[GHOST_SPMVM_MODE_NOMPI_IDX] = &ghost_solver_nompi;
#endif
*/
    (*context)->lnEnts   = (ghost_mnnz_t*)       ghost_malloc( nranks*sizeof(ghost_mnnz_t)); 
    (*context)->lfEnt    = (ghost_mnnz_t*)       ghost_malloc( nranks*sizeof(ghost_mnnz_t)); 
    (*context)->lnrows   = (ghost_midx_t*)       ghost_malloc( nranks*sizeof(ghost_midx_t)); 
    (*context)->lfRow    = (ghost_midx_t*)       ghost_malloc( nranks*sizeof(ghost_midx_t));

#ifdef GHOST_HAVE_MPI
    ghost_midx_t row;
    if ((*context)->flags & GHOST_CONTEXT_DISTRIBUTED) {
        (*context)->halo_elements = -1;


        if ((*context)->flags & GHOST_CONTEXT_DIST_NZ)
        { // read rpt and fill lfrow, lnrows, lfent, lnents
            ghost_midx_t *rpt = NULL;
            ghost_mnnz_t gnnz;

            if (me == 0) {
                rpt = (ghost_mnnz_t *)ghost_malloc(sizeof(ghost_mnnz_t)*((*context)->gnrows+1));
#pragma omp parallel for schedule(runtime)
                for( row = 0; row < (*context)->gnrows+1; row++ ) {
                    rpt[row] = 0;
                }
                if ((*context)->flags & GHOST_CONTEXT_ROWS_FROM_FILE) {
                    ghost_readRpt(rpt,(char *)matrixSource,0,(*context)->gnrows+1);  // read rpt
                } else if ((*context)->flags & GHOST_CONTEXT_ROWS_FROM_FUNC) {
                    ghost_spmFromRowFunc_t func = (ghost_spmFromRowFunc_t)matrixSource;
                    char *tmpval = ghost_malloc((*context)->gncols*sizeof(complex double));
                    ghost_midx_t *tmpcol = ghost_malloc((*context)->gncols*sizeof(ghost_midx_t));
                    rpt[0] = 0;
                    ghost_midx_t rowlen;
                    ghost_midx_t i;
                    for(row = 0; row < (*context)->gnrows; row++) {
                        func(row,&rowlen,tmpcol,tmpval);
                        rpt[row+1] = rpt[i]+rowlen;
                    }
                    free(tmpval);
                    free(tmpcol);
                } else {
                    ERROR_LOG("If distribution by nnz a matrix source has to be given!");
                    return GHOST_ERR_INVALID_ARG;
                }
                    
                    
                (*context)->rpt = rpt;

                gnnz = rpt[(*context)->gnrows];
                ghost_mnnz_t target_nnz;
                target_nnz = (gnnz/nranks)+1; /* sonst bleiben welche uebrig! */

                (*context)->lfRow[0]  = 0;
                (*context)->lfEnt[0] = 0;
                int j = 1;

                for (row=0;row<(*context)->gnrows;row++){
                    if (rpt[row] >= j*target_nnz){
                        (*context)->lfRow[j] = row;
                        (*context)->lfEnt[j] = rpt[row];
                        j = j+1;
                    }
                }
                for (i=0; i<nranks-1; i++){
                    (*context)->lnrows[i] = (*context)->lfRow[i+1] - (*context)->lfRow[i] ;
                    (*context)->lnEnts[i] = (*context)->lfEnt[i+1] - (*context)->lfEnt[i] ;
                }

                (*context)->lnrows[nranks-1] = (*context)->gnrows - (*context)->lfRow[nranks-1] ;
                (*context)->lnEnts[nranks-1] = gnnz - (*context)->lfEnt[nranks-1];

                //fclose(filed);
            }
            int err;
            MPI_CALL(MPI_Bcast((*context)->lfRow,  nranks, ghost_mpi_dt_midx, 0, (*context)->mpicomm),err);
            if (err != MPI_SUCCESS) {
                free(rpt); rpt = NULL;
                free((*context)->lnEnts); (*context)->lnEnts = NULL;
                free((*context)->lfEnt); (*context)->lfEnt = NULL;
                free((*context)->lnrows); (*context)->lnrows = NULL;
                free((*context)->lfRow); (*context)->lfRow = NULL;
                free(*context); *context = NULL;
                return GHOST_ERR_MPI;
            }
            MPI_CALL(MPI_Bcast((*context)->lfEnt,  nranks, ghost_mpi_dt_midx, 0, (*context)->mpicomm),err);
            if (err != MPI_SUCCESS) {
                free(rpt); rpt = NULL;
                free((*context)->lnEnts); (*context)->lnEnts = NULL;
                free((*context)->lfEnt); (*context)->lfEnt = NULL;
                free((*context)->lnrows); (*context)->lnrows = NULL;
                free((*context)->lfRow); (*context)->lfRow = NULL;
                free(*context); *context = NULL;
                return GHOST_ERR_MPI;
            }
            MPI_CALL(MPI_Bcast((*context)->lnrows, nranks, ghost_mpi_dt_midx, 0, (*context)->mpicomm),err);
            if (err != MPI_SUCCESS) {
                free(rpt); rpt = NULL;
                free((*context)->lnEnts); (*context)->lnEnts = NULL;
                free((*context)->lfEnt); (*context)->lfEnt = NULL;
                free((*context)->lnrows); (*context)->lnrows = NULL;
                free((*context)->lfRow); (*context)->lfRow = NULL;
                free(*context); *context = NULL;
                return GHOST_ERR_MPI;
            }
            MPI_CALL(MPI_Bcast((*context)->lnEnts, nranks, ghost_mpi_dt_midx, 0, (*context)->mpicomm),err);
            if (err != MPI_SUCCESS) {
                free(rpt); rpt = NULL;
                free((*context)->lnEnts); (*context)->lnEnts = NULL;
                free((*context)->lfEnt); (*context)->lfEnt = NULL;
                free((*context)->lnrows); (*context)->lnrows = NULL;
                free((*context)->lfRow); (*context)->lfRow = NULL;
                free(*context); *context = NULL;
                return GHOST_ERR_MPI;
            }


        } else
        { // don't read rpt, only fill lfrow, lnrows, rest will be done after some matrix from*() function
            UNUSED(matrixSource);
            int err;
            double allweights;
            MPI_CALL(MPI_Allreduce(&weight,&allweights,1,MPI_DOUBLE,MPI_SUM,(*context)->mpicomm),err)
            if (err != MPI_SUCCESS) {
                free((*context)->lnEnts); (*context)->lnEnts = NULL;
                free((*context)->lfEnt); (*context)->lfEnt = NULL;
                free((*context)->lnrows); (*context)->lnrows = NULL;
                free((*context)->lfRow); (*context)->lfRow = NULL;
                free(*context); *context = NULL;
                return GHOST_ERR_MPI;
            }

            ghost_midx_t my_target_rows = (ghost_midx_t)((*context)->gnrows*((double)weight/(double)allweights));
            ghost_midx_t *target_rows = (ghost_midx_t *)ghost_malloc(nranks*sizeof(ghost_midx_t));

            MPI_CALL(MPI_Allgather(&my_target_rows,1,ghost_mpi_dt_midx,target_rows,1,ghost_mpi_dt_midx,(*context)->mpicomm),err);
            if (err != MPI_SUCCESS) {
                free((*context)->lnEnts); (*context)->lnEnts = NULL;
                free((*context)->lfEnt); (*context)->lfEnt = NULL;
                free((*context)->lnrows); (*context)->lnrows = NULL;
                free((*context)->lfRow); (*context)->lfRow = NULL;
                free(*context); *context = NULL;
                free(target_rows); target_rows = NULL;
                return GHOST_ERR_MPI;
            }
                       
            (*context)->rpt = NULL;
            (*context)->lfRow[0] = 0;

            for (i=1; i<nranks; i++){
                (*context)->lfRow[i] = (*context)->lfRow[i-1]+target_rows[i-1];
            }
            for (i=0; i<nranks-1; i++){
                (*context)->lnrows[i] = (*context)->lfRow[i+1] - (*context)->lfRow[i] ;
            }
            (*context)->lnrows[nranks-1] = (*context)->gnrows - (*context)->lfRow[nranks-1] ;
            MPI_CALL(MPI_Bcast((*context)->lfRow,  nranks, ghost_mpi_dt_midx, 0, (*context)->mpicomm),err);
            if (err != MPI_SUCCESS) {
                free((*context)->lnEnts); (*context)->lnEnts = NULL;
                free((*context)->lfEnt); (*context)->lfEnt = NULL;
                free((*context)->lnrows); (*context)->lnrows = NULL;
                free((*context)->lfRow); (*context)->lfRow = NULL;
                free(*context); *context = NULL;
                free(target_rows); target_rows = NULL;
                return GHOST_ERR_MPI;
            }
            MPI_CALL(MPI_Bcast((*context)->lnrows, nranks, ghost_mpi_dt_midx, 0, (*context)->mpicomm),err);
            if (err != MPI_SUCCESS) {
                free((*context)->lnEnts); (*context)->lnEnts = NULL;
                free((*context)->lfEnt); (*context)->lfEnt = NULL;
                free((*context)->lnrows); (*context)->lnrows = NULL;
                free((*context)->lfRow); (*context)->lfRow = NULL;
                free(*context); *context = NULL;
                free(target_rows); target_rows = NULL;
                return GHOST_ERR_MPI;
            }
            (*context)->lnEnts[0] = -1;
            (*context)->lfEnt[0] = -1;

            free(target_rows);
            DEBUG_LOG("done");
        }

    } else {
        (*context)->lnrows[0] = (*context)->gnrows;
        (*context)->lfRow[0] = 0;
        (*context)->lnEnts[0] = 0;
        (*context)->lfEnt[0] = 0;
    }

#else
    UNUSED(weight);
    (*context)->lnrows[0] = (*context)->gnrows;
    (*context)->lfRow[0] = 0;
    (*context)->lnEnts[0] = 0;
    (*context)->lfEnt[0] = 0;
#endif

    DEBUG_LOG(1,"Context created successfully");
    return GHOST_SUCCESS;
}

ghost_error_t ghost_printContextInfo(ghost_context_t *context)
{
    int nranks;
    int myrank;
    GHOST_CALL_RETURN(ghost_getNumberOfRanks(context->mpicomm,&nranks));
    GHOST_CALL_RETURN(ghost_getRank(context->mpicomm,&myrank));

    if (myrank == 0) {
        char *contextType = "";
        if (context->flags & GHOST_CONTEXT_DISTRIBUTED)
            contextType = "Distributed";
        else if (context->flags & GHOST_CONTEXT_REDUNDANT)
            contextType = "Redundant";


        ghost_printHeader("Context");
        ghost_printLine("MPI processes",NULL,"%d",nranks);
        ghost_printLine("Number of rows",NULL,"%"PRmatIDX,context->gnrows);
        ghost_printLine("Type",NULL,"%s",contextType);
        ghost_printLine("Work distribution scheme",NULL,"%s",ghost_workdistName(context->flags));
        ghost_printFooter();
    }
    return GHOST_SUCCESS;

}

void ghost_destroyContext(ghost_context_t *context)
{
    DEBUG_LOG(1,"Freeing context");
    if (context != NULL) {
        free(context->rowPerm);
        free(context->invRowPerm);

        free(context);
    }
    DEBUG_LOG(1,"Context freed successfully");
}

ghost_error_t ghost_globalIndex(ghost_context_t *ctx, ghost_midx_t lidx, ghost_midx_t *gidx)
{
    int rank;
    GHOST_CALL_RETURN(ghost_getRank(ctx->mpicomm,&rank));

    if (ctx->flags & GHOST_CONTEXT_DISTRIBUTED) {
        *gidx = ctx->lfRow[rank] + lidx;
    } else {
        *gidx = lidx;
    }

    return GHOST_SUCCESS;    
}
ghost_error_t ghost_setupCommunication(ghost_context_t *ctx, ghost_midx_t *col)
{

    ghost_error_t ret = GHOST_SUCCESS;
    ghost_mnnz_t j;
    ghost_mnnz_t i;
    ghost_mnnz_t max_loc_elements, thisentry;
    ghost_mnnz_t *present_values = NULL;
    ghost_mnnz_t acc_dues;
    ghost_mnnz_t *tmp_transfers = NULL;
    ghost_mnnz_t acc_wishes;

    ghost_mnnz_t *item_from = NULL;

    ghost_mnnz_t *wishlist_counts = NULL;

    ghost_midx_t **wishlist = NULL;
    ghost_midx_t **cwishlist = NULL;


    ghost_midx_t this_pseudo_col;
    ghost_midx_t *pseudocol = NULL;
    ghost_midx_t *globcol = NULL;

    ghost_midx_t *comm_remotePE = NULL;
    ghost_midx_t *comm_remoteEl = NULL;
    ghost_midx_t *wishl_mem  = NULL;
    ghost_midx_t *duel_mem   = NULL;
    ghost_mnnz_t acc_transfer_wishes, acc_transfer_dues;

    size_t size_nint, size_col;
    size_t size_a2ai, size_nptr, size_pval;  
    size_t size_wish, size_dues;

    int nprocs;
    int me;
    GHOST_CALL_RETURN(ghost_getNumberOfRanks(ctx->mpicomm,&nprocs));
    GHOST_CALL_RETURN(ghost_getRank(ctx->mpicomm,&me));

#if GHOST_HAVE_MPI
    MPI_Request req[2*nprocs];
    MPI_Status stat[2*nprocs];
#endif

    size_nint = (size_t)( (size_t)(nprocs)   * sizeof(ghost_midx_t)  );
    size_nptr = (size_t)( nprocs             * sizeof(ghost_midx_t*) );
    size_a2ai = (size_t)( nprocs*nprocs * sizeof(ghost_midx_t)  );


    max_loc_elements = 0;
    for (i=0;i<nprocs;i++) {
        if (max_loc_elements<ctx->lnEnts[i]) {
            max_loc_elements = ctx->lnEnts[i];
        }
    }

    size_pval = (size_t)( max_loc_elements * sizeof(ghost_mnnz_t) );
    size_col  = (size_t)( (size_t)(ctx->lnEnts[me])   * sizeof( ghost_midx_t ) );

    /*       / 1  2  .  3  4  . \
     *       | .  5  6  7  .  . |
     * mat = | 8  9  .  .  . 10 |
     *       | . 11 12 13  .  . |
     *       | .  .  .  . 14 15 |
     *       \16  .  .  . 17 18 /
     *
     * nprocs = 3
     * max_loc_elements = 4
     * item_from       = <{0,0,0},{0,0,0},{0,0,0}>
     * wishlist_counts = <{0,0,0},{0,0,0},{0,0,0}>
     * comm_remotePE   = <{0,0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0}> PE where element is on
     * comm_remoteEl   = <{0,0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0}> local colidx of element
     * present_values  = <{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0}> 
     * tmp_transfers   = <{0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0}>
     */

      

    item_from       = (ghost_mnnz_t *) ghost_malloc( size_nint); 
    wishlist_counts = (ghost_mnnz_t *) ghost_malloc( nprocs*sizeof(ghost_mnnz_t)); 
    comm_remotePE   = (ghost_midx_t *) ghost_malloc(size_col);
    comm_remoteEl   = (ghost_midx_t *) ghost_malloc(size_col);
    present_values  = (ghost_mnnz_t *) ghost_malloc(size_pval); 
    tmp_transfers   = (ghost_mnnz_t *) ghost_malloc(size_a2ai); 

    for (i=0; i<nprocs; i++) wishlist_counts[i] = 0;

    
    for (i=0;i<ctx->lnEnts[me];i++){
        for (j=nprocs-1;j>=0; j--){
            if (ctx->lfRow[j]<col[i]+1) {
                comm_remotePE[i] = j;
                wishlist_counts[j]++;
                comm_remoteEl[i] = col[i] -ctx->lfRow[j];
                break;
            }
        }
    }
    /*
     * wishlist_counts = <{3,3,1},{3,2,1},{1,0,4}>
     * comm_remotePE   = <{0,0,1,2,0,1,1},{0,0,2,0,1,1},{2,2,0,2,2}>
     * comm_remoteEl   = <{0,1,1,0,1,0,1},{0,1,1,1,0,1},{0,1,0,0,1}>
     */

    acc_wishes = 0;
    for (i=0; i<nprocs; i++) {
        acc_wishes += wishlist_counts[i];
    }

    /*
     * acc_wishes = <7,6,5> equal to lnEnts
     */

    wishlist        = (ghost_midx_t**) ghost_malloc(size_nptr); 
    cwishlist       = (ghost_midx_t**) ghost_malloc(size_nptr);

    /*
     * wishlist  = <{NULL,NULL,NULL},{NULL,NULL,NULL},{NULL,NULL,NULL}>
     * cwishlist = <{NULL,NULL,NULL},{NULL,NULL,NULL},{NULL,NULL,NULL}>
     */

    for (i=0; i<nprocs; i++){
        cwishlist[i] = (ghost_midx_t *)ghost_malloc(wishlist_counts[i]*sizeof(ghost_midx_t));
        wishlist[i] = (ghost_midx_t *)ghost_malloc(wishlist_counts[i]*sizeof(ghost_midx_t));
    }
    /*
     * wishlist  = <{{0,0,0},{0,0,0},{0}},{{0,0,0},{0,0},{0}},{{0},NULL,{0,0,0,0}}>
     * cwishlist = <{{0,0,0},{0,0,0},{0}},{{0,0,0},{0,0},{0}},{{0},NULL,{0,0,0,0}}>
     */

    for (i=0;i<nprocs;i++) item_from[i] = 0;

    for (i=0;i<ctx->lnEnts[me];i++){
        wishlist[comm_remotePE[i]][item_from[comm_remotePE[i]]] = comm_remoteEl[i];
        item_from[comm_remotePE[i]]++;
    }
    /*
     * wishlist  = <{{0,1,1},{1,0,1},{0}},{{0,1,1},{0,1},{1}},{{0},NULL,{0,1,0,1}}> local column idx of wishes
     * item_from = <{3,3,1},{3,2,1},{1,0,4}> equal to wishlist_counts
     */


    ctx->wishes = (ghost_mnnz_t *)ghost_malloc(nprocs*sizeof(ghost_mnnz_t)); 
    ctx->dues = (ghost_mnnz_t *)ghost_malloc(nprocs*sizeof(ghost_mnnz_t)); 

    for (i=0; i<nprocs; i++) {
        for (j=0; j<max_loc_elements; j++) 
            present_values[j] = -1;

        if ( (i!=me) && (wishlist_counts[i]>0) ){
            thisentry = 0;
            for (j=0; j<wishlist_counts[i]; j++){
                if (present_values[wishlist[i][j]]<0){
                    present_values[wishlist[i][j]] = thisentry;
                    cwishlist[i][thisentry] = wishlist[i][j];
                    thisentry = thisentry + 1;
                }
            }
            ctx->wishes[i] = thisentry;
        } else {
            ctx->wishes[i] = 0; 
        }

    }

    /* 
     * cwishlist = <{{#,#,#},{1,0,#},{0}},{{0,1,#},{#,#},{1}},{{0},NULL,{#,#,#,#}}> compressed wish list
     * ctx->wishes = <{0,2,1},{2,0,1},{1,0,0}>
     */

#if GHOST_HAVE_MPI
    int err;
    MPI_CALL(MPI_Allgather(ctx->wishes, nprocs, ghost_mpi_dt_midx, tmp_transfers, 
                nprocs, ghost_mpi_dt_midx, ctx->mpicomm),err);
    if (err != MPI_SUCCESS) {
        ret = GHOST_ERR_MPI;
        goto err;
    }
#endif

    for (i=0; i<nprocs; i++) {
        ctx->dues[i] = tmp_transfers[i*nprocs+me];
    }

    ctx->dues[me] = 0; 
    
    /* 
     * ctx->dues = <{0,2,1},{2,0,0},{1,1,0}>
     */

    acc_transfer_dues = 0;
    acc_transfer_wishes = 0;
    for (i=0; i<nprocs; i++){
        acc_transfer_wishes += ctx->wishes[i];
        acc_transfer_dues   += ctx->dues[i];
    }
    
    /* 
     * acc_transfer_wishes = <3,3,1>
     * acc_transfer_dues = <3,2,2>
     */

    pseudocol       = (ghost_midx_t*) ghost_malloc(size_col);
    globcol         = (ghost_midx_t*) ghost_malloc(size_col);
    
    /*
     * pseudocol = <{0,0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0}> PE where element is on
     * globcol   = <{0,0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0}> local colidx of element
     */

    this_pseudo_col = ctx->lnrows[me];
    ctx->halo_elements = 0;
    ghost_midx_t tt = 0;
    i = me;
    int meHandled = 0;

    /*
     * col[i] = <{0,1,3,4,1,2,3},{0,1,5,1,2,3},{4,5,0,4,5}>
     */

    for (; i<nprocs; i++) { // iterate i=me,..,nprocs,0,..,me-1
        int t = 0;
        if (meHandled && (i == me)) continue;

        if (i != me){ 
            for (j=0;j<ctx->wishes[i];j++){
                pseudocol[ctx->halo_elements] = this_pseudo_col; 
                globcol[ctx->halo_elements]   = ctx->lfRow[i]+cwishlist[i][j]; 
                ctx->halo_elements++;
                this_pseudo_col++;
            }
            /*
             * pseudocol = <{0,1,2},{0,1,2},{0}> colidx of each halo element starting from 0
             * globcol   = <{2,3,4},{2,3,4},{2}> colidx of each halo element starting from lnrows[me]
             */

            // myrevcol maps the actual colidx to the new colidx
            DEBUG_LOG(2,"Allocating space for myrevcol");
            ghost_midx_t * myrevcol = (ghost_midx_t *)ghost_malloc(ctx->lnrows[i]*sizeof(ghost_midx_t));
            for (j=0;j<ctx->wishes[i];j++){
                myrevcol[globcol[tt]-ctx->lfRow[i]] = tt;
                tt++;
            }
            /*
             * 1st iter: myrevcol = <{1,0},{0,1},{0,#}>
             * 2nd iter: myrevcol = <{2,#},{#,2},{#,#}>
             */

            for (;t<ctx->lnEnts[me];t++) {
                if (comm_remotePE[t] == i) { // local element for rank i
                    col[t] =  pseudocol[myrevcol[col[t]-ctx->lfRow[i]]];
                }
            }
            free(myrevcol);
        } else { // first i iteration goes here
            for (;t<ctx->lnEnts[me];t++) {
                if (comm_remotePE[t] == me) { // local element for myself
                    col[t] =  comm_remoteEl[t];
                }
            }
            /*
             * col[i] = <{0,1,3,4,1,2,3},{0,1,5,1,0,1},{0,1,0,0,1}> local idx changed after first iteration
             */

        }

        if (!meHandled) {
            i = -1;
            meHandled = 1;
        }


    }
    /*
     * col[i] = <{0,1,2,4,1,3,2},{2,3,4,3,0,1},{0,1,2,0,1}>
     */


    size_wish = (size_t)( acc_transfer_wishes * sizeof(ghost_midx_t) );
    size_dues = (size_t)( acc_transfer_dues   * sizeof(ghost_midx_t) );

    wishl_mem  = (ghost_midx_t *)  ghost_malloc(size_wish); // we need a contiguous array in memory
    duel_mem   = (ghost_midx_t *)  ghost_malloc(size_dues); // we need a contiguous array in memory
    ctx->hput_pos      = (ghost_midx_t*)  ghost_malloc(size_nptr); 

    acc_dues = 0;
    acc_wishes = 0;


    for (i=0; i<nprocs; i++){

        ctx->duelist[i]    = &(duel_mem[acc_dues]);
        ctx->wishlist[i]   = &(wishl_mem[acc_wishes]);
        ctx->hput_pos[i]   = ctx->lnrows[me]+acc_wishes;

        if  ( (me != i) && !( (i == nprocs-2) && (me == nprocs-1) ) ){
            acc_dues   += ctx->dues[i];
            acc_wishes += ctx->wishes[i];
        }
    }

#if GHOST_HAVE_MPI
    for (i=0;i<2*nprocs;i++) 
        req[i] = MPI_REQUEST_NULL;

    for (i=0; i<nprocs; i++) 
        for (j=0;j<ctx->wishes[i];j++)
            ctx->wishlist[i][j] = cwishlist[i][j]; 

    int msgcount = 0;
    for(i=0; i<nprocs; i++) 
    { // receive _my_ dues from _other_ processes' wishes
        MPI_CALL(MPI_Irecv(ctx->duelist[i],ctx->dues[i],ghost_mpi_dt_midx,i,i,ctx->mpicomm,&req[msgcount]),err);
        if (err != MPI_SUCCESS) {
            ret = GHOST_ERR_MPI;
            goto err;
        }
        msgcount++;
    }


    for(i=0; i<nprocs; i++) { 
        MPI_CALL_GOTO(MPI_Isend(ctx->wishlist[i],ctx->wishes[i],ghost_mpi_dt_midx,i,me,ctx->mpicomm,&req[msgcount]),err,ret);
        msgcount++;
    }

    MPI_CALL_GOTO(MPI_Waitall(msgcount,req,stat),err,ret);
#endif

    goto out;

err:
    free(wishl_mem); wishl_mem = NULL;
    free(duel_mem); duel_mem = NULL;
    for (i=0; i<nprocs; i++) {
        free(ctx->wishlist[i]); ctx->wishlist[i] = NULL;
        free(ctx->duelist[i]); ctx->duelist[i] = NULL;
    }
    free(ctx->hput_pos); ctx->hput_pos = NULL;
    free(ctx->wishes); ctx->wishes = NULL;
    free(ctx->dues); ctx->dues = NULL;

out:
    for (i=0; i<nprocs; i++) {
        free(wishlist[i]); wishlist[i] = NULL;
    }
    free(wishlist); wishlist = NULL;
    for (i=0; i<nprocs; i++) {
        free(cwishlist[i]); cwishlist[i] = NULL;
    }
    free(cwishlist); cwishlist = NULL;
    free(tmp_transfers); tmp_transfers = NULL;
    free(wishlist_counts); wishlist_counts = NULL;
    free(item_from); item_from = NULL;
    free(comm_remotePE); comm_remotePE = NULL;
    free(comm_remoteEl); comm_remoteEl = NULL;
    free(present_values); present_values = NULL;
    free(pseudocol); pseudocol = NULL;
    free(globcol); globcol = NULL;
    
    return ret;
}
