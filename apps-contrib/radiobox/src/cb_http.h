/**
 * @brief CallBack functions of the HTTP GET/POST parameter transfer system
 *
 * @author Ulrich Habel (DF4IAH) <espero7757@gmx.net>
 *
 *  Created on: 09.10.2015
 *      Author: espero
 */

#ifndef APPS_CONTRIB_RADIOBOX_SRC_CB_HTTP_H_
#define APPS_CONTRIB_RADIOBOX_SRC_CB_HTTP_H_

#include "main.h"


/** @defgroup cb_http_h CallBack functions of the HTTP GET/POST parameter transfer system
 * @{
 */

/* Parameters indexes - these defines should be in the same order as
 * rp_app_params_t structure defined in cb_http.c */

/** @brief RadioBox parameters */
enum rb_enum_t {
	RB_RUN         = 0,
	RB_OSC1_QRG,
	RB_OSC1_AMP,
	RB_OSC1_MODSRC,
	RB_OSC1_MODTYP,
	RB_OSC2_QRG,
	RB_OSC2_MAG,

	RB_PARAMS_NUM
} RB_PARAMS_ENUM;


/* module entry points */
int rp_app_init(void);
int rp_app_exit(void);
int rp_set_params(const rp_app_params_t* p, int len);
int rp_get_params(rp_app_params_t** p);
int rp_get_signals(float*** s, int* sig_num, int* sig_len);

/** @} */


#endif /* APPS_CONTRIB_RADIOBOX_SRC_CB_HTTP_H_ */
