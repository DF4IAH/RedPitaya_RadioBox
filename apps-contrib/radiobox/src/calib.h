/**
 * @brief Red Pitaya RadioBox Calibration Module.
 *
 * @author Ulrich Habel (DF4IAH) <espero7757@gmx.net>
 *         
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#ifndef __CALIB_H
#define __CALIB_H

#include <stdint.h>


/** @defgroup calib_h Calibration data
 * @{
 */

/** @brief  Calibration parameters stored in the EEPROM device
 */
typedef struct rp_calib_params_s {

	/** @brief High gain front end full scale voltage, channel 1 */
	uint32_t fe_ch1_fs_g_hi;

	/** @brief High gain front end full scale voltage, channel 2 */
	uint32_t fe_ch2_fs_g_hi;

	/** @brief Low gain front end full scale voltage, channel 1  */
	uint32_t fe_ch1_fs_g_lo;

	/** @brief Low gain front end full scale voltage, channel 2  */
	uint32_t fe_ch2_fs_g_lo;

	/** @brief Front end DC offset, channel 1  */
	int32_t  fe_ch1_dc_offs;

	/** @brief Front end DC offset, channel 2  */
	int32_t  fe_ch2_dc_offs;

	/** @brief Back end full scale voltage, channel 1  */
	uint32_t be_ch1_fs;

	/** @brief Back end full scale voltage, channel 2  */
	uint32_t be_ch2_fs;

	/** @brief Back end DC offset, channel 1 */
	int32_t  be_ch1_dc_offs;

	/** @brief Back end DC offset, on channel 2 */
	int32_t  be_ch2_dc_offs;

	/** @brief Base attributes: real frequency of the 125 MHz ADC clock in Hz */
	float	 base_osc125mhz_realhz;

} rp_calib_params_t;


int rp_read_calib_params(rp_calib_params_t* calib_params);
int rp_write_calib_params(rp_calib_params_t* calib_params);
int rp_default_calib_params(rp_calib_params_t* calib_params);

float rp_calib_calc_max_v(uint32_t fe_gain_fs, int probe_att);

/** @} */


#endif //__CALIB_H
