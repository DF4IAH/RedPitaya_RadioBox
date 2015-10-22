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


/** @brief Thread handler for the worker */
static pthread_t*               worker_thread_handler = NULL;

/** @brief Thread parameter processing and run state */
static worker_state_t           worker_ctrl_state;
/** @brief Mutex for work_ctrl_state */
static pthread_mutex_t          worker_ctrl_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief Parameter list for the worker thread */
static rp_app_params_t*         worker_params = NULL;

/** @brief CallBack copy of params to inform the worker */
extern rp_app_params_t*			rp_cb_in_params;
/** @brief Holds mutex to access on parameters from outside to the worker thread */
extern pthread_mutex_t 			rp_cb_in_params_mutex;

/** @brief CallBack copy of params from the worker when requested */
extern rp_app_params_t*			rp_cb_out_params;
/** @brief Holds mutex to access on parameters from the worker thread to any other context */
extern pthread_mutex_t 			rp_cb_out_params_mutex;

static pthread_mutex_t          worker_traces_mutex = PTHREAD_MUTEX_INITIALIZER;
static float**                  worker_traces;
static int                      worker_traces_dirty = 0;
static int                      worker_traces_lastIdx = 0;
static float**                  worker_traces_tmp;  /* used for calculation, only from worker */


/** @brief Sets-up a running worker thread
 *
 * @args[in]     params        The initial parameter list the worker thread will take a copy from.
 * @args[in]     len           Count of parameters that params holds.
 * @retval       int           The return value of the pthread_create() call.
 */
