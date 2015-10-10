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
#define RB_OSC1_QRG   	   	0
#define RB_OSC1_AMP   	   	1
#define RB_OSC1_MODSRC     	2
#define RB_OSC1_MODTYP     	3
#define RB_OSC2_QRG   	   	4
#define RB_OSC2_MAG   	   	5

#define RB_ADD_A   	       	6
#define RB_ADD_B   	       	7
#define RB_ADD_RES	       	8

#define PARAMS_NUM         	9


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
