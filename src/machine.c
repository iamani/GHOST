#include "ghost/machine.h"
#include "ghost/log.h"
#include "ghost/util.h"
#include "ghost/locality.h"
#include "ghost/omp.h"

#include <strings.h>
#ifdef GHOST_HAVE_OPENMP
#include <omp.h>
#endif

/** \cond */
extern char ** environ;
/** \endcond */

static hwloc_topology_t ghost_topology = NULL;

static char *env(char *key)
{
    int i=0;
    while (environ[i]) {
        if (!strncasecmp(key,environ[i],strlen(key)))
        {
            return environ[i]+strlen(key)+1;
        }
        i++;
    }
    return "undefined";

}


ghost_error_t ghost_topology_create()
{
    if (ghost_topology) {
        return GHOST_SUCCESS;
    }

    if (hwloc_topology_init(&ghost_topology)) {
        ERROR_LOG("Could not init topology");
        return GHOST_ERR_HWLOC;
    }

    if (hwloc_topology_load(ghost_topology)) {
        ERROR_LOG("Could not load topology");
        return GHOST_ERR_HWLOC;
    }

    return GHOST_SUCCESS;
}

void ghost_topology_destroy()
{
    hwloc_topology_destroy(ghost_topology);

    return;
}

ghost_error_t ghost_topology_get(hwloc_topology_t *topo)
{
    if (!topo) {
        ERROR_LOG("NULL pointer");
        return GHOST_ERR_INVALID_ARG;
    }

    if (!ghost_topology) {
        ERROR_LOG("Topology does not exist!");
        return GHOST_ERR_UNKNOWN;
    }
    
    *topo = ghost_topology;

    return GHOST_SUCCESS;

}

ghost_error_t ghost_machine_outerCacheSize(uint64_t *size)
{
    hwloc_topology_t topology;
    GHOST_CALL_RETURN(ghost_topology_get(&topology));
    hwloc_obj_t obj;
    int depth;

    for (depth=0; depth<(int)hwloc_topology_get_depth(topology); depth++) {
        obj = hwloc_get_obj_by_depth(topology,depth,0);
        if (obj->type == HWLOC_OBJ_CACHE) {
            *size = obj->attr->cache.size;
            break;
        }
    }

#if GHOST_HAVE_MIC
    size = size*ghost_getNumberOfPhysicalCores(); // the cache is shared but not reported so
#endif

    return GHOST_SUCCESS;
}

ghost_error_t ghost_machine_cacheLineSize(unsigned *size)
{
    hwloc_topology_t topology;
    GHOST_CALL_RETURN(ghost_topology_get(&topology));

    
    hwloc_obj_t obj;
    int depth;

    for (depth=0; depth<(int)hwloc_topology_get_depth(topology); depth++) {
        obj = hwloc_get_obj_by_depth(topology,depth,0);
        if (obj->type == HWLOC_OBJ_CACHE) {
            *size = obj->attr->cache.linesize;
        }
    }

    return GHOST_SUCCESS;
}

ghost_error_t ghost_machine_nCores(int *nCores, int numaNode)
{
    int nPUs;
    int smt;
    GHOST_CALL_RETURN(ghost_machine_nPus(&nPUs, numaNode));
    GHOST_CALL_RETURN(ghost_machine_nSmt(&smt));

    if (smt == 0) {
        ERROR_LOG("The SMT level is zero");
        return GHOST_ERR_UNKNOWN;
    }

    if (nPUs % smt) {
        ERROR_LOG("The number of PUs is not a multiple of the SMT level");
        return GHOST_ERR_UNKNOWN;
    }

    *nCores = nPUs/smt;

    return GHOST_SUCCESS;
}

ghost_error_t ghost_machine_nPus(int *nPUs, int numaNode)
{
    hwloc_topology_t topology;
    GHOST_CALL_RETURN(ghost_topology_get(&topology));
    hwloc_const_cpuset_t cpuset = NULL;
    if (numaNode == GHOST_NUMANODE_ANY) {
        cpuset = hwloc_topology_get_allowed_cpuset(topology);
    } else {
        hwloc_obj_t node;
        ghost_machine_numaNode(&node,numaNode);
        cpuset = node->cpuset;
    }

    *nPUs = hwloc_bitmap_weight(cpuset);

    return GHOST_SUCCESS;
}

ghost_error_t ghost_machine_nSmt(int *nLevels)
{
    hwloc_topology_t topology;
    GHOST_CALL_RETURN(ghost_topology_get(&topology));

    if (!topology) {
        ERROR_LOG("The topology is NULL");
        return GHOST_ERR_UNKNOWN;
    }

    hwloc_obj_t firstCore = hwloc_get_obj_by_type(topology,HWLOC_OBJ_CORE,0);
    *nLevels = (int)firstCore->arity;
    
    return GHOST_SUCCESS;
}

