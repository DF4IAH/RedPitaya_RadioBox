/*
 * @brief CallBack functions of the HTTP GET/POST parameter transfer system
 *
 * @author Ulrich Habel (DF4IAH) <espero7757@gmx.net>
 *
 *  Created on: 09.10.2015
 *      Author: espero
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "main.h"
#include "worker.h"
#include "calib.h"
#include "fpga.h"

#include "cb_http.h"


/** @brief Calibration data layout within the EEPROM device. */
extern rp_calib_params_t	rp_main_calib_params;

/** @brief Describes app. parameters with some info/limitations */
extern rp_app_params_t		rp_default_params[];

/** @brief CallBack copy of params to inform the worker */
extern rp_app_params_t*		rp_cb_in_params;
/** @brief Holds mutex to access on parameters from outside to the worker thread */
extern pthread_mutex_t 		rp_cb_in_params_mutex;

/** @brief CallBack copy of params to inform the worker */
extern rp_app_params_t*		rp_cb_out_params;
/** @brief Holds mutex to access on parameters from the worker thread to any other context */
extern pthread_mutex_t 		rp_cb_out_params_mutex;

/** @brief params initialized */
extern int 					params_init_done;


/*----------------------------------------------------------------------------*/
int rp_app_init(void)
{
    fprintf(stderr, "Loading radiobox version %s-%s.\n", VERSION, REVISION);

    fpga_init();

    rp_default_calib_params(&rp_main_calib_params);
    if (rp_read_calib_params(&rp_main_calib_params) < 0) {
        fprintf(stderr, "rp_read_calib_params() failed, using default parameters\n");
    }

    /* start-up worker thread */
    if (worker_init(rp_default_params, RB_PARAMS_NUM) < 0) {
        fprintf(stderr, "ERROR rp_app_init - failed to start worker_init.\n");
        return -1;
    }

    /* Init with a CallBack fake */
    rp_set_params(rp_default_params, RB_PARAMS_NUM);

    /* since here the initialization is complete */
    params_init_done = 1;

    fprintf(stderr, "rp_app_init: END\n");
    return 0;
}

/*----------------------------------------------------------------------------*/
int rp_app_exit(void)
{
    fprintf(stderr, "rp_app_exit: BEGIN\n");
    fprintf(stderr, "Unloading radiobox version %s-%s.\n", VERSION, REVISION);

    // Debugging
    fprintf(stderr, "rp_app_exit: setting pattern HK LEDs\n");
    fpga_hk_setLeds(0, 0xff, 0xaa);

    fprintf(stderr, "rp_app_exit: calling fpga_exit()\n");
    fpga_exit();

    fprintf(stderr, "rp_app_exit: calling worker_exit()\n");
    /* shut-down worker thread */
    worker_exit();

    rp_free_params(&rp_cb_in_params);  // in case the pipe is not cleared

    fprintf(stderr, "rp_app_exit: END.\n");
    return 0;
}


/*----------------------------------------------------------------------------*/
int rp_set_params(const rp_app_params_t* p, int len)
{
    //int fpga_update = 0;

    fprintf(stderr, "rp_set_params: BEGIN\n");
    TRACE("%s()\n", __FUNCTION__);

    if (!p) {
        fprintf(stderr, "ERROR rp_set_params - non-valid parameter\n");
    	return -1;
    }

    if (!len) {  // short-cut
    	return 0;
    }

    /* create a local copy to release CallBack caller */
    pthread_mutex_lock(&rp_cb_in_params_mutex);
    rp_copy_params(&rp_cb_in_params, p, len, !params_init_done);  // piping to the worker thread
    pthread_mutex_unlock(&rp_cb_in_params_mutex);

    fprintf(stderr, "rp_set_params: END\n");
    return 0;
}

/*----------------------------------------------------------------------------*/
int rp_get_params(rp_app_params_t** p)
{
	int count = 0;

	fprintf(stderr, "rp_get_params: BEGIN\n");
	*p = NULL;  // TODO is input parameter filled? Is freeing needed here instead of dropping any input?

    pthread_mutex_lock(&rp_cb_out_params_mutex);
   	rp_cb_out_params = NULL;  // discard old data to receive a current copy from the worker - free'd externally
    pthread_mutex_unlock(&rp_cb_out_params_mutex);

    while (!(*p)) {
    	sleep(10000);  // delay the busy loop

    	pthread_mutex_lock(&rp_cb_out_params_mutex);
        *p = rp_cb_out_params;
        pthread_mutex_unlock(&rp_cb_out_params_mutex);
    }

    int i = 0;
    while ((*p)[i++].name) {
    	count++;
    }

    return count;
}

/*----------------------------------------------------------------------------*/
int rp_get_signals(float*** s, int* trc_num, int* trc_len)
{
    int ret_val = 0;

    fprintf(stderr, "rp_get_signals: BEGIN\n");

    if (!*s) {
        return -1;
    }

    *trc_num = TRACE_NUM;
    *trc_len = TRACE_LENGTH;

    fprintf(stderr, "rp_get_signals: END\n");
    return ret_val;
}
