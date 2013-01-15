#define _GNU_SOURCE
#include "ghost_util.h"
#include "ghost.h"
#include "ghost_vec.h"
#include "ghost_mat.h"
#include <sys/param.h>
#include <libgen.h>
#include <unistd.h>

#ifdef LIKWID
#include <likwid.h>
#endif

#include <sched.h>
#include <errno.h>
#include <omp.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <dlfcn.h>


//#define PRETTYPRINT

#define PRINTWIDTH 80
#define LABELWIDTH 40

#ifdef PRETTYPRINT
#define PRINTSEP "┊"
#else
#define PRINTSEP ":"
#endif

#define VALUEWIDTH (PRINTWIDTH-LABELWIDTH-(int)strlen(PRINTSEP))

static int allocatedMem;

static double ghost_wctime()
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return (double) (tp.tv_sec + tp.tv_usec/1000000.0);
}

void ghost_printHeader(const char *fmt, ...)
{
	if(ghost_getRank() == 0) {
		va_list args;
		va_start(args,fmt);
		char label[1024];
		vsnprintf(label,1024,fmt,args);
		va_end(args);

		const int spacing = 4;
		int len = strlen(label);
		int nDash = (PRINTWIDTH-2*spacing-len)/2;
		int rem = (PRINTWIDTH-2*spacing-len)%2;
		int i;
#ifdef PRETTYPRINT
		printf("┌");
		for (i=0; i<PRINTWIDTH-2; i++) printf("─");
		printf("┐");
		printf("\n");
		printf("├");
		for (i=0; i<nDash-1; i++) printf("─");
		for (i=0; i<spacing; i++) printf(" ");
		printf("%s",label);
		for (i=0; i<spacing+rem; i++) printf(" ");
		for (i=0; i<nDash-1; i++) printf("─");
		printf("┤");
		printf("\n");
		printf("├");
		for (i=0; i<LABELWIDTH; i++) printf("─");
		printf("┬");
		for (i=0; i<VALUEWIDTH; i++) printf("─");
		printf("┤");
		printf("\n");
#else
		for (i=0; i<PRINTWIDTH; i++) printf("-");
		printf("\n");
		for (i=0; i<nDash; i++) printf("-");
		for (i=0; i<spacing; i++) printf(" ");
		printf("%s",label);
		for (i=0; i<spacing+rem; i++) printf(" ");
		for (i=0; i<nDash; i++) printf("-");
		printf("\n");
		for (i=0; i<PRINTWIDTH; i++) printf("-");
		printf("\n");
#endif
	}
}

void ghost_printFooter() 
{
	if (ghost_getRank() == 0) {
		int i;
#ifdef PRETTYPRINT
		printf("└");
		for (i=0; i<LABELWIDTH; i++) printf("─");
		printf("┴");
		for (i=0; i<VALUEWIDTH; i++) printf("─");
		printf("┘");
#else
		for (i=0; i<PRINTWIDTH; i++) printf("-");
#endif
		printf("\n\n");
	}
}

void ghost_printLine(const char *label, const char *unit, const char *fmt, ...)
{
	if (ghost_getRank() == 0) {
		va_list args;
		va_start(args,fmt);
		char dummy[1024];
		vsnprintf(dummy,1024,fmt,args);
		va_end(args);

#ifdef PRETTYPRINT
		printf("│");
#endif
		if (unit) {
			int unitLen = strlen(unit);
			printf("%-*s (%s)%s%*s",LABELWIDTH-unitLen-3,label,unit,PRINTSEP,VALUEWIDTH,dummy);
		} else {
			printf("%-*s%s%*s",LABELWIDTH,label,PRINTSEP,VALUEWIDTH,dummy);
		}
#ifdef PRETTYPRINT
		printf("│");
#endif
		printf("\n");
	}
}

void ghost_printOptionsInfo(int options)
{
	int pin = (options & GHOST_OPTION_PIN || options & GHOST_OPTION_PIN_SMT)?
		1:0;
	char *pinStrategy = options & GHOST_OPTION_PIN?"phys. cores":"virt. cores";

	ghost_printHeader("Options");
	ghost_printLine("Equation",NULL,"%s",options&GHOST_SPMVM_AXPY?"y <- y+A*x":"y <- A*x");
	ghost_printLine("Work distribution scheme",NULL,"%s",ghost_workdistName(options));
	ghost_printLine("Automatic pinning",NULL,"%s",pin?"enabled":"disabled");
	if (pin)
		ghost_printLine("Pinning threads to ",NULL,"%s",pinStrategy);
	ghost_printFooter();

}

