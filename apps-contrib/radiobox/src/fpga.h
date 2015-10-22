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

#ifndef __FPGA_H
#define __FPGA_H


/** @defgroup fpga_h FPGA memory layout
 * @{
 */

#include "fpga_hk.h"

#include "fpga_rb.h"



/* function declarations, detailed descriptions is in apparent implementation file  */

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
int fpga_init(void);

/**
 * @brief Finalize and release all allocated resources to the FPGA sub-modules
 *
 * Function is intended to be  called at the program termination.
 *
 * @retval 0 Success, never fails
 */
int fpga_exit(void);

/** @} */


#endif /* __FPGA_H */
