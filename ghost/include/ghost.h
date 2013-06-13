#ifndef __GHOST_H__
#define __GHOST_H__

#ifdef GHOST_MPI
#ifndef CUDAKERNEL
#pragma warning (disable : 869)
#include <mpi.h>
#pragma warning (enable : 869)
#endif
#endif

#ifndef GHOST_CLKERNEL
#include <stdlib.h>
#ifndef __cplusplus
#include <math.h>
#include <complex.h>
#else
#include <complex>
#endif
#include <inttypes.h>
#include <sys/types.h>
#include <pthread.h>

#ifdef OPENCL
#include <CL/cl.h>
#endif

#ifdef CUDA
#include <cuda.h>
#endif
#endif

#include "ghost_types.h"
#include "ghost_constants.h"

#define GHOST_NAME "ghost"
#define GHOST_VERSION "0.2"


/******************************************************************************/
/*----  Global definitions  --------------------------------------------------*/
/******************************************************************************/
#define CL_MY_DEVICE_TYPE CL_DEVICE_TYPE_GPU
//#define CUDA_PINNEDMEM
/******************************************************************************/

#define GHOST_REGISTER_DT_D(name) \
	typedef double name ## _t; \
	int name = GHOST_BINCRS_DT_DOUBLE|GHOST_BINCRS_DT_REAL; \

#define GHOST_REGISTER_DT_S(name) \
	typedef float name ## _t; \
	int name = GHOST_BINCRS_DT_FLOAT|GHOST_BINCRS_DT_REAL; \

#define GHOST_REGISTER_DT_C(name) \
	typedef complex float name ## _t; \
	int name = GHOST_BINCRS_DT_FLOAT|GHOST_BINCRS_DT_COMPLEX; \

#define GHOST_REGISTER_DT_Z(name) \
	typedef complex double name ## _t; \
	int name = GHOST_BINCRS_DT_DOUBLE|GHOST_BINCRS_DT_COMPLEX; \




/******************************************************************************/
/*----  Type definitions  ----------------------------------------------------*/
/******************************************************************************/

typedef struct ghost_vec_t ghost_vec_t;
typedef struct ghost_mat_t ghost_mat_t;
typedef struct ghost_context_t ghost_context_t;
typedef struct ghost_comm_t ghost_comm_t;
typedef struct ghost_spmf_plugin_t ghost_spmf_plugin_t;
typedef struct ghost_vec_plugin_t ghost_vec_plugin_t;
typedef struct ghost_mtraits_t ghost_mtraits_t;
typedef struct ghost_vtraits_t ghost_vtraits_t;

struct ghost_vec_t 
{
	ghost_vtraits_t *traits;
	//ghost_vdat_t* val;
	void* val;

	void          (*fromFunc) (ghost_vec_t *, ghost_context_t *, void (*fp)(int,int,void *));
	void          (*fromVec) (ghost_vec_t *, ghost_vec_t *, int, int, int);
	void          (*fromFile) (ghost_vec_t *, ghost_context_t *, char *path, off_t);
	void          (*fromRand) (ghost_vec_t *, ghost_context_t *);
	void          (*fromScalar) (ghost_vec_t *, ghost_context_t *, void *);

	void          (*zero) (ghost_vec_t *);
	void          (*distribute) (ghost_vec_t *, ghost_vec_t **, ghost_comm_t *comm);
	void          (*collect) (ghost_vec_t *, ghost_vec_t *, ghost_context_t *, ghost_mat_t *);
	void          (*swap) (ghost_vec_t *, ghost_vec_t *);
	void          (*normalize) (ghost_vec_t *);
	void          (*destroy) (ghost_vec_t *);
	void          (*permute) (ghost_vec_t *, ghost_vidx_t *);
	int           (*equals) (ghost_vec_t *, ghost_vec_t *);
	void          (*dotProduct) (ghost_vec_t *, ghost_vec_t *, void *);
	void          (*scale) (ghost_vec_t *, void *);
	void          (*axpy) (ghost_vec_t *, ghost_vec_t *, void *);
	void          (*print) (ghost_vec_t *);
	void          (*toFile) (ghost_vec_t *, char *, off_t, int);
	void          (*entry) (ghost_vec_t *, int,  void *);

