#define _XOPEN_SOURCE 500 
#include "ghost/config.h"
#include "ghost/types.h"
#include "ghost/core.h"
#include "ghost/densemat_cm.h"
#include "ghost/util.h"
#include "ghost/locality.h"
#include "ghost/context.h"
#include "ghost/instr.h"
#include "ghost/machine.h"
#include "ghost/log.h"
#include "ghost/bindensemat.h"
#include "ghost/densemat_rm.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static ghost_error_t vec_cm_scale(ghost_densemat_t *vec, void *scale);
static ghost_error_t vec_cm_axpy(ghost_densemat_t *vec, ghost_densemat_t *vec2, void *scale);
static ghost_error_t vec_cm_axpby(ghost_densemat_t *vec, ghost_densemat_t *vec2, void *scale, void *b);
static ghost_error_t vec_cm_fromFunc(ghost_densemat_t *vec, void (*fp)(ghost_gidx_t, ghost_lidx_t, void *));
static ghost_error_t vec_cm_fromVec(ghost_densemat_t *vec, ghost_densemat_t *vec2, ghost_lidx_t roffs, ghost_lidx_t coffs);
static ghost_error_t vec_cm_fromScalar(ghost_densemat_t *vec, void *val);
static ghost_error_t vec_cm_fromFile(ghost_densemat_t *vec, char *path, bool singleFile);
static ghost_error_t vec_cm_toFile(ghost_densemat_t *vec, char *path, bool singleFile);
static ghost_error_t ghost_distributeVector(ghost_densemat_t *vec, ghost_densemat_t *nodeVec);
static ghost_error_t ghost_collectVectors(ghost_densemat_t *vec, ghost_densemat_t *totalVec); 
static void ghost_freeVector( ghost_densemat_t* const vec );
static ghost_error_t ghost_permuteVector( ghost_densemat_t* vec, ghost_permutation_t *permutation, ghost_permutation_direction_t dir); 
static ghost_error_t ghost_cloneVector(ghost_densemat_t *src, ghost_densemat_t **new, ghost_lidx_t nr, ghost_lidx_t roffs, ghost_lidx_t nc, ghost_lidx_t coffs);
static ghost_error_t vec_cm_entry(ghost_densemat_t *, void *, ghost_lidx_t, ghost_lidx_t);
static ghost_error_t vec_cm_view (ghost_densemat_t *src, ghost_densemat_t **new, ghost_lidx_t nr, ghost_lidx_t roffs, ghost_lidx_t nc, ghost_lidx_t coffs);
static ghost_error_t vec_cm_viewScatteredVec (ghost_densemat_t *src, ghost_densemat_t **new, ghost_lidx_t nr, ghost_lidx_t *roffs, ghost_lidx_t nc, ghost_lidx_t *coffs);
static ghost_error_t vec_cm_viewScatteredCols (ghost_densemat_t *src, ghost_densemat_t **new, ghost_lidx_t nc, ghost_lidx_t *coffs);
static ghost_error_t vec_cm_viewCols (ghost_densemat_t *src, ghost_densemat_t **new, ghost_lidx_t nc, ghost_lidx_t coffs);
static ghost_error_t vec_cm_viewPlain (ghost_densemat_t *vec, void *data, ghost_lidx_t roffs, ghost_lidx_t coffs, ghost_lidx_t lda);
static ghost_error_t vec_cm_compress(ghost_densemat_t *vec);
static ghost_error_t vec_cm_upload(ghost_densemat_t *vec);
static ghost_error_t vec_cm_download(ghost_densemat_t *vec);
static ghost_error_t vec_cm_uploadHalo(ghost_densemat_t *vec);
static ghost_error_t vec_cm_downloadHalo(ghost_densemat_t *vec);
static ghost_error_t vec_cm_uploadNonHalo(ghost_densemat_t *vec);
static ghost_error_t vec_cm_downloadNonHalo(ghost_densemat_t *vec);
static ghost_error_t vec_cm_memtranspose(ghost_densemat_t *vec);
static ghost_error_t vec_cm_averageHalo(ghost_densemat_t *vec);
ghost_error_t ghost_densemat_cm_malloc(ghost_densemat_t *vec);

ghost_error_t ghost_densemat_cm_setfuncs(ghost_densemat_t *vec)
{
    ghost_error_t ret = GHOST_SUCCESS;

    if (vec->traits.flags & GHOST_DENSEMAT_DEVICE)
    {
#ifdef GHOST_HAVE_CUDA
        vec->dot = &ghost_densemat_cm_cu_dotprod;
        vec->vaxpy = &ghost_densemat_cm_cu_vaxpy;
        vec->vaxpby = &ghost_densemat_cm_cu_vaxpby;
        vec->axpy = &ghost_densemat_cm_cu_axpy;
        vec->axpby = &ghost_densemat_cm_cu_axpby;
        vec->scale = &ghost_densemat_cm_cu_scale;
        vec->vscale = &ghost_densemat_cm_cu_vscale;
        vec->fromScalar = &ghost_densemat_cm_cu_fromScalar;
        vec->fromRand = &ghost_densemat_cm_cu_fromRand;
#endif
    }
    else if (vec->traits.flags & GHOST_DENSEMAT_HOST)
    {
        vec->dot = &ghost_densemat_cm_dotprod_selector;
        vec->vaxpy = &ghost_densemat_cm_vaxpy_selector;
        vec->vaxpby = &ghost_densemat_cm_vaxpby_selector;
        vec->axpy = &vec_cm_axpy;
        vec->axpby = &vec_cm_axpby;
        vec->scale = &vec_cm_scale;
        vec->vscale = &ghost_densemat_cm_vscale_selector;
        vec->fromScalar = &vec_cm_fromScalar;
        vec->fromRand = &ghost_densemat_cm_fromRand_selector;
    }

    vec->memtranspose = &vec_cm_memtranspose;
    vec->compress = &vec_cm_compress;
    vec->string = &ghost_densemat_cm_string_selector;
    vec->fromFunc = &vec_cm_fromFunc;
    vec->fromVec = &vec_cm_fromVec;
    vec->fromFile = &vec_cm_fromFile;
    vec->toFile = &vec_cm_toFile;
    vec->distribute = &ghost_distributeVector;
    vec->collect = &ghost_collectVectors;
    vec->normalize = &ghost_densemat_cm_normalize_selector;
    vec->destroy = &ghost_freeVector;
    vec->permute = &ghost_permuteVector;
    vec->clone = &ghost_cloneVector;
    vec->entry = &vec_cm_entry;
    vec->viewVec = &vec_cm_view;
    vec->viewPlain = &vec_cm_viewPlain;
    vec->viewScatteredVec = &vec_cm_viewScatteredVec;
    vec->viewScatteredCols = &vec_cm_viewScatteredCols;
    vec->viewCols = &vec_cm_viewCols;

    vec->averageHalo = &vec_cm_averageHalo;

    vec->upload = &vec_cm_upload;
    vec->download = &vec_cm_download;
    vec->uploadHalo = &vec_cm_uploadHalo;
    vec->downloadHalo = &vec_cm_downloadHalo;
    vec->uploadNonHalo = &vec_cm_uploadNonHalo;
    vec->downloadNonHalo = &vec_cm_downloadNonHalo;
#ifdef GHOST_HAVE_CUDA
    if (vec->traits.flags & GHOST_DENSEMAT_DEVICE) {
        vec->cu_val = NULL;
    }
#endif

    return ret;
}