void ghost_printContextInfo(ghost_context_t *context)
{

	size_t ws;


	ws = ((context->gnrows(context)+1)*sizeof(ghost_midx_t) + 
			context->gnnz(context)*(sizeof(ghost_mdat_t)+sizeof(ghost_midx_t)))/(1024*1024);

	char *matrixLocation = (char *)allocateMemory(64,"matrixLocation");
	if (context->fullMatrix->traits->flags & GHOST_SPM_DEVICE)
		matrixLocation = "Device";
	else if (context->fullMatrix->traits->flags & GHOST_SPM_HOST)
		matrixLocation = "Host";
	else
		matrixLocation = "Default";

	char *matrixPlacement = (char *)allocateMemory(64,"matrixPlacement");
	if (context->flags & GHOST_CONTEXT_DISTRIBUTED)
		matrixPlacement = "Distributed";
	else if (context->flags & GHOST_CONTEXT_GLOBAL)
		matrixPlacement = "Global";


	ghost_printHeader("Context");
	ghost_printLine("Matrix name",NULL,"%s",context->matrixName);
	ghost_printLine("Dimension",NULL,"%"PRmatIDX,context->gnrows(context));
	ghost_printLine("Nonzeros",NULL,"%"PRmatNNZ,context->gnnz(context));
	ghost_printLine("Avg. nonzeros per row",NULL,"%.3f",(double)context->gnnz(context)/context->gnrows(context));
	ghost_printLine("Matrix location",NULL,"%s",matrixLocation);
	ghost_printLine("Matrix placement",NULL,"%s",matrixPlacement);
	ghost_printLine("Global CRS size","MB","%lu",ws);

	ghost_printLine("Symmetry (full matrix)",NULL,"%s",ghost_symmetryName(context->fullMatrix->symmetry));
	ghost_printLine("Full   host matrix format",NULL,"%s",context->fullMatrix->formatName(context->fullMatrix));
	if (context->flags & GHOST_CONTEXT_DISTRIBUTED)
	{
		ghost_printLine("Local  host matrix format",NULL,"%s",context->localMatrix->formatName(context->fullMatrix));
		ghost_printLine("Remote host matrix format",NULL,"%s",context->remoteMatrix->formatName(context->fullMatrix));
	}
	ghost_printLine("Full   host matrix size (rank 0)","MB","%u",context->fullMatrix->byteSize(context->fullMatrix)/(1024*1024));
	if (context->flags & GHOST_CONTEXT_DISTRIBUTED)
	{
		ghost_printLine("Local  host matrix size (rank 0)","MB","%u",context->localMatrix->byteSize(context->localMatrix)/(1024*1024));
		ghost_printLine("Remote host matrix size (rank 0)","MB","%u",context->remoteMatrix->byteSize(context->remoteMatrix)/(1024*1024));
	}

	if (context->flags & GHOST_CONTEXT_GLOBAL)
	{ //additional information depending on format
		context->fullMatrix->printInfo(context->fullMatrix);
	}
	ghost_printFooter();

}

