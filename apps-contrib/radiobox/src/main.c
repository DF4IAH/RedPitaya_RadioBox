/**
 * @brief Red Pitaya RadioBox main module.
 *
 * @author Ulrich Habel (DF4IAH) <espero7757@gmx.net>
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
//#include <math.h>
#include <errno.h>
#include <pthread.h>
//#include <sys/types.h>
#include <sys/mman.h>

#include "version.h"
#include "worker.h"
#include "fpga.h"
#include "cb_http.h"
#include "cb_ws.h"

#include "main.h"


#ifndef VERSION
# define VERSION "(not set)"
#endif
#ifndef REVISION
# define REVISION "(not set)"
#endif


//extern pthread_mutex_t rp_main_params_mutex;
//extern rp_app_params_t rp_main_params[];
//extern rp_calib_params_t rp_main_calib_params;
//extern int params_init;


/*----------------------------------------------------------------------------------*/
/**
 * @brief Returns description cstring for this RadioBox sub-module
 *
 * This function returns a null terminated cstring.
 *
 * @retval      cstring    Description of this RadioBox sub-module
 */
const char* rp_app_desc(void)
{
    return (const char *)"RedPitaya RadioBox application.\n";
}

/*----------------------------------------------------------------------------------*/
/**
 * @brief Prepares buffers for signal traces (not used yet)
 *
 * This function allocates memory for TRACE_NUM traces in the memory and prepares
 * for its usage.
 *
 * @param[inout] a_traces  Pointer to traces buffer
 *
 * @retval       0         Success
 * @retval       -1        Failed to establish the traces buffer
 */
int rp_create_traces(float** a_traces[TRACE_NUM])
{
    int i;
    float** trc;

    fprintf(stderr, "rp_create_traces: BEGIN\n");

    // Debugging
    fpga_hk_setLeds(1, 0x20, 0);

    trc = malloc(TRACE_NUM * sizeof(float*));
    if (!trc) {
        return -1;
    }

    // First step - void all pointers in case of early return
    for (i = 0; i < TRACE_NUM; i++)
    	trc[i] = NULL;

    // Set-up for each trace
    for (i = 0; i < TRACE_NUM; i++) {
    	trc[i] = (float*) malloc(TRACE_LENGTH * sizeof(float));
        if (!(trc[i])) {
            // de-allocate all memory as much as already taken
            rp_free_traces(a_traces);
            return -2;
        }
        // wipe-out all random data
        memset(trc[i], 0, TRACE_LENGTH * sizeof(float));
    }

    // Success - assign to in/out parameter
    *a_traces = trc;
    fprintf(stderr, "rp_create_traces: END\n\n");

    return 0;
}

/*----------------------------------------------------------------------------------*/
/**
 * @brief Frees memory used by the traces buffer (not used yet)
 *
 * This function frees memory for TRACE_NUM traces in the memory.
 *
 * @param[inout] a_traces  Pointer to traces buffer
 */
void rp_free_traces(float** a_traces[TRACE_NUM])
{
    fprintf(stderr, "rp_free_traces: BEGIN\n");

    if (!a_traces) {
        return;
    }

    float** trc = *a_traces;

    // Debugging
    fpga_hk_setLeds(1, 0x40, 0);

    if (trc) {
        int i;
        for (i = 0; i < TRACE_NUM; i++) {
            if (trc[i]) {
                free(trc[i]);
                trc[i] = NULL;
            }
        }
        free(trc);
    }
    *a_traces = NULL;

    fprintf(stderr, "rp_free_traces: END\n\n");
}

/*----------------------------------------------------------------------------------*/
/**
 * @brief Make a copy of Application parameters
 *
 * Function copies actual Application parameters to the specified destination
 * buffer. This action was intended to prepare two parameter instances, where the first
 * one can be further modified from the user side, while the second one is processed by
 * the worker thread.
 * In case the destination buffer is not allocated yet, it is allocated internally and must
 * be freed outside of the function scope by calling rp_clean_params() function. Note that
 * if function returns failure, the destination buffer could be partially allocated and must
 * be freed in the same way.
 * If the specified destination buffer is already allocated, it is assumed the number of table
 * entries is the same as in the source table. No special check is made internally if this is really
 * the case.
 *
 * @param[out]  dst  Destination application parameters
 * @param[in]   src  Source application parameters
 * @retval      0    Successful operation
 * @retval      -1   Failure, error message is output on standard error
 */
int rp_copy_params(rp_app_params_t* dst[], rp_app_params_t src[])
{
    int i, num_params;

    /* check arguments */
    if (!dst || !(*dst)) {
        fprintf(stderr, "Internal error, the destination Application parameters variable is not set.\n");
        return -1;
    }
    if (!src) {
        fprintf(stderr, "Internal error, the source Application parameters are not specified.\n");
        return -2;
    }

    /* check if destination buffer is allocated or not */
    rp_app_params_t* p_new = *dst;

	/* destination buffer is already allocated */
	i = 0;
	while (src[i].name) {
		if (!strcmp(p_new[i].name, src[i].name)) {
			// direct mapping found - just copy each value
			p_new[i].value = src[i].value;
		} else {
			// due to differences in the structure, discard the old destination list and recreate it
	        fprintf(stderr, "RadioBox: main.c - rp_copy_params() - non direct params mapping found, recreating params list.\n");
			rp_free_params(dst);
			p_new = NULL;
			break;
		}
		i++;
	}

    if (!p_new) {  // recreate parameter list
        /* retrieve the number of source parameters */
        i = 0;
        num_params = 0;
        while (src[i++].name) {
            num_params++;
        }

        /* allocate array of parameter entries, parameter names must be allocated separately */
        p_new = (rp_app_params_t*) malloc(sizeof(rp_app_params_t) * (num_params + 1));
        if (!p_new) {
            fprintf(stderr, "Memory problem, the destination buffer could not be allocated (1).\n");
            return -3;
        }

        /* scan source parameters, allocate memory space for parameter names and copy values */
        i = 0;
        while (src[i].name) {
            int slen = strlen(src[i].name);
            p_new[i].name = (char*) malloc(slen + 1);
            if (!(p_new[i].name)) {
                fprintf(stderr, "Memory problem, the destination buffer could not be allocated (2).\n");
                return -4;
            }

            strncpy(p_new[i].name, src[i].name, slen);
            p_new[i].name[slen] = '\0';
            p_new[i].value = src[i].value;
            i++;
        }

        /* mark last one as final entry */
        p_new[num_params].name = NULL;
        p_new[num_params].value = -1;
    }
    *dst = p_new;

    return 0;
}


/*----------------------------------------------------------------------------------*/
/**
 * @brief Deallocate the specified buffer of Application parameters
 *
 * Function is used to deallocate the specified buffers, which were previously
 * allocated by calling rp_copy_params() function.
 *
 * @param[in]   params  Application parameters to be deallocated
 * @retval      0       Success
 * @retval      -1      Failed with non-valid params
 */
int rp_free_params(rp_app_params_t* params[])
{
    if (!params) {
        return -1;
    }

    /* free params structure */
    if (*params) {
        int i = 0;
        while (params[i]->name != NULL) {
            free(params[i]->name);
            params[i]->name = NULL;
            i++;
        }
        free(*params);
        *params = NULL;
    }

    return 0;
}
