#include <ghost.h>
#include <ghost_vec.h>
#include <ghost_util.h>
#include <ghost_taskq.h>

#ifdef VT
#include <VT.h>
#endif
#include <omp.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <libgen.h>
#include <strings.h>
#ifdef GHOST_MPI
#include <mpi.h>
#endif

#define TASKING
//#define CHECK // compare with reference solution

GHOST_REGISTER_DT_D(vecdt)
GHOST_REGISTER_DT_D(matdt)
#define EPS 1.e-3

#ifdef TASKING
typedef struct {
	ghost_mat_t *mat;
	ghost_vec_t **lhs, **rhs;
	ghost_vtraits_t *ltr, *rtr;
	ghost_context_t *ctx;
	char *matfile;
	vecdt_t *lhsInit;
	void (*rhsInit)(int,int,void*);
} createDataArgs;

typedef struct {
	ghost_context_t *ctx;
	ghost_mat_t *mat;
	ghost_vec_t *lhs, *rhs;
	int *spmvmOptions;
	int nIter;
	double *time;
} benchArgs;


static void *createDataFunc(void *vargs)
{
	createDataArgs *args = (createDataArgs *)vargs;
	args->mat->fromFile(args->mat,args->matfile);
	*(args->lhs) = ghost_createVector(args->ctx,args->ltr);
	*(args->rhs) = ghost_createVector(args->ctx,args->rtr);
	(*(args->lhs))->fromScalar(*(args->lhs),args->lhsInit);
	(*(args->rhs))->fromFunc(*(args->rhs),args->rhsInit);

	return NULL;
}
static void *benchFunc(void *vargs)
{
//#pragma omp parallel
//	WARNING_LOG("Thread %d running @ core %d",omp_get_thread_num(),ghost_getCore());
	benchArgs *args = (benchArgs *)vargs;
	*(args->time) = ghost_bench_spmvm(args->ctx,args->lhs,args->mat,args->rhs,args->spmvmOptions,args->nIter);

	return NULL;
}
#endif

static void rhsVal (int i, int v, void *val) 
{
	UNUSED(i);
	UNUSED(v);
	*(vecdt_t *)val = 1+I*1;//i + (vecdt_t)1.0 + I*i;
}