void ghost_printSysInfo()
{
	int nproc = ghost_getNumberOfProcesses();
	int nnodes = ghost_getNumberOfNodes();

#ifdef OPENCL
	ghost_cl_devinfo_t * devInfo = CL_getDeviceInfo();
#endif

	if (ghost_getRank()==0) {
		int nthreads;
		int nphyscores = ghost_getNumberOfPhysicalCores();
		int ncores = ghost_getNumberOfHwThreads();

		omp_sched_t omp_sched;
		int omp_sched_mod;
		char omp_sched_str[32];
		omp_get_schedule(&omp_sched,&omp_sched_mod);
		switch (omp_sched) {
			case omp_sched_static:
				sprintf(omp_sched_str,"static,%d",omp_sched_mod);
				break;
			case omp_sched_dynamic:
				sprintf(omp_sched_str,"dynamic,%d",omp_sched_mod);
				break;
			case omp_sched_guided:
				sprintf(omp_sched_str,"guided,%d",omp_sched_mod);
				break;
			case omp_sched_auto:
				sprintf(omp_sched_str,"auto,%d",omp_sched_mod);
				break;
			default:
				sprintf(omp_sched_str,"unknown");
				break;
		}
#pragma omp parallel
#pragma omp master
		nthreads = omp_get_num_threads();

		ghost_printHeader("System");
		ghost_printLine("Nodes",NULL,"%d",nnodes);
		ghost_printLine("MPI processes per node",NULL,"%d",nproc/nnodes);
		ghost_printLine("Avail. threads (phys/HW) per node",NULL,"%d/%d",nphyscores,ncores);
		ghost_printLine("OpenMP threads per node",NULL,"%d",nproc/nnodes*nthreads);
		ghost_printLine("OpenMP threads per process",NULL,"%d",nthreads);
		ghost_printLine("OpenMP scheduling",NULL,"%s",omp_sched_str);
#ifdef OPENCL
		ghost_printLine("OpenCL version",NULL,"%s",CL_getVersion());
		ghost_printLine("OpenCL devices",NULL,"%dx %s",devInfo->nDevices[0],devInfo->names[0]);
		int i;
		for (i=1; i<devInfo->nDistinctDevices; i++) {
			ghost_printLine("",NULL,"%dx %s",devInfo->nDevices[i],devInfo->names[i]);
		}
#endif
		ghost_printFooter();
	}
#ifdef OPENCL
	destroyCLdeviceInfo(devInfo);
#endif

}

void ghost_printGhostInfo() 
{

	if (ghost_getRank()==0) {
		ghost_printHeader("%s", GHOST_NAME);
		ghost_printLine("Version",NULL,"%s",GHOST_VERSION);
		ghost_printLine("Build date",NULL,"%s",__DATE__);
		ghost_printLine("Build time",NULL,"%s",__TIME__);
		ghost_printLine("Matrix data type",NULL,"%s",ghost_datatypeName(GHOST_MY_MDATATYPE));
		ghost_printLine("Vector data type",NULL,"%s",ghost_datatypeName(GHOST_MY_VDATATYPE));
#ifdef MIC
		ghost_printLine("MIC kernels",NULL,"enabled");
#else
		ghost_printLine("MIC kernels",NULL,"disabled");
#endif
#ifdef AVX
		ghost_printLine("AVX kernels",NULL,"enabled");
#else
		ghost_printLine("AVX kernels",NULL,"disabled");
#endif
#ifdef SSE
		ghost_printLine("SSE kernels",NULL,"enabled");
#else
		ghost_printLine("SSE kernels",NULL,"disabled");
#endif
#ifdef MPI
		ghost_printLine("MPI support",NULL,"enabled");
#else
		ghost_printLine("MPI support",NULL,"disabled");
#endif
#ifdef OPENCL
		ghost_printLine("OpenCL support",NULL,"enabled");
#else
		ghost_printLine("OpenCL support",NULL,"disabled");
#endif
#ifdef LIKWID
		ghost_printLine("Likwid support",NULL,"enabled");
		printf("Likwid support                   :      enabled\n");
		/*#ifdef LIKWID_MARKER_FINE
		  printf("Likwid Marker API (high res)     :      enabled\n");
#else
#ifdef LIKWID_MARKER
printf("Likwid Marker API                :      enabled\n");
#endif
#endif*/
#else
		ghost_printLine("Likwid support",NULL,"disabled");
#endif
		ghost_printFooter();

	}


}

