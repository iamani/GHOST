#include <mpi.h>
#include <omp.h>
#include <sys/types.h>
#include <string.h>

#include "ghost_util.h"

void hybrid_kernel_II(ghost_context_t *context, ghost_vec_t* res, ghost_mat_t* mat, ghost_vec_t* invec, int spmvmOptions)
{

	/*****************************************************************************
	 ********              Kernel ir -- cs -- lc -- wa -- nl              ********
	 ********   'Good faith'- Ueberlapp von Rechnung und Kommunikation    ********
	 ********     - das was andere als 'hybrid' bezeichnen                ********
	 ********     - ob es klappt oder nicht haengt vom MPI ab...          ********
	 ****************************************************************************/

	static int init_kernel=1; 
	static ghost_mnnz_t max_dues;
	static char *work_mem, **work;
	static int nprocs;
	static double hlp_sent;
	static double hlp_recv;

	static int me; 
	int i, from_PE, to_PE;
	int send_messages, recv_messages;

	//static MPI_Request *send_request, *recv_request;
	//static MPI_Status  *send_status,  *recv_status;
	static MPI_Request *request;
	static MPI_Status  *status;

	size_t size_request, size_status, size_work, size_mem;
	size_t sizeofRHS = ghost_sizeofDataType(invec->traits->datatype);


	if (init_kernel==1){
		MPI_safecall(MPI_Comm_rank(MPI_COMM_WORLD, &me));
		nprocs = ghost_getNumberOfProcesses();

		max_dues = 0;
		for (i=0;i<nprocs;i++)
			if (context->communicator->dues[i]>max_dues) 
				max_dues = context->communicator->dues[i];

		hlp_sent = 0.0;
		hlp_recv = 0.0;
		for (i=0;i<nprocs; i++){
			hlp_sent += context->communicator->dues[i];
			hlp_recv += context->communicator->wishes[i];
		}


		size_mem     = (size_t)( max_dues*nprocs * ghost_sizeofDataType(invec->traits->datatype) );
		size_work    = (size_t)( nprocs          * sizeof( void * ) );
		size_request = (size_t)( 2*nprocs          * sizeof( MPI_Request ) );
		size_status  = (size_t)( 2*nprocs          * sizeof( MPI_Status ) );

		work_mem = (char*)  allocateMemory( size_mem,  "work_mem" );
		work     = (char**) allocateMemory( size_work, "work" );

		for (i=0; i<nprocs; i++) work[i] = &work_mem[context->communicator->due_displ[i]];

		/*send_request = (MPI_Request*) allocateMemory( size_request, "send_request" );
		recv_request = (MPI_Request*) allocateMemory( size_request, "recv_request" );
		send_status  = (MPI_Status*)  allocateMemory( size_status,  "send_status" );
		recv_status  = (MPI_Status*)  allocateMemory( size_status,  "recv_status" );*/
		request = (MPI_Request*) allocateMemory( size_request, "request" );
		status  = (MPI_Status*)  allocateMemory( size_status,  "status" );

		init_kernel = 0;
	}


	send_messages=0;
	recv_messages = 0;
	for (i=0;i<nprocs;i++) request[i] = MPI_REQUEST_NULL;

#ifdef LIKWID_MARKER
#pragma omp parallel
	likwid_markerStartRegion("Kernel 2");
#endif
#ifdef LIKWID_MARKER_FINE
#pragma omp parallel
	likwid_markerStartRegion("Kernel 2 -- communication");
#endif

	for (from_PE=0; from_PE<nprocs; from_PE++){
		if (context->communicator->wishes[from_PE]>0){
			MPI_safecall(MPI_Irecv(&((char *)(invec->val))[context->communicator->hput_pos[from_PE]*sizeofRHS], context->communicator->wishes[from_PE]*sizeofRHS,MPI_CHAR, from_PE, from_PE, MPI_COMM_WORLD,&request[recv_messages] ));
			/*MPI_safecall(MPI_Irecv( &invec->val[context->communicator->hput_pos[from_PE]], context->communicator->wishes[from_PE], ghost_mpi_dt_vdat, from_PE, from_PE, MPI_COMM_WORLD, &request[recv_messages] ));*/
			recv_messages++;
		}
	}

	/*****************************************************************************
	 *******       Local assembly of halo-elements  & Communication       ********
	 ****************************************************************************/

//#pragma omp parallel private(to_PE,i) reduction(+:send_messages)
	for (to_PE=0 ; to_PE<nprocs ; to_PE++){
//#pragma omp for 
		for (i=0; i<context->communicator->dues[to_PE]; i++){
//			work[to_PE][i] = invec->val[context->communicator->duelist[to_PE][i]];
			memcpy(&(work[to_PE][i*sizeofRHS]),&((char *)(invec->val))[context->communicator->duelist[to_PE][i]*sizeofRHS],sizeofRHS);
			printf("copy %d->%d: %f [%d] -> %f ... %p->%p %lu bytes\n",ghost_getRank(),to_PE, ((double *)(invec->val))[context->communicator->duelist[to_PE][i]],context->communicator->duelist[to_PE][i],((double **)(work))[to_PE][i],&((char *)(invec->val))[context->communicator->duelist[to_PE][i]*sizeofRHS],&work[to_PE][i*sizeofRHS],sizeofRHS);
		}
	}
	for (to_PE=0 ; to_PE<nprocs ; to_PE++){
		DEBUG_LOG(1,"No. dues ->%d: %d",to_PE,context->communicator->dues[to_PE]);
		for (i=0; i<context->communicator->dues[to_PE]; i++)
			printf("send %d->%d: %f %p\n",ghost_getRank(),to_PE,((double **)work)[to_PE][i],&((double **)work)[to_PE][i] );
		if (context->communicator->dues[to_PE]>0){
			MPI_safecall(MPI_Isend( &work[to_PE][0], context->communicator->dues[to_PE]*sizeofRHS, 
					MPI_CHAR, to_PE, me, MPI_COMM_WORLD, 
					&request[recv_messages+send_messages] ));
			/*MPI_safecall(MPI_Isend( &work[to_PE][0], context->communicator->dues[to_PE], 
					ghost_mpi_dt_vdat, to_PE, me, MPI_COMM_WORLD, 
					&request[recv_messages+send_messages] ));*/
			send_messages++;
		}
	}

	/****************************************************************************
	 *******       Calculation of SpMVM for local entries of invec->val        *******
	 ***************************************************************************/

#ifdef LIKWID_MARKER_FINE
#pragma omp parallel
	likwid_markerStartRegion("Kernel 2 -- local computation");
#endif

#ifdef OPENCL
	CL_copyHostToDevice(invec->CL_val_gpu, invec->val, context->lnrows(context)*sizeof(ghost_vdat_t));
#endif
#ifdef CUDA
	CU_copyHostToDevice(invec->CU_val, invec->val, context->lnrows(context)*sizeof(ghost_vdat_t));
#endif

	mat->localPart->kernel(mat->localPart,res,invec,spmvmOptions);
	//spmvmKernAll( context->localMatrix->data, invec, res, spmvmOptions);

#ifdef LIKWID_MARKER_FINE
#pragma omp parallel
	likwid_markerStopRegion("Kernel 2 -- local computation");
#endif

	/****************************************************************************
	 *******       Finishing communication: MPI_Waitall                   *******
	 ***************************************************************************/

	//MPI_safecall(MPI_Waitall(send_messages, send_request, send_status));
	//MPI_safecall(MPI_Waitall(recv_messages, recv_request, recv_status));
	MPI_safecall(MPI_Waitall(send_messages+recv_messages, request, status));

#ifdef LIKWID_MARKER_FINE
#pragma omp parallel
	{
	likwid_markerStopRegion("Kernel 2 -- communication");
	likwid_markerStartRegion("Kernel 2 -- remote computation");
	}
#endif

	/****************************************************************************
	 *******     Calculation of SpMVM for non-local entries of invec->val      *******
	 ***************************************************************************/
#ifdef OPENCL
	CL_copyHostToDeviceOffset(invec->CL_val_gpu, 
			invec->val+context->lnrows(context), context->communicator->halo_elements*sizeof(ghost_vdat_t),
			context->lnrows(context)*sizeof(ghost_vdat_t));
#endif
#ifdef CUDA
	CU_copyHostToDevice(&invec->CU_val[context->lnrows(context)], 
			&invec->val[context->lnrows(context)], context->communicator->halo_elements*sizeof(ghost_vdat_t));
#endif

	mat->remotePart->kernel(mat->remotePart,res,invec,spmvmOptions|GHOST_SPMVM_AXPY);

#ifdef LIKWID_MARKER_FINE
#pragma omp parallel
	likwid_markerStopRegion("Kernel 2 -- remote computation");
#endif

#ifdef LIKWID_MARKER
#pragma omp parallel
	likwid_markerStopRegion("Kernel 2");
#endif


}
