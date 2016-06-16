#include <stdlib.h>
#include "ghost/sparsemat.h"
#include "ghost/util.h"
#include "ghost/mmio.h"
#include "ghost/matrixmarket.h"
#include "ghost/locality.h"

int ghost_sparsemat_rowfunc_mm(ghost_gidx row, ghost_lidx *rowlen, ghost_gidx *col, void *val, void *arg)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_INITIALIZATION);
    static ghost_gidx *colInd = NULL, *rowPtr = NULL;
    static char *values = NULL;
    static size_t dtsize = 0;

    if (row == GHOST_SPARSEMAT_ROWFUNC_MM_ROW_GETDIM) {
        ghost_sparsemat_rowfunc_mm_initargs args = 
            *(ghost_sparsemat_rowfunc_mm_initargs *)val;
        char *filename = args.filename;

        FILE *f;
        int ret_code;
        int M, N, nz;
        MM_typecode matcode;

        if ((f = fopen(filename,"r")) == NULL) {
            ERROR_LOG("fopen with %s failed!",filename);
            return 1;
        }

        if (mm_read_banner(f, &matcode) != 0){
            ERROR_LOG("Could not process Matrix Market banner!");
            return 1;
        }
        
        if (rowlen){
            if (mm_is_complex(matcode)) *rowlen = (ghost_lidx)GHOST_DT_COMPLEX;
            else *rowlen = (ghost_lidx)GHOST_DT_REAL;
        }

        if((ret_code = mm_read_mtx_crd_size(f, &M, &N, &nz)) != 0){
            ERROR_LOG("Could not read header!");
            return 1;
        }
        col[0] = M;
        col[1] = N;
        
        fclose(f);
    } else if ((row == GHOST_SPARSEMAT_ROWFUNC_MM_ROW_GETRPT) || (row == GHOST_SPARSEMAT_ROWFUNC_MM_ROW_INIT)) {

        ghost_sparsemat_rowfunc_mm_initargs args = 
            *(ghost_sparsemat_rowfunc_mm_initargs *)val;
        char *filename = args.filename;
        ghost_datatype matdt = args.dt;

        ghost_datatype_size(&dtsize,matdt);

        FILE *f;
        int ret_code;
        MM_typecode matcode;
        int M, N, nz, actualnz;
        ghost_gidx * offset;
        ghost_gidx i;
        int symm = 0;

        if ((f = fopen(filename,"r")) == NULL) {
            ERROR_LOG("fopen with %s failed!",filename);
            return 1;
        }

        if (mm_read_banner(f, &matcode) != 0)
        {
            ERROR_LOG("Could not process Matrix Market banner!");
            return 1;
        }
        if ((mm_is_complex(matcode) && !(matdt & GHOST_DT_COMPLEX)) ||
                (!mm_is_complex(matcode) && (matdt & GHOST_DT_COMPLEX))) {
            ERROR_LOG("On-the-fly casting between real and complex not implemented!");
            return 1;
        }
        if (!mm_is_general(matcode) && !mm_is_symmetric(matcode)) {
            ERROR_LOG("Only general and symmetric matrices supported at the moment!");
            return 1;
        }
        if (mm_is_pattern(matcode)) {
            ERROR_LOG("Pattern matrices not supported!");
            return 1;
        }

        if((ret_code = mm_read_mtx_crd_size(f, &M, &N, &nz)) != 0){
            ERROR_LOG("Could not read header!");
            return 1;
        }
        if (M < 0 || N < 0 || nz < 0) {
            ERROR_LOG("Probably integer overflow");
            return 1;
        }

        if (mm_is_symmetric(matcode)) {
            PERFWARNING_LOG("Will create a general matrix out of a symmetric matrix!");
            *(int *)arg = 1;
            actualnz = nz*2;
            symm = 1;
        } else {
            actualnz = nz;
            symm = 0;
        }


        ghost_malloc((void **)&colInd,actualnz * sizeof(ghost_gidx));
        ghost_malloc((void **)&rowPtr,(M + 1) * sizeof(ghost_gidx));
        ghost_malloc((void **)&values,actualnz * dtsize);
        ghost_malloc((void **)&offset,(M + 1) * sizeof(ghost_gidx));

        for(i = 0; i <= M; ++i){
            rowPtr[i] = 0;
            offset[i] = 0;
        }

        ghost_gidx readrow,readcol;
        char value[dtsize];
        fpos_t pos;
        fgetpos(f,&pos);

        for (i = 0; i < nz; ++i){
            if (matdt & GHOST_DT_COMPLEX) {
                if (matdt & GHOST_DT_DOUBLE) {
                    fscanf(f, "%"PRGIDX" %"PRGIDX" %lg %lg\n", &readrow,&readcol,(double *)value,(double *)(value+dtsize/2));
                } else {
                    fscanf(f, "%"PRGIDX" %"PRGIDX" %g %g\n", &readrow,&readcol,(float *)value,(float *)(value+dtsize/2));
                }
            } else {
                if (matdt & GHOST_DT_DOUBLE) {
                    fscanf(f, "%"PRGIDX" %"PRGIDX" %lg\n", &readrow,&readcol,(double *)value);
                } else {
                    fscanf(f, "%"PRGIDX" %"PRGIDX" %g\n", &readrow,&readcol,(float *)value);
                }
            }
            readcol--;
            readrow--;
            rowPtr[readrow+1]++;

            if (symm) {
               if (readrow != readcol) {
                   rowPtr[readcol+1]++; // insert sibling entry
               } else {
                   actualnz--; // do not count diagonal entries twice
               }
            }
        }

        for(i = 1; i<=M; ++i){
            rowPtr[i] += rowPtr[i-1];
        }

        if (row == GHOST_SPARSEMAT_ROWFUNC_MM_ROW_GETRPT) {
            col = rowPtr;
        } else {

            fsetpos(f,&pos);

            for (i = 0; i < nz; ++i){
                if (matdt & GHOST_DT_COMPLEX) {
                    if (matdt & GHOST_DT_DOUBLE) {
                        fscanf(f, "%"PRGIDX" %"PRGIDX" %lg %lg\n", &readrow,&readcol,(double *)value,(double *)(value+dtsize/2));
                    } else {
                        fscanf(f, "%"PRGIDX" %"PRGIDX" %g %g\n", &readrow,&readcol,(float *)value,(float *)(value+dtsize/2));
                    }
                } else {
                    if (matdt & GHOST_DT_DOUBLE) {
                        fscanf(f, "%"PRGIDX" %"PRGIDX" %lg\n", &readrow,&readcol,(double *)value);
                    } else {
                        fscanf(f, "%"PRGIDX" %"PRGIDX" %g\n", &readrow,&readcol,(float *)value);
                    }
                }
                readrow--;
                readcol--;

                memcpy(&values[(rowPtr[readrow] + offset[readrow])*dtsize],value,dtsize);
                colInd[rowPtr[readrow] + offset[readrow]] = readcol;
                offset[readrow]++;
                
                if (symm && (readrow != readcol)) {
                    memcpy(&values[(rowPtr[readcol] + offset[readcol])*dtsize],value,dtsize);
                    colInd[rowPtr[readcol] + offset[readcol]] = readrow;
                    offset[readcol]++;
                }

            }
        }


        free(offset);
        fclose(f);

    } else if (row == GHOST_SPARSEMAT_ROWFUNC_MM_ROW_FINALIZE) {
        free(colInd);
        free(rowPtr);
        free(values);
    } else {
        *rowlen = rowPtr[row+1]-rowPtr[row];
        memcpy(col,&colInd[rowPtr[row]],(*rowlen)*sizeof(ghost_gidx));
        memcpy(val,&values[rowPtr[row]*dtsize],(*rowlen)*dtsize);
    }


    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_INITIALIZATION);
    return 0;


}

