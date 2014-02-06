#ifndef GHOST_ERROR_H
#define GHOST_ERROR_H

#include "log.h"

typedef enum {
    GHOST_SUCCESS,
    GHOST_ERR_INVALID_ARG,
    GHOST_ERR_MPI,
    GHOST_ERR_CUDA,
    GHOST_ERR_HWLOC,
    GHOST_ERR_UNKNOWN,
    GHOST_ERR_NOT_IMPLEMENTED,
    GHOST_ERR_IO
} ghost_error_t;


#ifdef __cplusplus
extern "C" {
#endif

    char * ghost_errorString(ghost_error_t e);

#ifdef __cplusplus
}
#endif
/**
 * @brief This macro should be used for calling a GHOST function inside
 * a function which itself returns a ghost_error_t.
 * It calls the function and in case of an error LOGS the error message
 * and returns the according ghost_error_t which was return by the called function.
 *
 * @param call The complete function call.
 *
 * @return A ghost_error_t in case the function return an error.
 */
#define GHOST_CALL_RETURN(call) {\
    ghost_error_t err;\
    GHOST_CALL(call,err);\
    if (err != GHOST_SUCCESS) {\
        return err;\
    }\
}\

#define GHOST_CALL_GOTO(call,label,__err) {\
    GHOST_CALL(call,__err);\
    if (__err != GHOST_SUCCESS) {\
        goto label;\
    }\
}\

#define GHOST_CALL(call,__err) {\
    __err = call;\
    if (__err != GHOST_SUCCESS) {\
        LOG(GHOST_ERROR,ANSI_COLOR_RED,"%s",ghost_errorString((ghost_error_t)__err));\
    }\
}\

#define MPI_CALL_RETURN(call) {\
    int err = call;\
    if (err != MPI_SUCCESS) {\
        char errstr[MPI_MAX_ERROR_STRING];\
        int strlen;\
        MPI_Error_string(err,errstr,&strlen);\
        ERROR_LOG("MPI Error in : %s",errstr);\
        return GHOST_ERR_MPI;\
    }\
}\

#define MPI_CALL_GOTO(call,label,__err) {\
    int err = call;\
    if (err != MPI_SUCCESS) {\
        char errstr[MPI_MAX_ERROR_STRING];\
        int strlen;\
        MPI_Error_string(__err,errstr,&strlen);\
        ERROR_LOG("MPI Error in : %s",errstr);\
        __err = GHOST_ERR_MPI;\
        goto label;\
    }\
}\

#define MPI_CALL(call,__err) {\
    __err = call;\
    if (__err != MPI_SUCCESS) {\
        char errstr[MPI_MAX_ERROR_STRING];\
        int strlen;\
        MPI_Error_string(__err,errstr,&strlen);\
        ERROR_LOG("MPI Error in : %s",errstr);\
    }\
}\

#endif