int main( int argc, char* argv[] ) 
{

	int  mode, nIter = 100;
	double time;
	vecdt_t zero = 0.;
	matdt_t shift = 0.;

#ifdef CHECK
	ghost_midx_t i, errcount = 0;
	double mytol;
#endif

	int modes[] = {
	//	GHOST_SPMVM_MODE_NOMPI,
		GHOST_SPMVM_MODE_VECTORMODE,
		GHOST_SPMVM_MODE_GOODFAITH,
		GHOST_SPMVM_MODE_TASKMODE
	};
		int nModes = sizeof(modes)/sizeof(int);

	int spmvmOptions = GHOST_SPMVM_AXPY /* | GHOST_SPMVM_APPLY_SHIFT*/;

	ghost_mat_t *mat; // matrix
	ghost_vec_t *lhs = NULL; // lhs vector
	ghost_vec_t *rhs = NULL; // rhs vector

	ghost_context_t *context;

	char *matrixPath = argv[1];
	ghost_mtraits_t mtraits = GHOST_MTRAITS_INIT(.format = GHOST_SPM_FORMAT_CRS, .datatype = matdt, .shift = &shift);
	ghost_vtraits_t lvtraits = GHOST_VTRAITS_INIT(.flags = GHOST_VEC_LHS, .datatype = vecdt);
	ghost_vtraits_t rvtraits = GHOST_VTRAITS_INIT(.flags = GHOST_VEC_RHS, .datatype = vecdt);

	if (argc == 5) {
		if (!(strcasecmp(argv[2],"CRS")))
			mtraits.format = GHOST_SPM_FORMAT_CRS;
		else if (!(strcasecmp(argv[2],"SELL")))
			mtraits.format = GHOST_SPM_FORMAT_SELL;
		mtraits.flags = atoi(argv[3]);
		int sortBlock = atoi(argv[4]);
		int aux[2];
		aux[0] = sortBlock;
		//aux[1] = GHOST_SELL_CHUNKHEIGHT_ELLPACK; 
		aux[1] = 32; 
		mtraits.aux = &aux;
	}
	ghost_init(argc,argv);       // basic initialization

//	int nthreads[] = {ghost_getNumberOfPhysicalCores(),ghost_getNumberOfPhysicalCores()};
	int nthreads[] = {12,0};
	int firstthr[] = {0,0};
	int levels = 2;
	ghost_tasking_init(nthreads,firstthr,levels);
//	ghost_tasking_init(GHOST_THPOOL_NTHREADS_FULLNODE,GHOST_THPOOL_FTHREAD_DEFAULT,GHOST_THPOOL_LEVELS_FULLSMT);


#ifndef TASKING
	ghost_pinThreads(GHOST_PIN_PHYS,NULL);
#endif
/*	char *map;
	hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
	hwloc_get_cpubind(topology,cpuset,HWLOC_CPUBIND_THREAD);
	hwloc_bitmap_list_asprintf(&map,cpuset);
	WARNING_LOG("Running @ cores %s",map);
*/
	context = ghost_createContext(GHOST_GET_DIM_FROM_MATRIX,GHOST_GET_DIM_FROM_MATRIX,GHOST_CONTEXT_DEFAULT,matrixPath,MPI_COMM_WORLD,1.0);
	mat = ghost_createMatrix(context,&mtraits,1);


#ifdef TASKING
	createDataArgs args = {.mat = mat, .lhs = &lhs, .rhs = &rhs, .matfile = matrixPath, .lhsInit = &zero, .rhsInit = rhsVal, .rtr = &rvtraits, .ltr = &lvtraits, .ctx = context};
	ghost_task_t *createDataTask = ghost_task_init(nthreads[0], 0, &createDataFunc, &args, GHOST_TASK_NO_HYPERTHREADS);
	ghost_task_add(createDataTask);
#else
	mat->fromFile(mat,matrixPath);
	lhs = ghost_createVector(context,&lvtraits);
	rhs = ghost_createVector(context,&rvtraits);
	lhs->fromScalar(lhs,&zero);
	rhs->fromFunc(rhs,&rhsVal);
#endif

//	ghost_printSysInfo();
//	ghost_printGhostInfo();
//	ghost_printContextInfo(context);

#ifdef TASKING
	ghost_task_wait(createDataTask);
	ghost_task_destroy(createDataTask);
#endif
	ghost_printMatrixInfo(mat);
#ifdef CHECK
	ghost_vec_t *goldLHS = ghost_createVector(context,&lvtraits);
	ghost_referenceSolver(goldLHS,matrixPath,matdt,rhs,nIter,spmvmOptions);	
#endif


	if (ghost_getRank(MPI_COMM_WORLD) == 0)
		ghost_printHeader("Performance");

	for (mode=0; mode < nModes; mode++){

		int argOptions = spmvmOptions | modes[mode];
#ifdef TASKING
		benchArgs bargs = {.ctx = context, .mat = mat, .lhs = lhs, .rhs = rhs, .spmvmOptions = &argOptions, .nIter = nIter, .time = &time};
//		if (modes[mode] != GHOST_SPMVM_MODE_TASKMODE) {
		ghost_task_t *benchTask = ghost_task_init(nthreads[0], 0, &benchFunc, &bargs, GHOST_TASK_NO_HYPERTHREADS);
		ghost_task_add(benchTask);
		ghost_task_wait(benchTask);
		ghost_task_destroy(benchTask);
//		} else {
//		omp_set_num_threads(nthreads[0]);
//			ghost_pinThreads(GHOST_PIN_MANUAL,"0,1,2,3,4,5,6,7,8,9,10");
//			benchFunc(&bargs);
//		}
#else
		time = ghost_bench_spmvm(context,lhs,mat,rhs,&argOptions,nIter);
#endif

		if (time < 0.) {
			if (ghost_getRank(MPI_COMM_WORLD) == 0)
				ghost_printLine(ghost_modeName(modes[mode]),NULL,"SKIPPED");
			continue;
		}

#ifdef CHECK
		errcount=0;
		vecdt_t res,ref;
		for (i=0; i<mat->nrows(mat); i++){
			goldLHS->entry(goldLHS,i,0,&ref);
			lhs->entry(lhs,i,0,&res);

			mytol = EPS * mat->rowLen(mat,i);
			if (creal(cabs(ref-res)) > creal(mytol) ||
					cimag(cabs(ref-res)) > cimag(mytol)){
				printf( "PE%d: error in %s, row %"PRmatIDX": %.2e + %.2ei vs. %.2e +"
						"%.2ei [ref. vs. comp.] (tol: %.2e + %.2ei, diff: %e)\n", ghost_getRank(MPI_COMM_WORLD),ghost_modeName(modes[mode]), i, creal(ref),
						cimag(ref),
						creal(res),
						cimag(res),
						creal(mytol),cimag(mytol),creal(cabs(ref-res)));
				errcount++;
				printf("PE%d: There may be more errors...\n",ghost_getRank(MPI_COMM_WORLD));
				break;
			}
		}
		ghost_midx_t totalerrors;
#ifdef GHOST_MPI
		MPI_safecall(MPI_Allreduce(&errcount,&totalerrors,1,ghost_mpi_dt_midx,MPI_SUM,MPI_COMM_WORLD));
#else
		totalerrors = errcount;
#endif
		if (totalerrors) {
			if (ghost_getRank(MPI_COMM_WORLD) == 0)
				ghost_printLine(ghost_modeName(modes[mode]),NULL,"FAILED");
		}
		else {
			ghost_mnnz_t nnz = ghost_getMatNnz(mat);
			if (ghost_getRank(MPI_COMM_WORLD) == 0)
				ghost_printLine(ghost_modeName(modes[mode]),"GF/s","%.2f",
						2*(1.e-9*nnz)/time);
		}
#else
		ghost_mnnz_t nnz = ghost_getMatNnz(mat);
		if (ghost_getRank(MPI_COMM_WORLD) == 0)
			ghost_printLine(ghost_modeName(modes[mode]),"GF/s","%.2f",2*(1.e-9*nnz)/time);
#endif
		lhs->zero(lhs);

	}
	if (ghost_getRank(MPI_COMM_WORLD) == 0)
		ghost_printFooter();

	mat->destroy(mat);
	lhs->destroy(lhs);
	rhs->destroy(rhs);
	ghost_freeContext(context);

#ifdef CHECK
	goldLHS->destroy(goldLHS);
#endif

	ghost_tasking_finish(nthreads,firstthr,levels);
	ghost_finish();

	return EXIT_SUCCESS;

}
