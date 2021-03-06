#include "ghost/config.h"
#include "ghost/types.h"
#include "ghost/util.h"
#include "ghost/math.h"
#include "ghost/omp.h"
#include "ghost/rcm_dissection.h"
#include <vector>
#include <iostream>

//returns the virtual column index; ie takes into account the permutation of halo elements also
#define virtual_col(col_idx)\
   (mat->context->flags & GHOST_PERM_NO_DISTINCTION && mat->context->col_map->loc_perm)?( (col_ptr[col_idx]<mat->context->col_map->dimpad)?col_ptr[col_idx]:mat->context->col_map->loc_perm[col_ptr[col_idx]] ):col_ptr[col_idx]\


ghost_error find_transition_zone(ghost_sparsemat *mat, int n_threads)
{ 
if(n_threads>1)
{
   int nrows    = SPM_NROWS(mat);
   int ncols    = mat->context->maxColRange+1;
   int n_zones  = 2* n_threads;//odd zone = even zone
   int total_bw = mat->context->bandwidth;
   double diagonal_slope = static_cast<double>(nrows)/ncols;
   int min_local_height  = static_cast<int>((diagonal_slope*total_bw));// + 1 ;//min. required height by transition zone
   int max_local_height  = static_cast<int>(static_cast<double>(nrows)/n_zones);
   int height            = std::max(min_local_height,max_local_height);

   int total_odd_rows    = n_threads*height;
   int total_even_rows   = nrows - total_odd_rows;
 
   mat->context->zone_ptr         = new ghost_lidx[n_zones+1]; 
   int* zone_ptr         = mat->context->zone_ptr; 
 
   bool flag_level = true; //flag for recursive call in current thread does not fit
   bool flag_lb    = true; //flag for load balancing
   
   //if overlap occurs restart with less threads
   if(total_even_rows<total_odd_rows){
	flag_level = false;
        delete[] zone_ptr;
	find_transition_zone(mat, n_threads-1);
    }
  
   int local_even_rows = static_cast<int>(static_cast<double>(total_even_rows)/n_threads); 

   //initial uniform splitting
   if(flag_level){
    	for(int i=0; i<n_threads; ++i){
		zone_ptr[2*i+1]   = (i+1)* local_even_rows + i*height -1 ;
	      	zone_ptr[2*i+2]   = zone_ptr[2*i+1] + height ;
	}
   	zone_ptr[0] = 0;
	zone_ptr[n_zones] = nrows;
    }

   //now we try for load_balancing 
   if(flag_level && flag_lb){
        //initial check: whether there is sufficient room to play
	if(max_local_height <= min_local_height){
	  	flag_lb = false;
		WARNING_LOG("RED BLACK SPLITTING : Load balancing not possible"); 
                
	} else {
       
            int nnz = SPM_NNZ(mat);
            int* load;
            load  =  new int[n_zones];
 
            int min_rows  = nrows*10; //just for checking
            int uniform_nnz = static_cast<int>(static_cast<double>(nnz)/n_zones);

	
            for(int i=0; i<n_zones; ++i){
                    int ctr_nnz = 0;
                    for(int row=zone_ptr[i]; row<zone_ptr[i+1]; ++row){
                        ctr_nnz += mat->rowLen[row];
                    }
            load[i] = static_cast<int>( static_cast<double>(uniform_nnz)/ctr_nnz * (zone_ptr[i+1] - zone_ptr[i]) );

            min_rows = std::min( load[i],min_rows);
            }
       

            //check whether this splitting is possible
            if(min_rows < min_local_height){
                flag_lb = false;
                WARNING_LOG("RED BLACK SPLITTING : Load balancing not possible");
            }
            else{
	      	zone_ptr[0] = 0;
               
		//assign zone with new loads
		for(int i=1;i<n_zones+1; ++i){
                    zone_ptr[i]=0;
                    for(int j=0; j<i; ++j){
                        zone_ptr[i] += load[j];
                    }
		}
		zone_ptr[n_zones] = nrows;

            }
	
            delete[] load;
    }
 
  }//flag_level && flag_lb
 if(flag_level)
     {
        mat->context->nzones = n_zones;
        mat->context->zone_ptr = zone_ptr;  
     }
      
}
else{
  mat->context->nzones = 2;
  mat->context->zone_ptr = new int[mat->context->nzones+1];
  mat->context->zone_ptr[0] = 0;
  mat->context->zone_ptr[1] = static_cast<int>(static_cast<double>(SPM_NROWS(mat))/mat->context->nzones);
  mat->context->zone_ptr[2] = SPM_NROWS(mat);
}

//make it compatible for bmc kernel, so only one kernel is enough
  ghost_lidx new_nzones = 2*mat->context->nzones; //double the zones
  ghost_lidx *new_zone_ptr; // = new ghost_lidx[new_nzones+1];

  ghost_malloc((void **)&new_zone_ptr, (new_nzones+1)*sizeof(ghost_lidx));
//  ghost_malloc((void **)&new_zone_ptr,(new_nzones+1)*sizeof(ghost_lidx)); 
 
  for(int i=0; i< mat->context->nzones; i+=2) {
	new_zone_ptr[2*i] = mat->context->zone_ptr[i];
	new_zone_ptr[2*i+1] = mat->context->zone_ptr[i+1];
	new_zone_ptr[2*i+2] = mat->context->zone_ptr[i+1];
 	new_zone_ptr[2*i+3] = mat->context->zone_ptr[i+1];
   }
  new_zone_ptr[new_nzones] = mat->context->zone_ptr[mat->context->nzones];
//  new_zone_ptr[new_nzones+1] = mat->context->zone_ptr[mat->context->nzones];
 
  delete[] mat->context->zone_ptr;
  mat->context->zone_ptr = new_zone_ptr;
  mat->context->nzones   = new_nzones;
  mat->context->kacz_setting.kacz_method = GHOST_KACZ_METHOD_BMC_one_sweep;

  mat->context->ncolors = 1;//mat->context->zone_ptr[mat->context->nzones+1] - mat->context->zone_ptr[mat->context->nzones];
  ghost_malloc((void **)&mat->context->color_ptr,(mat->context->ncolors+1)*sizeof(ghost_lidx)); 
  
  for(int i=0; i<mat->context->ncolors+1; ++i) {
	mat->context->color_ptr[i] = mat->context->zone_ptr[mat->context->nzones];
  }
 
  return GHOST_SUCCESS;
}

