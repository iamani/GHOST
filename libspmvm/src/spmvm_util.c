#define _GNU_SOURCE
#include "spmvm_util.h"
#include "spmvm.h"
#include "referencesolvers.h"
#include "matricks.h"
#include <sys/param.h>
#include <libgen.h>
#include <unistd.h>
#include <mpihelper.h>

#ifdef OPENCL
#include "cl_matricks.h"
#endif

#ifdef LIKWID
#include <likwid.h>
#endif

#include <sched.h>
#include <errno.h>
#include <omp.h>
#include <string.h>







void SpMVM_printMatrixInfo(LCRP_TYPE *lcrp, char *matrixName, int options)
{

	int me;
	size_t ws;


	MPI_safecall(MPI_Comm_rank ( MPI_COMM_WORLD, &me ));

#ifdef OPENCL	
	size_t fullMemSize, localMemSize, remoteMemSize, 
		   totalFullMemSize = 0, totalLocalMemSize = 0, totalRemoteMemSize = 0;

	if (!(options & SPMVM_OPTION_NO_COMBINED_KERNELS)) { // combined computation
		fullMemSize = getBytesize(lcrp->fullMatrix, lcrp->fullFormat)/
			(1024*1024);
		MPI_safecall(MPI_Reduce(&fullMemSize, &totalFullMemSize,1,MPI_LONG,MPI_SUM,0,
					MPI_COMM_WORLD));
	} 
	if (!(options & SPMVM_OPTION_NO_SPLIT_KERNELS)) { // split computation
		localMemSize = getBytesize(lcrp->localMatrix,lcrp->localFormat)/
			(1024*1024);
		remoteMemSize = getBytesize(lcrp->remoteMatrix,lcrp->remoteFormat)/
			(1024*1024);
		MPI_safecall(MPI_Reduce(&localMemSize, &totalLocalMemSize,1,MPI_LONG,MPI_SUM,0,
					MPI_COMM_WORLD));
		MPI_safecall(MPI_Reduce(&remoteMemSize, &totalRemoteMemSize,1,MPI_LONG,MPI_SUM,0,
					MPI_COMM_WORLD));
	}
#endif	

	if(me==0){
		int pin = (options & SPMVM_OPTION_PIN || options & SPMVM_OPTION_PIN_SMT)?
			1:0;
		char *pinStrategy = options & SPMVM_OPTION_PIN?"phys. cores":"all cores";
		ws = ((lcrp->nRows+1)*sizeof(int) + 
				lcrp->nEnts*(sizeof(data_t)+sizeof(int)))/(1024*1024);
		printf("-----------------------------------------------\n");
		printf("-------        Matrix information       -------\n");
		printf("-----------------------------------------------\n");
		printf("Investigated matrix              : %12s\n", matrixName); 
		printf("Dimension of matrix              : %12.0f\n", (double)lcrp->nRows); 
		printf("Non-zero elements                : %12.0f\n", (double)lcrp->nEnts); 
		printf("Average elements per row         : %12.3f\n", (double)lcrp->nEnts/
				(double)lcrp->nRows); 
		printf("Host matrix (CRS)            [MB]: %12lu\n", ws);
#ifdef OPENCL	
	if (!(options & SPMVM_OPTION_NO_COMBINED_KERNELS)) { // combined computation
			printf("Dev. matrix (combin.%4s-%2d) [MB]: %12lu\n", SPM_FORMAT_NAMES[lcrp->fullFormat],lcrp->fullT,totalFullMemSize);
		}	
	if (!(options & SPMVM_OPTION_NO_SPLIT_KERNELS)) { // split computation
			printf("Dev. matrix (local  %4s-%2d) [MB]: %12lu\n", SPM_FORMAT_NAMES[lcrp->localFormat],lcrp->localT,totalLocalMemSize); 
			printf("Dev. matrix (remote %4s-%2d) [MB]: %12lu\n", SPM_FORMAT_NAMES[lcrp->remoteFormat],lcrp->remoteT,totalRemoteMemSize);
			printf("Dev. matrix (local & remote) [MB]: %12lu\n", totalLocalMemSize+
					totalRemoteMemSize); 
		}
#endif
		printf("-----------------------------------------------\n\n");
		printf("-----------------------------------------------\n");
		printf("-------        Setup information        -------\n");
		printf("-----------------------------------------------\n");
		printf("Equation                         : %12s\n", options&SPMVM_OPTION_AXPY?"y <- y+A*x":"y <- A*x"); 
		printf("Work distribution scheme         : %12s\n", SpMVM_workdistName(options));
		printf("Automatic pinning                : %12s\n", pin?"enabled":"disabled");
		if (pin)
			printf("Pinning threads to               : %12s\n", pinStrategy);
		printf("-----------------------------------------------\n\n");
		fflush(stdout);
	}
}