int ghost_sparsemat_rowfunc_mm_transpose(ghost_gidx row, ghost_lidx *rowlen, ghost_gidx *col, void *val, void *arg)
{
    GHOST_FUNC_ENTER(GHOST_FUNCTYPE_INITIALIZATION);
    static ghost_gidx *colInd = NULL, *rowPtr = NULL;
    static char *values = NULL;
    static size_t dtsize = 0;

    if (row == GHOST_SPARSEMAT_ROWFUNC_MM_ROW_GETDIM) {
        ghost_sparsemat_rowfunc_mm_initargs args = 
            *(ghost_sparsemat_rowfunc_mm_initargs *)val;
        char *filename = args.filename;

        FILE *f;
        int ret_code;
        int M, N, nz;
        MM_typecode matcode;

        if ((f = fopen(filename,"r")) == NULL) {
            ERROR_LOG("fopen with %s failed!",filename);
            return 1;
        }

        if (mm_read_banner(f, &matcode) != 0){
            ERROR_LOG("Could not process Matrix Market banner!");
            return 1;
        }
        
        if (rowlen){
            if (mm_is_complex(matcode)) *rowlen = (ghost_lidx)GHOST_DT_COMPLEX;
            else *rowlen = (ghost_lidx)GHOST_DT_REAL;
        }

        if((ret_code = mm_read_mtx_crd_size(f, &N, &M, &nz)) != 0){
            ERROR_LOG("Could not read header!");
            return 1;
        }
        col[0] = M;
        col[1] = N;
        
        fclose(f);
    } else if ((row == GHOST_SPARSEMAT_ROWFUNC_MM_ROW_GETRPT) || (row == GHOST_SPARSEMAT_ROWFUNC_MM_ROW_INIT)) {

        ghost_sparsemat_rowfunc_mm_initargs args = 
            *(ghost_sparsemat_rowfunc_mm_initargs *)val;
        char *filename = args.filename;
        ghost_datatype matdt = args.dt;

        ghost_datatype_size(&dtsize,matdt);

        FILE *f;
        int ret_code;
        MM_typecode matcode;
        int M, N, nz, actualnz;
        ghost_gidx * offset;
        ghost_gidx i;
        int symm = 0;

        if ((f = fopen(filename,"r")) == NULL) {
            ERROR_LOG("fopen with %s failed!",filename);
            return 1;
        }

        if (mm_read_banner(f, &matcode) != 0)
        {
            ERROR_LOG("Could not process Matrix Market banner!");
            return 1;
        }
        if ((mm_is_complex(matcode) && !(matdt & GHOST_DT_COMPLEX)) ||
                (!mm_is_complex(matcode) && (matdt & GHOST_DT_COMPLEX))) {
            ERROR_LOG("On-the-fly casting between real and complex not implemented!");
            return 1;
        }
        if (!mm_is_general(matcode) && !mm_is_symmetric(matcode)) {
            ERROR_LOG("Only general and symmetric matrices supported at the moment!");
            return 1;
        }
        if (mm_is_pattern(matcode)) {
            ERROR_LOG("Pattern matrices not supported!");
            return 1;
        }

        if((ret_code = mm_read_mtx_crd_size(f, &N, &M, &nz)) != 0){
            ERROR_LOG("Could not read header!");
            return 1;
        }
        if (M < 0 || N < 0 || nz < 0) {
            ERROR_LOG("Probably integer overflow");
            return 1;
        }

        if (mm_is_symmetric(matcode)) {
            PERFWARNING_LOG("Will create a general matrix out of a symmetric matrix!");
            *(int *)arg = 1;
            actualnz = nz*2;
            symm = 1;
        } else {
            actualnz = nz;
            symm = 0;
        }


        ghost_malloc((void **)&colInd,actualnz * sizeof(ghost_gidx));
        ghost_malloc((void **)&rowPtr,(M + 1) * sizeof(ghost_gidx));
        ghost_malloc((void **)&values,actualnz * dtsize);
        ghost_malloc((void **)&offset,(M + 1) * sizeof(ghost_gidx));

        for(i = 0; i <= M; ++i){
            rowPtr[i] = 0;
            offset[i] = 0;
        }

        ghost_gidx readrow,readcol;
        char value[dtsize];
        fpos_t pos;
        fgetpos(f,&pos);

        for (i = 0; i < nz; ++i){
            if (matdt & GHOST_DT_COMPLEX) {
                if (matdt & GHOST_DT_DOUBLE) {
                    fscanf(f, "%"PRGIDX" %"PRGIDX" %lg %lg\n", &readcol,&readrow,(double *)value,(double *)(value+dtsize/2));
                } else {
                    fscanf(f, "%"PRGIDX" %"PRGIDX" %g %g\n", &readcol,&readrow,(float *)value,(float *)(value+dtsize/2));
                }
            } else {
                if (matdt & GHOST_DT_DOUBLE) {
                    fscanf(f, "%"PRGIDX" %"PRGIDX" %lg\n", &readcol,&readrow,(double *)value);
                } else {
                    fscanf(f, "%"PRGIDX" %"PRGIDX" %g\n", &readcol,&readrow,(float *)value);
                }
            }
            readcol--;
            readrow--;
            rowPtr[readrow+1]++;

            if (symm) {
               if (readrow != readcol) {
                   rowPtr[readcol+1]++; // insert sibling entry
               } else {
                   actualnz--; // do not count diagonal entries twice
               }
            }
        }

        for(i = 1; i<=M; ++i){
            rowPtr[i] += rowPtr[i-1];
        }

        if (row == GHOST_SPARSEMAT_ROWFUNC_MM_ROW_GETRPT) {
            col = rowPtr;
        } else {

            fsetpos(f,&pos);

            for (i = 0; i < nz; ++i){
                if (matdt & GHOST_DT_COMPLEX) {
                    if (matdt & GHOST_DT_DOUBLE) {
                        fscanf(f, "%"PRGIDX" %"PRGIDX" %lg %lg\n", &readcol,&readrow,(double *)value,(double *)(value+dtsize/2));
                    } else {
                        fscanf(f, "%"PRGIDX" %"PRGIDX" %g %g\n", &readcol,&readrow,(float *)value,(float *)(value+dtsize/2));
                    }
                } else {
                    if (matdt & GHOST_DT_DOUBLE) {
                        fscanf(f, "%"PRGIDX" %"PRGIDX" %lg\n", &readcol,&readrow,(double *)value);
                    } else {
                        fscanf(f, "%"PRGIDX" %"PRGIDX" %g\n", &readcol,&readrow,(float *)value);
                    }
                }
                readrow--;
                readcol--;

                memcpy(&values[(rowPtr[readrow] + offset[readrow])*dtsize],value,dtsize);
                colInd[rowPtr[readrow] + offset[readrow]] = readcol;
                offset[readrow]++;
                
                if (symm && (readrow != readcol)) {
                    memcpy(&values[(rowPtr[readcol] + offset[readcol])*dtsize],value,dtsize);
                    colInd[rowPtr[readcol] + offset[readcol]] = readrow;
                    offset[readcol]++;
                }

            }
        }


        free(offset);
        fclose(f);

    } else if (row == GHOST_SPARSEMAT_ROWFUNC_MM_ROW_FINALIZE) {
        free(colInd);
        free(rowPtr);
        free(values);
    } else {
        *rowlen = rowPtr[row+1]-rowPtr[row];
        memcpy(col,&colInd[rowPtr[row]],(*rowlen)*sizeof(ghost_gidx));
        memcpy(val,&values[rowPtr[row]*dtsize],(*rowlen)*dtsize);
    }


    GHOST_FUNC_EXIT(GHOST_FUNCTYPE_INITIALIZATION);
    return 0;


}

