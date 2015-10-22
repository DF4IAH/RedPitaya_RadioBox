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


/** @brief HouseKeeping memory file descriptor used to mmap() the FPGA space */
int                             g_fpga_hk_mem_fd = -1;

/** @brief calibration data layout within the EEPROM device */
rp_calib_params_t        		rp_main_calib_params;

/** @brief HouseKeeping memory layout of the FPGA registers */
fpga_hk_reg_mem_t*              g_fpga_hk_reg_mem = NULL;

/** @brief RadioBox memory file descriptor used to mmap() the FPGA space */
int                             g_fpga_rb_mem_fd = -1;

/** @brief RadioBox memory layout of the FPGA registers */
fpga_rb_reg_mem_t*              g_fpga_rb_reg_mem = NULL;

/** @brief Describes app. parameters with some info/limitations */
const rp_app_params_t rp_default_params[RB_PARAMS_NUM + 1] = {
    { /* Running mode */
	    "RB_RUN",           0, 1, 0, 0,         1 },
    { /* Oscillator-1 frequency (Hz) */
        "osc1_qrg_i",       0, 1, 0, 0, 125000000 },
    { /* Oscillator-1 amplitude (µV) */
        "osc1_amp_i",       0, 1, 0, 0,   2048000 },
    { /* Oscillator-1 modulation source selector (0: none, 1: VCO2, 2: XADC0) */
        "osc1_modsrc_s",    0, 1, 0, 0,         2 },
    { /* Oscillator-1 modulation type selector (0: AM, 1: FM, 2: PM) */
        "osc1_modtyp_s",    0, 1, 0, 0,         2 },
    { /* Oscillator-2 frequency (Hz) */
        "osc2_qrg_i",       0, 1, 0, 0, 125000000 },
    { /* Oscillator-2 magnitude (AM:%, FM:Hz, PM:°) */
        "osc2_mag_i",       0, 1, 0, 0,   1000000 },
    { /* Must be last! */
        NULL,               0.0, -1, -1, 0.0, 0.0 }
};

/** @brief CallBack copy of params to inform the worker */
rp_app_params_t*				rp_cb_in_params = NULL;
/** @brief Holds mutex to access on parameters from outside to the worker thread */
pthread_mutex_t 				rp_cb_in_params_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief CallBack copy of params from the worker when requested */
rp_app_params_t*				rp_cb_out_params = NULL;
/** @brief Holds mutex to access on parameters from the worker thread to any other context */
pthread_mutex_t 				rp_cb_out_params_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief params initialized */
int								params_init_done = 0;  /* @see worker.c */


/*----------------------------------------------------------------------------------*/
const char* rp_app_desc(void)
{
    return (const char *)"RedPitaya RadioBox application by DF4IAH and DD8UU.\n";
}


/*----------------------------------------------------------------------------------*/
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
int rp_copy_params(rp_app_params_t** dst, const rp_app_params_t src[], int len, int do_copy_all_attr)
{
	const rp_app_params_t* s = src;
    int i, j, num_params;

    /* check arguments */
    if (!dst) {
        fprintf(stderr, "ERROR rp_copy_params - Internal error, the destination Application parameters variable is not set.\n");
        return -1;
    }
    if (!s) {
        fprintf(stderr, "INFO rp_copy_params - no source parameter list given, taking default parameters instead.\n");
        s = rp_default_params;
    }

    /* check if destination buffer is allocated already */
    rp_app_params_t* p_new = *dst;
    if (p_new) {
		/* destination buffer exists */
		i = 0;
		while (s[i].name) {
			/* process each parameter entry of the list */

			if (!strcmp(p_new[i].name, s[i].name)) {  // direct mapping found - just copy the value
				p_new[i].value			= s[i].value;
				p_new[i].fpga_update	= s[i].fpga_update;  // copy FPGA update marker in case it is present

				if (do_copy_all_attr) {  // if default parameters are taken, use all attributes
					p_new[i].min_val	= s[i].min_val;
					p_new[i].max_val	= s[i].max_val;
					p_new[i].read_only	= s[i].read_only;
				}
			} else {
				j = 0;
				while (p_new[j].name) {  // scanning the complete list
					if (j == i) {  // do a short-cut here
						continue;
					}

					if (!strcmp(p_new[j].name, s[i].name)) {
						p_new[j].value			= s[i].value;
						p_new[i].fpga_update	= s[i].fpga_update;  // copy FPGA update marker in case it is present

						if (!do_copy_all_attr) {  // if default parameters are taken, use all attributes
							p_new[i].min_val	= s[i].min_val;
							p_new[i].max_val	= s[i].max_val;
							p_new[i].read_only	= s[i].read_only;
						}
						break;
					}
					j++;
				}  // while (p_new[j].name)
			}  // if () else
			i++;
		}  // while (src[i].name)

    } else {
		/* destination buffer has to be allocated, create a new parameter list */

    	if (len >= 0) {
    		num_params = len;

    	} else {
			/* retrieve the number of source parameters */
			i = 0;
			num_params = 0;
			while (s[i++].name) {
				num_params++;
			}
    	}

        /* allocate array of parameter entries, parameter names must be allocated separately */
        p_new = (rp_app_params_t*) malloc(sizeof(rp_app_params_t) * (num_params + 1));
        if (!p_new) {
            fprintf(stderr, "ERROR rp_copy_params - memory problem, the destination buffer could not be allocated (1).\n");
            return -3;
        }
        /* prepare a copy for built-in attributes. Strings have to be handled on their own way */
        memcpy(p_new, s, (num_params + 1) * sizeof(rp_app_params_t));

        /* allocate memory and copy character strings for params names */
        i = 0;
        while (s[i].name) {
            int slen = strlen(s[i].name);
            p_new[i].name = (char*) malloc(slen + 1);  // old pointer to name does not belong to us and has to be discarded
            if (!(p_new[i].name)) {
                fprintf(stderr, "ERROR rp_copy_params - memory problem, the destination buffer could not be allocated (2).\n");
                return -4;
            }
            strncpy(p_new[i].name, s[i].name, slen);
            p_new[i].name[slen] = '\0';

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
int rp_free_params(rp_app_params_t** params)
{
    if (!params) {
        return -1;
    }

    fprintf(stderr, "rp_free_params: BEGIN\n");

    /* free params structure */
    if (*params) {
        rp_app_params_t* p = *params;

        int i = 0;
        while (p[i].name) {
            fprintf(stderr, "rp_free_params: freeing name=%s\n", p[i].name);
            free(p[i].name);
            fprintf(stderr, "rp_free_params: after freeing\n");
            fprintf(stderr, "rp_free_params: before NULLing\n");
            p[i].name = NULL;
            fprintf(stderr, "rp_free_params: after NULLing\n");
            i++;
        }

        free(p);
        *params = NULL;
    }

    fprintf(stderr, "rp_free_params: END\n");
    return 0;
}