void SpMVM_printEnvInfo() 
{

	int me;
	MPI_safecall(MPI_Comm_rank ( MPI_COMM_WORLD, &me ));

	int nnodes = getNumberOfNodes();
	int nphyscores = getNumberOfPhysicalCores();
	int ncores = getNumberOfHwThreads();

#ifdef OPENCL
	CL_DEVICE_INFO * devInfo = CL_getDeviceInfo();
#endif


	if (me==0) {
		int nproc;
		int nthreads;
		MPI_safecall(MPI_Comm_size ( MPI_COMM_WORLD, &nproc ));

#pragma omp parallel
#pragma omp master
		nthreads = omp_get_num_threads();

		printf("-----------------------------------------------\n");
		printf("-------       System information        -------\n");
		printf("-----------------------------------------------\n");
		printf("Nodes                            : %12d\n", nnodes); 
		//printf("MPI processes                    : %12d\n", nproc); 
		printf("MPI processes  per node          : %12d\n", nproc/nnodes); 
		printf("Physical cores per node          : %12d\n", nphyscores); 
		printf("HW threads     per node          : %12d\n", ncores); 
		printf("OpenMP threads per node          : %12d\n", nproc/nnodes*nthreads);
		printf("OpenMP threads per process       : %12d\n", nthreads);
#ifdef OPENCL
		printf("OpenCL devices                   :\n");
		int i;
		for (i=0; i<devInfo->nDistinctDevices; i++) {
			printf("                            %3d x %13s\n",devInfo->nDevices[i],devInfo->names[i]);
		}
#endif
		printf("-----------------------------------------------\n\n");

		printf("-----------------------------------------------\n");
		printf("-------      LibSpMVM information       -------\n");
		printf("-----------------------------------------------\n");
		printf("Build date                       : %12s\n", __DATE__); 
		printf("Build time                       : %12s\n", __TIME__); 
		printf("Data type                        : %12s\n", DATATYPE_NAMES[DATATYPE_DESIRED]);
#ifdef OPENCL
		printf("OpenCL support                   :      enabled\n");
#else
		printf("OpenCL support                   :     disabled\n");
#endif
#ifdef LIKWID
		printf("Likwid support                   :      enabled\n");
#ifdef LIKWID_MARKER_FINE
		printf("Likwid Marker API (high res)     :      enabled\n");
#else
#ifdef LIKWID_MARKER
		printf("Likwid Marker API                :      enabled\n");
#endif
#endif
#else
		printf("Likwid support                   :     disabled\n");
#endif
		printf("-----------------------------------------------\n\n");
		fflush(stdout);

	}

#ifdef OPENCL
	destroyCLdeviceInfo(devInfo);
#endif

}

HOSTVECTOR_TYPE * SpMVM_createGlobalHostVector(int nRows, data_t (*fp)(int))
{

	int me;
	MPI_safecall(MPI_Comm_rank ( MPI_COMM_WORLD, &me ));

	if (me==0) {
		return SpMVM_newHostVector( nRows,fp );
	} else {
		return SpMVM_newHostVector(0,NULL);
	}
}

