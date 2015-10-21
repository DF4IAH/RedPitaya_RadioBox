/**
 * @brief Red Pitaya Oscilloscope worker.
 *
 * @author Ulrich Habel (DF4IAH) <espero7757@gmx.net>
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


static pthread_t*               worker_thread_handler = NULL;

static pthread_mutex_t          worker_ctrl_mutex = PTHREAD_MUTEX_INITIALIZER;
static worker_state_t           worker_ctrl_state;

static rp_app_params_t*         worker_params = NULL;
static int                      worker_params_dirty;
static int                      worker_params_fpga_update;

static pthread_mutex_t          worker_traces_mutex = PTHREAD_MUTEX_INITIALIZER;
static float**                  worker_traces;
static int                      worker_traces_dirty = 0;
static int                      worker_traces_lastIdx = 0;
static float**                  worker_traces_tmp;  /* used for calculation, only from worker */


/* Calibration parameters read from EEPROM */
static rp_calib_params_t*       worker_calib_params = NULL;



/*----------------------------------------------------------------------------------*/
int worker_init(rp_app_params_t* params, int params_len, rp_calib_params_t* calib_params)
{
    int ret_val;

    fprintf(stderr, "worker_init: BEGIN\n");

    // make sure all previous data is vanished
    if (worker_thread_handler || worker_calib_params) {
        (void) worker_exit();
    }

    worker_ctrl_state         = worker_idle_state;
    worker_params_dirty       = 0;
    worker_params_fpga_update = 0;

    // set reference to the global EEPROM parameter set
    worker_calib_params = calib_params;

    // fill in parameters to the worker context
    rp_copy_params((rp_app_params_t**) &worker_params, params);

    /*
    osc_fpga_get_sig_ptr(&rp_fpga_cha_signal, &rp_fpga_chb_signal);
    */

    worker_thread_handler = (pthread_t*) malloc(sizeof(pthread_t));
    if (!worker_thread_handler) {
        worker_exit();
        return -1;
    }

    ret_val = pthread_create(worker_thread_handler, NULL, worker_thread, NULL);
    if (ret_val) {
        fprintf(stderr, "pthread_create() failed: %s\n", strerror(errno));
        free(worker_thread_handler);
        worker_thread_handler = NULL;
        worker_exit();
        return -1;
    }

    fprintf(stderr, "worker_init: END\n");
    return 0;
}

/*----------------------------------------------------------------------------------*/
int worker_exit(void)
{
    int ret_val = 0; 

    fprintf(stderr, "worker_exit: BEGIN\n");

    pthread_mutex_lock(&worker_ctrl_mutex);
    worker_ctrl_state = worker_quit_state;
    pthread_mutex_unlock(&worker_ctrl_mutex);

    if (worker_thread_handler) {
        ret_val = pthread_join(*worker_thread_handler, NULL);
        free(worker_thread_handler);
        worker_thread_handler = NULL;
    }
    if (ret_val) {
        fprintf(stderr, "pthread_join() failed: %s\n", strerror(errno));
    }

    rp_free_traces(&worker_traces);
    rp_free_traces(&worker_traces_tmp);

    rp_free_params(&worker_params);
    worker_calib_params = NULL;

    fprintf(stderr, "worker_exit: END\n");
    return 0;
}


/*----------------------------------------------------------------------------------*/
void* worker_thread(void* args)
{
    worker_state_t        old_state, state;
    rp_app_params_t*      curr_params   = NULL;
    int                   fpga_update   = 0;

    fprintf(stderr, "worker_thread: BEGIN\n");

    pthread_mutex_lock(&worker_ctrl_mutex);
    state = worker_ctrl_state;
    worker_params_dirty = 1;
    pthread_mutex_unlock(&worker_ctrl_mutex);

    while (1) {
        /* update states - we save also the old state to see
         * if we need to reset the FPGA
         */
        old_state = state;
        (void) old_state;

        pthread_mutex_lock(&worker_ctrl_mutex);
        state = worker_ctrl_state;
        if (worker_params_dirty) {
            rp_copy_params((rp_app_params_t**) &curr_params, worker_params);
            fpga_update = worker_params_fpga_update;
            worker_params_dirty = 0;
        }
        pthread_mutex_unlock(&worker_ctrl_mutex);

        /* request to stop worker thread, we will shut down */
        if (state == worker_quit_state) {
            rp_free_params(&curr_params);
            break;

        } else if (state == worker_abort_state) {
            continue;

        } else if (state == worker_idle_state) {
            usleep(10000);  // request for a 10 ms delay
            continue;

        } else {
            /* state == worker_normal_state */

            if (fpga_update) {
                if (fpga_rb_update_all_params(&curr_params)) {
                    fprintf(stderr, "worker - RadioBox: setting of FPGA registers failed\n");
                }

                /* data acknowledged */
                pthread_mutex_lock(&worker_ctrl_mutex);
                worker_params_fpga_update = 0;
                worker_ctrl_state = 0;
                pthread_mutex_unlock(&worker_ctrl_mutex);
            }
        }
    }  // while (1)

    fprintf(stderr, "worker_thread: END\n");
    return 0;
}


/*----------------------------------------------------------------------------------*/
int worker_update_params(rp_app_params_t* params, int fpga_update)
{
    fprintf(stderr, "worker_update_params: BEGIN\n");

    /*
    pthread_mutex_lock(&worker_ctrl_mutex);
    rp_copy_params(params, (rp_app_params_t **)&worker_params);
    worker_params_dirty       = 1;
    worker_params_fpga_update = fpga_update;
    worker_params[PARAMS_NUM].name = NULL;
    worker_params[PARAMS_NUM].value = -1;

    pthread_mutex_unlock(&worker_ctrl_mutex);
    */

    fprintf(stderr, "worker_update_params: END\n");
    return 0;
}


/*----------------------------------------------------------------------------------*/
int worker_get_signals(float*** traces, int* trc_idx)
{
    float** trc = *traces;

    fprintf(stderr, "worker_get_signals: BEGIN\n");

    pthread_mutex_lock(&worker_traces_mutex);
    *trc_idx = worker_traces_lastIdx;
    if (!worker_traces_dirty) {
        pthread_mutex_unlock(&worker_traces_mutex);
        return -1;
    }
    memcpy(&trc[0][0], &worker_traces[0][0], sizeof(float) * TRACE_LENGTH);
    memcpy(&trc[1][0], &worker_traces[1][0], sizeof(float) * TRACE_LENGTH);
    memcpy(&trc[2][0], &worker_traces[2][0], sizeof(float) * TRACE_LENGTH);
    worker_traces_dirty = 0;
    pthread_mutex_unlock(&worker_traces_mutex);

    fprintf(stderr, "worker_get_signals: END\n");
    return 0;
}

/*----------------------------------------------------------------------------------*/
int worker_set_signals(float** source, int index)
{
    fprintf(stderr, "worker_set_signals: BEGIN\n");

    pthread_mutex_lock(&worker_traces_mutex);
    memcpy(&worker_traces[0][0], &source[0][0], sizeof(float) * TRACE_LENGTH);
    memcpy(&worker_traces[1][0], &source[1][0], sizeof(float) * TRACE_LENGTH);
    memcpy(&worker_traces[2][0], &source[2][0], sizeof(float) * TRACE_LENGTH);
    worker_traces_lastIdx = index;
    worker_traces_dirty = 1;
    pthread_mutex_unlock(&worker_traces_mutex);

    fprintf(stderr, "worker_set_signals: END\n");
    return 0;
}
