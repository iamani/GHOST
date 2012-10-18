#include "kernel.h"
#include "spmvm_util.h"
#include <stdlib.h>
#include <stdio.h>
#include <immintrin.h>

void avx_kernel_0_intr(VECTOR_TYPE* res, BJDS_TYPE* mv, VECTOR_TYPE* invec, int spmvmOptions)
{
	int c,j,offs;
	__m256d tmp;
	__m256d val;
	__m256d rhs;
	__m128d rhstmp;

#pragma omp parallel for schedule(runtime) private(j,tmp,val,rhs,offs,rhstmp)
	for (c=0; c<mv->nRowsPadded>>2; c++) 
	{ // loop over chunks
		tmp = _mm256_setzero_pd(); // tmp = 0
		offs = mv->chunkStart[c];

		for (j=0; j<(mv->chunkStart[c+1]-mv->chunkStart[c])>>2; j++) 
		{ // loop inside chunk
			
			val    = _mm256_load_pd(&mv->val[offs]);                      // load values
			rhstmp = _mm_loadl_pd(rhstmp,&invec->val[(mv->col[offs++])]); // load first 128 bits of RHS
			rhstmp = _mm_loadh_pd(rhstmp,&invec->val[(mv->col[offs++])]);
			rhs    = _mm256_insertf128_pd(rhs,rhstmp,0);                  // insert to RHS
			rhstmp = _mm_loadl_pd(rhstmp,&invec->val[(mv->col[offs++])]); // load second 128 bits of RHS
			rhstmp = _mm_loadh_pd(rhstmp,&invec->val[(mv->col[offs++])]);
			rhs    = _mm256_insertf128_pd(rhs,rhstmp,1);                  // insert to RHS
			tmp    = _mm256_add_pd(tmp,_mm256_mul_pd(val,rhs));           // accumulate
		}
		if (spmvmOptions & SPMVM_OPTION_AXPY) {
			_mm256_store_pd(&res->val[c*BJDS_LEN],_mm256_add_pd(tmp,_mm256_load_pd(&res->val[c*BJDS_LEN])));
		} else {
			_mm256_stream_pd(&res->val[c*BJDS_LEN],tmp);
		}
	}
}

void avx_kernel_0_intr_sbjds(VECTOR_TYPE* res, SBJDS_TYPE* mv, VECTOR_TYPE* invec, int spmvmOptions)
{
	int c,j,offs;
	__m256d tmp;
	__m256d val;
	__m256d rhs;
	__m128d rhstmp;

	//printf("in AVX kernel\n");

#pragma omp parallel for schedule(runtime) private(j,tmp,val,rhs,offs,rhstmp)
	for (c=0; c<mv->nRowsPadded>>2; c++) 
	{ // loop over chunks
		tmp = _mm256_setzero_pd(); // tmp = 0
		offs = mv->chunkStart[c];

		for (j=0; j<(mv->chunkStart[c+1]-mv->chunkStart[c])>>2; j++) 
		{ // loop inside chunk
			
			val    = _mm256_load_pd(&mv->val[offs]);                      // load values
			rhstmp = _mm_loadl_pd(rhstmp,&invec->val[(mv->col[offs++])]); // load first 128 bits of RHS
			rhstmp = _mm_loadh_pd(rhstmp,&invec->val[(mv->col[offs++])]);
			rhs    = _mm256_insertf128_pd(rhs,rhstmp,0);                  // insert to RHS
			rhstmp = _mm_loadl_pd(rhstmp,&invec->val[(mv->col[offs++])]); // load second 128 bits of RHS
			rhstmp = _mm_loadh_pd(rhstmp,&invec->val[(mv->col[offs++])]);
			rhs    = _mm256_insertf128_pd(rhs,rhstmp,1);                  // insert to RHS
			tmp    = _mm256_add_pd(tmp,_mm256_mul_pd(val,rhs));           // accumulate
		}
		if (spmvmOptions & SPMVM_OPTION_AXPY) {
			_mm256_store_pd(&res->val[c*BJDS_LEN],_mm256_add_pd(tmp,_mm256_load_pd(&res->val[c*BJDS_LEN])));
		} else {
			_mm256_stream_pd(&res->val[c*BJDS_LEN],tmp);
		}
	}
}
