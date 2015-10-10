/**
 * $Id: main.h 881 2013-12-16 05:37:34Z rp_jmenart $
 *
 * @brief Red Pitaya Oscilloscope main module.
 *
 * @Author Jure Menart <juremenart@gmail.com>
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#ifndef __MAIN_H
#define __MAIN_H


#include <stdint.h>


#ifdef DEBUG
#  define TRACE(args...) fprintf(stderr, args)
#else
#  define TRACE(args...) {}
#endif


/* Parameters description structure - must be the same for all RP controllers */
typedef struct rp_app_params_s {
    char  *name;
    float  value;
    int    fpga_update;
    int    read_only;
    float  min_val;
    float  max_val;
} rp_app_params_t;


/* Output signals */
#define TRACE_NUM   3
#define TRACE_LENGTH (1024) /* Must be 2^n! */


/* Internal helper functions */
int  rp_create_traces(float*** a_traces);
void rp_free_traces(float*** a_traces);

/* copies parameters from src to dst - if dst does not exist, it is created */
int rp_copy_params(rp_app_params_t** dst, rp_app_params_t* src);

/* frees memory of parameters structure */
int rp_free_params(rp_app_params_t* params);


#endif /*  __MAIN_H */
