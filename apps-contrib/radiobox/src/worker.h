/**
 * $Id: worker.h 881 2013-12-16 05:37:34Z rp_jmenart $
 *
 * @brief Red Pitaya Oscilloscope worker.
 *
 * @Author Jure Menart <juremenart@gmail.com>
 *         
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#ifndef __WORKER_H
#define __WORKER_H

#include "main.h"
#include "calib.h"


typedef enum worker_state_e {
    worker_idle_state = 0, /* do nothing */
    worker_quit_state, /* shutdown worker */
    worker_abort_state, /* abort current measurement */
    worker_normal_state, /* normal mode */
    worker_nonexisting_state /* must be last */
} worker_state_t;


int worker_init(rp_app_params_t* params, int params_len, rp_calib_params_t* calib_params);
int worker_exit(void);
void* worker_thread(void* args);

int worker_update_params(rp_app_params_t* params, int fpga_update);


/* removes 'dirty' flags */
int worker_clear_signals(void);

/* Returns:
 *  0  - new signals (dirty signal) are copied to the output
 *  -1 - no new signals available (dirty signal was not set - we need to wait)
 */
int worker_get_signals(float*** traces, int* trc_idx);

/* Fills the output signal structure from temp one after calculation is done 
 * and marks it dirty 
 */
int worker_set_signals(float** source, int index);


#endif /* __WORKER_H*/
