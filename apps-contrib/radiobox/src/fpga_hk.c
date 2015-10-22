/**
 * @brief Red Pitaya FPGA Interface for the House-keeping sub-module.
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


/** @brief The House-keeping memory file descriptor used to mmap() the FPGA space. */
extern int           		g_fpga_hk_mem_fd;

/** @brief The House-keeping memory layout of the FPGA registers. */
extern fpga_hk_reg_mem_t*	g_fpga_hk_reg_mem;


/*----------------------------------------------------------------------------*/
int fpga_hk_init(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

    // make sure all previous data is vanished
    fpga_hk_exit();

    // init the HouseKeeping FPGA sub-module
	g_fpga_hk_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (g_fpga_hk_mem_fd < 0) {
		fprintf(stderr, "House-keeping: open(/dev/mem) failed: %s\n", strerror(errno));
		fpga_exit();
		return -1;
	}
	long page_addr = FPGA_HK_BASE_ADDR & (~(page_size-1));
	long page_offs = FPGA_HK_BASE_ADDR - page_addr;

	void* page_ptr = mmap(NULL, FPGA_HK_BASE_SIZE, PROT_READ | PROT_WRITE,
					  MAP_SHARED, g_fpga_hk_mem_fd, page_addr);
	if (page_ptr == MAP_FAILED) {
		fprintf(stderr, "House-keeping: mmap() failed: %s\n", strerror(errno));
		fpga_exit();
		return -1;
	}
	g_fpga_hk_reg_mem = (fpga_hk_reg_mem_t*) (page_ptr + page_offs);

    return 0;
}

/*----------------------------------------------------------------------------*/
int fpga_hk_exit(void)
{
	// unmap the House-keeping sub-module
	if (g_fpga_hk_reg_mem) {
		if (munmap(g_fpga_hk_reg_mem, FPGA_HK_BASE_SIZE) < 0) {
			fprintf(stderr, "House-keeping: munmap() failed: %s\n", strerror(errno));
			return -1;
		}
		g_fpga_hk_reg_mem = NULL;
	}

	if (g_fpga_hk_mem_fd >= 0) {
		close(g_fpga_hk_mem_fd);
		g_fpga_hk_mem_fd = -1;
	}

	return 0;
}


/*----------------------------------------------------------------------------*/
int fpga_hk_setLeds(unsigned char doToggle, unsigned char mask, unsigned char leds)
{
    if (!g_fpga_hk_reg_mem) {
    	return -1;
    }

    // setting LEDs
    if (doToggle) {
        g_fpga_hk_reg_mem->leds = (g_fpga_hk_reg_mem->leds          ^  mask       );

    } else {
        g_fpga_hk_reg_mem->leds = (g_fpga_hk_reg_mem->leds & ~mask) | (mask & leds);
    }

    return 0;
}
