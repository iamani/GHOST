#ifndef _OCLFUN_H_
#define _OCLFUN_H_

#include "spmvm_globals.h"
//#include <stdlib.h>
//#include <stdio.h>
#include <CL/cl.h>
//#include "my_ellpack.h"
#include <sys/param.h>



void CL_init( int, int, const char*, MATRIX_FORMATS *);
void CL_bindMatrixToKernel(void *mat, int format, int T, int kernelIdx);

void CL_uploadCRS (LCRP_TYPE *lcrp, MATRIX_FORMATS *matrixFormats);
void CL_uploadVector( VECTOR_TYPE *vec );
void CL_downloadVector( VECTOR_TYPE *vec );

cl_mem CL_allocDeviceMemory( size_t );
cl_mem CL_allocDeviceMemoryMapped( size_t bytesize, void *hostPtr );
void * CL_mapBuffer(cl_mem devmem, size_t bytesize);
void* allocHostMemory( size_t );
inline void CL_copyDeviceToHost( void*, cl_mem, size_t );
inline cl_event CL_copyDeviceToHostNonBlocking( void* hostmem, cl_mem devmem, size_t bytesize );
inline void CL_copyHostToDevice( cl_mem, void*, size_t );
inline void CL_copyHostToDeviceOffset( cl_mem, void*, size_t, size_t);
void CL_freeDeviceMemory( cl_mem );
void freeHostMemory( void* );
void CL_finish();

void CL_SpMVM(cl_mem rhsVec, cl_mem resVec, int type); 
void CL_vecscal(cl_mem a, double s, int nRows);
void CL_axpy(cl_mem a, cl_mem b, double s, int nRows);
void CL_dotprod(cl_mem a, cl_mem b, double *out, int nRows);
void CL_setup_communication(LCRP_TYPE* lcrp, MATRIX_FORMATS *matrixFormats);

#endif