ghost_error ghost_sparsemat_to_mm(char *path, ghost_sparsemat *mat)
{
    MM_typecode matcode;
    ghost_lidx row,entinrow;
    ghost_lidx sellidx;
    int nrank;
    FILE *fp = fopen(path,"w");

    if (!fp) {
        ERROR_LOG("Unable to open file %s!",path);
        return GHOST_ERR_INVALID_ARG;
    }
    
    ghost_nrank(&nrank,mat->context->mpicomm);
    
    if (nrank > 1 && !(mat->traits.flags & GHOST_SPARSEMAT_SAVE_ORIG_COLS)) {
        WARNING_LOG("The matrix is distributed and the non-compressed columns are not saved. The output will probably be useless!");
    }
    
    mm_initialize_typecode(&matcode);
    mm_set_matrix(&matcode);
    mm_set_coordinate(&matcode);
    if (mat->traits.datatype & GHOST_DT_REAL) {
        mm_set_real(&matcode);
    } else {
        mm_set_complex(&matcode);
    }

    mm_write_banner(fp,matcode);
    mm_write_mtx_crd_size(fp,mat->nrows,mat->ncols,mat->nnz);

    for (row=1; row<=mat->nrows; row++) {
        for (entinrow=0; entinrow<SELL(mat)->rowLen[row-1]; entinrow++) {
            sellidx = SELL(mat)->chunkStart[(row-1)/mat->traits.C] + entinrow*mat->traits.C + (row-1)%mat->traits.C;
            if (mat->traits.flags & GHOST_SPARSEMAT_SAVE_ORIG_COLS) {
                ghost_gidx col = mat->col_orig[sellidx]+1;
                if (mat->traits.datatype & GHOST_DT_REAL) {
                    if (mat->traits.datatype & GHOST_DT_DOUBLE) {
                        fprintf(fp,"%"PRLIDX" %"PRGIDX" %10.3g\n",row,col,((double *)SELL(mat)->val)[sellidx]);
                    } else {
                        fprintf(fp,"%"PRLIDX" %"PRGIDX" %10.3g\n",row,col,((float *)SELL(mat)->val)[sellidx]);
                    }
                } else {
                    if (mat->traits.datatype & GHOST_DT_DOUBLE) {
                        fprintf(fp,"%"PRLIDX" %"PRGIDX" %10.3g %10.3g\n",row,col,creal(((complex double *)SELL(mat)->val)[sellidx]),cimag(((complex double *)SELL(mat)->val)[sellidx]));
                    } else {
                        fprintf(fp,"%"PRLIDX" %"PRGIDX" %10.3g %10.3g\n",row,col,crealf(((complex float *)SELL(mat)->val)[sellidx]),cimagf(((complex float *)SELL(mat)->val)[sellidx]));
                    }
                }
            } else {
                ghost_lidx col = SELL(mat)->col[sellidx]+1;
                if (mat->traits.datatype & GHOST_DT_REAL) {
                    if (mat->traits.datatype & GHOST_DT_DOUBLE) {
                        fprintf(fp,"%"PRLIDX" %"PRLIDX" %10.3g\n",row,col,((double *)SELL(mat)->val)[sellidx]);
                    } else {
                        fprintf(fp,"%"PRLIDX" %"PRLIDX" %10.3g\n",row,col,((float *)SELL(mat)->val)[sellidx]);
                    }
                } else {
                    if (mat->traits.datatype & GHOST_DT_DOUBLE) {
                        fprintf(fp,"%"PRLIDX" %"PRLIDX" %10.3g %10.3g\n",row,col,creal(((complex double *)SELL(mat)->val)[sellidx]),cimag(((complex double *)SELL(mat)->val)[sellidx]));
                    } else {
                        fprintf(fp,"%"PRLIDX" %"PRLIDX" %10.3g %10.3g\n",row,col,crealf(((complex float *)SELL(mat)->val)[sellidx]),cimagf(((complex float *)SELL(mat)->val)[sellidx]));
                    }
                }
            }
            
        }
    }

    fclose(fp);


    return GHOST_SUCCESS;
        

}