void SpMVM_referenceSolver(CR_TYPE *cr, data_t *rhs, data_t *lhs, int nIter, int spmvmOptions) 
{

	int iteration;
	int me;

	MPI_safecall(MPI_Comm_rank ( MPI_COMM_WORLD, &me ));


/*	for( i = 0; i < cr->rowOffset[cr->nRows]; ++i) {
		cr->col[i] += 1;
	}	*/

	if (spmvmOptions & SPMVM_OPTION_AXPY) {

		for (iteration=0; iteration<nIter; iteration++) {
#ifdef DOUBLE
#ifdef COMPLEX
			fortrancrsaxpyc_(&(cr->nRows), &(cr->nEnts), lhs, rhs, cr->val ,
					cr->col, cr->rowOffset);
#else
			fortrancrsaxpy_(&(cr->nRows), &(cr->nEnts), lhs, rhs, cr->val ,
					cr->col, cr->rowOffset);
#endif
#endif
#ifdef SINGLE
#ifdef COMPLEX
			fortrancrsaxpycf_(&(cr->nRows), &(cr->nEnts), lhs, rhs, cr->val,
					cr->col, cr->rowOffset);
#else
			fortrancrsaxpyf_(&(cr->nRows), &(cr->nEnts), lhs, rhs, cr->val,
					cr->col, cr->rowOffset);
#endif
#endif
		}
	} else {
#ifdef DOUBLE
#ifdef COMPLEX
		fortrancrsc_(&(cr->nRows), &(cr->nEnts), lhs, rhs, cr->val, cr->col,
				cr->rowOffset);
#else
		fortrancrs_(&(cr->nRows), &(cr->nEnts), lhs, rhs, cr->val, cr->col,
				cr->rowOffset);
#endif
#endif
#ifdef SINGLE
#ifdef COMPLEX
		fortrancrscf_(&(cr->nRows), &(cr->nEnts), lhs, rhs, cr->val, cr->col,
				cr->rowOffset);
#else
		fortrancrsf_(&(cr->nRows), &(cr->nEnts), lhs, rhs, cr->val , cr->col,
				cr->rowOffset);
#endif
#endif
	}
/*	for( i = 0; i < cr->rowOffset[cr->nRows]; ++i) {
		cr->col[i] -= 1;
	}*/
}


void SpMVM_zeroVector(VECTOR_TYPE *vec) 
{
	int i;
	for (i=0; i<vec->nRows; i++) {
#ifdef COMPLEX
		vec->val[i] = 0;
#else
		vec->val[i] = 0+I*0;
#endif
	}

#ifdef OPENCL
	CL_uploadVector(vec);
#endif


}

HOSTVECTOR_TYPE* SpMVM_newHostVector( const int nRows, data_t (*fp)(int)) 
{
	HOSTVECTOR_TYPE* vec;
	size_t size_val;
	int i;

	size_val = (size_t)( nRows * sizeof(data_t) );
	vec = (HOSTVECTOR_TYPE*) allocateMemory( sizeof( VECTOR_TYPE ), "vec");


	vec->val = (data_t*) allocateMemory( size_val, "vec->val");
	vec->nRows = nRows;

	if (fp) {
#pragma omp parallel for schedule(static)
		for (i=0; i<nRows; i++) 
			vec->val[i] = fp(i);

	}else {
#ifdef COMPLEX
#pragma omp parallel for schedule(static)
		for (i=0; i<nRows; i++) vec->val[i] = 0.+I*0.;
#else
#pragma omp parallel for schedule(static)
		for (i=0; i<nRows; i++) vec->val[i] = 0.;
#endif
	}


	return vec;
}


VECTOR_TYPE* SpMVM_newVector( const int nRows ) 
{
	VECTOR_TYPE* vec;
	size_t size_val;
	int i;

	size_val = (size_t)( nRows * sizeof(data_t) );
	vec = (VECTOR_TYPE*) allocateMemory( sizeof( VECTOR_TYPE ), "vec");


	vec->val = (data_t*) allocateMemory( size_val, "vec->val");
	vec->nRows = nRows;

#pragma omp parallel for schedule(static) 
	for( i = 0; i < nRows; i++ ) 
		vec->val[i] = 0.0;

#ifdef OPENCL
#ifdef CL_IMAGE
	vec->CL_val_gpu = CL_allocDeviceMemoryCached( size_val,vec->val );
#else
	vec->CL_val_gpu = CL_allocDeviceMemoryMapped( size_val,vec->val,CL_MEM_READ_WRITE );
#endif
	//vec->CL_val_gpu = CL_allocDeviceMemory( size_val );
	//printf("before: %p\n",vec->val);
	//vec->val = CL_mapBuffer(vec->CL_val_gpu,size_val);
	//printf("after: %p\n",vec->val);
	//CL_uploadVector(vec);
#endif

	return vec;
}
VECTOR_TYPE * SpMVM_distributeVector(LCRP_TYPE *lcrp, HOSTVECTOR_TYPE *vec)
{
	int me;

	MPI_safecall(MPI_Comm_rank ( MPI_COMM_WORLD, &me ));
	int pseudo_ldim = lcrp->lnRows[me]+lcrp->halo_elements ;


	VECTOR_TYPE *nodeVec = SpMVM_newVector( pseudo_ldim ); 

	MPI_safecall(MPI_Scatterv ( vec->val, lcrp->lnRows, lcrp->lfRow, MPI_MYDATATYPE,
				nodeVec->val, lcrp->lnRows[me], MPI_MYDATATYPE, 0, MPI_COMM_WORLD ));

	return nodeVec;
}

