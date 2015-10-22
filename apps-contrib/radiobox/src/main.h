/**
 * @brief Red Pitaya RadioBox main module.
 *
 * @author Ulrich Habel (DF4IAH) <espero7757@gmx.net>
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


/** @defgroup main_h RadioBox main module
 * @{
 */

/** @brief Parameters description structure - must be the same for all RP controllers */
typedef struct rp_app_params_s {
	/** @brief name  Name of the parameter */
	char  *name;

	/** @brief value  Value of the parameter */
    float  value;

	/** @brief fpga_update  Do a FPGA register update based on this parameter */
    int    fpga_update;

	/** @brief read_only  The value of this parameter can not be changed */
    int    read_only;

	/** @brief min_val  The lower limit of the value */
    float  min_val;

	/** @brief max_val  The upper limit of the value */
    float  max_val;
} rp_app_params_t;


/* Output signals */
/** @brief The number of traces beeing hold in the traces buffer */
#define TRACE_NUM   3
/** @brief The number of points that a single trace holds */
#define TRACE_LENGTH (1024) /* Must be 2^n! */

/** @brief Returns application description for the RadioBox sub-module */
const char* rp_app_desc(void);

/* Internal helper functions */
/** @brief Creates traces buffers */
int  rp_create_traces(float** a_traces[TRACE_NUM]);
/** @brief Frees traces buffers */
void rp_free_traces(float** a_traces[TRACE_NUM]);

/** @brief Copies parameters from src to dst - if dst does not exist, it is created */
int rp_copy_params(rp_app_params_t** dst, const rp_app_params_t src[], int len, int do_copy_all_attr);
/** @brief Frees memory of parameters structure */
int rp_free_params(rp_app_params_t** params);

/** @} */


#endif /*  __MAIN_H */
