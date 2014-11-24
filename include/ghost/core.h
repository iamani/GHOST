/**
 * @file core.h
 * @brief Functions for GHOST's core functionality. 
 * @author Moritz Kreutzer <moritz.kreutzer@fau.de>
 */
#ifndef GHOST_CORE_H
#define GHOST_CORE_H

#include "error.h"

/**
 * @ingroup core
 *
 * @brief Available GHOST types.
 */
typedef enum {
    /**
     * @brief Type is not set
     */
    GHOST_TYPE_INVALID, 
    /**
     * @brief This GHOST instance does actual work
     */
    GHOST_TYPE_WORK, 
    /**
     * @brief This GHOST instance talks to a CUDA GPU.
     */
    GHOST_TYPE_CUDA
} ghost_type_t;

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @ingroup core
     *
     * @brief Initialize GHOST. 
     *
     * @param argc argc as it has been passed to main() (needed for MPI_Init()).
     * @param argv argv as it has been passed to main() (needed for MPI_Init()).
     *
     * @return GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error_t ghost_init(int argc, char **argv);
    /**
     * @ingroup core
     *
     * @brief Check if GHOST is initialized. 
     *
     * @return True if GHOST is initialized, false otherwise.
     */
    int ghost_initialized();
    /**
     * @ingroup core
     *
     * @brief Finalize GHOST.
     *
     * @return GHOST_SUCCESS on success or an error indicator.
     *
     * Finalization includes:
     * - Wait for all outstanding tasks to finish with ghost_taskq_waitall()
     * - Destroy the Task queue with ghost_taskq_destroy()
     * - Destroy the thread pool with ghost_thpool_destroy()
     * - Destroy the PU map with ghost_pumap_destroy()
     * - Destroy the machine topology
     * - Call MPI_Finalize() if MPI_Init() has been called by GHOST
     * - Destroy the random number states
     * - Destroy the MPI operations and data types
     */
    ghost_error_t ghost_finalize();
    ghost_error_t ghost_type_set(ghost_type_t t);
    ghost_error_t ghost_type_get(ghost_type_t *t);
    char * ghost_type_string(ghost_type_t t);
    
    /**
     * @ingroup stringification
     *
     * @brief Construct string holding information on GHOST.
     *
     * @param str Where to store the string.
     *
     * @return GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error_t ghost_string(char **str);

#ifdef __cplusplus
}
#endif

#endif