void SpMVM_collectVectors(LCRP_TYPE *lcrp, VECTOR_TYPE *vec, 
		HOSTVECTOR_TYPE *totalVec, int kernel) {

	int me;


	MPI_safecall(MPI_Comm_rank ( MPI_COMM_WORLD, &me ));

	if ( 0x1<<kernel & SPMVM_KERNELS_COMBINED)  {
		SpMVM_permuteVector(vec->val,lcrp->fullInvRowPerm,lcrp->lnRows[me]);
	} else if ( 0x1<<kernel & SPMVM_KERNELS_SPLIT ) {
		SpMVM_permuteVector(vec->val,lcrp->splitInvRowPerm,lcrp->lnRows[me]);
	}

	MPI_safecall(MPI_Gatherv(vec->val,lcrp->lnRows[me],MPI_MYDATATYPE,totalVec->val,
				lcrp->lnRows,lcrp->lfRow,MPI_MYDATATYPE,0,MPI_COMM_WORLD));
}

LCRP_TYPE * SpMVM_distributeCRS (CR_TYPE *cr, void *deviceFormats, int options)
{
	int me;

	MPI_safecall(MPI_Comm_rank ( MPI_COMM_WORLD, &me ));

	LCRP_TYPE *lcrp = setup_communication(cr, options);

	if (deviceFormats == NULL) {
#ifdef OPENCL
		SpMVM_abort("Device matrix formats have to be passed to SPMVM_distributeCRS");
#endif
	}
#ifdef OPENCL
	SPM_GPUFORMATS *formats = (SPM_GPUFORMATS *)deviceFormats;
	CL_uploadCRS ( lcrp, formats, options);
#endif
	return lcrp;
}
CR_TYPE * SpMVM_createGlobalCRS (char *matrixPath)
{


	int me;
	CR_TYPE *cr;
	MM_TYPE *mm;

	MPI_safecall(MPI_Comm_rank ( MPI_COMM_WORLD, &me ));

	if (me == 0){
		if (!isMMfile(matrixPath)){

			cr = (CR_TYPE*) allocateMemory( sizeof( CR_TYPE ), "cr" );

				readCRbinFile(cr, matrixPath);

		} else{
			mm = readMMFile( matrixPath);
			cr = convertMMToCRMatrix( mm );
			freeMMMatrix(mm);
		}

		crColIdToFortran(cr);
	} else{

		/* Allokiere minimalen Speicher fuer Dummyversion der globalen Matrix */
		cr            = (CR_TYPE*) allocateMemory( sizeof(CR_TYPE), "cr");
		cr->nRows     = 0;
		cr->nEnts     = 1;
		cr->rowOffset = (int*)     allocateMemory(sizeof(int), "rowOffset");
		cr->col       = (int*)     allocateMemory(sizeof(int), "col");
		cr->val       = (data_t*)  allocateMemory(sizeof(data_t), "val");
	}
	return cr;

}

void SpMVM_swapVectors(VECTOR_TYPE *v1, VECTOR_TYPE *v2) 
{
	data_t *dtmp;

	dtmp = v1->val;
	v1->val = v2->val;
	v2->val = dtmp;
#ifdef OPENCL
	cl_mem tmp;
	tmp = v1->CL_val_gpu;
	v1->CL_val_gpu = v2->CL_val_gpu;
	v2->CL_val_gpu = tmp;
#endif

}

void SpMVM_normalizeVector( VECTOR_TYPE *vec)
{
	int i;
	data_t sum = 0;

	for (i=0; i<vec->nRows; i++)	
		sum += vec->val[i]*vec->val[i];

	data_t f = 1./SQRT(sum);

	for (i=0; i<vec->nRows; i++)	
		vec->val[i] *= f;

#ifdef OPENCL
	CL_uploadVector(vec);
#endif
}
void SpMVM_normalizeHostVector( HOSTVECTOR_TYPE *vec)
{
	int i;
	data_t sum = 0;

	for (i=0; i<vec->nRows; i++)	
		sum += vec->val[i]*vec->val[i];

	data_t f = 1./SQRT(sum);

	for (i=0; i<vec->nRows; i++)	
		vec->val[i] *= f;
}


void SpMVM_freeHostVector( HOSTVECTOR_TYPE* const vec ) {
	if( vec ) {
		freeMemory( (size_t)(vec->nRows*sizeof(data_t)), "vec->val",  vec->val );
		free( vec );
	}
}

