/**
 * $Id: fpga.h 881 2013-12-16 05:37:34Z rp_jmenart $
 *
 * @brief Red Pitaya Oscilloscope FPGA Interface.
 *
 * @author Jure Menart <juremenart@gmail.com>
 *         
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#ifndef __FPGA_H
#define __FPGA_H

#include <stdint.h>


/** @defgroup fpga_h FPGA memory layout
 * @{
 */

/** HouseKeeping starting address of FPGA registers. */
#define HK_FPGA_BASE_ADDR 	0x40000000

/** HouseKeeping memory map size of FPGA registers. */
#define HK_FPGA_BASE_SIZE 0x10000

/** @brief FPGA registry structure for the HouseKeeping sub-module.
 *
 * This structure is the direct image of the physical FPGA memory for the HouseKeeping sub-module.
 * It assures direct read / write FPGA access when it is mapped to the appropriate memory address
 * through the /dev/mem device.
 */
typedef struct hk_fpga_reg_mem_s {
    /* configuration:
     * bit   [3:0] - hw revision
     * bits [31:4] - reserved
     */

    /** @brief  ID of the FPGA : 0x40000000
     */
    uint32_t rev;

    /** @brief  DNA of the FPGA - the LSB part : 0x40000004
     */
    uint32_t dna_lo;

    /** @brief  DNA of the FPGA - the MSB part : 0x40000008
     */
    uint32_t dna_hi;

    /** @brief  digital loop control : 0x4000000c
     */
    uint32_t digital_loop;


    /** @brief  expansion port output, positive lines, direction mask : 0x40000010
     */
    uint32_t exp_dir_p;

    /** @brief  expansion port output, negative lines, direction mask : 0x40000014
     */
    uint32_t exp_dir_n;

    /** @brief  expansion port output, positive lines, data register : 0x40000018
     */
    uint32_t exp_out_p;

    /** @brief  expansion port output, negative lines, data register : 0x4000001c
     */
    uint32_t exp_out_n;

    /** @brief  expansion port input, positive lines, data register : 0x40000020
     */
    uint32_t exp_in_p;

    /** @brief  expansion port input, negative lines, data register : 0x40000024
     */
    uint32_t exp_in_n;

    uint32_t _reserved01;
    uint32_t _reserved02;


    /** @brief  LEDs, data register : 0x40000030
     */
    uint32_t leds;

} hk_fpga_reg_mem_t;



/** RadioBox starting address of FPGA registers. */
#define RB_FPGA_BASE_ADDR 	0x40600000

/** RadioBox memory map size of FPGA registers. */
#define RB_FPGA_BASE_SIZE 	0x10000


/** @brief FPGA registry structure for the RadioBox sub-module.
 *
 * This structure is the direct image of the physical FPGA memory for the RadioBox sub-module.
 * It assures direct read / write FPGA access when it is mapped to the appropriate memory address
 * through the /dev/mem device.
 */
typedef struct rb_fpga_reg_mem_s {

	/** @brief  Test adder - A : 0x40600000
     */
    uint32_t rb_add_a;

    /** @brief  Test adder - B : 0x0x40600004
     */
    uint32_t rb_add_b;


    /** @brief  Test adder - Result = A + B : 0x40600008
     */
    uint32_t rb_add_res;

} rb_fpga_reg_mem_t;

/** @} */



/* function declarations, detailed descriptions is in apparent implementation file  */

int fpga_init(void);
int fpga_exit(void);

// HouseKeepking FPGA accessors
int hk_fpga_setLeds(unsigned char doToggle, unsigned char mask, unsigned char leds);

// RadioBox FPGA accessors
int rb_fpga_reset(void);
int rb_fpga_update_params();


#endif /* __FPGA_H */
