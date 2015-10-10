/**
 * $Id: fpga.c 881 2013-12-16 05:37:34Z rp_jmenart $
 *
 * @brief Red Pitaya Oscilloscope FPGA Interface.
 *
 * @Author Jure Menart <juremenart@gmail.com>
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


/* @brief The HouseKeeping memory file descriptor used to mmap() the FPGA space. */
static int                g_hk_fpga_mem_fd = -1;

/* @brief The HouseKeeping memory layout of the FPGA registers. */
static hk_fpga_reg_mem_t *g_hk_fpga_reg_mem = NULL;


/* @brief The RadioBox memory file descriptor used to mmap() the FPGA space. */
static int                g_rb_fpga_mem_fd = -1;

/* @brief The RadioBox memory layout of the FPGA registers. */
static rb_fpga_reg_mem_t *g_rb_fpga_reg_mem = NULL;



/*----------------------------------------------------------------------------*/
/**
 * @brief Initialize interface to Oscilloscope FPGA module
 *
 * Function first optionally cleanups previously established access to Oscilloscope
 * FPGA module. Afterwards a new connection to the Memory handler is instantiated
 * by opening file descriptor over /dev/mem device. Access to Oscilloscope FPGA module
 * is further provided by mapping memory regions through resulting file descriptor.
 *
 * @retval  0 Success
 * @retval -1 Failure, error message is printed on standard error device
 *
 */
int fpga_init(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

    // make sure all previous data is vanished
    fpga_exit();

    // init the HouseKeeping FPGA sub-module
    {
        g_hk_fpga_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
		if (g_hk_fpga_mem_fd < 0) {
			fprintf(stderr, "HouseKeeping: open(/dev/mem) failed: %s\n", strerror(errno));
			fpga_exit();
			return -1;
		}
		long page_addr = HK_FPGA_BASE_ADDR & (~(page_size-1));
		long page_offs = HK_FPGA_BASE_ADDR - page_addr;

		void* page_ptr = mmap(NULL, HK_FPGA_BASE_SIZE, PROT_READ | PROT_WRITE,
                          MAP_SHARED, g_hk_fpga_mem_fd, page_addr);
		if (page_ptr == MAP_FAILED) {
			fprintf(stderr, "HouseKeeping: mmap() failed: %s\n", strerror(errno));
			fpga_exit();
			return -1;
		}
		g_hk_fpga_reg_mem = page_ptr + page_offs;
    }

    // init the RadioBox FPGA sub-module
    {
		g_rb_fpga_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
		if (g_rb_fpga_mem_fd < 0) {
			fprintf(stderr, "RadioBox: open(/dev/mem) failed: %s\n", strerror(errno));
			fpga_exit();
			return -1;
		}
		long page_addr = HK_FPGA_BASE_ADDR & (~(page_size-1));
		long page_offs = HK_FPGA_BASE_ADDR - page_addr;

		void* page_ptr = mmap(NULL, HK_FPGA_BASE_SIZE, PROT_READ | PROT_WRITE,
                          MAP_SHARED, g_hk_fpga_mem_fd, page_addr);
		if (page_ptr == MAP_FAILED) {
			fprintf(stderr, "RadioBox: mmap() failed: %s\n", strerror(errno));
			fpga_exit();
			return -1;
		}
		g_hk_fpga_reg_mem = page_ptr + page_offs;
    }

    return 0;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Finalize and release allocated resources while accessing the
 * Oscilloscope FPGA module
 *
 * Function is intended to be  called at the program termination.
 *
 * @retval 0 Success, never fails
 */
int fpga_exit(void)
{
	// unmap the RadioBox sub-module
	{
		if (g_rb_fpga_reg_mem) {
			if (munmap(g_rb_fpga_reg_mem, RB_FPGA_BASE_SIZE) < 0) {
				fprintf(stderr, "RadioBox: munmap() failed: %s\n", strerror(errno));
				return -1;
			}
			g_rb_fpga_reg_mem = NULL;
		}

		if (g_rb_fpga_mem_fd >= 0) {
			close(g_rb_fpga_mem_fd);
			g_rb_fpga_mem_fd = -1;
		}
	}

	// unmap the RadioBox sub-module
	{
		if (g_hk_fpga_reg_mem) {
			if (munmap(g_hk_fpga_reg_mem, HK_FPGA_BASE_SIZE) < 0) {
				fprintf(stderr, "HouseKeeping: munmap() failed: %s\n", strerror(errno));
				return -1;
			}
			g_hk_fpga_reg_mem = NULL;
		}

		if (g_hk_fpga_mem_fd >= 0) {
			close(g_hk_fpga_mem_fd);
			g_hk_fpga_mem_fd = -1;
		}
	}

	return 0;
}


/*----------------------------------------------------------------------------*/
/**
 * @brief Activates the LEDs on the board.
 *
 * @param[in] doToggle   true: mask defines which LED states to be toggled, leds  is voided. false: mask defines the LED bits to be set, leds  each led is bright when its masked bit is set.
 * @param[in] mask   defines which LED states to be changed.
 * @param[in] leds   defines which masked LEDs should be bright. When toggling this parameter is voided
 * @retval 0 Success
 * @retval -1 FPGA HouseKeeping sub-module not initialized
 */
int hk_fpga_setLeds(unsigned char doToggle, unsigned char mask, unsigned char leds)
{
    if (!g_hk_fpga_reg_mem) {
    	return -1;
    }

    // setting LEDs
    if (doToggle) {
        g_hk_fpga_reg_mem->leds = (g_hk_fpga_reg_mem->leds          ^  mask       );

    } else {
        g_hk_fpga_reg_mem->leds = (g_hk_fpga_reg_mem->leds & ~mask) | (mask & leds);
    }

    return 0;
}


/*----------------------------------------------------------------------------*/
/**
 * @brief Reset write state machine
 *
 * @retval 0 Success, never fails
 */
int rb_fpga_reset(void)
{
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

int rb_fpga_update_params()
{
    return 0;
}
