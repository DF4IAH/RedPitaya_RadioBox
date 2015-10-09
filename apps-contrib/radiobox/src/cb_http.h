/*
 * cb_http.h
 *
 *  Created on: 09.10.2015
 *      Author: espero
 */

#ifndef APPS_CONTRIB_RADIOBOX_SRC_CB_HTTP_H_
#define APPS_CONTRIB_RADIOBOX_SRC_CB_HTTP_H_

#include "main.h"


/* module entry points */
int rp_app_init(void);
int rp_app_exit(void);
int rp_set_params(rp_app_params_t *p, int len, int internal_flag);
int rp_get_params(rp_app_params_t **p);
int rp_get_signals(float ***s, int *sig_num, int *sig_len);

/* Updates all parameters (structure must be aligned with main parameter
 * structure - this includes also ready-only parameters. After the
 * parameters are updated it also changed the worker state machine.
 */
int rp_update_main_params(rp_app_params_t *params);

void write_cal_eeprom( void);


#endif /* APPS_CONTRIB_RADIOBOX_SRC_CB_HTTP_H_ */
