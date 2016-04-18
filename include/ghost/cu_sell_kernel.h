#ifndef GHOST_CU_SELL_KERNEL_H
#define GHOST_CU_SELL_KERNEL_H

#include "ghost/cu_complex.h"

extern __shared__ char shared[];

// double precision version only present from CUDA 6.5 onwards
#if __CUDA_API_VERSION < 6050
__device__ inline
double __shfl_down(double var, unsigned int srcLane, int width=32) {
    int2 a = *reinterpret_cast<int2*>(&var);
    a.x = __shfl_down(a.x, srcLane, width);
    a.y = __shfl_down(a.y, srcLane, width);
    return *reinterpret_cast<double*>(&a);
}
#endif

template<typename v_t>
__device__ inline
v_t ghost_shfl_down(v_t var, unsigned int srcLane, int width) {
    return __shfl_down(var, srcLane, width);
}

template<>
__device__ inline
cuFloatComplex ghost_shfl_down<cuFloatComplex>(cuFloatComplex var, unsigned int srcLane, int width) {
    float2 a = *reinterpret_cast<float2*>(&var);
    a.x = __shfl_down(a.x, srcLane, width);
    a.y = __shfl_down(a.y, srcLane, width);
    return *reinterpret_cast<cuFloatComplex*>(&a);
}

template<>
__device__ inline
cuDoubleComplex ghost_shfl_down<cuDoubleComplex>(cuDoubleComplex var, unsigned int srcLane, int width) {
    double2 a = *reinterpret_cast<double2*>(&var);
    a.x = __shfl_down(a.x, srcLane, width);
    a.y = __shfl_down(a.y, srcLane, width);
    return *reinterpret_cast<cuDoubleComplex*>(&a);
}

// This assumes that the warpSize is 32.
// Hard-coding this enhances the performance significantly due to unrolling
template<typename v_t>
__inline__ __device__
v_t ghost_warpReduceSum(v_t val) {
#pragma unroll
    for (int offset = 32/2; offset > 0; offset /= 2) { 
        val = accu<v_t>(val,ghost_shfl_down(val, offset, 32));
    }
    return val;
}

template<typename v_t>
__inline__ __device__
v_t ghost_partialWarpReduceSum(v_t val,int size, int width) {
    for (int offset = size/2; offset > 0; offset /= 2) { 
        val = accu<v_t>(val,ghost_shfl_down(val, offset, width));
    }
    return val;
}

template<>
__inline__ __device__
double3 ghost_warpReduceSum<double3>(double3 val) {
    for (int offset = warpSize/2; offset > 0; offset /= 2) { 
        val.x += ghost_shfl_down(val.x, offset, warpSize);
        val.y += ghost_shfl_down(val.y, offset, warpSize);
        val.z += ghost_shfl_down(val.z, offset, warpSize);
    }
    return val;
}

template<typename v_t>
__inline__ __device__
v_t ghost_partialBlockReduceSum(v_t val,int size) {

    v_t * shmem = (v_t *)shared; // Shared mem for 32 partial sums

    int lane = (threadIdx.x % warpSize);
    int wid = (threadIdx.x / warpSize);

    val = ghost_warpReduceSum(val);     // Each warp performs partial reduction

    if (threadIdx.x%warpSize == 0) shmem[wid]=val; // Write reduced value to shared memory

    __syncthreads();              // Wait for all partial reductions

    //read from shared memory only if that warp existed
    if (threadIdx.x < blockDim.x / warpSize) {
        val = shmem[lane];
    } else {
        zero<v_t>(val);
    }

    if (threadIdx.x/warpSize == 0) val = ghost_partialWarpReduceSum(val,size,warpSize); //Final reduce within first warp

    return val;
}

template<typename v_t>
__inline__ __device__
v_t ghost_1dPartialBlockReduceSum(v_t val, int nwarps) {

    v_t * shmem = (v_t *)shared; // Shared mem for 32 partial sums

    int lane = (threadIdx.x % warpSize);
    int wid = (threadIdx.x / warpSize);

    val = ghost_warpReduceSum(val);     // Each warp performs partial reduction

    if (threadIdx.x%warpSize == 0) shmem[wid]=val; // Write reduced value to shared memory

    __syncthreads();              // Wait for all partial reductions

    //read from shared memory only if that warp existed
    if (threadIdx.x < blockDim.x / warpSize) {
        val = shmem[lane];
    } else {
        zero<v_t>(val);
    }

    if (threadIdx.x/warpSize == 0) {
        val = ghost_partialWarpReduceSum(val,nwarps,nwarps); //Final reduce within first warp
    }

    return val;
}

template<typename v_t>
__inline__ __device__
v_t ghost_blockReduceSum(v_t val) {

    v_t * shmem = (v_t *)shared; // Shared mem for 32 partial sums

    int lane = (threadIdx.x % warpSize) + (32*threadIdx.y);
    int wid = (threadIdx.x / warpSize) + (32*threadIdx.y);

    val = ghost_warpReduceSum(val);     // Each warp performs partial reduction

    if (threadIdx.x%warpSize == 0) shmem[wid]=val; // Write reduced value to shared memory

    __syncthreads();              // Wait for all partial reductions

    //read from shared memory only if that warp existed
    if (threadIdx.x < blockDim.x / warpSize) {
        val = shmem[lane];
    } else {
        zero<v_t>(val);
    }

    if (threadIdx.x/warpSize == 0) val = ghost_warpReduceSum(val); //Final reduce within first warp

    return val;
}

