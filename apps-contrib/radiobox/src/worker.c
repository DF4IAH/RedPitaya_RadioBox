/**
 * $Id: worker.c 881 2013-12-16 05:37:34Z rp_jmenart $
 *
 * @brief Red Pitaya Oscilloscope worker.
 *
 * @Author Jure Menart <juremenart@gmail.com>
 *         
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>

#include "cb_http.h"
#include "fpga.h"

#include "worker.h"

pthread_t *worker_thread_handler = NULL;

pthread_mutex_t       worker_ctrl_mutex = PTHREAD_MUTEX_INITIALIZER;
worker_state_t        worker_ctrl_state;

rp_app_params_t*      rp_app_params = NULL;
int                   rp_app_params_dirty;
int                   rp_app_params_fpga_update;

pthread_mutex_t       rp_app_traces_mutex = PTHREAD_MUTEX_INITIALIZER;
float**               rp_app_traces;
int                   rp_app_traces_dirty = 0;
int                   rp_app_traces_lastIdx = 0;
float**               rp_app_traces_tmp; /* used for calculation, only from worker */


/* Calibration parameters read from EEPROM */
rp_calib_params_t*    rp_calib_params = NULL;



/*----------------------------------------------------------------------------------*/
int worker_init(rp_app_params_t* params, int params_len, rp_calib_params_t* calib_params)
{
    int ret_val;

    // make sure all previous data is vanished
    (void) worker_exit();

    worker_ctrl_state         = worker_idle_state;
    rp_app_params_dirty       = 0;
    rp_app_params_fpga_update = 0;

    // get access to the global EEPROM parameter set
    rp_calib_params = calib_params;

    // fill in parameters to the worker context
    rp_copy_params((rp_app_params_t **) &rp_app_params, params);

    if (fpga_init() < 0) {
    	worker_exit();
        return -1;
    }

    /*
    osc_fpga_get_sig_ptr(&rp_fpga_cha_signal, &rp_fpga_chb_signal);
    */

    worker_thread_handler = (pthread_t*) malloc(sizeof(pthread_t));
    if (worker_thread_handler == NULL) {
    	worker_exit();
        return -1;
    }

    ret_val = pthread_create(worker_thread_handler, NULL, worker_thread, NULL);
    if (ret_val != 0) {
        fprintf(stderr, "pthread_create() failed: %s\n", strerror(errno));
        free(worker_thread_handler);
        worker_thread_handler = NULL;
    	worker_exit();
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------------*/
int worker_exit(void)
{
    int ret_val = 0; 

    pthread_mutex_lock(&worker_ctrl_mutex);
    worker_ctrl_state = worker_quit_state;
    pthread_mutex_unlock(&worker_ctrl_mutex);

    if (worker_thread_handler) {
        ret_val = pthread_join(*worker_thread_handler, NULL);
        free(worker_thread_handler);
        worker_thread_handler = NULL;
    }
    if(ret_val != 0) {
        fprintf(stderr, "pthread_join() failed: %s\n", strerror(errno));
    }

    fpga_exit();

    rp_free_traces(&rp_app_traces);
    rp_free_traces(&rp_app_traces_tmp);

    rp_free_params(rp_app_params);
    rp_calib_params = NULL;

    return 0;
}

/*----------------------------------------------------------------------------------*/
void* worker_thread(void* args)
{
    worker_state_t        old_state, state;
    rp_app_params_t*      curr_params   = NULL;
    int                   fpga_update   = 0;

    pthread_mutex_lock(&worker_ctrl_mutex);
    state = worker_ctrl_state;
    rp_app_params_dirty = 1;
    pthread_mutex_unlock(&worker_ctrl_mutex);

    while (1) {
        /* update states - we save also the old state to see
         * if we need to reset the FPGA
         */
        old_state = state;
        (void) old_state;

        pthread_mutex_lock(&worker_ctrl_mutex);
        state = worker_ctrl_state;
        if (rp_app_params_dirty) {
            rp_copy_params((rp_app_params_t**) &curr_params, rp_app_params);
            fpga_update = rp_app_params_fpga_update;
            rp_app_params_dirty = 0;
        }
        pthread_mutex_unlock(&worker_ctrl_mutex);

        /* request to stop worker thread, we will shut down */
        if (state == worker_quit_state) {
            rp_free_params(curr_params);
            break;

        } else if (state == worker_abort_state) {
            continue;

        } else if (state == worker_idle_state) {
            usleep(10000);  // request for a 10 ms delay
            continue;

        } else {
        	/* state == worker_normal_state */

			if (fpga_update) {
				rb_fpga_reset();

				if (rb_fpga_update_params()) {
					fprintf(stderr, "worker - RadioBox: setting of FPGA registers failed\n");
				}

				/* data acknowledged */
		        pthread_mutex_lock(&worker_ctrl_mutex);
		        rp_app_params_fpga_update = 0;
		        worker_ctrl_state = 0;
		        pthread_mutex_unlock(&worker_ctrl_mutex);
			}
        }
    }  // while (1)

    return 0;
}


/*----------------------------------------------------------------------------------*/
int worker_update_params(rp_app_params_t* params, int fpga_update)
{
	/*
    pthread_mutex_lock(&worker_ctrl_mutex);
    rp_copy_params(params, (rp_app_params_t **)&worker_params);
    worker_params_dirty       = 1;
    worker_params_fpga_update = fpga_update;
    worker_params[PARAMS_NUM].name = NULL;
    worker_params[PARAMS_NUM].value = -1;

    pthread_mutex_unlock(&worker_ctrl_mutex);
    */

	return 0;
}


/*----------------------------------------------------------------------------------*/
int worker_clear_signals(void)
{
    pthread_mutex_lock(&rp_app_traces_mutex);
    rp_app_traces_dirty = 0;
    pthread_mutex_unlock(&rp_app_traces_mutex);
    return 0;
}

/*----------------------------------------------------------------------------------*/
int worker_get_signals(float*** traces, int* trc_idx)
{
    float** trc = *traces;

    pthread_mutex_lock(&rp_app_traces_mutex);
    *trc_idx = rp_app_traces_lastIdx;
    if (!rp_app_traces_dirty) {
        pthread_mutex_unlock(&rp_app_traces_mutex);
        return -1;
    }
    memcpy(&trc[0][0], &rp_app_traces[0][0], sizeof(float) * TRACE_LENGTH);
    memcpy(&trc[1][0], &rp_app_traces[1][0], sizeof(float) * TRACE_LENGTH);
    memcpy(&trc[2][0], &rp_app_traces[2][0], sizeof(float) * TRACE_LENGTH);
    rp_app_traces_dirty = 0;
    pthread_mutex_unlock(&rp_app_traces_mutex);
    return 0;
}

/*----------------------------------------------------------------------------------*/
int worker_set_signals(float** source, int index)
{
    pthread_mutex_lock(&rp_app_traces_mutex);
    memcpy(&rp_app_traces[0][0], &source[0][0], sizeof(float) * TRACE_LENGTH);
    memcpy(&rp_app_traces[1][0], &source[1][0], sizeof(float) * TRACE_LENGTH);
    memcpy(&rp_app_traces[2][0], &source[2][0], sizeof(float) * TRACE_LENGTH);
    rp_app_traces_lastIdx = index;
    rp_app_traces_dirty = 1;
    pthread_mutex_unlock(&rp_app_traces_mutex);

    return 0;
}
