#define _XOPEN_SOURCE 500
#include "ghost/io.h"
#include "ghost/util.h"
#include "ghost/constants.h"
#include <byteswap.h>
#include <errno.h>
#include <limits.h>

void (*ghost_castArray_funcs[4][4]) (void *, void *, int) = 
{{&ss_ghost_castArray,&sd_ghost_castArray,&sc_ghost_castArray,&sz_ghost_castArray},
    {&ds_ghost_castArray,&dd_ghost_castArray,&dc_ghost_castArray,&dz_ghost_castArray},
    {&cs_ghost_castArray,&cd_ghost_castArray,&cc_ghost_castArray,&cz_ghost_castArray},
    {&zs_ghost_castArray,&zd_ghost_castArray,&zc_ghost_castArray,&zz_ghost_castArray}};


ghost_error_t ghost_readColOpen(ghost_midx_t *col, char *matrixPath, ghost_mnnz_t offsEnts, ghost_mnnz_t nEnts, FILE *filed)
{
    ghost_matfile_header_t header;
    size_t ret;
    int swapReq;
    off64_t offs;
    ghost_mnnz_t i;
    
    GHOST_SAFECALL(ghost_readMatFileHeader(matrixPath,&header));
    GHOST_SAFECALL(ghost_endianessDiffers(&swapReq,matrixPath));

    DEBUG_LOG(1,"Reading array with column indices");
    offs = GHOST_BINCRS_SIZE_HEADER+
        GHOST_BINCRS_SIZE_RPT_EL*(header.nrows+1)+
        GHOST_BINCRS_SIZE_COL_EL*offsEnts;
    if (fseeko(filed,offs,SEEK_SET)) {
        ABORT("Seek failed");
    }

#ifdef LONGIDX
    if (swapReq) {
        int64_t *tmp = (int64_t *)ghost_malloc(nEnts*8);
        if ((ret = fread(tmp, GHOST_BINCRS_SIZE_COL_EL, nEnts,filed)) != (nEnts)){
            ABORT("fread failed: %s (%zu)",strerror(errno),ret);
        }
        for( i = 0; i < nEnts; i++ ) {
            col[i] = bswap_64(tmp[i]);
        }
    } else {
        if ((ret = fread(CR(mat)->col, GHOST_BINCRS_SIZE_COL_EL, nEnts,filed)) != (nEnts)){
            ABORT("fread failed: %s (%zu)",strerror(errno),ret);
        }
    }
#else // casting from 64 to 32 bit
    DEBUG_LOG(1,"Casting from 64 bit to 32 bit column indices");
    int64_t *tmp = (int64_t *)ghost_malloc(nEnts*8);
    if ((ghost_midx_t)(ret = fread(tmp, GHOST_BINCRS_SIZE_COL_EL, nEnts,filed)) != (nEnts)){
        ABORT("fread failed: %s (%zu)",strerror(errno),ret);
    }
    for(i = 0 ; i < nEnts; ++i) {
        if (tmp[i] >= (int64_t)INT_MAX) {
            ABORT("The matrix is too big for 32-bit indices. Recompile with LONGIDX!");
        }
        if (swapReq) {
            col[i] = (ghost_midx_t)(bswap_64(tmp[i]));
        } else {
            col[i] = (ghost_midx_t)tmp[i];
        }
    }
    free(tmp);
#endif

    return GHOST_SUCCESS;


}


ghost_error_t ghost_readCol(ghost_midx_t *col, char *matrixPath, ghost_mnnz_t offsEnts, ghost_mnnz_t nEnts)
{
    FILE *filed;
    
    if ((filed = fopen64(matrixPath, "r")) == NULL){
        return GHOST_ERR_IO;
        ABORT("Could not open binary CRS file %s",matrixPath);
    }

    GHOST_SAFECALL(ghost_readColOpen(col,matrixPath,offsEnts,nEnts,filed));

    fclose(filed);

    return GHOST_SUCCESS;
}

