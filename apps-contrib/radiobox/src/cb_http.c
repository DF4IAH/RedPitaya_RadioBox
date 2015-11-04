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
extern rp_calib_params_t    g_rp_main_calib_params;

/** @brief Describes app. parameters with some info/limitations */
extern rb_app_params_t      g_rb_default_params[];

/** @brief CallBack copy of params to inform the worker */
extern rp_app_params_t*     g_rp_cb_in_params;
/** @brief Holds mutex to access on parameters from outside to the worker thread */
extern pthread_mutex_t      g_rp_cb_in_params_mutex;

/** @brief CallBack copy of params to inform the worker */
extern rp_app_params_t*     g_rp_cb_out_params;
/** @brief Holds mutex to access on parameters from the worker thread to any other context */
extern pthread_mutex_t      g_rp_cb_out_params_mutex;

/** @brief params initialized */
extern int                  g_params_init_done;


/*----------------------------------------------------------------------------*/
int rp_app_init(void)
{
    fprintf(stderr, "\n<=== Loading RadioBox version %s-%s ===>\n\n", VERSION, REVISION);

    fpga_init();

    //fprintf(stderr, "INFO rp_app_init: sizeof(double)=%d, sizeof(float)=%d, sizeof(long long)=%d, sizeof(long)=%d, sizeof(int)=%d, sizeof(short)=%d\n",
    //        sizeof(double), sizeof(float), sizeof(long long), sizeof(long), sizeof(int), sizeof(short));

    /* Set check pattern @ HK LEDs */
    //fprintf(stderr, "rp_app_init: setting pattern HK LEDs\n");
    fpga_hk_setLeds(0, 0xff, 0xaa);

    rp_default_calib_params(&g_rp_main_calib_params);
    double default_osc125mhz = g_rp_main_calib_params.base_osc125mhz_realhz;
    //fprintf(stderr, "INFO rp_app_init: default_osc125mhz = %lf\n", default_osc125mhz);
    if (rp_read_calib_params(&g_rp_main_calib_params) < 0) {
        //fprintf(stderr, "rp_read_calib_params() failed, using default parameters\n");
    }
    if (!((uint32_t) g_rp_main_calib_params.base_osc125mhz_realhz)) {  // non-valid data
        //fprintf(stderr, "WARNING rp_app_init: non-valid osc125mhz data found, overwriting with default value\n");
        g_rp_main_calib_params.base_osc125mhz_realhz = default_osc125mhz;
    }
    //fprintf(stderr, "INFO rp_app_init: osc125mhz = %lf\n", rp_main_calib_params.base_osc125mhz_realhz);

    /* start-up worker thread */
    if (worker_init(g_rb_default_params, RB_PARAMS_NUM) < 0) {
        fprintf(stderr, "ERROR rp_app_init - failed to start worker_init.\n");
        return -1;
    }

    //fprintf(stderr, "rp_app_init: END\n");
    return 0;
}

/*----------------------------------------------------------------------------*/
int rp_app_exit(void)
{
    //fprintf(stderr, "rp_app_exit: BEGIN\n");
    fprintf(stderr, ">### Unloading radiobox version %s-%s. ###<\n", VERSION, REVISION);

    /* turn off all LEDs */
    fpga_hk_setLeds(0, 0xff, 0x00);

    //fprintf(stderr, "rp_app_exit: calling fpga_exit()\n");
    fpga_exit();

    //fprintf(stderr, "rp_app_exit: calling worker_exit()\n");
    /* shut-down worker thread */
    worker_exit();

    //fprintf(stderr, "rp_app_exit: END.\n");
    //fprintf(stderr, "RadioBox unloaded\n\n");
    return 0;
}


/*----------------------------------------------------------------------------*/
int rp_set_params(rp_app_params_t* p, int len)
{
    //int fpga_update = 0;

    fprintf(stderr, "!!! rp_set_params: BEGIN\n");
    //TRACE("%s()\n", __FUNCTION__);

    if (!p || (len < 0)) {
        fprintf(stderr, "ERROR rp_set_params - non-valid parameter\n");
        return -1;
    }

    if (!len) {                                                                                         // short-cut
        return 0;
    }

    /* create a local copy to release CallBack caller */
    pthread_mutex_lock(&g_rp_cb_in_params_mutex);
    fprintf(stderr, "INFO rp_set_params: g_rp_cb_in_params - freeing (1) ...\n");
    rp_free_params(&g_rp_cb_in_params);
    fprintf(stderr, "INFO rp_set_params: g_rp_cb_in_params - rp_copy_params(&g_rp_cb_in_params, p, ) ...\n");
    rp_copy_params(&g_rp_cb_in_params, p, len, !g_params_init_done);                                    // piping to the worker thread
    pthread_mutex_unlock(&g_rp_cb_in_params_mutex);

#if 0
    rp_free_params(&p);                                                                                 // do NOT free this object!
#endif

    fprintf(stderr, "!!! rp_set_params: END - pktIdx = %s = %lf\n", p[0].name, p[0].value);
    return 0;
}

/*----------------------------------------------------------------------------*/
int rp_get_params(rp_app_params_t** p)
{
    int count = 0;

    fprintf(stderr, "??? rp_get_params: BEGIN\n");
    if (!p) {
        fprintf(stderr, "ERROR rp_get_params: no valid p argument. Null pointer.\n");
        return -1;
    } else if (*p) {
        fprintf(stderr, "WARNING rp_get_params: no valid p argument. Destination already in use.\n");
        *p = NULL;
    }

    //fprintf(stderr, "INFO rp_get_params - before mutex out_param NULLing\n");
    pthread_mutex_lock(&g_rp_cb_out_params_mutex);
#if 0
    rp_free_params(&g_rp_cb_out_params);                                                                // do NOT free this object! - do overwrite instead!
#else
    g_rp_cb_out_params = NULL;
#endif
    pthread_mutex_unlock(&g_rp_cb_out_params_mutex);
    //fprintf(stderr, "INFO rp_get_params - after  mutex out_param NULLing\n");

    while (!(*p)) {
        usleep(100000);  // delay the busy loop

        //fprintf(stderr, "INFO rp_get_params - before mutex out_param getting\n");
        pthread_mutex_lock(&g_rp_cb_out_params_mutex);
        *p = g_rp_cb_out_params;
        pthread_mutex_unlock(&g_rp_cb_out_params_mutex);
        //fprintf(stderr, "INFO rp_get_params - after  mutex out_param getting\n");
    }

    int i = 0;
    while ((*p)[i++].name) {
        count++;
        fprintf(stderr, "?.. rp_get_params - out name = %s\n", (*p)[i - 1].name);
    }
    fprintf(stderr, "?.> rp_get_params - having list with count = %d\n", count);

    fprintf(stderr, "??? rp_get_params: END\n");
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
