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
#include <string.h>
#include <pthread.h>

#include "main.h"
#include "worker.h"
#include "calib.h"
#include "fpga.h"

#include "cb_http.h"


/* @brief The HouseKeeping memory file descriptor used to mmap() the FPGA space. */
int                             g_fpga_hk_mem_fd = -1;

/* @brief The HouseKeeping memory layout of the FPGA registers. */
fpga_hk_reg_mem_t*              g_fpga_hk_reg_mem = NULL;


/* @brief The RadioBox memory file descriptor used to mmap() the FPGA space. */
int                             g_fpga_rb_mem_fd = -1;

/* @brief The RadioBox memory layout of the FPGA registers. */
fpga_rb_reg_mem_t*              g_fpga_rb_reg_mem = NULL;


static rp_calib_params_t        rp_main_calib_params;


/* Describe app. parameters with some info/limitations */
static rp_app_params_t rp_main_params[RB_PARAMS_NUM + 1] = {
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

pthread_mutex_t rp_main_params_mutex = PTHREAD_MUTEX_INITIALIZER;

/* params initialized - accessed by the nginx loader */
static int params_init = 0;

extern int worker_params_dirty;


int rp_app_init(void)
{
    fprintf(stderr, "Loading radiobox version %s-%s.\n", VERSION, REVISION);

    fpga_init();

    // Debugging
    fpga_hk_setLeds(0, 0xff, 0x01);

    rp_default_calib_params(&rp_main_calib_params);
    if (rp_read_calib_params(&rp_main_calib_params) < 0) {
        fprintf(stderr, "rp_read_calib_params() failed, using default parameters\n");
    }

    if (worker_init(&rp_main_params[0], RB_PARAMS_NUM, &rp_main_calib_params) < 0) {
        fprintf(stderr, "rp_app_init: failed to start rp_osc_worker_init.\n");
        return -1;
    }

    rp_set_params(&rp_main_params[0], RB_PARAMS_NUM, 0);

    fprintf(stderr, "rp_app_init: END.\n");
    return 0;
}

int rp_app_exit(void)
{
    fprintf(stderr, "Unloading radiobox version %s-%s.\n", VERSION, REVISION);

    worker_exit();

    // Debugging
    fpga_hk_setLeds(0, 0xff, 0x00);

    fpga_exit();

    fprintf(stderr, "rp_app_exit: END.\n");
    return 0;
}


int rp_set_params(rp_app_params_t* p, int len, int requesterIsServer)
{
    int fpga_update = 0;

    fprintf(stderr, "rp_set_params: BEGIN\n");
    TRACE("%s()\n", __FUNCTION__);

    // Debugging
    fpga_hk_setLeds(1, 0x02, 0);

    if (len > RB_PARAMS_NUM) {
        fprintf(stderr, "Too many parameters: len=%d (max:%d)\n", len, RB_PARAMS_NUM);
        return -1;
    }

    pthread_mutex_lock(&rp_main_params_mutex);
    int i;
    for (i = 0; i < len || p[i].name; i++) {
        int p_idx = -1;
        int j = 0;

        /* Search for correct parameter name in defined parameters */
        fprintf(stderr, "rp_set_params: next param name = %s\n", p[i].name);
        while (rp_main_params[j].name) {
            int p_strlen = strlen(p[i].name);

            if (p_strlen != strlen(rp_main_params[j].name)) {
                j++;
                continue;
            }
            if (!strncmp(p[i].name, rp_main_params[j].name, p_strlen)) {
                p_idx = j;
                break;
            }
            j++;
        }

        if (p_idx == -1) {
            fprintf(stderr, "Parameter %s not found, ignoring it\n", p[i].name);
            continue;
        }

        if (rp_main_params[p_idx].read_only)
            continue;

        if (rp_main_params[p_idx].value != p[i].value) {
            if (rp_main_params[p_idx].fpga_update) {
                rp_main_params[p_idx].fpga_update |= 0x80;	// indicate this entry to be updated in the FPGA
                fpga_update = 1;
            }
        }

        if (rp_main_params[p_idx].min_val > p[i].value) {
            fprintf(stderr, "Incorrect parameters value: %f (min:%f), "
                    " correcting it\n", p[i].value, rp_main_params[p_idx].min_val);
            p[i].value = rp_main_params[p_idx].min_val;

        } else if (rp_main_params[p_idx].max_val < p[i].value) {
            fprintf(stderr, "Incorrect parameters value: %f (max:%f), "
                    " correcting it\n", p[i].value, rp_main_params[p_idx].max_val);
            p[i].value = rp_main_params[p_idx].max_val;
        }
        rp_main_params[p_idx].value = p[i].value;
        fprintf(stderr, "rp_set_params: param name = %s, value = %lf\n", p[i].name, p[i].value);
    }

    if (fpga_update) {
        worker_params_dirty = 1;  // indicate that some data attributes were changed
    }
    pthread_mutex_unlock(&rp_main_params_mutex);

    fprintf(stderr, "rp_set_params: END\n");
    return 0;
}

/* Returned vector must be free'd externally! */
int rp_get_params(rp_app_params_t** p)
{
    fprintf(stderr, "rp_get_params: BEGIN\n");

    // Debugging
    fpga_hk_setLeds(1, 0x08, 0);

    rp_app_params_t* p_copy = NULL;
    p_copy = (rp_app_params_t*) malloc((RB_PARAMS_NUM + 1) * sizeof(rp_app_params_t));
    if (!p_copy) {
        return -1;
    }

    pthread_mutex_lock(&rp_main_params_mutex);
    int i;
    for (i = 0; i < RB_PARAMS_NUM; i++) {
        int p_strlen = strlen(rp_main_params[i].name);

        p_copy[i].name = (char*) malloc(p_strlen+1);
        strncpy((char*) &p_copy[i].name[0], &rp_main_params[i].name[0], p_strlen);
        p_copy[i].name[p_strlen]='\0';

        p_copy[i].value       = rp_main_params[i].value;
        p_copy[i].fpga_update = rp_main_params[i].fpga_update;
        p_copy[i].read_only   = rp_main_params[i].read_only;
        p_copy[i].min_val     = rp_main_params[i].min_val;
        p_copy[i].max_val     = rp_main_params[i].max_val;

        fprintf(stderr, "rp_get_params: param name = %s, value = %lf\n", p_copy[i].name, p_copy[i].value);
    }
    pthread_mutex_unlock(&rp_main_params_mutex);
    p_copy[RB_PARAMS_NUM].name = NULL;
    *p = p_copy;

    fprintf(stderr, "rp_get_params: END\n");
    return RB_PARAMS_NUM;
}

int rp_get_signals(float*** s, int* trc_num, int* trc_len)
{
    int ret_val = 0;

    fprintf(stderr, "rp_get_signals: BEGIN\n");

    // Debugging
    fpga_hk_setLeds(1, 0x10, 0);

    if (!*s) {
        return -1;
    }

    *trc_num = TRACE_NUM;
    *trc_len = TRACE_LENGTH;

    fprintf(stderr, "rp_get_signals: END\n");
    return ret_val;
}


int rp_update_main_params(rp_app_params_t* params)
{
    int i = 0;

    fprintf(stderr, "rp_update_main_params: BEGIN\n");

    if (!params) {
        return -1;
    }

    /* make own copy */
    pthread_mutex_lock(&rp_main_params_mutex);
    while (params[i].name) {
        rp_main_params[i].value = params[i].value;
        i++;
    }
    pthread_mutex_unlock(&rp_main_params_mutex);

    /* inform rp_set_params(), also */
    params_init = 0;
    rp_set_params(&rp_main_params[0], RB_PARAMS_NUM, 1);

    fprintf(stderr, "rp_update_main_params: END\n");
    return 0;
}