	ghost_vec_t * (*clone) (ghost_vec_t *);
	ghost_vec_t * (*extract) (ghost_vec_t *, int, int);
	ghost_vec_t * (*view) (ghost_vec_t *, int, int);

	void          (*CUupload) (ghost_vec_t *);
	void          (*CUdownload) (ghost_vec_t *);
	void          (*CLupload) (ghost_vec_t *);
	void          (*CLdownload) (ghost_vec_t *);

	void *so;
	
	int isView;

#ifdef OPENCL
	cl_mem CL_val_gpu;
#endif
#ifdef CUDA
	void * CU_val;
#endif
};

struct ghost_vtraits_t
{
	int flags;
	void * aux;
	int datatype;
	int nrows;
	int nvecs;
}; 
#define GHOST_VTRAITS_INIT(...) {.flags = GHOST_VEC_DEFAULT, .aux = NULL, .datatype = GHOST_BINCRS_DT_DOUBLE|GHOST_BINCRS_DT_REAL, .nrows = 0, .nvecs = 1, ## __VA_ARGS__ }

typedef struct 
{
	int format[3];
	int T[3];
} 
GHOST_SPM_GPUFORMATS;

typedef struct
{
	int state;
	char *desc;
} ghost_threadstate_t;


typedef struct
{
	const char *desc;
	int flags;
	pthread_t tid;
	int *coreList;
	int nThreads;
	void *(*func) (void *);
	void *arg;
} ghost_task_t;
	


/*typedef struct{
	ghost_mat_t *mat;
	char *matrixPath;
} CRS_readRpt_args_t;

typedef struct{
	ghost_mat_t *mat;
	char *matrixPath;
	ghost_mnnz_t offsetEnts;
	ghost_midx_t offsetRows;
	ghost_midx_t nRows;
	ghost_mnnz_t nEnts;
	int IOtype;
} CRS_readColValOffset_args_t;

typedef struct {
	ghost_mat_t *mat;
	int options;
	ghost_comm_t *lcrp;
} CRS_createDistribution_args_t;

typedef struct {
	ghost_mat_t *mat;
	ghost_mat_t *lmat;
	ghost_mat_t *rmat;
	int options;
	ghost_context_t *context;
} CRS_createCommunication_args_t;

#define GHOST_CRS_EXTRAFUN_READ_RPT 0
#define GHOST_CRS_EXTRAFUN_READ_COL_VAL_OFFSET 1
#define GHOST_CRS_EXTRAFUN_READ_HEADER 2
#define GHOST_CRS_EXTRAFUN_CREATE_DISTRIBUTION 3
#define GHOST_CRS_EXTRAFUN_CREATE_COMMUNICATION 4*/

typedef void (*ghost_kernel_t)(ghost_mat_t*, ghost_vec_t*, ghost_vec_t*, int);
typedef void (*ghost_solver_t)(ghost_context_t *, ghost_vec_t*, ghost_mat_t *, ghost_vec_t*, int);
typedef void (*ghost_dummyfun_t)(void *);
typedef ghost_mat_t * (*ghost_spmf_init_t) (ghost_mtraits_t *);
typedef ghost_vec_t * (*ghost_vec_init_t) (ghost_vtraits_t *);

struct ghost_comm_t 
{
	ghost_midx_t halo_elements; // number of nonlocal RHS vector elements
	ghost_mnnz_t* lnEnts;
	ghost_mnnz_t* lfEnt;
	ghost_midx_t* lnrows;
	ghost_midx_t* lfRow;
	int* wishes;
	//ghost_midx_t* wishlist_mem; // TODO delete
	int** wishlist;    // TODO delete
	int* dues;
	//ghost_midx_t* duelist_mem;  // TODO delete
	int** duelist;
	ghost_midx_t* due_displ;    
	ghost_midx_t* wish_displ;   // TODO delete
//	int32_t * wish_displ_split;   
//	ghost_midx_t * comm_start_displ;   
	ghost_midx_t* hput_pos;
}; 

