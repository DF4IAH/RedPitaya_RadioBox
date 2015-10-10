/*
 * cb_http.h
 *
 *  Created on: 09.10.2015
 *      Author: espero
 */

#ifndef APPS_CONTRIB_RADIOBOX_SRC_CB_HTTP_H_
#define APPS_CONTRIB_RADIOBOX_SRC_CB_HTTP_H_

#include "main.h"


/* Parameters indexes - these defines should be in the same order as
 * rp_app_params_t structure defined in cb_http.c */

/* RadioBox parameters */
enum rb_enum_t {
	RB_RUN         = 0,
	RB_OSC1_QRG,
	RB_OSC1_AMP,
	RB_OSC1_MODSRC,
	RB_OSC1_MODTYP,
	RB_OSC2_QRG,
	RB_OSC2_MAG,
	RB_ADD_A,
	RB_ADD_B,
	RB_ADD_RES,

	PARAMS_NUM
};


/* module entry points */
int rp_app_init(void);
int rp_app_exit(void);
int rp_set_params(rp_app_params_t* p, int len, int internal_flag);
int rp_get_params(rp_app_params_t** p);
int rp_get_signals(float*** s, int* sig_num, int* sig_len);


/* Updates all parameters (structure must be aligned with main parameter
 * structure - this includes also ready-only parameters. After the
 * parameters are updated it also changes the worker state machine.
 */
int rp_update_main_params(rp_app_params_t* params);


#endif /* APPS_CONTRIB_RADIOBOX_SRC_CB_HTTP_H_ */