//checks whether the partitioning was correct, its just for debugging
ghost_error checker_rcm(ghost_sparsemat *mat)
{
  ghost_lidx *row_ptr = mat->chunkStart;
  ghost_lidx *col_ptr = mat->col;

  ghost_lidx *upper_col_ptr = new ghost_lidx[mat->context->nzones];
  ghost_lidx *lower_col_ptr = new ghost_lidx[mat->context->nzones];

  for(int i=0; i<mat->context->nzones; ++i){
    upper_col_ptr[i] = 0;
    lower_col_ptr[i] = 0;
  }

  for(int i=0; i<mat->context->nzones;++i){
   for(int j=mat->context->zone_ptr[i]; j<mat->context->zone_ptr[i+1] ; ++j){
       ghost_lidx upper_virtual_col = virtual_col(row_ptr[j+1]-1);
       ghost_lidx lower_virtual_col = virtual_col(row_ptr[j]); 
       upper_col_ptr[i] = std::max(upper_virtual_col , upper_col_ptr[i]);
       lower_col_ptr[i] = std::max(lower_virtual_col , lower_col_ptr[i]);
   }
  }

  for(int i=0; i<mat->context->nzones-2; ++i){
   if(upper_col_ptr[i] >= lower_col_ptr[i+1] ){
     return  GHOST_ERR_RED_BLACK;
    }
  }

 return GHOST_SUCCESS;    
}

extern "C" ghost_error ghost_rcm_dissect(ghost_sparsemat *mat){	
    	
        int n_threads = 0;

//        int *zone_ptr;

#pragma omp parallel
 {
    #pragma omp single
    {
        n_threads = ghost_omp_nthread();
	find_transition_zone(mat, n_threads);
 
        int n_zones = mat->context->nzones;

	if(mat->traits.C == 1) {
        	INFO_LOG("CHECKING BLOCK COLORING")
        	checker_rcm(mat);
        	INFO_LOG("CHECKING FINISHED");     
        }

         if(n_threads > n_zones/2){
            WARNING_LOG("RED BLACK splitting: Can't use all the threads , Usable threads = %d",n_zones/2);
         }

	mat->context->kacz_setting.active_threads = int( (double)(n_zones)/4);
     }    
  }  

#ifdef GHOST_KACZ_ANALYZE 
 kacz_analyze_print(mat);
#endif

   return GHOST_SUCCESS; 
}



