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


/* Parameters indexes - these defines should be in the same order as
 * rp_app_params_t structure defined in main.c */

/** @brief RadioBox parameters */
enum rb_params_enum_t {
    RB_RUN                  =  0,
    RB_OSC1_MODSRC,
    RB_OSC1_MODTYP,
    RB_OSC1_QRG,
    RB_OSC2_QRG,
    RB_OSC1_AMP,
    RB_OSC2_MAG,

    RB_PARAMS_NUM
} RB_PARAMS_ENUM;


/** @brief RadioBox modulation sources */
enum rb_modsrc_enum_t {
    RB_MODSRC_NONE          =  0,
    RB_MODSRC_RF_IN1,
    RB_MODSRC_RF_IN2,
    RB_MODSRC_EXP_AI0       =  4,
    RB_MODSRC_EXP_AI1,
    RB_MODSRC_EXP_AI2,
    RB_MODSRC_EXP_AI3,
    RB_MODSRC_OSC2          = 15
} RB_MODSRC_ENUM;


/** @brief RadioBox modulation types */
enum rb_modtyp_enum_t {
    RB_MODTYP_AM            =  0,
    RB_MODTYP_FM,
    RB_MODTYP_PM
} RB_MODTYP_ENUM;


/* Output signals */
/** @brief The number of traces beeing hold in the traces buffer */
#define TRACE_NUM   3
/** @brief The number of points that a single trace holds */
#define TRACE_LENGTH (1024) /* Must be 2^n! */


/**
 * @brief Returns description cstring for this RadioBox sub-module
 *
 * This function returns a null terminated cstring.
 *
 * @retval      cstring    Description of this RadioBox sub-module
 */
const char* rp_app_desc(void);


/* Internal helper functions */

/**
 * @brief Prepares buffers for signal traces (not used yet)
 *
 * This function allocates memory for TRACE_NUM traces in the memory and prepares
 * for its usage.
 *
 * @param[inout] a_traces  Pointer to traces buffer
 *
 * @retval       0         Success
 * @retval       -1        Failed to establish the traces buffer
 */
int  rp_create_traces(float** a_traces[TRACE_NUM]);

/**
 * @brief Frees memory used by the traces buffer (not used yet)
 *
 * This function frees memory for TRACE_NUM traces in the memory.
 *
 * @param[inout] a_traces  Pointer to traces buffer
 */
void rp_free_traces(float** a_traces[TRACE_NUM]);

/**
 * @brief Make a copy of Application parameters
 *
 * Function copies actual Application parameters to the specified destination
 * buffer. This action was intended to prepare two parameter instances, where the first
 * one can be further modified from the user side, while the second one is processed by
 * the worker thread.
 * In case the destination buffer is not allocated yet, it is allocated internally and must
 * be freed outside of the function scope by calling rp_clean_params() function. Note that
 * if function returns failure, the destination buffer could be partially allocated and must
 * be freed in the same way.
 * If the specified destination buffer is already allocated, it is assumed the number of table
 * entries is the same as in the source table. No special check is made internally if this is really
 * the case.
 *
 * @param[out]  dst               Destination application parameters, in case of ptr to NULL a new parameter list is generated.
 * @param[in]   src               Source application parameters. In case of a NULL point the default parameters are take instead.
 * @param[in]   len               The count of parameters in the src vector.
 * @param[in]   do_copy_all_attr  Do a fully copy of all attributes, not just the name, value and fpga_update entries.
 * @retval      0                 Successful operation
 * @retval      -1                Failure, error message is output on standard error
 */
int rp_copy_params(rp_app_params_t** dst, const rp_app_params_t src[], int len, int do_copy_all_attr);

/**
 * @brief Deallocate the specified buffer of Application parameters
 *
 * Function is used to deallocate the specified buffers, which were previously
 * allocated by calling rp_copy_params() function.
 *
 * @param[in]   params  Application parameters to be deallocated
 * @retval      0       Success
 * @retval      -1      Failed with non-valid params
 */
int rp_free_params(rp_app_params_t** params);

/** @} */


#endif /*  __MAIN_H */