ghost_vec_t *ghost_referenceSolver(char *matrixPath, ghost_context_t *distContext, ghost_vdat_t (*rhsVal)(int), int nIter, int spmvmOptions)
{

	int me = ghost_getRank();
	//ghost_vec_t *res = ghost_createVector(distContext,GHOST_VEC_LHS|GHOST_VEC_HOST,NULL);
	ghost_vec_t *globLHS; 

	if (me==0) {
		DEBUG_LOG(1,"Computing reference solution");
		ghost_mtraits_t trait = {.format = "CRS", .flags = GHOST_SPM_HOST, .aux = NULL};
		ghost_context_t *context = ghost_createContext(matrixPath, &trait, 1, GHOST_CONTEXT_GLOBAL);
		globLHS = ghost_createVector(context,GHOST_VEC_LHS|GHOST_VEC_HOST,NULL); 
		ghost_vec_t *globRHS = ghost_createVector(context,GHOST_VEC_RHS|GHOST_VEC_HOST,rhsVal);

		CR_TYPE *cr = (CR_TYPE *)(context->fullMatrix->data);
		int iter;

		if (context->fullMatrix->symmetry == GHOST_BINCRS_SYMM_GENERAL) {
			for (iter=0; iter<nIter; iter++)
				ghost_referenceKernel(globLHS->val, cr->col, cr->rpt, cr->val, globRHS->val, cr->nrows, spmvmOptions);
		} else if (context->fullMatrix->symmetry == GHOST_BINCRS_SYMM_SYMMETRIC) {
			for (iter=0; iter<nIter; iter++)
				ghost_referenceKernel_symm(globLHS->val, cr->col, cr->rpt, cr->val, globRHS->val, cr->nrows, spmvmOptions);
		}

		ghost_freeVector(globRHS);
		ghost_freeContext(context);
	} else {
		globLHS = ghost_newVector(0,GHOST_VEC_LHS|GHOST_VEC_HOST);
	}
	DEBUG_LOG(1,"Scattering result of reference solution");

	ghost_vec_t *res = ghost_distributeVector(distContext->communicator,globLHS);

	ghost_freeVector(globLHS);

	DEBUG_LOG(1,"Reference solution has been computed and scattered successfully");
	return res;

}

// FIXME
void ghost_referenceKernel_symm(ghost_vdat_t *res, ghost_mnnz_t *col, ghost_midx_t *rpt, ghost_mdat_t *val, ghost_vdat_t *rhs, ghost_midx_t nrows, int spmvmOptions)
{
		ghost_midx_t i, j;
		ghost_vdat_t hlp1;

#pragma omp	parallel for schedule(runtime) private (hlp1, j)
		for (i=0; i<nrows; i++){
			hlp1 = 0.0;
			for (j=rpt[i]; j<rpt[i+1]; j++){
				hlp1 = hlp1 + (ghost_vdat_t)val[j] * rhs[col[j]];
		
				if (i!=col[j]) {	
					if (spmvmOptions & GHOST_SPMVM_AXPY) { 
#pragma omp atomic
						res[col[j]] += (ghost_vdat_t)val[j] * rhs[i];
					} else {
#pragma omp atomic
						res[col[j]] += (ghost_vdat_t)val[i] * rhs[i];  // FIXME non-axpy case doesnt work
					}
				}

			}
			if (spmvmOptions & GHOST_SPMVM_AXPY) {
				res[i] += hlp1;
			} else {
				res[i] = hlp1;
			}
		}
}

void ghost_referenceKernel(ghost_vdat_t *res, ghost_mnnz_t *col, ghost_midx_t *rpt, ghost_mdat_t *val, ghost_vdat_t *rhs, ghost_midx_t nrows, int spmvmOptions)
{
	ghost_midx_t i, j;
	ghost_vdat_t hlp1;

#pragma omp	parallel for schedule(runtime) private (hlp1, j)
	for (i=0; i<nrows; i++){
			hlp1 = 0.0;
		for (j=rpt[i]; j<rpt[i+1]; j++){
			hlp1 = hlp1 + (ghost_vdat_t)val[j] * rhs[col[j]]; // TODO do not multiply with zero if different datatypes 
		}
		if (spmvmOptions & GHOST_SPMVM_AXPY) 
			res[i] += hlp1;
		else
			res[i] = hlp1;
	}
}	

void ghost_freeCommunicator( ghost_comm_t* const comm ) 
{
	if(comm) {
		free(comm->lnEnts);
		free(comm->lnrows);
		free(comm->lfEnt);
		free(comm->lfRow);
		free(comm->wishes);
		free(comm->wishlist_mem);
		free(comm->wishlist);
		free(comm->dues);
		free(comm->duelist_mem);
		free(comm->duelist);
		free(comm->due_displ);
		free(comm->wish_displ);
		free(comm->hput_pos);
		free(comm);
	}
}

char * ghost_modeName(int mode) 
{
	switch (mode) {
		case GHOST_MODE_NOMPI:
			return "non-MPI";
		case GHOST_MODE_VECTORMODE:
			return "vector mode";
		case GHOST_MODE_GOODFAITH:
			return "g/f hybrid";
		case GHOST_MODE_TASKMODE:
			return "task mode";
		default:
			return "invalid";
	}
}

