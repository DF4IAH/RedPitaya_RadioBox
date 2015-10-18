/**
 * $Id: fpga.h 2015-10-18 20:50:00Z DF4IAH $
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

#ifndef __FPGA_H
#define __FPGA_H


/** @defgroup fpga_h FPGA memory layout
 * @{
 */

#include "fpga_hk.h"

#include "fpga_rb.h"

/** @} */


/* function declarations, detailed descriptions is in apparent implementation file  */

int fpga_init(void);
int fpga_exit(void);


#endif /* __FPGA_H */
