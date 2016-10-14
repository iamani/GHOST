/**
 * @file autogen.h
 * @brief Utility functions for automatic code generation.
 * @author Moritz Kreutzer <moritz.kreutzer@fau.de>
 */
#ifndef GHOST_AUTOGEN_H
#define GHOST_AUTOGEN_H

#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

ghost_error ghost_autogen_kacz_nvecs(int **nvecs, int *n, int chunkheight, int nshifts);
ghost_error ghost_autogen_spmmv_nvecs(int **nvecs, int *n, int chunkheight);
int ghost_autogen_spmmv_next_nvecs(int desired_nvecs, int chunkheight);

#ifdef __cplusplus
}
#endif

#endif