int ghost_symmetryValid(int symmetry)
{
	if ((symmetry & GHOST_BINCRS_SYMM_GENERAL) &&
			(symmetry & ~GHOST_BINCRS_SYMM_GENERAL))
		return 0;
	
	if ((symmetry & GHOST_BINCRS_SYMM_SYMMETRIC) &&
			(symmetry & ~GHOST_BINCRS_SYMM_SYMMETRIC))
		return 0;

	return 1;
}

char * ghost_symmetryName(int symmetry)
{
	if (symmetry & GHOST_BINCRS_SYMM_GENERAL)
		return "general";
	
	if (symmetry & GHOST_BINCRS_SYMM_SYMMETRIC)
		return "symmetric";

	if (symmetry & GHOST_BINCRS_SYMM_SKEW_SYMMETRIC) {
		if (symmetry & GHOST_BINCRS_SYMM_HERMITIAN)
			return "skew-hermitian";
		else
			return "skew-symmetric";
	} else {
		if (symmetry & GHOST_BINCRS_SYMM_HERMITIAN)
			return "hermitian";
	}

	return "invalid";
}

int ghost_datatypeValid(int datatype)
{
	if ((datatype & GHOST_BINCRS_DT_FLOAT) &&
			(datatype & GHOST_BINCRS_DT_DOUBLE))
		return 0;

	if (!(datatype & GHOST_BINCRS_DT_FLOAT) &&
			!(datatype & GHOST_BINCRS_DT_DOUBLE))
		return 0;
	
	if ((datatype & GHOST_BINCRS_DT_REAL) &&
			(datatype & GHOST_BINCRS_DT_COMPLEX))
		return 0;

	if (!(datatype & GHOST_BINCRS_DT_REAL) &&
			!(datatype & GHOST_BINCRS_DT_COMPLEX))
		return 0;

	return 1;
}

char * ghost_datatypeName(int datatype)
{
	if (datatype & GHOST_BINCRS_DT_FLOAT) {
		if (datatype & GHOST_BINCRS_DT_REAL)
			return "float";
		else
			return "complex float";
	} else {
		if (datatype & GHOST_BINCRS_DT_REAL)
			return "double";
		else
			return "complex double";
	}
}

char * ghost_workdistName(int options)
{
	if (options & GHOST_OPTION_WORKDIST_NZE)
		return "equal nze";
	else if (options & GHOST_OPTION_WORKDIST_LNZE)
		return "equal lnze";
	else
		return "equal rows";
}

int ghost_getRank() 
{
#ifdef MPI
	int rank;
	MPI_safecall(MPI_Comm_rank ( MPI_COMM_WORLD, &rank ));
	return rank;
#else
	return 0;
#endif
}

int ghost_getLocalRank() 
{
#ifdef MPI
	int rank;
	MPI_safecall(MPI_Comm_rank ( getSingleNodeComm(), &rank));

	return rank;
#else
	return 0;
#endif
}

int ghost_getNumberOfRanksOnNode()
{
#ifdef MPI
	int size;
	MPI_safecall(MPI_Comm_size ( getSingleNodeComm(), &size));

	return size;
#else
	return 1;
#endif

}
int ghost_getNumberOfPhysicalCores()
{
	FILE *fp;
	char nCoresS[4];
	int nCores;

	fp = popen("cat /sys/devices/system/cpu/cpu*/topology/thread_siblings_list | sort -u | wc -l","r");
	if (!fp) {
		printf("Failed to get number of physical cores\n");
	}

	fgets(nCoresS,sizeof(nCoresS)-1,fp);
	nCores = atoi(nCoresS);

	pclose(fp);

	return nCores;

}

int ghost_getNumberOfHwThreads()
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}

int ghost_getNumberOfThreads() 
{
	int nthreads;
#pragma omp parallel
	nthreads = omp_get_num_threads();

	return nthreads;
}

