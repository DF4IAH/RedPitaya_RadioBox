/*
 * cb_http.c
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

#include "cb_http.h"


/* Describe app. parameters with some info/limitations */
pthread_mutex_t rp_main_params_mutex = PTHREAD_MUTEX_INITIALIZER;
static rp_app_params_t rp_main_params[PARAMS_NUM+1] = {
    { /* Oscillator-1 frequency (Hz) */
        "osc1_qrg_i", 100, 1, 0, 0, 125000000 },
    { /* Oscillator-1 amplitude (µV) */
        "osc1_amp_i", 90, 1, 0, 0, 2048000 },
    { /* Oscillator-1 modulation source selector (0: none, 1: VCO2, 2: XADC0) */
        "osc1_modsrc_s", 1, 1, 0, 0, 2 },
    { /* Oscillator-1 modulation type selector (0: AM, 1: FM, 2: PM) */
        "osc1_modtyp_s", 2, 1, 0, 0, 2 },
    { /* Oscillator-2 frequency (Hz) */
        "osc2_qrg_i", 80, 1, 0, 0, 125000000 },
    { /* Oscillator-2 magnitude (AM:%, FM:Hz, PM:°) */
        "osc2_mag_i", 70, 1, 0, 0, 1000000 },

    { /* calculator register A */
        "rb_add_a_i", 51, 1, 0, 0, 1000 },
    { /* calculator register B */
        "rb_add_b_i", 52, 1, 0, 0, 1000 },
    { /* calculator sum result register */
        "rb_add_res_i", 53, 0, 1, 0, 10000 },
    { /* Must be last! */
        NULL, 0.0, -1, -1, 0.0, 0.0 }
};
/* params initialized */
static int params_init = 0;

rp_calib_params_t rp_main_calib_params;


int rp_app_init(void)
{
    fprintf(stderr, "Loading radiobox version %s-%s.\n", VERSION, REVISION);

    // Debugging
    set_leds(0, 0xff, 0x01);

    /*
    rp_default_calib_params(&rp_main_calib_params);
    if (rp_read_calib_params(&rp_main_calib_params) < 0) {
        fprintf(stderr, "rp_read_calib_params() failed, using default"
                " parameters\n");
    }
    */

    if (rp_osc_worker_init(&rp_main_params[0], PARAMS_NUM,
                           &rp_main_calib_params) < 0) {
        fprintf(stderr, "rp_app_init: failed to start rp_osc_worker_init.\n");
        return -1;
    }

    fprintf(stderr, "rp_app_init: before rp_set_params.\n");
    rp_set_params(&rp_main_params[0], PARAMS_NUM, 0);
    fprintf(stderr, "rp_app_init: after rp_set_params.\n");

    return 0;
}

int rp_app_exit(void)
{
    fprintf(stderr, "Unloading radiobox version %s-%s.\n", VERSION, REVISION);

    rp_osc_worker_exit();

    // Debugging
    set_leds(0, 0xff, 0x00);

    return 0;
}

int rp_set_params(rp_app_params_t *p, int len, int requesterIsServer)
{
    int i;
    int params_change = 0;
    int fpga_update = 1;

    fprintf(stderr, "rp_set_params: BEGIN\n");
    TRACE("%s()\n", __FUNCTION__);

    // Debugging
    set_leds(1, 0x02, 0);

    if (len > PARAMS_NUM) {
        fprintf(stderr, "Too many parameters: len=%d (max:%d)\n", len, PARAMS_NUM);
        return -1;
    }

    pthread_mutex_lock(&rp_main_params_mutex);
    for (i = 0; i < len || p[i].name != NULL; i++) {
        int p_idx = -1;
        int j = 0;

        /* Search for correct parameter name in defined parameters */
        fprintf(stderr, "rp_set_params: next param name = %s\n", p[i].name);
        while (rp_main_params[j].name != NULL) {

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
            params_change = 1;
            if (rp_main_params[p_idx].fpga_update) {
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
    pthread_mutex_unlock(&rp_main_params_mutex);

    // DF4IAH  v
    if (params_change || !params_init) {
        (void) fpga_update;

        // Debugging
        set_leds(1, 0x04, 0);

        fprintf(stderr, "rp_set_params: setting FPGA - A=%lf, B=%lf ...\n", rp_main_params[RB_ADD_A].value, rp_main_params[RB_ADD_B].value);
        rb_set_fpga(0x00, rp_main_params[RB_ADD_A].value);
        rb_set_fpga(0x04, rp_main_params[RB_ADD_B].value);
        rp_main_params[RB_ADD_RES].value = rb_get_fpga(0x08);
        fprintf(stderr, "rp_set_params: getting FPGA -  ... RES=%lf\n", rp_main_params[RB_ADD_RES].value);
    }
    // DF4IAH  ^

    fprintf(stderr, "rp_set_params: END\n\n");
    return 0;
}

/* Returned vector must be free'd externally! */
int rp_get_params(rp_app_params_t **p)
{
    fprintf(stderr, "rp_get_params: BEGIN\n");

    rp_app_params_t *p_copy = NULL;
    int i;

    // Debugging
    set_leds(1, 0x08, 0);

    p_copy = (rp_app_params_t *)malloc((PARAMS_NUM+1) * sizeof(rp_app_params_t));
    if (p_copy == NULL)
        return -1;

    pthread_mutex_lock(&rp_main_params_mutex);
    for (i = 0; i < PARAMS_NUM; i++) {
        int p_strlen = strlen(rp_main_params[i].name);

        p_copy[i].name = (char *)malloc(p_strlen+1);
        strncpy((char *)&p_copy[i].name[0], &rp_main_params[i].name[0],
                p_strlen);
        p_copy[i].name[p_strlen]='\0';

        p_copy[i].value       = rp_main_params[i].value;
        p_copy[i].fpga_update = rp_main_params[i].fpga_update;
        p_copy[i].read_only   = rp_main_params[i].read_only;
        p_copy[i].min_val     = rp_main_params[i].min_val;
        p_copy[i].max_val     = rp_main_params[i].max_val;
    }
    pthread_mutex_unlock(&rp_main_params_mutex);
    p_copy[PARAMS_NUM].name = NULL;

    *p = p_copy;
    fprintf(stderr, "rp_get_params: END\n\n");
    return PARAMS_NUM;
}

int rp_get_signals(float ***s, int *sig_num, int *sig_len)
{
    int ret_val = 0;

    fprintf(stderr, "rp_get_signals: BEGIN\n");

    // Debugging
    set_leds(1, 0x10, 0);

    if (*s == NULL)
        return -1;

    *sig_num = SIGNALS_NUM;
    *sig_len = SIGNAL_LENGTH;

    fprintf(stderr, "rp_get_signals: END\n\n");
    return ret_val;
}



int rp_update_main_params(rp_app_params_t *params)
{
    int i = 0;
    if (params == NULL)
        return -1;

    pthread_mutex_lock(&rp_main_params_mutex);
    while (params[i].name != NULL) {
        rp_main_params[i].value = params[i].value;
        i++;
    }
    pthread_mutex_unlock(&rp_main_params_mutex);
    params_init = 0;
    rp_set_params(&rp_main_params[0], PARAMS_NUM, 1);

    return 0;
}


void write_cal_eeprom( void)
{
    if (rp_write_calib_params(&rp_main_calib_params) < 0) {
        fprintf(stderr, "rp_write_calib_params() failed. \n");
    }
}
