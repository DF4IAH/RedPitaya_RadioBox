/*
 * cb_ws.h
 *
 *  Created on: 08.10.2015
 *      Author: espero
 */

#ifndef APPS_CONTRIB_RADIOBOX_SRC_CB_WS_H_
#define APPS_CONTRIB_RADIOBOX_SRC_CB_WS_H_


/* v0.94 WebSocket call-back functions */

void ws_set_params_interval(int interval);
void ws_set_signals_interval(int interval);
int ws_get_params_interval();
int ws_get_signals_interval();
void ws_set_params();
void ws_get_params();
void ws_set_signals();
void ws_get_signals();
void ws_set_demo_mode(int isDemo);
int verify_app_license();


#endif /* APPS_CONTRIB_RADIOBOX_SRC_CB_WS_H_ */
