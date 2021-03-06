/**
 * @ingroup task @{
 * @file task.h
 * @brief Types and functions for the tasks.
 * @author Moritz Kreutzer <moritz.kreutzer@fau.de>
 */
#ifndef GHOST_TASK_H
#define GHOST_TASK_H

#include <pthread.h>
#include <semaphore.h>
#include <hwloc.h>
#include "error.h"

#define GHOST_TASK_LD_UNDEFINED -2 // initializer
//#define GHOST_TASK_LD_ANY -1 // execute task on any LD


typedef enum {
    /**
     * @brief The default task
     */
    GHOST_TASK_DEFAULT = 0,
    /**
     * @brief The task will be treated as high-priority
     */
    GHOST_TASK_PRIO_HIGH = 1,
    /**
     * @brief The task _must_ be executed in the given NUMA node
     */
    GHOST_TASK_LD_STRICT = 2,
    /**
     * @brief A child task must not use this task's resources.
     */
    GHOST_TASK_NOT_ALLOW_CHILD = 4, 
    GHOST_TASK_NOT_PIN = 8, 
    GHOST_TASK_ONLY_HYPERTHREADS = 16,
    GHOST_TASK_NO_HYPERTHREADS = 32
} 
ghost_task_flags;


/**
 * @brief Use all processing units in the given NUMA domain.
 */
#define GHOST_TASK_FILL_LD -1
/**
 * @brief Use all available processing units.
 */
#define GHOST_TASK_FILL_ALL -2

typedef enum {
    /**
     * @brief Task is invalid (e.g., not yet created). 
     */
    GHOST_TASK_INVALID,
    /**
     * @brief Task has been created. 
     */
    GHOST_TASK_CREATED,
    /**
     * @brief Task has been enqueued.
     */
    GHOST_TASK_ENQUEUED,
    /**
     * @brief Task is running.
     */
    GHOST_TASK_RUNNING,
    /**
     * @brief Task has finished.
     */
    GHOST_TASK_FINISHED
} ghost_task_state;

/**
 * @brief This structure represents a GHOST task.
 *
 * This data structure holds all necessary information for
 * a task. The members ghost_task#nThreads, ghost_task#LD, ghost_task#flags, 
 * ghost_task#func, ghost_task#arg, ghost_task#depends, and ghost_task#ndepends have to be set by
 * the user in ghost_task_init(). All other members are set by the library at
 * some point.
 */
typedef struct ghost_task {
    /**
     * @brief The number of threads the task should use. (user-defined)
     */
    int nThreads;
    /**
     * @brief The index of the queue in which the task should be present and
     * (preferrably) running. (user-defined)
     */
    int LD;
    /**
     * @brief Optional flags for the task. (user-defined)
     */
    int flags;
    /**
     * @brief The function to be executed by the task. (user-defined)
     */
    void *(*func)(void *);
    /**
     * @brief The arguments to the task's function. (user-defined)
     */
    void *arg;
    /**
     * @brief A list of tasks which have to be finished before this task can start. (user-defined)
     */
    struct ghost_task ** depends;
    /**
     * @brief The number of dependencies. (user-defined)
     */
    int ndepends;
    /**
     * @brief A semaphore for the *user* to mark progress in this thread. (set by the library)
     */
    sem_t *progressSem;

    // set by the library
    /**
     * @brief The current state of the task. (set by the library)
     */
    ghost_task_state state;
    /**
     * @brief The list of cores where the task's threads are running. (set by the library)
     */
//    int *cores;
    /**
     * @brief Map of cores this task is using. (set by the library)
     */
    hwloc_bitmap_t coremap;
    /**
     * @brief Map of cores a child of this task is using. (set by the library)
     */
    hwloc_bitmap_t childusedmap;
    /**
     * @brief The return value of the task's funtion. (set by the library)
     */
    void *ret;
    /**
     * @brief Pointer to the next task in the queue. (set by the library)
     */
    struct ghost_task *next; 
    /**
     * @brief Pointer to the previous task in the queue. (set by the library)
     */
    struct ghost_task *prev;
    /**
     * @brief The enqueueing task if the task has been enqueued from within a task.
     * (set by the library, can be overwritten by the user *before* enqueueing)
     */
    struct ghost_task *parent;
    /**
     * @brief Indicator that the task is finished. (set by the library)
     */
    pthread_cond_t *finishedCond;
    /**
     * @brief Protect accesses to the task's members. (set by the library)
     */
    pthread_mutex_t *mutex;
    pthread_mutex_t *finishedMutex;
    pthread_mutex_t *stateMutex;
    /**
     * @brief Set to one as soon as the task's resources have been free'd.
     * This can be the case when the task waits for a child-task to finish or
     * when the task itself is finished.
     * @deprecated
     */
    int freed;
} ghost_task;

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Create a task. 
     *
     * @param task Where to store the task
     * @param nThreads The number of threads which are reserved for the task
     * @param LD The index of the task queue this task should be added to
     * @param func The function the task should execute
     * @param arg The arguments to the task's function
     * @param flags The task's flags
     * @param depends List of ghost_task * on which this task depends
     * @param ndepends Length of the depends argument
     *
     * @return ::GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error ghost_task_create(ghost_task **task, int nThreads, int LD, void *(*func)(void *), void *arg, ghost_task_flags flags, ghost_task **depends, int ndepends);
    /**
     * @brief Enqueue a task.
     *
     * @param t The task.
     *
     * @return ::GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error ghost_task_enqueue(ghost_task *t);
    /**
     * @brief Wait for a task to finish
     *
     * @param t The task to wait for
     *
     * @return ::GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error ghost_task_wait(ghost_task *t);
    /**
     * @brief Test the task's current state
     *
     * @param t The task to test
     *
     * @return  The state of the task
     */
    ghost_task_state ghost_taskest(ghost_task *t);
    /**
     * @brief Destroy a task.
     *
     * @param[inout] t The task to be destroyed
     */
    void ghost_task_destroy(ghost_task *t); 
    /**
     * @brief Unpin a task's threads.
     *
     * @param[inout] task The task.
     *
     * @return ::GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error ghost_task_unpin(ghost_task *task);
    /**
     * @ingroup stringification
     *
     * @brief Return a string representing the task's state
     *
     * @param[in] state The task state
     *
     * @return The state string
     */
    const char *ghost_task_state_string(ghost_task_state state);
    /**
     * @ingroup stringification
     *
     * @brief Stringify a task
     *
     * @param[out] str Where to store the string.
     * @param[in] t The task
     *
     * @return ::GHOST_SUCCESS on success or an error indicator.
     */
    ghost_error ghost_task_string(char **str, ghost_task *t); 

    ghost_error ghost_task_cur(ghost_task **task);

    /**
     * @brief Check whether tasking is globally enabled.
     *
     * @return false if the GHOST_TASK environment variable is set to "disable", true otherwise.
     */
    bool ghost_tasking_enabled();


#ifdef __cplusplus
}// extern "C"
#endif

#endif

/** @} */