int ghost_getNumberOfNodes() 
{
#ifndef MPI
	return 1;
#else
	static int stringcmp(const void *x, const void *y)
	{
		return (strcmp((char *)x, (char *)y));
	}

	int nameLen,me,size,i,distinctNames = 1;
	char name[MPI_MAX_PROCESSOR_NAME];
	char *names = NULL;

	MPI_safecall(MPI_Comm_rank(MPI_COMM_WORLD,&me));
	MPI_safecall(MPI_Comm_size(MPI_COMM_WORLD,&size));
	MPI_safecall(MPI_Get_processor_name(name,&nameLen));


	if (me==0) {
		names = (char *)allocateMemory(size*MPI_MAX_PROCESSOR_NAME*sizeof(char),
				"names");
	}


	MPI_safecall(MPI_Gather(name,MPI_MAX_PROCESSOR_NAME,MPI_CHAR,names,
				MPI_MAX_PROCESSOR_NAME,MPI_CHAR,0,MPI_COMM_WORLD));

	if (me==0) {
		qsort(names,size,MPI_MAX_PROCESSOR_NAME*sizeof(char),stringcmp);
		for (i=1; i<size; i++) {
			if (strcmp(names+(i-1)*MPI_MAX_PROCESSOR_NAME,names+
						i*MPI_MAX_PROCESSOR_NAME)) {
				distinctNames++;
			}
		}
		free(names);
	}

	MPI_safecall(MPI_Bcast(&distinctNames,1,MPI_INT,0,MPI_COMM_WORLD));

	return distinctNames;
#endif
}

int ghost_getNumberOfProcesses() 
{
#ifndef MPI
	return 1;
#else

	int nnodes;

	MPI_safecall(MPI_Comm_size(MPI_COMM_WORLD, &nnodes));

	return nnodes;
#endif
}

size_t ghost_sizeofDataType(int dt)
{
	size_t size;

	if (dt & GHOST_BINCRS_DT_FLOAT)
		size = sizeof(float);
	else
		size = sizeof(double);

	if (dt & GHOST_BINCRS_DT_COMPLEX)
		size *= 2;

	return size;
}

int ghost_pad(int nrows, int padding) 
{
	int nrowsPadded;

	if(  nrows % padding != 0) {
		nrowsPadded = nrows + padding - nrows % padding;
	} else {
		nrowsPadded = nrows;
	}
	return nrowsPadded;
}

void* allocateMemory( const size_t size, const char* desc ) 
{

	/* allocate size bytes of posix-aligned memory;
	 * check for success and increase global counter */

	size_t boundary = 1024;
	int ierr;

	void* mem;

	DEBUG_LOG(2,"Allocating %8.2f MB of memory for %-18s  -- %6.3f", 
			size/(1024.0*1024.0), desc, (1.0*allocatedMem)/(1024.0*1024.0));

	if (  (ierr = posix_memalign(  (void**) &mem, boundary, size)) != 0 ) {
		ABORT("Error while allocating using posix_memalign: %s",strerror(ierr));
	}

	if( ! mem ) {
		ABORT("Error in memory allocation of %lu bytes for %s",size,desc);
	}

	allocatedMem += size;
	return mem;
}

void freeMemory( size_t size, const char* desc, void* this_array ) 
{

	DEBUG_LOG(2,"Freeing %8.2f MB of memory for %s", size/(1024.*1024.), desc);

	allocatedMem -= size;
	free (this_array);

}

double ghost_bench_spmvm(ghost_vec_t *res, ghost_context_t *context, ghost_vec_t *invec, 
		int kernel, int options, int nIter)
{
	int it;
	double time = 0;
	double oldtime=1e9;

	ghost_solver_t solver = NULL;
	solver = context->solvers[kernel];

	if (!solver)
		return -1.0;

#ifdef MPI
	MPI_safecall(MPI_Barrier(MPI_COMM_WORLD));
#endif
#ifdef OPENCL
	CL_barrier();
#endif

	for( it = 0; it < nIter; it++ ) {
		time = ghost_wctime();
		solver(res,context,invec,options);

#ifdef OPENCL
		CL_barrier();
#endif
#ifdef MPI
		MPI_safecall(MPI_Barrier(MPI_COMM_WORLD));
#endif
		time = ghost_wctime()-time;
		time = time<oldtime?time:oldtime;
		oldtime=time;
	}

#ifdef OPENCL
	DEBUG_LOG(1,"Downloading result from OpenCL device");
	CL_downloadVector(res);
#endif

	if ( 0x1<<kernel & GHOST_MODES_COMBINED)  {
		ghost_permuteVector(res->val,context->fullMatrix->invRowPerm,context->lnrows(context));
	} else if ( 0x1<<kernel & GHOST_MODES_SPLIT ) {
		// one of those must return immediately
		ghost_permuteVector(res->val,context->localMatrix->invRowPerm,context->lnrows(context));
		ghost_permuteVector(res->val,context->remoteMatrix->invRowPerm,context->lnrows(context));
	}

	return time;





}

