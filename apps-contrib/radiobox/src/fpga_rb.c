/**
 * @brief Red Pitaya FPGA Interface for the RadioBox sub-module.
 *
 * @author Ulrich Habel (DF4IAH) <espero7757@gmx.net>
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "calib.h"
#include "fpga.h"
#include "cb_http.h"


/* @brief The RadioBox memory file descriptor used to mmap() the FPGA space. */
extern int                	g_fpga_rb_mem_fd;

/* @brief The RadioBox memory layout of the FPGA registers. */
extern fpga_rb_reg_mem_t*	g_fpga_rb_reg_mem;

/* @brief Access to the calibration data. */
extern rp_calib_params_t 	rp_main_calib_params;


/*----------------------------------------------------------------------------*/
/**
 * @brief Initialize interface to RadioBox FPGA sub-module
 *
 * Set-up for FPGA access to the RadioBox sub-module.
 *
 * @retval  0 Success
 * @retval -1 Failure, error message is printed on standard error device
 *
 */
int fpga_rb_init(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

    // make sure all previous data is vanished
    fpga_rb_exit();

    fprintf(stderr, "fpga_rb_init: BEGIN\n");

    // init the RadioBox FPGA sub-module
	g_fpga_rb_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (g_fpga_rb_mem_fd < 0) {
		fprintf(stderr, "ERROR - fpga_rb_init: open(/dev/mem) failed: %s\n", strerror(errno));
		fpga_exit();
		return -1;
	}
	long page_addr = FPGA_RB_BASE_ADDR & (~(page_size-1));
	long page_offs = FPGA_RB_BASE_ADDR - page_addr;

	void* page_ptr = mmap(NULL, FPGA_RB_BASE_SIZE, PROT_READ | PROT_WRITE,
					      MAP_SHARED, g_fpga_rb_mem_fd, page_addr);
	if (page_ptr == MAP_FAILED) {
		fprintf(stderr, "ERROR - fpga_rb_init: mmap() failed: %s\n", strerror(errno));
		fpga_exit();
		return -1;
	}
	g_fpga_rb_reg_mem = (fpga_rb_reg_mem_t*) page_ptr + page_offs;

    fprintf(stderr, "fpga_rb_init: END\n");
    return 0;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Finalize and release allocated resources of the RadioBox sub-module
 *
 * @retval 0 Success, never fails
 */
int fpga_rb_exit(void)
{
    fprintf(stderr, "fpga_rb_exit: BEGIN\n");

    // unmap the RadioBox sub-module
	if (g_fpga_rb_reg_mem) {
		if (munmap(g_fpga_rb_reg_mem, FPGA_RB_BASE_SIZE) < 0) {
			fprintf(stderr, "ERROR - fpga_rb_exit: munmap() failed: %s\n", strerror(errno));
			return -1;
		}
		g_fpga_rb_reg_mem = NULL;
	}

	if (g_fpga_rb_mem_fd >= 0) {
		close(g_fpga_rb_mem_fd);
		g_fpga_rb_mem_fd = -1;
	}

    fprintf(stderr, "fpga_rb_exit: END\n");
	return 0;
}


/*----------------------------------------------------------------------------*/
/**
 * @brief Updates all modified data attributes to the RadioBox FPGA sub-module
 *
 * @param[in] rp_app_params_t*  list of parameters to be scanned for marked entries.
 *
 * @retval  0 Success
 * @retval -1 Failure, parameter list or RB accessor not valid
 */
int fpga_rb_update_all_params(rp_app_params_t* p[])
{
    fprintf(stderr, "fpga_rb_update_all_params: BEGIN\n");

    if (!g_fpga_rb_reg_mem || !p) {
        fprintf(stderr, "ERROR - fpga_rb_update_all_params: bad parameter (p=%p) or not inited(g=%p)\n", p, g_fpga_rb_reg_mem);
		return -1;
	}

	int idx = 0;
	while (1) {
		if (!(p[idx]->name)) {
			break;  // end of list
		}

		if (!(p[idx]->fpga_update & 0x80)) {
	        fprintf(stderr, "INFO - fpga_rb_update_all_params: skipped not modified parameter (name=%s)\n", p[idx]->name);
			continue;  // this value has not been updated
		}

		if (!strcmp("RB_RUN", p[idx]->name)) {  // @see cb_http.h
	        fprintf(stderr, "INFO - fpga_rb_update_all_params: got RB_RUN\n");
			fpga_rb_enable(p[idx]->value);

		} else if (!strcmp("osc1_qrg_i", p[idx]->name)) {  // @see cb_http.h
	        fprintf(stderr, "INFO - fpga_rb_update_all_params: got osc1_qrg_i\n");

	        float qrg1 = 0.5f + p[idx]->value * ((1ULL << 48) / rp_main_calib_params.base_osc125mhz_realhz);

	        fpga_rb_write_register(FPGA_RB_OSC1_INC_LO, (uint32_t) (((uint64_t) qrg1) & 0xffffffff));
	        fpga_rb_write_register(FPGA_RB_OSC1_INC_HI, (uint32_t) (((uint64_t) qrg1) >> 32));

		} else if (!strcmp("osc2_qrg_i", p[idx]->name)) {  // @see cb_http.h
	        fprintf(stderr, "INFO - fpga_rb_update_all_params: got osc2_qrg_i\n");

			float qrg2 = 0.5f + p[idx]->value * ((1ULL << 48) / rp_main_calib_params.base_osc125mhz_realhz);

			fpga_rb_write_register(FPGA_RB_OSC2_INC_LO, (uint32_t) (((uint64_t) qrg2) & 0xffffffff));
			fpga_rb_write_register(FPGA_RB_OSC2_INC_HI, (uint32_t) (((uint64_t) qrg2) >> 32));

		} else if (!strcmp("osc1_amp_i", p[idx]->name)) {  // @see cb_http.h
	        fprintf(stderr, "INFO - fpga_rb_update_all_params: got osc1_amp_i\n");

	        float amp = 0.5f + (1ULL << 32) * p[idx]->value / 2.048;									// TODO: DAC amplitude correction goes into here
			float ofs = 0.0f;																		// TODO: DAC offset correction goes into here

			fpga_rb_write_register(FPGA_RB_OSC1_MIX_GAIN, (uint32_t) amp);
			fpga_rb_write_register(FPGA_RB_OSC1_MIX_OFS_LO, (uint32_t) (((uint64_t) ofs) & 0xffffffff));
			fpga_rb_write_register(FPGA_RB_OSC1_MIX_OFS_HI, (uint32_t) (((uint64_t) ofs) >> 32));

		// } else if (!strcmp("osc1_amp_i", p[idx]->name)) {  // @see cb_http.h
		//   no FPGA register is set here, wait until "osc1_modtyp_s" is set

		} else if (!strcmp("osc1_modtyp_s", p[idx]->name)) {  // @see cb_http.h
	        fprintf(stderr, "INFO - fpga_rb_update_all_params: got osc1_modtyp_s\n");

			switch ((int) (p[idx]->value)) {

			default:
			case 0: {  // Modulation: AM
		        fprintf(stderr, "INFO - fpga_rb_update_all_params: setting FPGA for AM modulation\n");

		        float qrg1 = 0.5f +  p[RB_OSC1_QRG]->value * ((1ULL << 48) / rp_main_calib_params.base_osc125mhz_realhz);
				float gain = 0.5f + (p[RB_OSC2_MAG]->value / 100.0) *     0x3fffffff;
				float offs = 0.5f + (p[RB_OSC2_MAG]->value / 100.0) * 0x3fffffffffff;

				fpga_rb_write_register(FPGA_RB_CTRL, 0x00001017);									// control: resync OSC1 & OSC2
				fpga_rb_write_register(FPGA_RB_OSC1_INC_LO, (uint32_t) (((uint64_t) qrg1) & 0xffffffff));
				fpga_rb_write_register(FPGA_RB_OSC1_INC_HI, (uint32_t) (((uint64_t) qrg1) >> 32));
				fpga_rb_write_register(FPGA_RB_OSC1_OFS_LO, 0);										// no carrier phase offset
				fpga_rb_write_register(FPGA_RB_OSC1_OFS_HI, 0);										// no carrier phase offset
				fpga_rb_write_register(FPGA_RB_OSC2_MIX_GAIN, (uint32_t) ((uint64_t) gain));
				fpga_rb_write_register(FPGA_RB_OSC2_MIX_OFS_LO, (uint32_t) (((uint64_t) offs) & 0xffffffff));
				fpga_rb_write_register(FPGA_RB_OSC2_MIX_OFS_HI, (uint32_t) (((uint64_t) offs) >> 32));
				fpga_rb_write_register(FPGA_RB_CTRL, 0x00000081);									// control: amplitude modulation  @see red_pitaya_radiobox_tb.sv
			}
			break;

			case 1: {  // Modulation: FM
		        fprintf(stderr, "INFO - fpga_rb_update_all_params: setting FPGA for FM modulation\n");

				float qrg1 = 0.5f + p[RB_OSC1_QRG]->value * ((1ULL << 48) / rp_main_calib_params.base_osc125mhz_realhz);
				float devi = 0.5f + p[RB_OSC2_MAG]->value * ((1ULL << 32) / rp_main_calib_params.base_osc125mhz_realhz);

				fpga_rb_write_register(FPGA_RB_CTRL, 0x00001017);									// control: resync OSC1 & OSC2
				fpga_rb_write_register(FPGA_RB_OSC1_INC_LO, 0);										// not used while streaming in
				fpga_rb_write_register(FPGA_RB_OSC1_INC_HI, 0);										// not used while streaming in
				fpga_rb_write_register(FPGA_RB_OSC1_OFS_LO, 0);										// no carrier phase offset
				fpga_rb_write_register(FPGA_RB_OSC1_OFS_HI, 0);										// no carrier phase offset
				fpga_rb_write_register(FPGA_RB_OSC2_MIX_GAIN, (uint32_t) devi);
				fpga_rb_write_register(FPGA_RB_OSC2_MIX_OFS_LO, (uint32_t) (((uint64_t) qrg1) & 0xffffffff));
				fpga_rb_write_register(FPGA_RB_OSC2_MIX_OFS_HI, (uint32_t) (((uint64_t) qrg1) >> 32));
				fpga_rb_write_register(FPGA_RB_CTRL, 0x00000021);									// control: frequency modulation  @see red_pitaya_radiobox_tb.sv
			}
			break;

			case 2: {  // Modulation: PM
		        fprintf(stderr, "INFO - fpga_rb_update_all_params: setting FPGA for PM modulation\n");

		        float qrg1 = 0.5f +  p[RB_OSC1_QRG]->value * ((1ULL << 48) / rp_main_calib_params.base_osc125mhz_realhz);
				float gain = 0.5f +  p[RB_OSC2_MAG]->value * ((1ULL << 32) / rp_main_calib_params.base_osc125mhz_realhz);

				fpga_rb_write_register(FPGA_RB_CTRL, 0x00001017);									// control: resync OSC1 & OSC2
				fpga_rb_write_register(FPGA_RB_OSC1_INC_LO, (uint32_t) (((uint64_t) qrg1) & 0xffffffff));
				fpga_rb_write_register(FPGA_RB_OSC1_INC_HI, (uint32_t) (((uint64_t) qrg1) >> 32));
				fpga_rb_write_register(FPGA_RB_OSC1_OFS_LO, 0);										// not used while streaming in
				fpga_rb_write_register(FPGA_RB_OSC1_OFS_HI, 0);										// not used while streaming in
				fpga_rb_write_register(FPGA_RB_OSC2_MIX_GAIN, (uint32_t) gain);
				fpga_rb_write_register(FPGA_RB_OSC2_MIX_OFS_LO, 0);
				fpga_rb_write_register(FPGA_RB_OSC2_MIX_OFS_HI, 0);
				fpga_rb_write_register(FPGA_RB_CTRL, 0x00000041);									// control: phase modulation  @see red_pitaya_radiobox_tb.sv
			}
			break;

			}  // switch()
		}  // else if ()
	}  // while (1)

    fprintf(stderr, "fpga_rb_update_all_params: END\n");
	return 0;
}


/*----------------------------------------------------------------------------*/
/**
 * @brief Reads value from the specific RadioBox sub-module register
 *
 * @param[in] rb_reg_ofs  offset value for the RadioBox base address to be written to.
 *
 * @retval  value of the specified register.
 */
uint32_t fpga_rb_read_register(unsigned int rb_reg_ofs)
{
	if (!g_fpga_rb_reg_mem) {
		return -1;
	}

	return *((uint32_t*) ((void*) g_fpga_rb_reg_mem) + rb_reg_ofs);
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Writes value to the specific RadioBox sub-module register
 *
 * @param[in] rb_reg_ofs  offset value for the RadioBox base address to be written to.
 * @param[in] value  value that is written to the specified register.
 *
 * @retval  0 Success
 * @retval -1 Failure, error message is output on standard error device
 */
int fpga_rb_write_register(unsigned int rb_reg_ofs, uint32_t value)
{
	if (!g_fpga_rb_reg_mem) {
		return -1;
	}

	*((uint32_t*) ((void*) g_fpga_rb_reg_mem) + rb_reg_ofs) = value;
    return 0;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Enables or disables RadioBox FPGA sub-module
 *
 * @param[in] enable  nonzero enables the RadioBox sub-module, zero disables it.
 *
 */
void fpga_rb_enable(int enable)
{
	if (enable) {
		// enable RadioBox
		uint32_t newreg = 0x00000001 | fpga_rb_read_register(FPGA_RB_CTRL);
		fpga_rb_write_register(FPGA_RB_CTRL, newreg);
		fpga_rb_reset();

		fpga_rb_write_register(FPGA_RB_LED_CTRL, 3);  // show OSC1 output at RB LEDs

	} else {
		fpga_rb_write_register(FPGA_RB_LED_CTRL, 0);  // disable RB LEDs

		// disable RadioBox
		fpga_rb_reset();
		fpga_rb_write_register(FPGA_RB_CTRL, 0x00000000);
	}
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Resets RadioBox FPGA sub-module
 *
 */
void fpga_rb_reset(void)
{
	// send resync to OSC1 and OSC2
	fpga_rb_write_register(FPGA_RB_CTRL, 0x00001011);

	// send resync and reset to OSC1 and OSC2
	fpga_rb_write_register(FPGA_RB_CTRL, 0x00001017);

	// send resync to OSC1 and OSC2
	fpga_rb_write_register(FPGA_RB_CTRL, 0x00001011);

	// run mode of both oscillators
	fpga_rb_write_register(FPGA_RB_CTRL, 0x00000001);
}
