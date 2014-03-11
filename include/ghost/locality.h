/**
 * @file locality.h
 * @brief Types and functions for gathering locality information.
 * @author Moritz Kreutzer <moritz.kreutzer@fau.de>
 */
#ifndef GHOST_LOCALITY_H
#define GHOST_LOCALITY_H

#include "error.h"
#include <hwloc.h>

#ifdef GHOST_HAVE_MPI
#include <mpi.h>
#endif

typedef struct {
    int ncore;
    int nsmt;
} ghost_hwconfig_t;

typedef enum {
    GHOST_HYBRIDMODE_INVALID, 
    GHOST_HYBRIDMODE_ONEPERNODE, 
    GHOST_HYBRIDMODE_ONEPERNUMA, 
    GHOST_HYBRIDMODE_ONEPERCORE,
    GHOST_HYBRIDMODE_CUSTOM
} ghost_hybridmode_t;

#define GHOST_HWCONFIG_INVALID -1
#define GHOST_HWCONFIG_INITIALIZER (ghost_hwconfig_t) {.ncore = GHOST_HWCONFIG_INVALID, .nsmt = GHOST_HWCONFIG_INVALID}

#ifdef __cplusplus
extern "C" {
#endif

    ghost_error_t ghost_hybridmode_set(ghost_hybridmode_t hm);
    ghost_error_t ghost_hybridmode_get(ghost_hybridmode_t *hm);
    ghost_error_t ghost_hwconfig_set(ghost_hwconfig_t);
    ghost_error_t ghost_hwconfig_get(ghost_hwconfig_t * hwconfig);
    ghost_error_t ghost_rank(int *rank, ghost_mpi_comm_t comm);
    ghost_error_t ghost_nnode(int *nnode, ghost_mpi_comm_t comm);
    ghost_error_t ghost_nrank(int *nrank, ghost_mpi_comm_t comm);
    ghost_error_t ghost_cpu(int *core);
    ghost_error_t ghost_thread_pin(int core);
    ghost_error_t ghost_thread_unpin();
    ghost_error_t ghost_nodecomm_get(ghost_mpi_comm_t *comm);
    ghost_error_t ghost_nodecomm_setup(ghost_mpi_comm_t comm);

#ifdef __cplusplus
}
#endif

#endif