/**
 * @file log.h
 * @brief Macros for logging.
 * @author Moritz Kreutzer <moritz.kreutzer@fau.de>
 */
#ifndef GHOST_LOG_H
#define GHOST_LOG_H

#include "config.h"

#ifdef GHOST_HAVE_MPI
#include <mpi.h>
#endif

#ifdef __cplusplus
#include <cstdio>
#else
#include <stdio.h>
#endif

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define IF_DEBUG(level) if(GHOST_VERBOSITY > level)
#define FILE_BASENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/* taken from http://stackoverflow.com/a/11172679 */
/* expands to the first argument */
#define FIRST(...) FIRST_HELPER(__VA_ARGS__, throwaway)
#define FIRST_HELPER(first, ...) first

/*
 * if there's only one argument, expands to nothing.  if there is more
 * than one argument, expands to a comma followed by everything but
 * the first argument.  only supports up to 9 arguments but can be
 * trivially expanded.
 */
#define REST(...) REST_HELPER(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_HELPER(qty, ...) REST_HELPER2(qty, __VA_ARGS__)
#define REST_HELPER2(qty, ...) REST_HELPER_##qty(__VA_ARGS__)
#define REST_HELPER_ONE(first)
#define REST_HELPER_TWOORMORE(first, ...) , __VA_ARGS__
#define NUM(...) \
    SELECT_10TH(__VA_ARGS__, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,\
            TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, ONE, throwaway)
#define SELECT_10TH(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) a10

#ifdef GHOST_HAVE_MPI

#define LOG(type,color,...) {\
    int me;\
    int err = MPI_Comm_rank(MPI_COMM_WORLD,&me);\
    if (err != MPI_SUCCESS) {\
        me=-1;\
    }\
    fprintf(stderr, color "PE%d " #type " at %s() <%s:%d>: " FIRST(__VA_ARGS__) ANSI_COLOR_RESET "\n", me, __func__, FILE_BASENAME, __LINE__ REST(__VA_ARGS__)); \
    fflush(stderr);\
}\

#else

#define LOG(type,color,...) {\
    fprintf(stderr, color #type " at %s() <%s:%d>: " FIRST(__VA_ARGS__) ANSI_COLOR_RESET "\n", __func__, FILE_BASENAME, __LINE__ REST(__VA_ARGS__));\
}\

#endif


#define DEBUG_LOG(level,...) {if(GHOST_VERBOSITY > level) { LOG(DEBUG,ANSI_COLOR_RESET,__VA_ARGS__) }}
#define INFO_LOG(...) {static int __printed = 0; if(!__printed && GHOST_VERBOSITY) { LOG(INFO,ANSI_COLOR_BLUE,__VA_ARGS__); __printed=1; }}
#define WARNING_LOG(...) {static int __printed = 0; if(!__printed && GHOST_VERBOSITY) { WARNING_LOG_ALWAYS(__VA_ARGS__); __printed=1; }}
#define ERROR_LOG(...) {if(GHOST_VERBOSITY) { LOG(ERROR,ANSI_COLOR_RED,__VA_ARGS__) }}

#define WARNING_LOG_ALWAYS(...) {if(GHOST_VERBOSITY) { LOG(WARNING,ANSI_COLOR_YELLOW,__VA_ARGS__) }}

#endif
