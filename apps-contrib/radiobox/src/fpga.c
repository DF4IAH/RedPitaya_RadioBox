/**
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
int fpga_exit(void)
{
	// exit access to House-keeping sub-module
	fpga_hk_exit();

	// exit access to RadioBox sub-module
	fpga_rb_exit();

	return 0;
}
