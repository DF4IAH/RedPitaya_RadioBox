/**
 * $Id: fpga.c 2015-10-18 20:50:00Z DF4IAH $
 *
 * @brief Red Pitaya FPGA Interface for its sub-modules.
 *
 * @author Ulrich Habel (DF4IAH) <espero7757@gmx.net>
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#include "fpga.h"


/*----------------------------------------------------------------------------*/
/**
 * @brief Initialize interface to the FPGA sub-modules
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
	int state = 0;
	int ret;

    // init the HouseKeeping FPGA sub-module
	ret = fpga_hk_init();
	if (ret) {
		state = ret;
	}

    // init the RadioBox FPGA sub-module
	ret = fpga_rb_init();
	if (ret) {
		state = ret;
	}

    return state;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Finalize and release all allocated resources to the FPGA sub-modules
 *
 * Function is intended to be  called at the program termination.
 *
 * @retval 0 Success, never fails
 */
int fpga_exit(void)
{
	// exit access to House-keeping sub-module
	fpga_hk_exit();

	// exit access to RadioBox sub-module
	fpga_rb_exit();

	return 0;
}
