/**
 * @ingroup task @{
 * @file taskq.h
 * @brief Types and functions for the task queue.
 * @author Moritz Kreutzer <moritz.kreutzer@fau.de>
 */
#ifndef GHOST_TASKQ_H
#define GHOST_TASKQ_H

#include <pthread.h>
#include <hwloc.h>
#include "error.h"
#include "task.h"

/**
 * @brief The task queue.
 */
typedef struct {
    /**
     * @brief The first (= highest priority) task in the queue
     */
    ghost_task *head;
    /**
     * @brief The last (= lowest priority) task in the queue
     */
    ghost_task *tail;
    /**
     * @brief Serialize access to the queue
     */
    pthread_mutex_t mutex;
} ghost_taskq;

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Initializes a task queues.
 * @return GHOST_SUCCESS on success or GHOST_FAILURE on failure.
 */
ghost_error ghost_taskq_create();
ghost_error ghost_taskq_destroy();

ghost_error ghost_taskq_waitall();
ghost_error ghost_taskq_waitsome(ghost_task **, int, int*);
ghost_error ghost_taskq_add(ghost_task *task);
ghost_error ghost_taskq_startroutine(void *(**func)(void *));

extern ghost_taskq *taskq;

#ifdef __cplusplus
}// extern "C"
#endif

#endif

/** @} */