void SpMVM_freeVector( VECTOR_TYPE* const vec ) {
	if( vec ) {
		freeMemory( (size_t)(vec->nRows*sizeof(data_t)), "vec->val",  vec->val );
#ifdef OPENCL
		CL_freeDeviceMemory( vec->CL_val_gpu );
#endif
		free( vec );
	}
}


void SpMVM_freeCRS( CR_TYPE* const cr ) {

	if (cr) {
		if (cr->rowOffset)
			free(cr->rowOffset);
		if (cr->col)
			free(cr->col);
		if (cr->val)
			free(cr->val);
		free(cr);
	}
	//TODO
/*	size_t size_rowOffset, size_col, size_val;

	if( cr ) {

		size_rowOffset  = (size_t)( (cr->nRows+1) * sizeof( int ) );
		size_col        = (size_t)( cr->nEnts     * sizeof( int ) );
		size_val        = (size_t)( cr->nEnts     * sizeof( data_t) );

		freeMemory( size_rowOffset,  "cr->rowOffset", cr->rowOffset );
		freeMemory( size_col,        "cr->col",       cr->col );
		freeMemory( size_val,        "cr->val",       cr->val );
		freeMemory( sizeof(CR_TYPE), "cr",            cr );

	}*/
}

void SpMVM_freeLCRP( LCRP_TYPE* const lcrp ) {
	if( lcrp ) {
		free( lcrp->lnEnts );
		free( lcrp->lnRows );
		free( lcrp->lfEnt );
		free( lcrp->lfRow );
		free( lcrp->wishes );
		free( lcrp->wishlist_mem );
		free( lcrp->wishlist );
		free( lcrp->dues );
		free( lcrp->duelist_mem );
		free( lcrp->duelist );
		free( lcrp->due_displ );
		free( lcrp->wish_displ );
		free( lcrp->hput_pos );
		free( lcrp->val );
		free( lcrp->col );
		free( lcrp->lrow_ptr );
		free( lcrp->lrow_ptr_l );
		free( lcrp->lrow_ptr_r );
		free( lcrp->lcol );
		free( lcrp->rcol );
		free( lcrp->lval );
		free( lcrp->rval );
		free( lcrp->fullRowPerm );
		free( lcrp->fullInvRowPerm );
		free( lcrp->splitRowPerm );
		free( lcrp->splitInvRowPerm );
#ifdef OPENCL
		CL_freeMatrix( lcrp->fullMatrix, lcrp->fullFormat );
		CL_freeMatrix( lcrp->localMatrix, lcrp->localFormat );
		CL_freeMatrix( lcrp->remoteMatrix, lcrp->remoteFormat );
#endif
		free( lcrp );
	}
}

void SpMVM_permuteVector( data_t* vec, int* perm, int len) {
	/* permutes values in vector so that i-th entry is mapped to position perm[i] */
	int i;
	data_t* tmp;

	if (perm == NULL) {
		IF_DEBUG(1) {printf("permutation vector is NULL, returning\n");}
		return;

	}


	tmp = (data_t*)allocateMemory(sizeof(data_t)*len, "permute tmp");

	for(i = 0; i < len; ++i) {
		if( perm[i] >= len ) {
			fprintf(stderr, "ERROR: permutation index out of bounds: %d > %d\n",perm[i],len);
			free(tmp);
			exit(-1);
		}
		tmp[perm[i]] = vec[i];
	}
	for(i=0; i < len; ++i) {
		vec[i] = tmp[i];
	}

	free(tmp);
}

char * SpMVM_kernelName(int kernel) {

	switch (kernel) {
		case SPMVM_KERNEL_NOMPI:
			return "non-MPI";
			break;
		case SPMVM_KERNEL_VECTORMODE:
			return "vector mode";
			break;
		case SPMVM_KERNEL_GOODFAITH:
			return "g/f hybrid";
			break;
		case SPMVM_KERNEL_TASKMODE:
			return "task mode";
			break;
		default:
			return "invalid";
			break;
	}
}

char *SpMVM_workdistName(int options)
{
	if (options & SPMVM_OPTION_WORKDIST_NZE)
		return "equal nze";
	else if (options & SPMVM_OPTION_WORKDIST_LNZE)
		return "equal lnze";
	else
		return "equal rows";
}

void SpMVM_abort(char *s) {
	fprintf(stderr,"ABORT -- %s\n",s);
	SpMVM_finish();
	exit(EXIT_FAILURE);
}

