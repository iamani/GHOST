#include "ghost_util.h"
#include <stdio.h>

#ifdef LIKWID_PERFMON
#include <likwid.h>
#endif

void ghost_solver_nompi(ghost_vec_t* res, ghost_context_t* context, ghost_vec_t* invec, int spmvmOptions)
{
#ifdef LIKWID_PERFMON
#pragma omp parallel
	LIKWID_MARKER_START("full SpMVM");
#endif
	context->fullMatrix->kernel(context->fullMatrix,res,invec,spmvmOptions);
#ifdef LIKWID_PERFMON
#pragma omp parallel
	LIKWID_MARKER_STOP("full SpMVM");
#endif
}