ghost_error_t ghost_readValOpen(char *val, int datatype, char *matrixPath, ghost_mnnz_t offsEnts, ghost_mnnz_t nEnts, FILE *filed)
{
    ghost_matfile_header_t header;
    size_t ret;
    int swapReq;
    off64_t offs;
    ghost_mnnz_t i;
    size_t sizeofdt = ghost_sizeofDataType(datatype);
    
    GHOST_SAFECALL(ghost_readMatFileHeader(matrixPath,&header));
    GHOST_SAFECALL(ghost_endianessDiffers(&swapReq,matrixPath));
    
    size_t valSize = sizeof(float);
    if (header.datatype & GHOST_BINCRS_DT_DOUBLE)
        valSize *= 2;

    if (header.datatype & GHOST_BINCRS_DT_COMPLEX)
        valSize *= 2;

    DEBUG_LOG(1,"Reading array with values");
    offs = GHOST_BINCRS_SIZE_HEADER+
        GHOST_BINCRS_SIZE_RPT_EL*(header.nrows+1)+
        GHOST_BINCRS_SIZE_COL_EL*header.nnz+
        ghost_sizeofDataType(header.datatype)*offsEnts;
    if (fseeko(filed,offs,SEEK_SET)) {
        ABORT("Seek failed");
    }

    if (datatype == header.datatype) {
        if (swapReq) {
            uint8_t *tmpval = (uint8_t *)ghost_malloc(nEnts*valSize);
            if ((ghost_midx_t)(ret = fread(tmpval, valSize, nEnts,filed)) != (nEnts)){
                ERROR_LOG("fread failed for val: %s (%zu)",strerror(errno),ret);
                return GHOST_ERR_IO;
            }
            if (datatype & GHOST_BINCRS_DT_COMPLEX) {
                if (datatype & GHOST_BINCRS_DT_FLOAT) {
                    for (i = 0; i<nEnts; i++) {
                        uint32_t *a = (uint32_t *)tmpval;
                        uint32_t rswapped = bswap_32(a[2*i]);
                        uint32_t iswapped = bswap_32(a[2*i+1]);
                        memcpy(&(val[i]),&rswapped,4);
                        memcpy(&(val[i])+4,&iswapped,4);
                    }
                } else {
                    for (i = 0; i<nEnts; i++) {
                        uint64_t *a = (uint64_t *)tmpval;
                        uint64_t rswapped = bswap_64(a[2*i]);
                        uint64_t iswapped = bswap_64(a[2*i+1]);
                        memcpy(&(val[i]),&rswapped,8);
                        memcpy(&(val[i])+8,&iswapped,8);
                    }
                }
            } else {
                if (datatype & GHOST_BINCRS_DT_FLOAT) {
                    for (i = 0; i<nEnts; i++) {
                        uint32_t *a = (uint32_t *)tmpval;
                        uint32_t swapped = bswap_32(a[i]);
                        memcpy(&(val[i]),&swapped,4);
                    }
                } else {
                    for (i = 0; i<nEnts; i++) {
                        uint64_t *a = (uint64_t *)tmpval;
                        uint64_t swapped = bswap_64(a[i]);
                        memcpy(&(val[i]),&swapped,8);
                    }
                }

            }
        } else {
            if ((ghost_midx_t)(ret = fread(val, ghost_sizeofDataType(datatype), nEnts,filed)) != (nEnts)){
                ERROR_LOG("fread failed for val: %s (%zu)",strerror(errno),ret);
                return GHOST_ERR_IO;
            }
        }
    } else {
        INFO_LOG("This matrix is supposed to be of %s data but"
                " the file contains %s data. Casting...",ghost_datatypeName(datatype),ghost_datatypeName(header.datatype));


        uint8_t *tmpval = (uint8_t *)ghost_malloc(nEnts*valSize);
        if ((ghost_midx_t)(ret = fread(tmpval, valSize, nEnts,filed)) != (nEnts)){
            ABORT("fread failed for val: %s (%zu)",strerror(errno),ret);
        }

        if (swapReq) {
            ABORT("Not yet supported!");
            if (datatype & GHOST_BINCRS_DT_COMPLEX) {
                if (datatype & GHOST_BINCRS_DT_FLOAT) {
                    for (i = 0; i<nEnts; i++) {
                        uint32_t re = bswap_32(tmpval[i*valSize]);
                        uint32_t im = bswap_32(tmpval[i*valSize+valSize/2]);
                        memcpy(&val[i*sizeofdt],&re,4);
                        memcpy(&val[i*sizeofdt+4],&im,4);
                    }
                } else {
                    for (i = 0; i<nEnts; i++) {
                        uint32_t re = bswap_64(tmpval[i*valSize]);
                        uint32_t im = bswap_64(tmpval[i*valSize+valSize/2]);
                        memcpy(&val[i*sizeofdt],&re,8);
                        memcpy(&val[i*sizeofdt+8],&im,8);
                    }
                }
            } else {
                if (datatype & GHOST_BINCRS_DT_FLOAT) {
                    for (i = 0; i<nEnts; i++) {
                        uint32_t swappedVal = bswap_32(tmpval[i*valSize]);
                        memcpy(&val[i*sizeofdt],&swappedVal,4);
                    }
                } else {
                    for (i = 0; i<nEnts; i++) {
                        uint32_t swappedVal = bswap_64(tmpval[i*valSize]);
                        memcpy(&val[i*sizeofdt],&swappedVal,8);
                    }
                }

            }

        } else {
            ghost_castArray_funcs[ghost_dataTypeIdx(datatype)][ghost_dataTypeIdx(header.datatype)](val,tmpval,nEnts);
        }

        free(tmpval);
    }

    return GHOST_SUCCESS;
}

ghost_error_t ghost_readVal(char *val, int datatype, char *matrixPath, ghost_mnnz_t offsEnts, ghost_mnnz_t nEnts)
{
    FILE *filed;

    if ((filed = fopen64(matrixPath, "r")) == NULL){
        return GHOST_ERR_IO;
        ABORT("Could not open binary CRS file %s",matrixPath);
    }

    ghost_readValOpen(val,datatype,matrixPath,offsEnts,nEnts,filed);

    fclose(filed);

    return GHOST_SUCCESS;
}

ghost_error_t ghost_endianessDiffers(int *differs, char *matrixPath)
{
    ghost_matfile_header_t header;
    ghost_error_t err;
    GHOST_SAFECALL(ghost_readMatFileHeader(matrixPath,&header));

    if (header.endianess == GHOST_BINCRS_LITTLE_ENDIAN && ghost_archIsBigEndian()) {
        *differs = 1;
    } else if (header.endianess != GHOST_BINCRS_LITTLE_ENDIAN && !ghost_archIsBigEndian()) {
        *differs = 1;
    } else {
        *differs = 0;
    }

    return GHOST_SUCCESS;

}