struct ghost_mat_t 
{
	ghost_mtraits_t *traits; // TODO rename
	void *so;
	ghost_midx_t *rowPerm;     // may be NULL
	ghost_midx_t *invRowPerm;  // may be NULL
	int symmetry;
	ghost_mat_t *localPart;
	ghost_mat_t *remotePart;
	ghost_context_t *context;
	char *name;
	void *data;
	ghost_dummyfun_t *extraFun; // TODO still needed?
	// TODO MPI-IO
	ghost_kernel_t kernel;

	// access functions
	void       (*destroy) (ghost_mat_t *);
	void       (*printInfo) (ghost_mat_t *);
	ghost_mnnz_t  (*nnz) (ghost_mat_t *);
	ghost_midx_t  (*nrows) (ghost_mat_t *);
	ghost_midx_t  (*ncols) (ghost_mat_t *);
	ghost_midx_t  (*rowLen) (ghost_mat_t *, ghost_midx_t i);
//	ghost_mdat_t (*entry) (ghost_mat_t *, ghost_midx_t i, ghost_midx_t j);
	char *     (*formatName) (ghost_mat_t *);
	void       (*fromFile)(ghost_mat_t *, ghost_context_t *ctx, char *matrixPath);
	void       (*fromMM)(ghost_mat_t *, char *matrixPath);
	void       (*CLupload)(ghost_mat_t *);
	void       (*CUupload)(ghost_mat_t *);
	size_t     (*byteSize)(ghost_mat_t *);
	void       (*fromCRS)(ghost_mat_t *, void *);
	void       (*split)(ghost_mat_t *, ghost_context_t *);
#ifdef OPENCL
	cl_kernel clkernel[4];
#endif
}; 

struct ghost_spmf_plugin_t
{
	void *so;
	ghost_spmf_init_t init;
	char *name;
	char *version;
	char *formatID;
};

struct ghost_vec_plugin_t
{
	void *so;
	ghost_vec_init_t init;
	char *name;
	char *version;
};

struct ghost_context_t
{
	ghost_solver_t *solvers;

	ghost_comm_t *communicator; // TODO shorter
//	ghost_mat_t *fullMatrix; // TODO array
//	ghost_mat_t *localMatrix;
//	ghost_mat_t *remoteMatrix;
/*
	ghost_mnnz_t  (*gnnz) (ghost_context_t *);
	ghost_midx_t  (*gnrows) (ghost_context_t *);
	ghost_midx_t  (*gncols) (ghost_context_t *);
	ghost_mnnz_t  (*lnnz) (ghost_context_t *);
	ghost_midx_t  (*lnrows) (ghost_context_t *);
	ghost_midx_t  (*lncols) (ghost_context_t *);*/

//	char *matrixName;

	ghost_midx_t gnrows;
	int flags;

};

struct ghost_mtraits_t
{
	const char * format;
	int flags;
	void * aux;
	int nAux;
	int datatype;
	void * shift; 
}; 
#define GHOST_MTRAITS_INIT(...) {.flags = GHOST_SPM_DEFAULT, .aux = NULL, .nAux = 0, .datatype = GHOST_BINCRS_DT_DOUBLE|GHOST_BINCRS_DT_REAL, .format = "CRS", .shift = NULL,  ## __VA_ARGS__ }

typedef struct
{
	int nDistinctDevices;
	int *nDevices;
	char **names;
} 
ghost_acc_info_t;


typedef struct
{
	int32_t endianess;
	int32_t version;
	int32_t base;
	int32_t symmetry;
	int32_t datatype;
	int64_t nrows;
	int64_t ncols;
	int64_t nnz;
} ghost_matfile_header_t;


void ghost_normalizeVec(ghost_vec_t *);
void ghost_dotProduct(ghost_vec_t *, ghost_vec_t *, void *);
void ghost_vecToFile(ghost_vec_t *, char *, ghost_context_t *);
void ghost_vecFromFile(ghost_vec_t *, char *, ghost_context_t *);

/******************************************************************************/

/******************************************************************************/
/*----  Function prototypes  -------------------------------------------------*/
/******************************************************************************/

