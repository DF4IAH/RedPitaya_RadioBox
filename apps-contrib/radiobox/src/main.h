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


typedef struct hk_fpga_reg_mem_s {
    /* configuration:
     * bit   [3:0] - hw revision
     * bits [31:4] - reserved
     */
    uint32_t rev;					// 0x00
    /* DNA low word */
    uint32_t dna_lo;				// 0x04
    /* DNA high word */
    uint32_t dna_hi;				// 0x08

    uint32_t reserved00;			// 0x0C

    uint32_t exp_dir_p;				// 0x10
    uint32_t exp_dir_n;				// 0x14
    uint32_t exp_out_p;				// 0x18
    uint32_t exp_out_n;				// 0x1C
    uint32_t exp_in_p;				// 0x20
    uint32_t exp_in_n;				// 0x24

    uint32_t reserved01;			// 0x28
    uint32_t reserved02;			// 0x2C

    uint32_t leds;					// 0x30
} hk_fpga_reg_mem_t;

/* Parameters description structure - must be the same for all RP controllers */
typedef struct rp_app_params_s {
    char  *name;
    float  value;
    int    fpga_update;
    int    read_only;
    float  min_val;
    float  max_val;
} rp_app_params_t;

/* Signal measurement results structure - filled in worker and updated when
 * also measurement signal is stored from worker
 */
typedef struct rp_osc_meas_res_s {
    float min;
    float max;
    float amp;
    float avg;
    float freq;
    float period;
} rp_osc_meas_res_t;


/** Base OSC FPGA address */
#define HK_FPGA_BASE_ADDR 0x40000000
/** Base OSC FPGA core size */
#define HK_FPGA_BASE_SIZE 0x01000

/** Base RB FPGA address */
#define RB_FPGA_BASE_ADDR 0x40600000
/** Base RB FPGA core size */
#define RB_FPGA_BASE_SIZE 0x01000


/* Parameters indexes - these defines should be in the same order as
 * rp_app_params_t structure defined in main.c */
/* RadioBox parameters */
#define RB_OSC1_QRG   	   0
#define RB_OSC1_AMP   	   1
#define RB_OSC1_MODSRC     2
#define RB_OSC1_MODTYP     3
#define RB_OSC2_QRG   	   4
#define RB_OSC2_MAG   	   5

#define RB_ADD_A   	       6
#define RB_ADD_B   	       7
#define RB_ADD_RES	       8

#define PARAMS_NUM         9


/* Output signals */
#define SIGNAL_LENGTH (1024) /* Must be 2^n! */
#define SIGNALS_NUM   3


/* module entry points */
int rp_app_init(void);
int rp_app_exit(void);
int rp_set_params(rp_app_params_t *p, int len, int internal_flag);
int rp_get_params(rp_app_params_t **p);
int rp_get_signals(float ***s, int *sig_num, int *sig_len);

/* Internal helper functions */
int  rp_create_signals(float ***a_signals);
void rp_cleanup_signals(float ***a_signals);

/* copies parameters from src to dst - if dst does not exists, it creates it */
int rp_copy_params(rp_app_params_t *src, rp_app_params_t **dst);

/* cleans up memory of parameters structure */
int rp_clean_params(rp_app_params_t *params);

/* Updates all parameters (structure must be aligned with main parameter
 * structure - this includes also ready-only parameters. After the
 * parameters are updated it also changed the worker state machine.
 */
int rp_update_main_params(rp_app_params_t *params);

void write_cal_eeprom( void);

void set_leds(unsigned char doToggle, unsigned char mask, unsigned char leds);
void rb_set_fpga(unsigned int base_offs, unsigned int value);
unsigned int rb_get_fpga(unsigned int base_offs);


#endif /*  __MAIN_H */
