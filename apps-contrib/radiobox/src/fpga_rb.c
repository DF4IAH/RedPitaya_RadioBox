/**
 * $Id: fpga_rb.c 2015-10-18 20:50:00Z DF4IAH $
 *
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

#include "fpga.h"


/* @brief The RadioBox memory file descriptor used to mmap() the FPGA space. */
extern int                	g_fpga_rb_mem_fd;

/* @brief The RadioBox memory layout of the FPGA registers. */
extern fpga_rb_reg_mem_t*	g_fpga_rb_reg_mem;


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

    // init the RadioBox FPGA sub-module
	g_fpga_rb_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (g_fpga_rb_mem_fd < 0) {
		fprintf(stderr, "RadioBox: open(/dev/mem) failed: %s\n", strerror(errno));
		fpga_exit();
		return -1;
	}
	long page_addr = FPGA_RB_BASE_ADDR & (~(page_size-1));
	long page_offs = FPGA_RB_BASE_ADDR - page_addr;

	void* page_ptr = mmap(NULL, FPGA_RB_BASE_SIZE, PROT_READ | PROT_WRITE,
					  MAP_SHARED, g_fpga_rb_mem_fd, page_addr);
	if (page_ptr == MAP_FAILED) {
		fprintf(stderr, "RadioBox: mmap() failed: %s\n", strerror(errno));
		fpga_exit();
		return -1;
	}
	g_fpga_rb_reg_mem = (fpga_rb_reg_mem_t*) page_ptr + page_offs;

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
	// unmap the RadioBox sub-module
	if (g_fpga_rb_reg_mem) {
		if (munmap(g_fpga_rb_reg_mem, FPGA_RB_BASE_SIZE) < 0) {
			fprintf(stderr, "Radio-Box: munmap() failed: %s\n", strerror(errno));
			return -1;
		}
		g_fpga_rb_reg_mem = NULL;
	}

	if (g_fpga_rb_mem_fd >= 0) {
		close(g_fpga_rb_mem_fd);
		g_fpga_rb_mem_fd = -1;
	}

	return 0;
}


/*----------------------------------------------------------------------------*/
/**
 * @brief Reset write state machine
 *
 * @retval 0 Success, never fails
 */
int fpga_rb_reset(void)
{
    return 0;
}


/*----------------------------------------------------------------------------*/
/**
 * @brief RadioBox register settings based on current local parameters
 *
 * Knows each parameter for the RadioBox sub-module and do a corresponding
 * register setting.
 *
 * @retval  0 Success
 * @retval -1 Failure, error message is output on standard error device
 */
int fpga_rb_update_all_params(rp_app_params_t* p[])
{
	if (!g_fpga_rb_reg_mem || !p) {
		return -1;
	}

	int idx = 0;
	while (1) {
		if (!(p[idx]->name)) {
			break;
		}

		if (!strcmp("RB_OSC1_QRG", p[idx]->name)) {  // @see cb_http.h
			uint32_t hi, lo;
			float inc = 0.5f + 125E+6f / p[idx]->value;
			uint64_t inc_ll = (uint64_t) inc;
			fpga_rb_update_params(RB_OSC1_INC_HI, inc_ll >> 32);
			fpga_rb_update_params(RB_OSC1_INC_LO, inc_ll & 0xffffffff);
		}
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Setup of RadioBox FPGA module based on specified settings
 *
 * @param[in] xxx           nonzero if acquisition is applied immediately, zero if acquisition is trigger dependent
 *
 * @retval  0 Success
 * @retval -1 Failure, error message is output on standard error device
 */

int fpga_rb_update_params(unsigned int rb_reg_ofs, uint32_t value)
{

    return 0;
}