template<>
__inline__ __device__
double3 ghost_blockReduceSum<double3>(double3 val) {

    double3 * shmem = (double3 *)shared; // Shared mem for 32 partial sums

    int lane = (threadIdx.x % warpSize) + (32*threadIdx.y);
    int wid = (threadIdx.x / warpSize) + (32*threadIdx.y);

    val = ghost_warpReduceSum(val);     // Each warp performs partial reduction

    if (threadIdx.x%warpSize == 0) shmem[wid]=val; // Write reduced value to shared memory

    __syncthreads();              // Wait for all partial reductions

    //read from shared memory only if that warp existed
    if (threadIdx.x < blockDim.x / warpSize) {
        val = shmem[lane];
    } else {
        val.x = 0.;
        val.y = 0.;
        val.z = 0.;
    }

    if (threadIdx.x/warpSize == 0) val = ghost_warpReduceSum(val); //Final reduce within first warp

    return val;
}
/*
template<typename v_t>
__global__ void deviceReduceKernel(v_t *in, v_t* out, int N) {
    v_t sum;
    //printf("<%d,%d>::: %f {%p} %f\n",threadIdx.x,threadIdx.y,in[0],in,sum);
    zero<v_t>(sum);
    //reduce multiple elements per thread
    for (int i = blockIdx.x * blockDim.x + threadIdx.x; 
            i < N; 
            i += blockDim.x * gridDim.x) {
        sum = axpy<v_t>(sum,in[i],1.f);
    }
    sum = blockReduceSum(sum);
    if (threadIdx.x==0) {
        out[blockIdx.x*blockDim.y+threadIdx.y]=sum;
    }
    if (gridDim.x > 1 && threadIdx.x==0 && threadIdx.y == 0 && blockIdx.x == 0 && blockIdx.y == 0) {
        int N = gridDim.x;
        dim3 grid((int)(ceil(gridDim.x/(double)blockDim.x)),gridDim.y);
        dim3 block(blockDim.x,blockDim.y);
        printf("recursive call with grid %dx%d block %dx%d N %d from grid %dx%d block %dx%d\n",grid.x,grid.y,block.x,block.y,N,gridDim.x,gridDim.y,blockDim.x,blockDim.y);
        __syncthreads();
        deviceReduceKernel<<<grid,block,grid.y*block.y*32*sizeof(v_t)>>> (in,out,N);
        __syncthreads();
    }

}

template<typename v_t>
__global__ void localdotKernel(v_t *lhs, int lhs_lda, v_t* rhs, int rhs_lda, v_t *localdot) {
        v_t dot1, dot2, dot3;
        zero<v_t>(dot1);
        zero<v_t>(dot2);
        zero<v_t>(dot3);
        int i = threadIdx.x+blockIdx.x*blockDim.x;
        int col = blockDim.y*blockIdx.y+threadIdx.y;

        dot1 = axpy<v_t>(dot1,lhs[lhs_lda*i+col],lhs[lhs_lda*i+col]);
        dot2 = axpy<v_t>(dot2,rhs[rhs_lda*i+col],lhs[lhs_lda*i+col]);
        dot3 = axpy<v_t>(dot3,rhs[rhs_lda*i+col],rhs[rhs_lda*i+col]);

        dot1 = blockReduceSum(dot1);
        __syncthreads();
        dot2 = blockReduceSum(dot2);
        __syncthreads();
        dot3 = blockReduceSum(dot3);
        __syncthreads();

        if (threadIdx.x==0) {
            localdot[3*col + 0] = dot1;
            localdot[3*col + 1] = dot2;
            localdot[3*col + 2] = dot3;
        }
}
*/

struct CustomSum
{
    template<typename T>
    __device__ __forceinline__ 
    T operator() (const T &a, const T &b) const 
    {
        return a+b;
    }
    __device__ __forceinline__ 
    cuDoubleComplex operator() (const cuDoubleComplex &a, const cuDoubleComplex &b) const 
    {
        return cuCadd(a,b);
    }
    __device__ __forceinline__ 
    cuFloatComplex operator() (const cuFloatComplex &a, const cuFloatComplex &b) const 
    {
        return cuCaddf(a,b);
    }
};

template<typename v_t>
__global__ void ghost_deviceReduceSum(v_t *in, v_t *out, ghost_lidx N)
{

    ghost_lidx i;
    v_t sum;
    zero<v_t>(sum);

    for (i=blockIdx.x*blockDim.x+threadIdx.x; i<N; i += blockDim.x*gridDim.x) {
        sum = accu<v_t>(sum,in[i]);
    }
    sum = ghost_blockReduceSum(sum);
    if (threadIdx.x == 0) {
        out[blockIdx.x] = sum;
    }
}

template <typename T>
inline void deviceReduce3(T *cu_data_in, T *cu_data_out, unsigned int stride, unsigned int N)
{
    ghost_deviceReduceSum<T><<<1,1024,32*sizeof(T)>>>(cu_data_in,cu_data_out,N);
    ghost_deviceReduceSum<T><<<1,1024,32*sizeof(T)>>>(&cu_data_in[stride],&cu_data_out[stride],N);
    ghost_deviceReduceSum<T><<<1,1024,32*sizeof(T)>>>(&cu_data_in[2*stride],&cu_data_out[2*stride],N);
}

#endif