//finds transition part of  RCM matrix , This function even finds whether there are 
//pure zones within the transitional part which we don't require and has much more
//overhead, compared to the currently used version
//This function(with some modification) can be used to predict rows which could 
//possibly false share 
/*std::vector<int> find_transition_zone_not_used(ghost_sparsemat *mat, int n_zones)
{
    if(n_zones >= 2)
    {
    std::vector<int> transition_zone;
    std::vector<int> compressed_transition;
     
    int n_t_zones = n_zones-1;
    int nrows     =  SPM_NROWS(mat);

    int* lower_col_ptr = new int[n_t_zones];
    int* upper_col_ptr = new int[n_t_zones];
    
    for(int i=0; i!= n_t_zones; ++i){
        lower_col_ptr[i] = -1;
        upper_col_ptr[i] = -1;
    }
    
    ghost_lidx *row_ptr = mat->chunkStart;
    ghost_lidx *col_ptr = mat->col;
    
    //approximate
    ghost_lidx local_size = (int) (SPM_NROWS(mat) / n_zones);
    std::cout<<"local_size = "<<local_size<<std::endl;

    int *rhs_split;
    rhs_split = new int[n_t_zones];
    
    for(int i=0; i<n_t_zones; ++i){
        rhs_split[i] = (i+1)*local_size;
    }

    bool flag = false;
    
    for(int i=0; i<nrows; ++i){
        for(int k=0; k<n_t_zones; ++k){
            if(col_ptr[row_ptr[i]] < rhs_split[k] && col_ptr[row_ptr[i+1]-1] > rhs_split[k]) {
                lower_col_ptr[k] = std::max(col_ptr[row_ptr[i]],lower_col_ptr[k]);
                upper_col_ptr[k] = std::max(col_ptr[row_ptr[i+1]-1],upper_col_ptr[k]);
                //then we can't use this many threads, reduce threads and begin again
                if(!transition_zone.empty() &&( (transition_zone.back() == i)||check_transition_overlap(lower_col_ptr,upper_col_ptr,n_t_zones) ) ){
                    std::cout<<n_zones<<" threads not possible, Trying "<<n_zones-1<<std::endl;
                    flag = true;
                    transition_zone.clear();
                    //recursive call
                    transition_zone = find_transition_zone_not_used(mat,n_zones-1);
                    break;
                }
                else{
                    transition_zone.push_back(i);
                }
            }
            if(flag == true)
                break;
                
        }
    }

    delete[] lower_col_ptr;
    delete[] upper_col_ptr;
    return transition_zone;
    }
    else
        WARNING_LOG("RCM method for Kaczmarz cannot be done using multiple threads, Either matrix is too small or Bandwidth is large");
}
   
   
bool check_transition_overlap(int* lower_col_ptr, int* upper_col_ptr, int n_t_zones) {
    bool flag = false;
    for(int k=1; k!=n_t_zones; ++k){
        if( upper_col_ptr[k-1] != -1 && lower_col_ptr[k] != -1 && upper_col_ptr[k-1] > lower_col_ptr[k]) {
            flag = true;
            break;
        }
    }
return flag;
}
*/


/*
//this function  compresses transition zones, into [begin1; end1; begin2; end2]
std::vector<int> compress_vec(std::vector<int> transition_zone){

std::vector<int> compressed_transition;

    for(int i=0; i!= transition_zone.size(); ++i) {
        if(i==0 || i==transition_zone.size()-1){
            compressed_transition.push_back(transition_zone[i]);
        }
        else if(transition_zone[i-1] != transition_zone[i]-1){
            compressed_transition.push_back(transition_zone[i-1]);
            compressed_transition.push_back(transition_zone[i]);
        }
    }
return compressed_transition;
      
}

ghost_error adaptor_vec_ptr(int* ptr, std::vector<int> vec){
    ptr = new int [vec.size()];
    
    for(int i=0; i!=vec.size(); ++i){
        ptr[i] = vec[i];
    }
}

//not used
ghost_error ghost_rcm_dissect_not_used(ghost_sparsemat *mat, int n_zones){
    std::vector<int> transition_zone;
    std::vector<int> compressed_transition;
    int n_zones_out;
    transition_zone = find_transition_zone_not_used(mat, n_zones);
    std::cout<<"% of Transition_zone = "<< 100*(static_cast<double> (transition_zone.size())/SPM_NROWS(mat))<<std::endl;
    compressed_transition = compress_vec(transition_zone);
    std::cout<<"Transition_zones are :\n";
    for(int i=0; i!=compressed_transition.size(); ++i)
        std::cout<<compressed_transition[i]<<std::endl;
    
    n_zones_out = compressed_transition.size()/2 + 1 ;
    std::cout<<"No. of threads to be used = "<< n_zones_out<<std::endl;
}*/                             