static ghost_error_t vec_cm_memtranspose(ghost_densemat_t *vec)
{
    if (vec->traits.flags & GHOST_DENSEMAT_SCATTERED) {
        ERROR_LOG("Cannot memtranspose scattered densemat views!");
        return GHOST_ERR_NOT_IMPLEMENTED;
    }

    ghost_lidx_t col,row;
    if (vec->traits.flags & GHOST_DENSEMAT_VIEW) {
        INFO_LOG("Memtranspose of a view. The densemat will loose its view property.");
        char *oldval;
        ghost_densemat_valptr(vec,(void **)&oldval);
        
        vec->traits.storage = GHOST_DENSEMAT_ROWMAJOR;
        vec->traits.flags &= ~GHOST_DENSEMAT_VIEW;
        ghost_bitmap_set_range(vec->ldmask,0,vec->traits.nrows-1);
        vec->traits.ncolsorig = vec->traits.ncols;
        vec->traits.nrowsorig = vec->traits.nrows;
        ghost_densemat_rm_setfuncs(vec);
        
        free(vec->val);vec->val = NULL; 
        ghost_densemat_rm_malloc(vec);
        for (col=0; col<vec->traits.ncols; col++) {
            for (row=0; row<vec->traits.nrows; row++) {
                memcpy(vec->val[0]+row*vec->traits.ncolspadded*vec->elSize+col*vec->elSize,
                        oldval+vec->traits.nrowspadded*col*vec->elSize+row*vec->elSize,
                        vec->elSize);
            }
        }
    } else {
        vec->traits.storage = GHOST_DENSEMAT_ROWMAJOR;
        ghost_densemat_rm_setfuncs(vec);
        if (vec->viewing) {
            INFO_LOG("In-place back-transpose. The densemat will regain its view property.");
            vec->traits.flags |= GHOST_DENSEMAT_VIEW;
            vec->traits.ncolsorig = vec->viewing->traits.ncols;
            vec->traits.nrowsorig = vec->viewing->traits.nrows;
            char *str;
            ghost_bitmap_list_asprintf(&str,vec->ldmask);
            if (vec->viewing_col) {
                ghost_bitmap_clr_range(vec->ldmask,0,vec->viewing_col-1);
            }
            ghost_bitmap_clr_range(vec->ldmask,vec->traits.ncols+vec->viewing_col,-1);

            ghost_bitmap_list_asprintf(&str,vec->ldmask);
            WARNING_LOG("%s",str);

            for (col=0; col<vec->traits.ncols; col++) {
                for (row=0; row<vec->traits.nrows; row++) {
                    memcpy(&vec->viewing->val[vec->viewing_row+row][(vec->viewing_col+col)*vec->elSize],
                            vec->val[0]+vec->traits.nrowspadded*col*vec->elSize+row*vec->elSize,
                            vec->elSize);
                }
            }
            vec->val = NULL;
            GHOST_CALL_RETURN(ghost_malloc((void **)&vec->val,vec->traits.nrowspadded*sizeof(char *)));
            vec->val[0] = vec->viewing->val[vec->viewing_row];
            for (row=1; row<vec->traits.nrowspadded; row++) {
                vec->val[row] = vec->val[0]+vec->viewing->traits.ncolspadded*row*vec->elSize;
            }
        } else {
        
            char *oldval = vec->val[0];
            free(vec->val); 
            vec->val = NULL;
            GHOST_CALL_RETURN(ghost_malloc((void **)&vec->val,vec->traits.nrowspadded*sizeof(char *)));
            vec->val[0] = oldval;
            for (row=1; row<vec->traits.nrowspadded; row++) {
                vec->val[row] = vec->val[0]+vec->traits.ncolspadded*row*vec->elSize;
            }

            char *tmp;
            GHOST_CALL_RETURN(ghost_malloc((void **)&tmp,vec->elSize*vec->traits.nrowspadded*vec->traits.ncolspadded));
            memcpy(tmp,vec->val[0],vec->elSize*vec->traits.nrowspadded*vec->traits.ncolspadded);

            for (col=0; col<vec->traits.ncols; col++) {
                for (row=0; row<vec->traits.nrows; row++) {
                    memcpy(vec->val[0]+row*vec->traits.ncolspadded*vec->elSize+col*vec->elSize,
                            tmp+vec->traits.nrowspadded*col*vec->elSize+row*vec->elSize,
                            vec->elSize);
                }
            }

            free(tmp);
        }
    }

    return GHOST_SUCCESS; 
}