/******************************************************************************
 * Initializes the basic functionality of the ghost. This includes:
 *   - initialize MPI
 *   - create and commit custom MPI datatypes (if necessary)
 *   - pin threads to CPU cores (if defined)
 *   - context the MPI communicator for the node
 *   - initialize the OpenCL functionality of the library (if enabled)
 *   - initialize the Likwid Marker API (if defined)
 *
 * Arguments:
 *   - int argc
 *     The number of arguments of the main function (will be passed to
 *     MPI_init_thread())
 *   - char ** argv
 *     The arguments of the main functions (will be passed to 
 *     MPI_init_thread())
 *   - int options
 *     This argument contains the options for the sparse matrix-vector product.
 *     It can be assembled by OR-ing several of the available options which
 *     are defined as GHOST_OPTION_* (explained above).
 *
 * Returns:
 *   an integer which holds the rank of the calling MPI process within
 *   MPI_COMM_WORLD
 *
 * The call to ghost_init() has to be done before any other ghost_*() call.
 *****************************************************************************/
int ghost_init(int argc, char **argv);

/******************************************************************************
 * Clean up and finalize before termination. This includes:
 *   - call MPI_Finalize()
 *   - finish the OpenCL functionality
 *   - close the Likwid Marker API
 *
 * The ghost_finish() call is usually the last call of the main program. 
 *****************************************************************************/
void ghost_finish();

/******************************************************************************
 * Creates a distributed vector with specified values in order to use it for 
 * SpMVM. Depending on the type, the length of the vector may differ.
 * If OpenCL is enabled, the vector is also being created and initialized on
 * the device.
 *
 * Arguments:
 *   - ghost_context_t *lcrp
 *     The local CRS matrix portion to use with the vector.
 *   - int type
 *     Specifies whether the vector is a right hand side vector 
 *     (GHOST_VEC_RHS), left hand side vector (GHOST_VEC_LHS) or a vector
 *     which may be used as both right and left hand side (GHOST_VEC_BOTH).
 *     The length of the vector depends on this argument.
 *   - ghost_mdat_t (*fp)(int)
 *     A function pointer to a function taking an integer value and returning
 *     a ghost_mdat_t. This function returns the initial value for the i-th (globally)
 *     element of the vector.
 *     If NULL, the vector is initialized to zero.
 *
 * Returns:
 *   a pointer to an ghost_comm_t structure which holds the local matrix data as
 *   well as the necessary data structures for communication.
 *****************************************************************************/
ghost_vec_t *ghost_createVector(ghost_vtraits_t *traits);

/******************************************************************************
 * Perform the sparse matrix vector product using a specified kernel with a
 * fixed number of iterations.
 *
 * Arguments:
 *   - ghost_vec_t *res 
 *     The result vector. Its values are being accumulated if GHOST_SPMVM_AXPY
 *     is defined.  
 *   - ghost_comm_t *lcrp
 *     The local CRS matrix part.
 *   - ghost_vec_t *invec
 *     The left hand side vector.
 *   - int kernel
 *     The kernel which should be used. This has to be one out of
 *       + GHOST_SPMVM_MODE_NOMPI
 *       + GHOST_SPMVM_MODE_VECTORMODE
 *       + GHOST_SPMVM_MODE_GOODFAITH
 *       + GHOST_SPMVM_MODE_TASKMODE
 *   - int nIter
 *     The number of iterations to run.
 *     
 * Returns:
 *   the wallclock time (in seconds) the kernel execution took. 
 *****************************************************************************/
int ghost_spmvm(ghost_context_t *context, ghost_vec_t *res, ghost_mat_t *mat, ghost_vec_t *invec, 
		int *spmvmOptions);

ghost_context_t *ghost_createContext(int64_t, int);
//ghost_mat_t *ghost_createMatrix(ghost_context_t * context, char *matrixPath, ghost_mtraits_t *traits, int nTraits);
ghost_mat_t *ghost_createMatrix(ghost_mtraits_t *traits, int nTraits);
ghost_mat_t * ghost_initMatrix(ghost_mtraits_t *traits);
void ghost_freeContext(ghost_context_t *context);
/******************************************************************************/
void ghost_vecFromScalar(ghost_vec_t *v, ghost_context_t *, void *s);
void ghost_vecFromFunc(ghost_vec_t *v, ghost_context_t *, void (*func)(int,int,void*));
void ghost_freeVec(ghost_vec_t *vec);
void ghost_matFromFile(ghost_mat_t *, ghost_context_t *, char *);

#endif