int worker_init(rp_app_params_t* params, int params_len)
{
	int ret_val;

    fprintf(stderr, "worker_init: BEGIN\n");

    // make sure all previous data is vanished
    if (worker_thread_handler) {
        (void) worker_exit();
    }

    worker_ctrl_state         = worker_idle_state;

    /* create a new parameter list to the worker context */
    rp_copy_params((rp_app_params_t**) &worker_params, params, params_len, 1);

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

/** @brief Shuts-down the running worker thread
 *
 * @retval       int   The return value of the pthread_join() call.
 */
int worker_exit(void)
{
	int ret_val = 0;

    fprintf(stderr, "worker_exit: BEGIN\n");

    fprintf(stderr, "worker_exit: before signaling quit\n");
    pthread_mutex_lock(&worker_ctrl_mutex);
    worker_ctrl_state = worker_quit_state;
    pthread_mutex_unlock(&worker_ctrl_mutex);
    fprintf(stderr, "worker_exit: after signaling quit\n");

    if (worker_thread_handler) {
        fprintf(stderr, "worker_exit: before joining\n");
        ret_val = pthread_join(*worker_thread_handler, NULL);
        fprintf(stderr, "worker_exit: after joining\n");
        fprintf(stderr, "worker_exit: before freeing thread handler\n");
        free(worker_thread_handler);
        fprintf(stderr, "worker_exit: after freeing thread handler\n");
        fprintf(stderr, "worker_exit: before setting worker_thread_handler=NULL\n");
        worker_thread_handler = NULL;
        fprintf(stderr, "worker_exit: after setting worker_thread_handler=NULL\n");
    }
    if (ret_val) {
        fprintf(stderr, "pthread_join() failed: %s\n", strerror(errno));
    }

    fprintf(stderr, "worker_exit: before freeing traces\n");
    rp_free_traces(&worker_traces);
    rp_free_traces(&worker_traces_tmp);
    fprintf(stderr, "worker_exit: after freeing traces\n");

    fprintf(stderr, "worker_exit: before freeing worker_params\n");
    rp_free_params(&worker_params);
    fprintf(stderr, "worker_exit: after freeing worker_params\n");

    fprintf(stderr, "worker_exit: END\n");
    return 0;
}


/** @brief The worker thread that runs the state-machine for parameter handling
 *
 * All data transfered between this thread and outer context has to be handled
 * strictly by mutex access.
 *
 * @args[in]     args  @see pthread_create for details. Not used in this context.
 * @retval       void* @see pthread_create for details. Not used in this context.
 */
void* worker_thread(void* args)
{
    worker_state_t        state;
//  worker_state_t        old_state;

    fprintf(stderr, "worker_thread: BEGIN\n");

    pthread_mutex_lock(&worker_ctrl_mutex);
    worker_ctrl_state = state = worker_idle_state;
    pthread_mutex_unlock(&worker_ctrl_mutex);

    while (1) {
        rp_app_params_t* curr_params = NULL;
    	int do_normal_state = 0;

    	/* update states - we save also the old state to see
         * if we need to reset the FPGA
         */
//    	old_state = state;

        pthread_mutex_lock(&worker_ctrl_mutex);
        state = worker_ctrl_state;
        pthread_mutex_unlock(&worker_ctrl_mutex);

        pthread_mutex_lock(&rp_cb_in_params_mutex);
        /* check if new parameters are available */
        if (rp_cb_in_params) {
            fprintf(stderr, "worker_thread: worker_params_dirty flag is set, check for new updated parameters\n");
        	rp_copy_params((rp_app_params_t**) &curr_params, rp_cb_in_params, -1, 0);
        	rp_free_params(&rp_cb_in_params);

            /* take FSM out of idle state */
        	do_normal_state = 1;
        }
        pthread_mutex_unlock(&rp_cb_in_params_mutex);

        pthread_mutex_lock(&rp_cb_out_params_mutex);
        if (!rp_cb_out_params) {  // the outer context removed the list, a current new one has to be created here
            /* take FSM out of idle state */
        	do_normal_state = 1;
        }
        pthread_mutex_unlock(&rp_cb_out_params_mutex);

        if (do_normal_state) {
            pthread_mutex_lock(&worker_ctrl_mutex);
            worker_ctrl_state = state = worker_normal_state;
            pthread_mutex_unlock(&worker_ctrl_mutex);

            do_normal_state = 0;
        }


        /* request to stop worker thread, we will shut down */
        if (state == worker_quit_state) {
            fprintf(stderr, "worker_thread: worker_quit_state received\n");
            fprintf(stderr, "worker_thread: before freeing curr_params\n");
            rp_free_params(&curr_params);
            fprintf(stderr, "worker_thread: after freeing curr_params\n");
            break;

        } else if (state == worker_abort_state) {  // TODO is worker_abort_state needed anymore?
            fprintf(stderr, "WARNING worker_thread - FSM in worker_abort_state\n");
            usleep(1000000);  // TODO remove me!
            continue;

        } else if (state == worker_idle_state) {
            usleep(10000);  // request for a 10 ms delay
            continue;

        } else if (state == worker_normal_state) {
        	int fpga_update_count = mark_changed_fpga_update_entries(worker_params, curr_params);  // return count of modified FPGA update values
        	if (fpga_update_count > 0) {
                fprintf(stderr, "worker_thread: fpga_update:count > 0  -->  delegate to fpga_rb_update_all_params()\n");
                if (fpga_rb_update_all_params(curr_params)) {
                    fprintf(stderr, "worker - RadioBox: setting of FPGA registers failed\n");
                }
            }

        	/* update worker_params */
        	rp_copy_params(&worker_params, curr_params, -1, 0);

            pthread_mutex_lock(&worker_ctrl_mutex);
            worker_ctrl_state = worker_idle_state;
            pthread_mutex_unlock(&worker_ctrl_mutex);

        } else {  // any unknown states are mapped to QUIT
            pthread_mutex_lock(&worker_ctrl_mutex);
            worker_ctrl_state = worker_quit_state;
            pthread_mutex_unlock(&worker_ctrl_mutex);
        }


        pthread_mutex_lock(&rp_cb_out_params_mutex);
        if (!rp_cb_out_params) {  // the outer context removed the list, a current new one has to be created here
        	rp_free_params(&rp_cb_out_params);
        	rp_copy_params(&rp_cb_out_params, worker_params, -1, 1);  // get all attributes
        }
        pthread_mutex_unlock(&rp_cb_out_params_mutex);

    }  // while (1)

    fprintf(stderr, "worker_thread: END\n");
    return 0;
}


/** @brief Marks all changed values for that entries which are having a fpga_update flag set
 *
 * This function marks all changed parameter entries which are having the fpga_update attribute set.
 * Additional the count of this modified parameter entries is returned.
 *
 * @param[in]    ref  Reference parameter list for old values taken as reference.
 * @param[inout] cmp  Comparison parameter list for new values to be compare against the reference.
 * @retval       int  Number of parameters that changed the value AND their attribute fpga_update is set.
 */
int mark_changed_fpga_update_entries(const rp_app_params_t* ref, rp_app_params_t* cmp)
{
	if (!ref || !cmp) {
		return -1;
	}

	int count = 0;
	int i = 0;
	while (cmp[i].name) {  // for each cmp parameter entry of the list do a check and mark
		int idx = -1;
		int j = 0;
		while (ref[j].name) {
			if (!strcmp(ref[j].name, cmp[i].name)) {  // known parameter
				idx = j;
				break;
			}
		}

		if (idx == -1) {  // ignore unknown parameter
			continue;
		}

		if (ref[idx].value != cmp[i].value) {
			if (ref[idx].fpga_update & ~0x80) {  // if fpga_update is set but the MARKER is masked out before the comparison
				cmp[i].fpga_update |= 0x80;  // add FPGA update MARKER
				count++;
			}
		}
	}
	return count;
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