static ghost_error_t vec_cm_uploadHalo(ghost_densemat_t *vec)
{
#ifdef GHOST_HAVE_CUDA
    if ((vec->traits.flags & GHOST_DENSEMAT_HOST) && (vec->traits.flags & GHOST_DENSEMAT_DEVICE)) {
        DEBUG_LOG(1,"Uploading halo elements of vector");
        ghost_lidx_t v,c;
        for (v=0, c=0; v<vec->traits.ncolsorig; v++) {
            if (ghost_bitmap_isset(vec->trmask,v)) {
                GHOST_CALL_RETURN(ghost_cu_upload(&vec->cu_val[(vec->traits.nrowspadded*v+vec->traits.nrowsorig)*vec->elSize],VECVAL_CM(vec,vec->val,c,vec->traits.nrowsorig), (vec->traits.nrowshalo-vec->traits.nrows)*vec->elSize));
                c++;
            }
        }
    }
#else
    UNUSED(vec);
#endif
    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_downloadHalo(ghost_densemat_t *vec)
{
#ifdef GHOST_HAVE_CUDA
    if ((vec->traits.flags & GHOST_DENSEMAT_HOST) && (vec->traits.flags & GHOST_DENSEMAT_DEVICE)) {
        DEBUG_LOG(1,"Downloading halo elements of vector");
        ghost_lidx_t v,c;
        for (v=0, c=0; v<vec->traits.ncolsorig; v++) {
            if (ghost_bitmap_isset(vec->trmask,v)) {
                GHOST_CALL_RETURN(ghost_cu_download(VECVAL_CM(vec,vec->val,c,vec->traits.nrowsorig),&vec->cu_val[(vec->traits.nrowspadded*v+vec->traits.nrowsorig)*vec->elSize], (vec->traits.nrowshalo-vec->traits.nrows)*vec->elSize));
                c++;
            }
        }
    }
#else
    UNUSED(vec);
#endif
    return GHOST_SUCCESS;
}
static ghost_error_t vec_cm_uploadNonHalo(ghost_densemat_t *vec)
{
#ifdef GHOST_HAVE_CUDA
    if ((vec->traits.flags & GHOST_DENSEMAT_HOST) && (vec->traits.flags & GHOST_DENSEMAT_DEVICE)) {
        DEBUG_LOG(1,"Uploading %"PRLIDX" rows of vector",vec->traits.nrowshalo);
        ghost_lidx_t v,c,r;
        for (v=0, c=0; v<vec->traits.ncolsorig; v++) {
            if (ghost_bitmap_isset(vec->trmask,v)) {
                for (r=0; r<vec->traits.nrowsorig; r++) {
                    if (ghost_bitmap_isset(vec->ldmask,r)) {
                        GHOST_CALL_RETURN(ghost_cu_upload(&vec->cu_val[(vec->traits.nrowspadded*v+r)*vec->elSize],VECVAL_CM(vec,vec->val,c,r), vec->elSize));
                    }
                }
                c++;
            }
        }
    }
#else
    UNUSED(vec);
#endif
    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_downloadNonHalo(ghost_densemat_t *vec)
{
#ifdef GHOST_HAVE_CUDA
    if ((vec->traits.flags & GHOST_DENSEMAT_HOST) && (vec->traits.flags & GHOST_DENSEMAT_DEVICE)) {
        DEBUG_LOG(1,"Downloading vector");
        ghost_lidx_t v,c,r;
        for (v=0, c=0; v<vec->traits.ncolsorig; v++) {
            if (ghost_bitmap_isset(vec->trmask,v)) {
                for (r=0; r<vec->traits.nrowsorig; r++) {
                    if (ghost_bitmap_isset(vec->ldmask,r)) {
                        GHOST_CALL_RETURN(ghost_cu_download(VECVAL_CM(vec,vec->val,c,r),&vec->cu_val[(vec->traits.nrowspadded*v+r)*vec->elSize], vec->elSize));
                    }
                }
                c++;
            }
        }
    }
#else
    UNUSED(vec);
#endif
    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_upload(ghost_densemat_t *vec) 
{
    GHOST_CALL_RETURN(vec->uploadNonHalo(vec));
    GHOST_CALL_RETURN(vec->uploadHalo(vec));
    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_download(ghost_densemat_t *vec)
{
    GHOST_CALL_RETURN(vec->downloadNonHalo(vec));
    GHOST_CALL_RETURN(vec->downloadHalo(vec));
    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_view (ghost_densemat_t *src, ghost_densemat_t **new, ghost_lidx_t nr, ghost_lidx_t roffs, ghost_lidx_t nc, ghost_lidx_t coffs)
{
    DEBUG_LOG(1,"Viewing a %"PRLIDX"x%"PRLIDX" densemat from a %"PRLIDX"x%"PRLIDX" densemat with offset %"PRLIDX"x%"PRLIDX,nr,nc,src->traits.nrows,src->traits.ncols,roffs,coffs);
    ghost_densemat_traits_t newTraits = src->traits;
    newTraits.ncols = nc;
    newTraits.nrows = nr;
    newTraits.flags |= GHOST_DENSEMAT_VIEW;

    ghost_densemat_create(new,src->context,newTraits);
    ghost_bitmap_copy((*new)->ldmask,src->ldmask);
    ghost_bitmap_copy((*new)->trmask,src->trmask);
    ghost_densemat_cm_malloc(*new);
    ghost_lidx_t v,r,viewedrow;

    //char *bm;
    //ghost_bitmap_list_asprintf(&bm,src->ldmask);
    //WARNING_LOG("%s %p",bm,src->ldmask);

    //roffs += ghost_bitmap_first((*new)->ldmask);
    
    for (viewedrow=0, r=0; r<src->traits.nrowsorig; r++) {
        if (viewedrow<roffs || (viewedrow >= roffs+nr)) {
            //INFO_LOG("clr");
            ghost_bitmap_clr((*new)->ldmask,r);
        }
        if (ghost_bitmap_isset(src->ldmask,r)) {
            viewedrow++;
        }
        //printf("row to view: %d..%d, current viewed row: %d, srcbitmap[%d]=%d newbitmap[%d]=%d\n",roffs,roffs+nr-1,viewedrow,r,ghost_bitmap_isset(src->ldmask,r),r,ghost_bitmap_isset((*new)->ldmask,r));
    }
    //ghost_bitmap_list_asprintf(&bm,(*new)->ldmask);
    //WARNING_LOG("%s %p",bm,(*new)->ldmask);

    if ((*new)->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
        ghost_lidx_t viewedcol;
        (*new)->cu_val = src->cu_val;
        for (viewedcol=0, v=0; v<src->traits.ncolsorig; v++) {
            if (viewedcol<coffs || (viewedcol >= coffs+nc)) {
                ghost_bitmap_clr((*new)->trmask,v);
            }
            if (ghost_bitmap_isset(src->trmask,v)) {
                viewedcol++;
            }
        //printf("col to view: %d..%d, current viewed col: %d, srcbitmap[%d]=%d newbitmap[%d]=%d\n",coffs,coffs+nc-1,viewedcol,v,ghost_bitmap_isset(src->trmask,v),v,ghost_bitmap_isset((*new)->trmask,v));
        }
#endif
    } 
    if ((*new)->traits.flags & GHOST_DENSEMAT_HOST) {
        for (v=0; v<(*new)->traits.ncols; v++) {
            (*new)->val[v] = VECVAL_CM(src,src->val,coffs+v,0);
        }
    }
    //char *newstr;
    //(*new)->string(*new,&newstr);
    //printf("\n\n\n%s\n\n\n",newstr);

    (*new)->viewing = src;
    (*new)->viewing_col = coffs;
    (*new)->viewing_row = roffs;
    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_viewPlain (ghost_densemat_t *vec, void *data, ghost_lidx_t roffs, ghost_lidx_t coffs, ghost_lidx_t lda)
{
    DEBUG_LOG(1,"Viewing a %"PRLIDX"x%"PRLIDX" dense matrix from plain data with offset %"PRLIDX"x%"PRLIDX,vec->traits.nrows,vec->traits.ncols,roffs,coffs);

    ghost_lidx_t v;
    ghost_densemat_cm_malloc(vec);

    if (vec->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
        INFO_LOG("The plain memory has to be valid CUDA device memory!");
        INFO_LOG("The row offset is being ignored!");
        vec->cu_val = &((char *)data)[lda*coffs*vec->elSize];
        vec->traits.nrowspadded = vec->traits.nrows;
#endif
    } else {
        for (v=0; v<vec->traits.ncols; v++) {
            vec->val[v] = &((char *)data)[(lda*(coffs+v)+roffs)*vec->elSize];
        }
    }

    vec->traits.flags |= GHOST_DENSEMAT_VIEW;
    ghost_bitmap_set_range(vec->ldmask,0,vec->traits.nrowsorig);
    ghost_bitmap_set_range(vec->trmask,0,vec->traits.ncolsorig);
    vec->traits.ncolsorig = vec->traits.ncols;
    vec->traits.nrowsorig = vec->traits.nrows;

    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_viewCols (ghost_densemat_t *src, ghost_densemat_t **new, ghost_lidx_t nc, ghost_lidx_t coffs)
{
    DEBUG_LOG(1,"Viewing a %"PRLIDX"x%"PRLIDX" contiguous dense matrix",src->traits.nrows,nc);
    ghost_lidx_t v;
    ghost_densemat_traits_t newTraits = src->traits;
    newTraits.ncols = nc;
    newTraits.ncolsorig = src->traits.ncolsorig;
    newTraits.flags |= GHOST_DENSEMAT_VIEW;

    ghost_densemat_create(new,src->context,newTraits);
    ghost_densemat_cm_malloc(*new);

    if ((*new)->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
        ghost_lidx_t viewedcol;
        (*new)->cu_val = src->cu_val;
        for (viewedcol=0, v=0; v<src->traits.ncolsorig; v++) {
            if (viewedcol<coffs || (viewedcol >= coffs+nc)) {
                ghost_bitmap_clr((*new)->trmask,v);
            }
            if (ghost_bitmap_isset(src->trmask,v)) {
                viewedcol++;
            }
        }
#endif
    } 
    if ((*new)->traits.flags & GHOST_DENSEMAT_HOST) {
        for (v=0; v<(*new)->traits.ncols; v++) {
            (*new)->val[v] = VECVAL_CM(src,src->val,coffs+v,0);
        }
    }

    (*new)->viewing = src;
    (*new)->viewing_col = coffs;
    (*new)->viewing_row = 0;
    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_viewScatteredCols (ghost_densemat_t *src, ghost_densemat_t **new, ghost_lidx_t nc, ghost_lidx_t *coffs)
{
#ifdef GHOST_HAVE_CUDA
    if (src->traits.flags & GHOST_DENSEMAT_DEVICE) {
        if (!array_strictly_ascending(coffs,nc)) {
            ERROR_LOG("Can only view sctrictly ascending scattered columns for row-major densemats!");
            return GHOST_ERR_INVALID_ARG;
        }
    }
#endif
    DEBUG_LOG(1,"Viewing a %"PRLIDX"x%"PRLIDX" scattered dense matrix",src->traits.nrows,nc);
    ghost_lidx_t v;
    ghost_densemat_traits_t newTraits = src->traits;
    newTraits.ncols = nc;
    newTraits.flags |= GHOST_DENSEMAT_VIEW;
    newTraits.flags |= GHOST_DENSEMAT_SCATTERED;

    ghost_densemat_create(new,src->context,newTraits);
    ghost_densemat_cm_malloc(*new);

    if ((*new)->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
        ghost_lidx_t c,i,viewedcol;
        (*new)->cu_val = src->cu_val;
        for (viewedcol=-1, c=0, i=0; c<src->traits.ncolsorig; c++) {
            if (ghost_bitmap_isset(src->trmask,c)) {
                viewedcol++;
            }
            if (coffs[i] != viewedcol) {
                ghost_bitmap_clr((*new)->trmask,c);
            } else {
                i++;
            }
        }
#endif
    } 
    if ((*new)->traits.flags & GHOST_DENSEMAT_HOST) {
        for (v=0; v<nc; v++) {
            (*new)->val[v] = VECVAL_CM(src,src->val,coffs[v],0);
        }    
    }

    (*new)->viewing = src;
    (*new)->viewing_col = -1;
    (*new)->viewing_row = -1;
    return GHOST_SUCCESS;
}


static ghost_error_t vec_cm_viewScatteredVec (ghost_densemat_t *src, ghost_densemat_t **new, ghost_lidx_t nr, ghost_lidx_t *roffs, ghost_lidx_t nc, ghost_lidx_t *coffs)
{
    DEBUG_LOG(1,"Viewing a %"PRLIDX"x%"PRLIDX" scattered dense matrix",src->traits.nrows,nc);
    ghost_lidx_t v,r,i,viewedrow;
    ghost_densemat_traits_t newTraits = src->traits;
    newTraits.ncols = nc;
    newTraits.nrows = nr;
    newTraits.ncolsorig = src->traits.ncolsorig;
    newTraits.nrowsorig = src->traits.nrowsorig;
    newTraits.flags |= GHOST_DENSEMAT_VIEW;
    newTraits.flags |= GHOST_DENSEMAT_SCATTERED;

    ghost_densemat_create(new,src->context,newTraits);
    ghost_bitmap_copy((*new)->ldmask,src->ldmask);
    ghost_bitmap_copy((*new)->trmask,src->trmask);
    ghost_densemat_cm_malloc(*new);
    
    for (viewedrow=-1,r=0,i=0; r<(*new)->traits.nrowsorig; r++) {
        if (ghost_bitmap_isset(src->ldmask,r)) {
            viewedrow++;
        }
        if (roffs[i] != viewedrow) {
            ghost_bitmap_clr((*new)->ldmask,r);
        } else {
            i++;
        }
    }


    if ((*new)->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
    ghost_lidx_t c,viewedcol;
        (*new)->cu_val = src->cu_val;
        for (viewedcol=-1,c=0,i=0; c<(*new)->traits.ncolsorig; c++) {
            if (ghost_bitmap_isset(src->trmask,c)) {
                viewedcol++;
            }
            if (coffs[i] != viewedcol) {
                ghost_bitmap_clr((*new)->trmask,c);
            } else {
                i++;
            }
        }
#endif
    } 
    if ((*new)->traits.flags & GHOST_DENSEMAT_HOST) {
        for (v=0; v<nc; v++) {
            (*new)->val[v] = VECVAL_CM(src,src->val,coffs[v],0);
        }    
    }
    (*new)->viewing = src;
    (*new)->viewing_col = -1;
    (*new)->viewing_row = -1;
    return GHOST_SUCCESS;
}

ghost_error_t ghost_densemat_cm_malloc(ghost_densemat_t *vec)
{
    ghost_lidx_t v;
    if (vec->val == NULL) {
        GHOST_CALL_RETURN(ghost_malloc((void **)&vec->val,vec->traits.ncols*sizeof(char *)));

        for (v=0; v<vec->traits.ncols; v++) {
            vec->val[v] = NULL;
        }
    }

    if (vec->traits.flags & GHOST_DENSEMAT_VIEW) {
        return GHOST_SUCCESS;
    }

    if (vec->traits.flags & GHOST_DENSEMAT_HOST) {
        if (vec->val[0] == NULL) {
            DEBUG_LOG(2,"Allocating host side of vector");
            GHOST_CALL_RETURN(ghost_malloc_align((void **)&vec->val[0],(size_t)vec->traits.ncols*vec->traits.nrowspadded*vec->elSize,GHOST_DATA_ALIGNMENT));
            for (v=1; v<vec->traits.ncols; v++) {
                vec->val[v] = vec->val[0]+v*vec->traits.nrowspadded*vec->elSize;
            }
        }
    }

    if (vec->traits.flags & GHOST_DENSEMAT_DEVICE) {
        DEBUG_LOG(2,"Allocating device side of vector");
#ifdef GHOST_HAVE_CUDA
        if (vec->cu_val == NULL) {
#ifdef GHOST_HAVE_CUDA_PINNEDMEM
            WARNING_LOG("CUDA pinned memory is disabled");
            //ghost_cu_safecall(cudaHostGetDevicePointer((void **)&vec->cu_val,vec->val,0));
            GHOST_CALL_RETURN(ghost_cu_malloc(&vec->cu_val,vec->traits.nrowspadded*vec->traits.ncols*vec->elSize));
#else
            //ghost_cu_safecall(cudaMallocPitch(&(void *)vec->cu_val,&vec->traits.nrowspadded,vec->traits.nrowshalo*sizeofdt,vec->traits.ncols));
            GHOST_CALL_RETURN(ghost_cu_malloc(&vec->cu_val,vec->traits.nrowspadded*vec->traits.ncols*vec->elSize));
#endif
        }
#endif
    }   

    return GHOST_SUCCESS; 
}

static ghost_error_t vec_cm_fromVec(ghost_densemat_t *vec, ghost_densemat_t *vec2, ghost_lidx_t roffs, ghost_lidx_t coffs)
{
    ghost_densemat_cm_malloc(vec);
    DEBUG_LOG(1,"Initializing vector from vector w/ col offset %"PRLIDX,coffs);
    ghost_lidx_t r,c;
    
    if (vec->traits.flags & GHOST_DENSEMAT_SCATTERED || vec2->traits.flags & GHOST_DENSEMAT_SCATTERED) {
        WARNING_LOG("Potentially slow fromVec operation for scattered densemats!");
        int v1row = -1, v2row = -1, v1col = -1, v2col = -1;

        for (r=0; r<vec->traits.nrows; r++) {
            v1row = ghost_bitmap_next(vec->ldmask,v1row);
            v2row = ghost_bitmap_next(vec2->ldmask,v2row);
            v1col = -1;
            v2col = -1;
            for (c=0; c<vec->traits.ncols; c++) {
                v1col = ghost_bitmap_next(vec->trmask,v1col);
                v2col = ghost_bitmap_next(vec2->trmask,v2col);
                
                if (vec->traits.flags & GHOST_DENSEMAT_DEVICE) {
                    if (vec2->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
                        GHOST_CALL_RETURN(ghost_cu_memcpy(CUVECVAL_CM(vec,vec->cu_val,v1col,v1row),CUVECVAL_CM(vec2,vec2->cu_val,coffs+v2col,roffs+v2row),vec->elSize));

#endif
                    } else {
#ifdef GHOST_HAVE_CUDA
                        GHOST_CALL_RETURN(ghost_cu_upload(CUVECVAL_CM(vec,vec->cu_val,v1col,v1row),VECVAL_CM(vec2,vec2->val,coffs+c,roffs+v2row),vec->elSize));
#endif
                    }
                } else {
                    if (vec2->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
                        GHOST_CALL_RETURN(ghost_cu_download(VECVAL_CM(vec,vec->val,c,v1row),CUVECVAL_CM(vec2,vec2->cu_val,coffs+v2col,roffs+v2row),vec->elSize));
#endif
                    } else {
                        memcpy(VECVAL_CM(vec,vec->val,c,v1row),VECVAL_CM(vec2,vec2->val,coffs+c,roffs+v2row),vec->elSize);
                    }
                }

            }
        }
    } else {
        char *v1val, *v2val, *cuv1val, *cuv2val;
        GHOST_CALL_RETURN(ghost_densemat_cu_valptr(vec,(void **)&cuv1val));
        GHOST_CALL_RETURN(ghost_densemat_valptr(vec,(void **)&v1val));
        GHOST_CALL_RETURN(ghost_densemat_cu_valptr(vec2,(void **)&cuv2val));
        GHOST_CALL_RETURN(ghost_densemat_valptr(vec2,(void **)&v2val));
                
        cuv2val += coffs*vec2->traits.nrowspadded*vec2->elSize + roffs*vec->elSize;
        v2val += coffs*vec2->traits.nrowspadded*vec2->elSize + roffs*vec->elSize;
        
        if (vec->traits.nrowsorig != vec->traits.nrows || vec2->traits.nrowsorig != vec2->traits.nrows || vec->traits.nrowsorig != vec2->traits.nrowsorig) {
            WARNING_LOG("Potentially slow fromVec operation because rows are not dense and copying has to be done col-wise!");
           
            for (c=0; c<vec->traits.ncols; c++) {
                if (vec->traits.flags & GHOST_DENSEMAT_DEVICE) {
                    if (vec2->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
                        GHOST_CALL_RETURN(ghost_cu_memcpy(cuv1val,cuv2val,vec2->traits.nrows*vec->elSize));
#endif
                    } else {
#ifdef GHOST_HAVE_CUDA
                        GHOST_CALL_RETURN(ghost_cu_upload(cuv1val,v2val,vec2->traits.nrows*vec->elSize));
#endif
                    }
                } else {
                    if (vec2->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
                        GHOST_CALL_RETURN(ghost_cu_download(v1val,cuv2val,vec2->traits.nrows*vec->elSize));
#endif
                    } else {
                        memcpy(v1val,v2val,vec->traits.nrows*vec->elSize);
                    }
                }
                cuv1val += vec->traits.nrowspadded*vec->elSize;
                cuv2val += vec2->traits.nrowspadded*vec->elSize;
                v1val += vec->traits.nrowspadded*vec->elSize;
                v2val += vec2->traits.nrowspadded*vec->elSize;
            }
        } else {
            if (vec->traits.flags & GHOST_DENSEMAT_DEVICE) {
                if (vec2->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
                    GHOST_CALL_RETURN(ghost_cu_memcpy(cuv1val,cuv2val,vec2->traits.nrowspadded*vec2->traits.ncols*vec->elSize));
#endif
                } else {
#ifdef GHOST_HAVE_CUDA
                    GHOST_CALL_RETURN(ghost_cu_upload(cuv1val,v2val,vec2->traits.nrowspadded*vec2->traits.ncols*vec->elSize));
#endif
                }
            } else {
                if (vec2->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
                    GHOST_CALL_RETURN(ghost_cu_download(v1val,cuv2val,vec2->traits.nrowspadded*vec2->traits.ncols*vec->elSize));
#endif
                } else {
                    memcpy(v1val,v2val,vec2->traits.nrowspadded*vec2->traits.ncols*vec->elSize);
                }
            }
        }
    }


    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_axpy(ghost_densemat_t *vec, ghost_densemat_t *vec2, void *scale)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH);
    ghost_error_t ret = GHOST_SUCCESS;
    ghost_lidx_t nc = MIN(vec->traits.ncols,vec2->traits.ncols);
    char *s = NULL;
    GHOST_CALL_GOTO(ghost_malloc((void **)&s,nc*vec->elSize),err,ret);

    ghost_lidx_t i;
    for (i=0; i<nc; i++) {
        memcpy(&s[i*vec->elSize],scale,vec->elSize);
    }
    
    ret = vec->vaxpy(vec,vec2,s);

    goto out;
err:

out:
    free(s);
    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH);
    return ret;
}

static ghost_error_t vec_cm_axpby(ghost_densemat_t *vec, ghost_densemat_t *vec2, void *scale, void *_b)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH);
    ghost_error_t ret = GHOST_SUCCESS;
    ghost_lidx_t nc = MIN(vec->traits.ncols,vec2->traits.ncols);
    char *s = NULL;
    char *b = NULL;
    GHOST_CALL_GOTO(ghost_malloc((void **)&s,nc*vec->elSize),err,ret);
    GHOST_CALL_GOTO(ghost_malloc((void **)&b,nc*vec->elSize),err,ret);

    ghost_lidx_t i;
    for (i=0; i<nc; i++) {
        memcpy(&s[i*vec->elSize],scale,vec->elSize);
        memcpy(&b[i*vec->elSize],_b,vec->elSize);
    }
    
    ret = vec->vaxpby(vec,vec2,s,b);

    goto out;
err:

out:
    free(s);
    free(b);
    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH);
    return ret;
}

static ghost_error_t vec_cm_scale(ghost_densemat_t *vec, void *scale)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_MATH);
    ghost_error_t ret = GHOST_SUCCESS;
    ghost_lidx_t nc = vec->traits.ncols;
    char *s;
    GHOST_CALL_GOTO(ghost_malloc((void **)&s,nc*vec->elSize),err,ret);

    ghost_lidx_t i;
    for (i=0; i<nc; i++) {
        memcpy(&s[i*vec->elSize],scale,vec->elSize);
    }

    ret = vec->scale(vec,scale);

    goto out;
err:

out:
    free(s);
    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_MATH);
    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_entry(ghost_densemat_t * vec, void *val, ghost_lidx_t r, ghost_lidx_t c) 
{
    int i = 0;
    int idx = ghost_bitmap_first(vec->ldmask);
    for (i=0; i<r; i++) {
        idx = ghost_bitmap_next(vec->ldmask,idx);
    }
    
    if (vec->traits.flags & GHOST_DENSEMAT_DEVICE)
    {
#ifdef GHOST_HAVE_CUDA
        int cidx = ghost_bitmap_first(vec->trmask);
        for (i=0; i<c; i++) {
            cidx = ghost_bitmap_next(vec->trmask,cidx);
        }
        ghost_cu_download(val,&vec->cu_val[(cidx*vec->traits.nrowspadded+idx)*vec->elSize],vec->elSize);
#endif
    }
    else if (vec->traits.flags & GHOST_DENSEMAT_HOST)
    {
        memcpy(val,VECVAL_CM(vec,vec->val,c,idx),vec->elSize);
    }

    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_fromScalar(ghost_densemat_t *vec, void *val)
{
    ghost_densemat_cm_malloc(vec);
    DEBUG_LOG(1,"Initializing vector from scalar value with %"PRLIDX" rows",vec->traits.nrows);

    int row,col,rowidx;
    ITER_BEGIN_CM(vec,col,row,rowidx)
    memcpy(VECVAL_CM(vec,vec->val,col,row),val,vec->elSize);
    ITER_END_CM(rowidx)
    vec->upload(vec);

    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_toFile(ghost_densemat_t *vec, char *path, bool singleFile)
{ 

#ifndef GHOST_HAVE_MPI
    singleFile = false;
#endif

    if (singleFile) {
#ifdef GHOST_HAVE_MPI
        int rank;
        GHOST_CALL_RETURN(ghost_rank(&rank, vec->context->mpicomm));

        int32_t endianess = ghost_machine_bigendian();
        int32_t version = 1;
        int32_t order = GHOST_BINDENSEMAT_ORDER_COL_FIRST;
        int32_t datatype = vec->traits.datatype;
        int64_t nrows = (int64_t)vec->context->gnrows;
        int64_t ncols = (int64_t)vec->traits.ncols;
        MPI_File fileh;
        MPI_Status status;
        MPI_CALL_RETURN(MPI_File_open(vec->context->mpicomm,path,MPI_MODE_WRONLY|MPI_MODE_CREATE,MPI_INFO_NULL,&fileh));

        if (rank == 0) 
        { // write header AND portion
            MPI_CALL_RETURN(MPI_File_write(fileh,&endianess,1,MPI_INT,&status));
            MPI_CALL_RETURN(MPI_File_write(fileh,&version,1,MPI_INT,&status));
            MPI_CALL_RETURN(MPI_File_write(fileh,&order,1,MPI_INT,&status));
            MPI_CALL_RETURN(MPI_File_write(fileh,&datatype,1,MPI_INT,&status));
            MPI_CALL_RETURN(MPI_File_write(fileh,&nrows,1,MPI_LONG_LONG,&status));
            MPI_CALL_RETURN(MPI_File_write(fileh,&ncols,1,MPI_LONG_LONG,&status));

        }    
        ghost_lidx_t v;
        ghost_mpi_datatype_t mpidt;
        GHOST_CALL_RETURN(ghost_mpi_datatype(&mpidt,vec->traits.datatype));
        MPI_CALL_RETURN(MPI_File_set_view(fileh,4*sizeof(int32_t)+2*sizeof(int64_t),mpidt,mpidt,"native",MPI_INFO_NULL));
        MPI_Offset fileoffset = vec->context->lfRow[rank];
        ghost_lidx_t vecoffset = 0;
        for (v=0; v<vec->traits.ncols; v++) {
            char *val = NULL;
            int copied = 0;
            if (vec->traits.flags & GHOST_DENSEMAT_HOST)
            {
                vec->download(vec);
                val = VECVAL_CM(vec,vec->val,v,0);
            }
            else if (vec->traits.flags & GHOST_DENSEMAT_DEVICE)
            {
#ifdef GHOST_HAVE_CUDA
                GHOST_CALL_RETURN(ghost_malloc((void **)&val,vec->traits.nrows*vec->elSize));
                copied = 1;
                ghost_cu_download(val,&vec->cu_val[v*vec->traits.nrowspadded*vec->elSize],vec->traits.nrows*vec->elSize);
#endif
            }
            MPI_CALL_RETURN(MPI_File_write_at(fileh,fileoffset,val,vec->traits.nrows,mpidt,&status));
            fileoffset += nrows;
            vecoffset += vec->traits.nrowspadded*vec->elSize;
            if (copied)
                free(val);
        }
        MPI_CALL_RETURN(MPI_File_close(&fileh));
#endif
    } else {
        DEBUG_LOG(1,"Writing (local) vector to file %s",path);
        size_t ret;

        int32_t endianess = ghost_machine_bigendian();
        int32_t version = 1;
        int32_t order = GHOST_BINDENSEMAT_ORDER_COL_FIRST;
        int32_t datatype = vec->traits.datatype;
        int64_t nrows = (int64_t)vec->traits.nrows;
        int64_t ncols = (int64_t)vec->traits.ncols;

        FILE *filed;

        if ((filed = fopen64(path, "w")) == NULL){
            ERROR_LOG("Could not open vector file %s: %s",path,strerror(errno));
            return GHOST_ERR_IO;
        }

        if ((ret = fwrite(&endianess,sizeof(endianess),1,filed)) != 1) {
            ERROR_LOG("fwrite failed: %zu",ret);
            fclose(filed);
            return GHOST_ERR_IO;
        }
        if ((ret = fwrite(&version,sizeof(version),1,filed)) != 1) {
            ERROR_LOG("fwrite failed: %zu",ret);
            fclose(filed);
            return GHOST_ERR_IO;
        }
        if ((ret = fwrite(&order,sizeof(order),1,filed)) != 1) {
            ERROR_LOG("fwrite failed: %zu",ret);
            fclose(filed);
            return GHOST_ERR_IO;
        }
        if ((ret = fwrite(&datatype,sizeof(datatype),1,filed)) != 1) {
            ERROR_LOG("fwrite failed: %zu",ret);
            fclose(filed);
            return GHOST_ERR_IO;
        }
        if ((ret = fwrite(&nrows,sizeof(nrows),1,filed)) != 1) {
            ERROR_LOG("fwrite failed: %zu",ret);
            fclose(filed);
            return GHOST_ERR_IO;
        }
        if ((ret = fwrite(&ncols,sizeof(ncols),1,filed)) != 1) {
            ERROR_LOG("fwrite failed: %zu",ret);
            fclose(filed);
            return GHOST_ERR_IO;
        }

        ghost_lidx_t v;
        for (v=0; v<vec->traits.ncols; v++) {
            char *val = NULL;
            int copied = 0;
            if (vec->traits.flags & GHOST_DENSEMAT_HOST)
            {
                vec->download(vec);
                val = VECVAL_CM(vec,vec->val,v,0);
            }
            else if (vec->traits.flags & GHOST_DENSEMAT_DEVICE)
            {
#ifdef GHOST_HAVE_CUDA
                GHOST_CALL_RETURN(ghost_malloc((void **)&val,vec->traits.nrows*vec->elSize));
                copied = 1;
                ghost_cu_download(val,&vec->cu_val[v*vec->traits.nrowspadded*vec->elSize],vec->traits.nrows*vec->elSize);
#endif
            }

            if ((ret = fwrite(val, vec->elSize, vec->traits.nrows,filed)) != (size_t)vec->traits.nrows) {
                ERROR_LOG("fwrite failed: %zu",ret);
                fclose(filed);
                if (copied) {
                    free(val); val = NULL;
                }
                return GHOST_ERR_IO;
            }

            if (copied) {
                free(val);
            }
        }
        fclose(filed);
    }

    return GHOST_SUCCESS;

}

static ghost_error_t vec_cm_fromFile(ghost_densemat_t *vec, char *path, bool singleFile)
{

#ifndef GHOST_HAVE_MPI
    singleFile = false;
#endif

    int rank;
    GHOST_CALL_RETURN(ghost_rank(&rank, vec->context->mpicomm));

    off_t offset;
    off_t stride;
    if ((vec->context == NULL) || !(vec->context->flags & GHOST_CONTEXT_DISTRIBUTED) || !singleFile) {
        offset = 0;
        stride = 0;
    } else {
        offset = vec->context->lfRow[rank];
        stride = vec->context->gnrows-vec->context->lnrows[rank];
    }

    ghost_densemat_cm_malloc(vec);
    DEBUG_LOG(1,"Reading vector from file %s",path);

    FILE *filed;
    size_t ret;

    if ((filed = fopen64(path, "r")) == NULL){
        ERROR_LOG("Could not open vector file %s: %s",path,strerror(errno));
        vec->destroy(vec);
        return GHOST_ERR_IO;
    }

    int32_t endianess;
    int32_t version;
    int32_t order;
    int32_t datatype;

    int64_t nrows;
    int64_t ncols;


    if ((ret = fread(&endianess, sizeof(endianess), 1,filed)) != 1) {
        ERROR_LOG("fread failed: %zu",ret);
        vec->destroy(vec);
        return GHOST_ERR_IO;
    }

    if (endianess != GHOST_BINDENSEMAT_LITTLE_ENDIAN) {
        ERROR_LOG("Cannot read big endian vectors");
        vec->destroy(vec);
        return GHOST_ERR_IO;
    }

    if ((ret = fread(&version, sizeof(version), 1,filed)) != 1) {
        ERROR_LOG("fread failed: %zu",ret);
        vec->destroy(vec);
        return GHOST_ERR_IO;
    }

    if (version != 1) {
        ERROR_LOG("Cannot read vector files with format != 1 (is %d)",version);
        vec->destroy(vec);
        return GHOST_ERR_IO;
    }

    if ((ret = fread(&order, sizeof(order), 1,filed)) != 1) {
        ERROR_LOG("fread failed: %zu",ret);
        vec->destroy(vec);
        return GHOST_ERR_IO;
    }
    if (order != GHOST_BINDENSEMAT_ORDER_COL_FIRST) {
        ERROR_LOG("Can only read col-major files!");
        return GHOST_ERR_IO;
    }

    if ((ret = fread(&datatype, sizeof(datatype), 1,filed)) != 1) {
        ERROR_LOG("fread failed: %zu",ret);
        vec->destroy(vec);
        return GHOST_ERR_IO;
    }

    if (datatype != (int)vec->traits.datatype) {
        ERROR_LOG("The data types don't match! Cast-while-read is not yet implemented for vectors.");
        return GHOST_ERR_IO;
    }

    if ((ret = fread(&nrows, sizeof(nrows), 1,filed)) != 1) {
        ERROR_LOG("fread failed: %zu",ret);
        vec->destroy(vec);
        return GHOST_ERR_IO;
    }
    // I will read as many rows as the vector has

    if ((ret = fread(&ncols, sizeof(ncols), 1,filed)) != 1) {
        ERROR_LOG("fread failed: %zu",ret);
        vec->destroy(vec);
        return GHOST_ERR_IO;
    }
    
    if (!singleFile && (vec->traits.nrows != nrows)) {
        ERROR_LOG("The number of rows does not match between the file and the densemat!");
        return GHOST_ERR_IO;
    }
    if (singleFile && (vec->context->gnrows != nrows)) {
        ERROR_LOG("The number of rows does not match between the file and the densemat's context!");
        return GHOST_ERR_IO;
    }
    if (fseeko(filed,offset*vec->elSize,SEEK_CUR)) {
        ERROR_LOG("seek failed");
        vec->destroy(vec);
        return GHOST_ERR_IO;
    }

    int v;
    for (v=0; v<vec->traits.ncols; v++) {
        if (vec->traits.flags & GHOST_DENSEMAT_HOST)
        {
            if ((ghost_lidx_t)(ret = fread(VECVAL_CM(vec,vec->val,v,0), vec->elSize, vec->traits.nrows,filed)) != vec->traits.nrows) {
                ERROR_LOG("fread failed: %zu",ret);
                vec->destroy(vec);
                return GHOST_ERR_IO;
            }
            vec->upload(vec);
        }
        else if (vec->traits.flags & GHOST_DENSEMAT_DEVICE)
        {
#ifdef GHOST_HAVE_CUDA
            char *val;
            GHOST_CALL_RETURN(ghost_malloc((void **)&val,vec->traits.nrows*vec->elSize));
            if ((ret = fread(val, vec->elSize, vec->traits.nrows,filed)) != vec->traits.nrows) {
                ERROR_LOG("fread failed: %zu",ret);
                vec->destroy(vec);
                return GHOST_ERR_IO;
            }
            ghost_cu_upload(&vec->cu_val[v*vec->traits.nrowspadded*vec->elSize],val,vec->traits.nrows*vec->elSize);
            free(val);
#endif
        }
        else
        {
            WARNING_LOG("Invalid vector placement, not writing vector");
            fclose(filed);
        }

        if (fseeko(filed,stride*vec->elSize,SEEK_CUR)) {
            ERROR_LOG("seek failed");
            vec->destroy(vec);
            return GHOST_ERR_IO;
        }
    }

    fclose(filed);

    return GHOST_SUCCESS;

}

static ghost_error_t vec_cm_fromFunc(ghost_densemat_t *vec, void (*fp)(ghost_gidx_t, ghost_lidx_t, void *))
{
    int rank;
    ghost_gidx_t offset;
    if (vec->context) {
        GHOST_CALL_RETURN(ghost_rank(&rank, vec->context->mpicomm));
        offset = vec->context->lfRow[rank];
    } else {
        rank = 0;
        offset = 0;
    }
    GHOST_CALL_RETURN(ghost_densemat_cm_malloc(vec));
    DEBUG_LOG(1,"Filling vector via function");

    ghost_lidx_t row,col,rowidx;

    if (vec->traits.flags & GHOST_DENSEMAT_HOST) { // vector is stored _at least_ at host
        ITER_BEGIN_CM(vec,col,row,rowidx)
        fp(offset+rowidx,col,VECVAL_CM(vec,vec->val,col,row));
        ITER_END_CM(rowidx)
        vec->upload(vec);
    } else {
        ghost_densemat_t *hostVec;
        ghost_densemat_traits_t htraits = vec->traits;
        htraits.flags &= ~GHOST_DENSEMAT_DEVICE;
        htraits.flags |= GHOST_DENSEMAT_HOST;
        GHOST_CALL_RETURN(ghost_densemat_create(&hostVec,vec->context,htraits));
        GHOST_CALL_RETURN(hostVec->fromFunc(hostVec,fp));
        GHOST_CALL_RETURN(vec->fromVec(vec,hostVec,0,0));
        hostVec->destroy(hostVec);
    }

    return GHOST_SUCCESS;
}

static ghost_error_t ghost_distributeVector(ghost_densemat_t *vec, ghost_densemat_t *nodeVec)
{
    DEBUG_LOG(1,"Distributing vector");
    int me;
    int nprocs;
    GHOST_CALL_RETURN(ghost_rank(&me, nodeVec->context->mpicomm));
    GHOST_CALL_RETURN(ghost_nrank(&nprocs, nodeVec->context->mpicomm));
    
    bool uniformstorage;
    GHOST_CALL_RETURN(ghost_densemat_uniformstorage(&uniformstorage,vec));
    if (!uniformstorage) {
        ERROR_LOG("Cannot collect vectors of different storage order");
        return GHOST_ERR_INVALID_ARG;
    }

    ghost_lidx_t c;
#ifdef GHOST_HAVE_MPI
    DEBUG_LOG(2,"Scattering global vector to local vectors");

    ghost_mpi_datatype_t mpidt;
    GHOST_CALL_RETURN(ghost_mpi_datatype(&mpidt,vec->traits.datatype));

    int i;

    MPI_Request req[vec->traits.ncols*2*(nprocs-1)];
    MPI_Status stat[vec->traits.ncols*2*(nprocs-1)];
    int msgcount = 0;

    for (i=0;i<vec->traits.ncols*2*(nprocs-1);i++) 
        req[i] = MPI_REQUEST_NULL;

    if (me != 0) {
        for (c=0; c<vec->traits.ncols; c++) {
            MPI_CALL_RETURN(MPI_Irecv(nodeVec->val[c],nodeVec->context->lnrows[me],mpidt,0,me,nodeVec->context->mpicomm,&req[msgcount]));
            msgcount++;
        }
    } else {
        for (c=0; c<vec->traits.ncols; c++) {
            memcpy(nodeVec->val[c],vec->val[c],vec->elSize*nodeVec->context->lnrows[0]);
            for (i=1;i<nprocs;i++) {
                MPI_CALL_RETURN(MPI_Isend(VECVAL_CM(vec,vec->val,c,nodeVec->context->lfRow[i]),nodeVec->context->lnrows[i],mpidt,i,i,nodeVec->context->mpicomm,&req[msgcount]));
                msgcount++;
            }
        }
    }
    MPI_CALL_RETURN(MPI_Waitall(msgcount,req,stat));
#else

    for (c=0; c<vec->traits.ncols; c++) {
        memcpy(nodeVec->val[c],vec->val[c],vec->traits.nrows*vec->elSize);
    }
    //    *nodeVec = vec->clone(vec);
#endif

    nodeVec->upload(nodeVec);

    DEBUG_LOG(1,"Vector distributed successfully");

    return GHOST_SUCCESS;
}

static ghost_error_t ghost_collectVectors(ghost_densemat_t *vec, ghost_densemat_t *totalVec) 
{
    ghost_lidx_t c;
#ifdef GHOST_HAVE_MPI
    int me;
    int nprocs;
    ghost_mpi_datatype_t mpidt;
    GHOST_CALL_RETURN(ghost_rank(&me, vec->context->mpicomm));
    GHOST_CALL_RETURN(ghost_nrank(&nprocs, vec->context->mpicomm));
    GHOST_CALL_RETURN(ghost_mpi_datatype(&mpidt,vec->traits.datatype));

    bool uniformstorage;
    GHOST_CALL_RETURN(ghost_densemat_uniformstorage(&uniformstorage,vec));
    if (!uniformstorage) {
        ERROR_LOG("Cannot collect vectors of different storage order");
        return GHOST_ERR_INVALID_ARG;
    }
//    if (vec->context != NULL)
//        vec->permute(vec,vec->context->invRowPerm); 

    int i;

    MPI_Request req[vec->traits.ncols*2*(nprocs-1)];
    MPI_Status stat[vec->traits.ncols*2*(nprocs-1)];
    int msgcount = 0;

    for (i=0;i<vec->traits.ncols*2*(nprocs-1);i++) 
        req[i] = MPI_REQUEST_NULL;

    if (me != 0) {
        for (c=0; c<vec->traits.ncols; c++) {
            MPI_CALL_RETURN(MPI_Isend(vec->val[c],vec->context->lnrows[me],mpidt,0,me,vec->context->mpicomm,&req[msgcount]));
            msgcount++;
        }
    } else {
        for (c=0; c<vec->traits.ncols; c++) {
            memcpy(totalVec->val[c],vec->val[c],vec->elSize*vec->context->lnrows[0]);
            for (i=1;i<nprocs;i++) {
                MPI_CALL_RETURN(MPI_Irecv(VECVAL_CM(totalVec,totalVec->val,c,vec->context->lfRow[i]),vec->context->lnrows[i],mpidt,i,i,vec->context->mpicomm,&req[msgcount]));
                msgcount++;
            }
        }
    }
    MPI_CALL_RETURN(MPI_Waitall(msgcount,req,stat));
#else
    if (vec->context != NULL) {
//        vec->permute(vec,vec->context->invRowPerm);
        for (c=0; c<vec->traits.ncols; c++) {
            memcpy(totalVec->val[c],vec->val[c],totalVec->traits.nrows*vec->elSize);
        }
    }
#endif

    return GHOST_SUCCESS;

}

static void ghost_freeVector( ghost_densemat_t* vec ) 
{
    if( vec ) {
        if (!(vec->traits.flags & GHOST_DENSEMAT_VIEW)) {
            ghost_lidx_t v;
#ifdef GHOST_HAVE_CUDA_PINNEDMEM
            WARNING_LOG("CUDA pinned memory is disabled");
            /*if (vec->traits.flags & GHOST_DENSEMAT_DEVICE) {
              for (v=0; v<vec->traits.ncols; v++) { 
              ghost_cu_safecall(cudaFreeHost(vec->val[v]));
              }
              }*/
            if (vec->traits.flags & GHOST_DENSEMAT_SCATTERED) {
                for (v=0; v<vec->traits.ncols; v++) {
                    free(vec->val[v]); vec->val[v] = NULL
                }
            }
            else {
                free(vec->val[0]); vec->val[0] = NULL;
            }
#else
            //note: a 'scattered' vector (one with non-constant stride) is
            //      always a view of some other (there is no method to                        
            //      construct it otherwise), but we check anyway in case
            //      the user has built his own funny vector in memory.
            if (vec->traits.flags & GHOST_DENSEMAT_SCATTERED) {
                for (v=0; v<vec->traits.ncols; v++) {
                    free(vec->val[v]); vec->val[v] = NULL;
                }
            }
            else {
                free(vec->val[0]); vec->val[0] = NULL;
            }
#endif
#ifdef GHOST_HAVE_CUDA
            if (vec->traits.flags & GHOST_DENSEMAT_DEVICE) {
                ghost_cu_free(vec->cu_val);
            }
#endif
        }
        free(vec->val); vec->val = NULL;
        ghost_bitmap_free(vec->ldmask);
        ghost_bitmap_free(vec->trmask);
#ifdef GHOST_HAVE_MPI
        MPI_Type_free(&vec->row_mpidt);
#endif
        free(vec);
    }
}
static ghost_error_t ghost_permuteVector( ghost_densemat_t* vec, ghost_permutation_t *permutation, ghost_permutation_direction_t dir) 
{
    // TODO enhance performance
    
    if (!permutation) {
        return GHOST_SUCCESS;
    }

    ghost_lidx_t i;
    ghost_lidx_t len, c;
    char* tmp = NULL;
    ghost_densemat_t *permvec = NULL;
    ghost_densemat_t *combined = NULL; 
    ghost_densemat_traits_t traits;

    if (permutation->len > vec->traits.nrows && !vec->context) {
        ERROR_LOG("The permutation scope is larger than the vector but the vector does not have a context (i.e.,\
            process-local vectors cannot be combined to a big vector for permuting.");
        return GHOST_ERR_INVALID_ARG;
    }
    if (permutation->len > vec->traits.nrows && vec->context->gnrows != permutation->len) {
        ERROR_LOG("The permutation scope and the context size do not match!");
        return GHOST_ERR_INVALID_ARG;
    }

    if ((vec->traits.storage & GHOST_DENSEMAT_DEVICE) && !(vec->traits.storage & GHOST_DENSEMAT_HOST)) {
        ERROR_LOG("Permutation of pure device vectors not yet implemented!");
        return GHOST_ERR_NOT_IMPLEMENTED;
    }
    
    vec->downloadNonHalo(vec);

    if (permutation->scope == GHOST_PERMUTATION_GLOBAL && vec->traits.nrows != permutation->len) {
        traits = vec->traits;
        traits.nrows = vec->context->gnrows;
        traits.flags = GHOST_DENSEMAT_HOST;
        char zero[vec->elSize];
        memset(zero,0,vec->elSize);

        ghost_densemat_create(&combined,vec->context,traits);
        combined->fromScalar(combined,&zero);
        vec->collect(vec,combined);
        permvec = combined;

        WARNING_LOG("Global permutation not tested");
    } else {
        permvec = vec;
    }
    if (permvec->traits.nrows != permutation->len) {
        WARNING_LOG("Lenghts do not match: vec has %d rows, permutation has length %d",permvec->traits.nrows,permutation->len);
        return GHOST_ERR_INVALID_ARG;
    }
    len = permvec->traits.nrows;

    ghost_gidx_t *perm = NULL;
    if (dir == GHOST_PERMUTATION_ORIG2PERM) {
        perm = permutation->perm;
    } else {
        perm = permutation->invPerm;
    }

    if (perm == NULL) {
        DEBUG_LOG(1,"Permutation vector is NULL, returning.");
        return GHOST_SUCCESS;
    } else {
        DEBUG_LOG(1,"Permuting vector");
    }


    for (c=0; c<permvec->traits.ncols; c++) {
        GHOST_CALL_RETURN(ghost_malloc((void **)&tmp,permvec->elSize*len));
        for(i = 0; i < len; ++i) {
            if( perm[i] >= len ) {
                ERROR_LOG("Permutation index out of bounds: %"PRGIDX" > %"PRLIDX,perm[i],len);
                free(tmp);
                return GHOST_ERR_UNKNOWN;
            }

            memcpy(&tmp[vec->elSize*perm[i]],VECVAL_CM(permvec,permvec->val,c,i),permvec->elSize);
        }
        for(i=0; i < len; ++i) {
            memcpy(VECVAL_CM(permvec,permvec->val,c,i),&tmp[permvec->elSize*i],permvec->elSize);
        }
        free(tmp);
    }
    
    if (permutation->scope == GHOST_PERMUTATION_GLOBAL && vec->traits.nrows != permutation->len) {
        INFO_LOG("Re-distributing globally permuted vector");
        permvec->distribute(permvec,vec);
        permvec->destroy(permvec);
    }

    vec->uploadNonHalo(vec);

    if (dir == GHOST_PERMUTATION_ORIG2PERM) {
        vec->traits.flags |= GHOST_DENSEMAT_PERMUTED;
    } else {
        vec->traits.flags &= ~GHOST_DENSEMAT_PERMUTED;
    }

    return GHOST_SUCCESS;
}

static ghost_error_t ghost_cloneVector(ghost_densemat_t *src, ghost_densemat_t **new, ghost_lidx_t nr, ghost_lidx_t roffs, ghost_lidx_t nc, ghost_lidx_t coffs)
{
    ghost_densemat_traits_t newTraits = src->traits;
    newTraits.ncols = nc;
    newTraits.ncolsorig = nc;
    newTraits.nrows = nr;
    newTraits.nrowsorig = nr;
    ghost_densemat_create(new,src->context,newTraits);

    // copy the data even if the input vector is itself a view
    // (bitwise NAND operation to unset the view flag if set)
    (*new)->traits.flags &= ~GHOST_DENSEMAT_VIEW;
    (*new)->traits.flags &= ~GHOST_DENSEMAT_SCATTERED;

    (*new)->fromVec(*new,src,roffs,coffs);
    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_compress(ghost_densemat_t *vec)
{
    if (!(vec->traits.flags & GHOST_DENSEMAT_SCATTERED)) {
        return GHOST_SUCCESS;
    }

    if (vec->traits.flags & GHOST_DENSEMAT_HOST) {
        ghost_lidx_t v,i;

        char *val = NULL;
        GHOST_CALL_RETURN(ghost_malloc((void **)&val,vec->traits.nrowspadded*vec->traits.ncols*vec->elSize));

#pragma omp parallel for schedule(runtime) private(v)
        for (i=0; i<vec->traits.nrowspadded; i++)
        {
            for (v=0; v<vec->traits.ncols; v++)
            {
                val[(v*vec->traits.nrowspadded+i)*vec->elSize] = 0;
            }
        }

        for (v=0; v<vec->traits.ncols; v++)
        {
            memcpy(&val[(v*vec->traits.nrowspadded)*vec->elSize],
                    VECVAL_CM(vec,vec->val,v,0),vec->traits.nrowspadded*vec->elSize);

            if (!(vec->traits.flags & GHOST_DENSEMAT_VIEW)) {
                free(vec->val[v]);
            }
            vec->val[v] = &val[(v*vec->traits.nrowspadded)*vec->elSize];
        }
    }
    if (vec->traits.flags & GHOST_DENSEMAT_DEVICE) {
#ifdef GHOST_HAVE_CUDA
        ghost_lidx_t v,i,r,j;

        char *cu_val;
        GHOST_CALL_RETURN(ghost_cu_malloc((void **)&cu_val,vec->traits.nrowspadded*vec->traits.ncols*vec->elSize));

        for (v=0, i=0; v<vec->traits.ncolsorig; v++) {
            if (ghost_bitmap_isset(vec->trmask,v)) {
                for (r=0, j=0; r<vec->traits.nrowsorig; r++) {
                    if (ghost_bitmap_isset(vec->ldmask,r)) {
                        ghost_cu_memcpy(&cu_val[(i*vec->traits.nrowspadded+j)*vec->elSize],
                                &vec->cu_val[(v*vec->traits.nrowspadded+r)*vec->elSize],
                                vec->elSize);
                        j++;
                    }
                }
                i++;
            }
        }
        if (!(vec->traits.flags & GHOST_DENSEMAT_VIEW)) {
            ghost_cu_free(vec->cu_val);
        }
        vec->cu_val = cu_val;
#endif 
    }

    ghost_bitmap_set_range(vec->ldmask,0,vec->traits.nrows-1);
    ghost_bitmap_set_range(vec->trmask,0,vec->traits.ncols-1);
    vec->traits.flags &= ~GHOST_DENSEMAT_VIEW;
    vec->traits.flags &= ~GHOST_DENSEMAT_SCATTERED;
    vec->traits.ncolsorig = vec->traits.ncols;
    vec->traits.nrowsorig = vec->traits.nrows;

    vec->viewing = NULL;

    return GHOST_SUCCESS;
}

ghost_error_t ghost_densemat_traits_clone(ghost_densemat_traits_t *t1, ghost_densemat_traits_t **t2)
{
    GHOST_CALL_RETURN(ghost_malloc((void **)t2,sizeof(ghost_densemat_traits_t)));
    memcpy(*t2,t1,sizeof(ghost_densemat_traits_t));

    return GHOST_SUCCESS;
}

static ghost_error_t vec_cm_averageHalo(ghost_densemat_t *vec)
{

    return ghost_densemat_cm_averagehalo(vec);

}
