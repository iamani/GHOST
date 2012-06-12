#include <matricks.h>
#include "kernel_helper.h"
#include <mpi.h>
#include <omp.h>
#include <sys/types.h>

void hybrid_kernel_XIII(int current_iteration, VECTOR_TYPE* res, LCRP_TYPE* lcrp, VECTOR_TYPE* invec){

   /*****************************************************************************
    ********         Kernel ir -- c|x -- x|sw  -- lc|x -- nl|x           ********
    ********     Expliziter Ueberlapp von Rechnung und Kommunikation     ********
    ********        durch Abspalten eines Kommunikationsthreads          ********
    ********     - Aufbrechen in einzelne Anteile zum Ausmessen des      ********
    ********       Verhaeltnisses von lokaler Rechnung & Kommunikation   ********
    ********     - abgespaltener Kommunikationsthread pausiert waehrend  ********
    ********       des Umkopierens und der nichtlokalen Rechnung         ********
    ****************************************************************************/


   static int init_kernel=1; 
   static int max_dues;
   static double *work_mem, **work;
   static double hlp_sent;
   static double hlp_recv;

   int me; 
   int i, j, ierr;
   int from_PE, to_PE;
   int send_messages, recv_messages;

   uint64 asm_cycles, asm_cyclecounter, asm_acccyclecounter;
   uint64 cp_cycles;
   double time_it_took;
   uint64 glob_cycles, glob_cyclecounter;

   /* Required cycles for the individual contributions */
   uint64 ir_cycles, nl_cycles, lc_cycles, sw_cycles;
   uint64 cp_lin_cycles, cp_nlin_cycles, cp_res_cycles;

   double hlp1;
   static MPI_Request *send_request, *recv_request;
   static MPI_Status  *send_status,  *recv_status;

   int n_per_thread, n_local;
   int allshare, numelements;

   /* Thread-ID */
   int tid;

   size_t size_request, size_status, size_work, size_mem;

   /*****************************************************************************
    *******            ........ Executable statements ........           ********
    ****************************************************************************/
   IF_DEBUG(1) for_timing_start_asm_( &glob_cyclecounter);

   ierr = MPI_Comm_rank(MPI_COMM_WORLD, &me);

   if (init_kernel==1){

      max_dues = 0;
      for (i=0;i<lcrp->nodes;i++)
	 if (lcrp->dues[i]>max_dues) 
	    max_dues = lcrp->dues[i];

      hlp_sent = 0.0;
      hlp_recv = 0.0;
      for (i=0;i<lcrp->nodes; i++){
	 hlp_sent += lcrp->dues[i];
	 hlp_recv += lcrp->wishes[i];
      }

      IF_DEBUG(2) printf("Hybrid_kernel: PE %d: max_dues= %d\n", me, max_dues);

      size_mem     = (size_t)( max_dues*lcrp->nodes * sizeof( double  ) );
      size_work    = (size_t)( lcrp->nodes          * sizeof( double* ) );
      size_request = (size_t)( lcrp->nodes          * sizeof( MPI_Request ) );
      size_status  = (size_t)( lcrp->nodes          * sizeof( MPI_Status ) );

      work_mem = (double*)  allocateMemory( size_mem,  "work_mem" );
      work     = (double**) allocateMemory( size_work, "work" );

      for (i=0; i<lcrp->nodes; i++) work[i] = &work_mem[lcrp->due_displ[i]];

      send_request = (MPI_Request*) allocateMemory( size_request, "send_request" );
      recv_request = (MPI_Request*) allocateMemory( size_request, "recv_request" );
      send_status  = (MPI_Status*)  allocateMemory( size_status,  "send_status" );
      recv_status  = (MPI_Status*)  allocateMemory( size_status,  "recv_status" );

      init_kernel = 0;
   }


   asm_cycles = 0;
   send_messages=0;
   n_per_thread = lcrp->lnRows[me]/(lcrp->threads-1);
   for (i=0;i<lcrp->nodes;i++) send_request[i] = MPI_REQUEST_NULL;


   /*****************************************************************************
    *******        Post of Irecv to ensure that we are prepared...       ********
    ****************************************************************************/
   IF_DEBUG(1) for_timing_start_asm_( &asm_acccyclecounter);

   IF_DEBUG(2){
      for (from_PE=0; from_PE<lcrp->nodes; from_PE++){
	 if (lcrp->wishes[from_PE]>0) printf
	    ("HyK_XIII: PE%d: erwarte %d Elemente von PE%d (tag=%d) und schreibe diese an Stelle %d in invec->val\n", 
	     me,  lcrp->wishes[from_PE], from_PE, from_PE, lcrp->hput_pos[from_PE]);
      }
   }

   recv_messages = 0;
   for (from_PE=0; from_PE<lcrp->nodes; from_PE++){
      if (lcrp->wishes[from_PE]>0){
	 ierr = MPI_Irecv( &invec->val[lcrp->hput_pos[from_PE]], lcrp->wishes[from_PE], 
	       MPI_DOUBLE, from_PE, from_PE, MPI_COMM_WORLD, 
	       &recv_request[recv_messages] );
	 recv_messages++;
      }
   }

   IF_DEBUG(1){
      for_timing_stop_asm_( &asm_acccyclecounter, &asm_cycles);
      ir_cycles = asm_cycles - cycles4measurement; 
   } 
   /*****************************************************************************
    *******       Local assembly of halo-elements to be transfered       ********
    ****************************************************************************/
   IF_DEBUG(1) for_timing_start_asm_( &asm_acccyclecounter);

#ifdef OPEN_MPI
#pragma omp parallel                                                            \
   default   (none)                                                             \
   private   (i, j, ierr, to_PE, hlp1, tid, n_local, numelements, allshare)     \
   shared    (ompi_mpi_double, ompi_mpi_comm_world, lcrp, me, work, invec, send_request, res, n_per_thread,           \
	 send_status, recv_status, recv_request, recv_messages,                 \
	 asm_cycles, asm_cyclecounter, asm_acccyclecounter, cycles4measurement,                   \
	 cp_cycles, sw_cycles, lc_cycles, nl_cycles, cp_lin_cycles, cp_nlin_cycles, cp_res_cycles)  \
   reduction (+:send_messages) 
#else
#pragma omp parallel                                                            \
   default   (none)                                                             \
   private   (i, j, ierr, to_PE, hlp1, tid, n_local, numelements, allshare)     \
   shared    (lcrp, me, work, invec, send_request, res, n_per_thread,           \
	 send_status, recv_status, recv_request, recv_messages,                 \
	 asm_cycles, asm_cyclecounter, asm_acccyclecounter, cycles4measurement,                   \
	 cp_cycles, sw_cycles, lc_cycles, nl_cycles, cp_lin_cycles, cp_nlin_cycles, cp_res_cycles)  \
   reduction (+:send_messages)
#endif 

   {

#ifdef _OPENMP
      tid = omp_get_thread_num();
#endif

      /* spaetere Kommunikations-thread  beteiligt sich nicht am copy */
      if (tid < lcrp->threads-1){ 
	 /***********************************************************************
	  *******  Gather of elements to be distributed into work buffer ********
	  **********************************************************************/
	 for (to_PE=0 ; to_PE<lcrp->nodes ; to_PE++){

	    allshare = lcrp->dues[to_PE]/(lcrp->threads-1);
	    if (tid == lcrp->threads-2)
	       numelements = lcrp->dues[to_PE]-(lcrp->threads-2)*allshare;
	    else
	       numelements = allshare;

	    for (j=0; j<numelements; j++){
	       work[to_PE][tid*allshare+j] = 
		  invec->val[lcrp->duelist[to_PE][tid*allshare+j]];
	    }
	 }
      }

#pragma omp barrier

      IF_DEBUG(1){
#pragma omp single
	 {
	    for_timing_stop_asm_( &asm_acccyclecounter, &asm_cycles);
	    cp_cycles = asm_cycles - cycles4measurement; 
	    for_timing_start_asm_( &asm_acccyclecounter);
	 }
      }

      if (tid == lcrp->threads-1){ /* Kommunikations-thread */

	 /***********************************************************************
	  *******                     Communication                      ********
	  **********************************************************************/

	 /* Kommunikation in Form eines ringshifts um Staus zu vermeiden */
	 /* moegliche Optimierung mittels Liste gemaess Transfervolumen? */
	 /* erst zu allen mit groesser PE */ 
	 for (to_PE=me+1 ; to_PE<lcrp->nodes ; to_PE++){
	    if (lcrp->dues[to_PE]>0){
	       ierr = MPI_Isend( &work[to_PE][0], lcrp->dues[to_PE], MPI_DOUBLE,
		     to_PE, me, MPI_COMM_WORLD, &send_request[to_PE] );
	       send_messages++;
	    }
	 }
	 /* jetzt zu allen mit kleinerer PE */ 
	 for (to_PE=0; to_PE<me; to_PE++){
	    if (lcrp->dues[to_PE]>0){
	       ierr = MPI_Isend( &work[to_PE][0], lcrp->dues[to_PE], MPI_DOUBLE,
		     to_PE, me, MPI_COMM_WORLD, &send_request[to_PE] );
	       send_messages++;
	    }
	 }

	 ierr = MPI_Waitall(lcrp->nodes, send_request, send_status);
	 ierr = MPI_Waitall(recv_messages, recv_request, recv_status);
      }

#pragma omp barrier

      IF_DEBUG(1){
#pragma omp single
	 {
	    for_timing_stop_asm_( &asm_acccyclecounter, &asm_cycles);
	    sw_cycles = asm_cycles - cycles4measurement; 
	    for_timing_start_asm_( &asm_acccyclecounter);
	 }
      }


      if (tid < lcrp->threads-1){ /* Rechen-threads */

	 /***********************************************************************
	  *******     Calculation of SpMVM for local entries of invec->val     *******
	  **********************************************************************/
	 #ifdef OCLKERNEL
	 
	 if( tid == lcrp->threads-2 ) {
		 spmvmKernLocalXThread( lcrp, invec, res, &asm_cyclecounter, 
			&asm_cycles, &cycles4measurement, &lc_cycles, &cp_lin_cycles, &me);
	 }
	 
	 #else
	 
	 /* Alle threads gleichviel; letzter evtl. mehr */
	 if (tid < lcrp->threads-2)  n_local = n_per_thread;
	 else                        n_local = lcrp->lnRows[me]-(lcrp->threads-2)*n_per_thread;

	 IF_DEBUG(2) printf("HyK_XIII: PE%d thread%d: von %d bis %d\n", 
	       me, tid, tid*n_per_thread, tid*n_per_thread+n_local-1);


	 for (i=tid*n_per_thread; i<tid*n_per_thread+n_local; i++){
	    hlp1 = 0.0;
	    for (j=lcrp->lrow_ptr_l[i]; j<lcrp->lrow_ptr_l[i+1]; j++){
	       hlp1 = hlp1 + lcrp->lval[j] * invec->val[lcrp->lcol[j]]; 
	    }
	    res->val[i] = hlp1;
	 }
	 
	 #endif
	 
      }

#pragma omp barrier
      IF_DEBUG(1){
#pragma omp single
	 {
	    for_timing_stop_asm_( &asm_acccyclecounter, &asm_cycles);
	    lc_cycles = asm_cycles - cycles4measurement; 
	    for_timing_start_asm_( &asm_acccyclecounter);
	 }
      }

      /**************************************************************************
       *******    Calculation of SpMVM for non-local entries of invec->val     *******
       *************************************************************************/
      if (tid < lcrp->threads-1){ /* wieder nur die Rechenthreads */

	  #ifdef OCLKERNEL
	 
	 if( tid == lcrp->threads-2 ) {
		 spmvmKernRemoteXThread( lcrp, invec, res, &asm_cyclecounter, &asm_cycles, &cycles4measurement,
								&nl_cycles, &cp_nlin_cycles, &cp_res_cycles, &me);
	 }
	 
	 #else
	
	 /* Alle threads gleichviel; letzter evtl. mehr */
	 /* Imbalance !!!!!!!!!!!!!!!!!!!*/
	 if (tid < lcrp->threads-2)  n_local = n_per_thread;
	 else                        n_local = lcrp->lnRows[me]-(lcrp->threads-2)*n_per_thread;

	 for (i=tid*n_per_thread; i<tid*n_per_thread+n_local; i++){
	    hlp1 = 0.0;
	    for (j=lcrp->lrow_ptr_r[i]; j<lcrp->lrow_ptr_r[i+1]; j++){
	       hlp1 = hlp1 + lcrp->rval[j] * invec->val[lcrp->rcol[j]]; 
	    }
	    res->val[i] += hlp1;
	 }
	 #endif
	 
      }

      IF_DEBUG(1){
#pragma omp barrier
#pragma omp single
	 {
	    for_timing_stop_asm_( &asm_acccyclecounter, &asm_cycles);
	    nl_cycles = asm_cycles - cycles4measurement; 
	 }
      }
   }

   IF_DEBUG(1){
      for_timing_stop_asm_( &glob_cyclecounter, &glob_cycles);
      glob_cycles = glob_cycles - cycles4measurement; 
   }
   /*****************************************************************************
    *******    Writeout of timing res->valults for individual contributions    *******
    ****************************************************************************/
   IF_DEBUG(1){

      time_it_took = (1.0*ir_cycles)/clockfreq;
      printf("HyK_XIII: PE %d: It %d: Absetzen des Irecv [ms]           : %8.3f",
	    me, current_iteration, 1000*time_it_took);
      printf(" (in cycles %llu) Nachrichten: %d\n", ir_cycles, recv_messages );

      time_it_took = (1.0*cp_cycles)/clockfreq;
      printf("HyK_XIII: PE %d: It %d: Umkopieren [ms]                   : %8.3f",
	    me, current_iteration, 1000*time_it_took);
      printf(" Datenvolumen: %6.3f MB (@%6.3f GB/s)\n", 8e-6*hlp_sent, 
	    8e-9*hlp_sent/time_it_took);

      time_it_took = (1.0*sw_cycles)/clockfreq;
      printf("HyK_XIII: PE %d: It %d: Kommunikation [ms]                : %8.3f",
	    me, current_iteration, 1000*time_it_took);
      printf(" Datenvolumen: %6.3f MB (@%6.3f GB/s) Nachrichten: %d\n", 
	    8e-6*hlp_sent, 8e-9*hlp_sent/time_it_took, send_messages );

      time_it_took = (1.0*lc_cycles)/clockfreq;
      printf("HyK_XIII: PE %d: It %d: SpMVM (lokale Elemente) [ms]      : %8.3f",
	    me, current_iteration, 1000*time_it_took);
      printf(" nnz_l = %d (@%7.3f GFlop/s)\n", lcrp->lrow_ptr_l[lcrp->lnRows[me]], 
	    2e-9*lcrp->lrow_ptr_l[lcrp->lnRows[me]]/time_it_took);

      time_it_took = (1.0*nl_cycles)/clockfreq;
      printf("HyK_XIII: PE %d: It %d: SpMVM (nichtlokale Elemente) [ms] : %8.3f",
	    me, current_iteration, 1000*time_it_took);
      printf(" nnz_nl= %d (@%7.3f GFlop/s)\n", lcrp->lrow_ptr_r[lcrp->lnRows[me]], 
	    2e-9*lcrp->lrow_ptr_r[lcrp->lnRows[me]]/time_it_took);

	#ifdef OCLKERNEL
      time_it_took = (1.0*cp_lin_cycles)/clockfreq;
      printf("HyK_XIII: PE %d: It %d: Rhs (lokal) nach Device [ms]      : %8.3f",
	    me, current_iteration, 1000*time_it_took);
      printf(" Datenvolumen: %6.3f MB (@%6.3f GB/s)\n", 8e-6*lcrp->lnRows[me], 
	     8e-9*lcrp->lnRows[me]/time_it_took);

      time_it_took = (1.0*cp_nlin_cycles)/clockfreq;
      printf("HyK_XIII: PE %d: It %d: Rhs (nichtlokal) nach Device [ms] : %8.3f",
	    me, current_iteration, 1000*time_it_took);
      printf(" Datenvolumen: %6.3f MB (@%6.3f GB/s)\n", 8e-6*lcrp->halo_elements, 
	     8e-9*lcrp->halo_elements/time_it_took);

      time_it_took = (1.0*cp_res_cycles)/clockfreq;
      printf("HyK_XIII: PE %d: It %d: Res von Device [ms]               : %8.3f",
	    me, current_iteration, 1000*time_it_took);
      printf(" Datenvolumen: %6.3f MB (@%6.3f GB/s)\n", 8e-6*res->nRows, 
	     8e-9*res->nRows/time_it_took);
    #endif
    
      time_it_took = (1.0*glob_cycles)/clockfreq;
      printf("HyK_XIII: PE %d: It %d: Kompletter Hybrid-kernel [ms]     : %8.3f\n", 
	    me, current_iteration, 1000*time_it_took); fflush(stdout); 

   }

}