ghost_error_t ghost_machine_nNumaNodes(int *nNodes)
{
    hwloc_topology_t topology;
    GHOST_CALL_RETURN(ghost_topology_get(&topology));
    int nnodes = hwloc_get_nbobjs_by_type(topology,HWLOC_OBJ_NODE);
    if (nnodes < 0) {
        ERROR_LOG("Could not obtain number of NUMA nodes");
        return GHOST_ERR_HWLOC;
    } else if (nnodes == 0) {
        *nNodes = 1;
    } else {
        *nNodes = nnodes;
    }

    return GHOST_SUCCESS;

}

char ghost_machine_bigEndian()
{
    int test = 1;
    unsigned char *endiantest = (unsigned char *)&test;

    return (endiantest[0] == 0);
}

ghost_error_t ghost_machine_numaNode(hwloc_obj_t *node, int idx)
{
    if (!node) {
        ERROR_LOG("NULL pointer");
        return GHOST_ERR_INVALID_ARG;
    }

    hwloc_topology_t topology;
    GHOST_CALL_RETURN(ghost_topology_get(&topology));
    
    int nNodes = hwloc_get_nbobjs_by_type(topology,HWLOC_OBJ_NODE);
    
    if (idx < 0 || idx >= nNodes) {
        ERROR_LOG("Index out of range");
        return GHOST_ERR_INVALID_ARG;
    }

    if (nNodes == 0) {    
        *node = hwloc_get_obj_by_type(topology,HWLOC_OBJ_SOCKET,0);
    } else {
        *node = hwloc_get_obj_by_type(topology,HWLOC_OBJ_NODE,idx);
    }

    return GHOST_SUCCESS;
}

ghost_error_t ghost_machine_string(char **str)
{

    UNUSED(&env);

    int nranks;
    int nnodes;

    GHOST_CALL_RETURN(ghost_malloc((void **)str,1));
    memset(*str,'\0',1);
    GHOST_CALL_RETURN(ghost_getNumberOfRanks(MPI_COMM_WORLD,&nranks));
    GHOST_CALL_RETURN(ghost_getNumberOfNodes(MPI_COMM_WORLD,&nnodes));


#ifdef GHOST_HAVE_CUDA
    int cuVersion;
    GHOST_CALL_RETURN(ghost_cu_getVersion(&cuVersion));

    ghost_gpu_info_t * CUdevInfo;
    GHOST_CALL_RETURN(ghost_cu_getDeviceInfo(&CUdevInfo));
#endif


    int nphyscores;
    int ncores;
    ghost_machine_nCores(&nphyscores,GHOST_NUMANODE_ANY);
    ghost_machine_nPus(&ncores,GHOST_NUMANODE_ANY);

#ifdef GHOST_HAVE_OPENMP
    char omp_sched_str[32];
    omp_sched_t omp_sched;
    int omp_sched_mod;
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
#else
    char omp_sched_str[] = "N/A";
#endif

    uint64_t cacheSize;
    unsigned int cacheLineSize;
    ghost_machine_outerCacheSize(&cacheSize);
    ghost_machine_cacheLineSize(&cacheLineSize);

    ghost_printHeader(str,"Machine");

    ghost_printLine(str,"Overall nodes",NULL,"%d",nnodes); 
    ghost_printLine(str,"Overall MPI processes",NULL,"%d",nranks);
    ghost_printLine(str,"MPI processes per node",NULL,"%d",nranks/nnodes);
    ghost_printLine(str,"Avail. cores/PUs per node",NULL,"%d/%d",nphyscores,ncores);
    ghost_printLine(str,"OpenMP scheduling",NULL,"%s",omp_sched_str);
    ghost_printLine(str,"LLC size","MiB","%.2f",cacheSize*1.0/(1024.*1024.));
    ghost_printLine(str,"Cache line size","B","%zu",cacheLineSize);
#ifdef GHOST_HAVE_CUDA
    ghost_printLine(str,"CUDA version",NULL,"%d",cuVersion);
    ghost_printLine(str,"CUDA devices",NULL,NULL);
    int j;
    for (j=0; j<CUdevInfo->nDistinctDevices; j++) {
        if (strcasecmp(CUdevInfo->names[j],"None")) {
            ghost_printLine(str,"",NULL,"%dx %s",CUdevInfo->nDevices[j],CUdevInfo->names[j]);
        }
    }
#endif
    ghost_printFooter(str);

    return GHOST_SUCCESS;

}
