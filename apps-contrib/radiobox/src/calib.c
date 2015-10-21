/**
 * @brief Red Pitaya Oscilloscope Calibration Module.
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
#include <errno.h>
#include <string.h>

#include "calib.h"

const char eeprom_device[]="/sys/bus/i2c/devices/0-0050/eeprom";
const int  eeprom_calib_off=0x0008;


/*----------------------------------------------------------------------------*/
/**
 * @brief Read calibration parameters from EEPROM device.
 *
 * Function reads calibration parameters from EEPROM device and stores them to the
 * specified buffer. Communication to the EEPROM device is taken place through
 * appropriate system driver accessed through the file system device
 * /sys/bus/i2c/devices/0-0050/eeprom.
 *
 * @param[out]   calib_params  Pointer to destination buffer.
 * @retval       0 Success
 * @retval      -1 Failure, error message is put on stderr device
 *
 */
int rp_read_calib_params(rp_calib_params_t *calib_params)
{
    FILE   *fp;
    size_t  size;

    /* sanity check */
    if(calib_params == NULL) {
        fprintf(stderr, "rp_read_calib_params(): input structure "
                "not initialized\n");
        return -1;
    }

    /* open eeprom device */
    fp=fopen(eeprom_device, "r");
    if(fp == NULL) {
        fprintf(stderr, "rp_read_calib_params(): Can not open EEPROM device: "
                " %s\n", strerror(errno));
       return -1;
    }

    /* ...and seek to the appropriate storage offset */
    if(fseek(fp, eeprom_calib_off, SEEK_SET) < 0) {
        fclose(fp);
        fprintf(stderr, "rp_read_calib_params(): fseek() failed: %s\n", 
                strerror(errno));
        return -1;
    }

    /* read data from eeprom component and store it to the specified buffer */
    size=fread(calib_params, sizeof(char), sizeof(rp_calib_params_t), fp);
    if(size != sizeof(rp_calib_params_t)) {
        fclose(fp);
        fprintf(stderr, "rp_read_calib_params(): fread() failed, "
                "returned bytes: %d (should be :%d)\n", size, 
                sizeof(rp_calib_params_t));
        return -1;
    }
    fclose(fp);

    return 0;
}


/*----------------------------------------------------------------------------*/
/**
 * @brief Write calibration parameters to EEPROM device.
 *
 * Function writes calibration parameters to EEPROM device. 
 * Communication to the EEPROM device is taken place through
 * appropriate system driver accessed through the file system device
 * /sys/bus/i2c/devices/0-0050/eeprom.
 *
 * @param[out]   calib_params  Pointer to source buffer.
 * @retval       0 Success
 * @retval      -1 Failure, error message is put on stderr device
 *
 */
int rp_write_calib_params(rp_calib_params_t *calib_params)
{
    FILE   *fp;
    size_t  size;

    /* sanity check */
    if(calib_params == NULL) {
        fprintf(stderr, "rp_write_calib_params(): input structure "
                "not initialized\n");
        return -1;
    }

    /* open eeprom device */
    fp=fopen(eeprom_device, "rw+");
    if(fp == NULL) {
        fprintf(stderr, "rp_write_calib_params(): Can not open EEPROM device: "
                " %s\n", strerror(errno));
	fclose(fp);
       return -1;
    }

    /* ...and seek to the appropriate storage offset */
    if(fseek(fp, eeprom_calib_off, SEEK_SET) < 0) {
        fclose(fp);
        fprintf(stderr, "rp_write_calib_params(): fseek() failed: %s\n", 
                strerror(errno));
        return -1;
    }

    /* write data to eeprom component from specified buffer */
    size=fwrite(calib_params, sizeof(char), sizeof(rp_calib_params_t), fp);
    if(size != sizeof(rp_calib_params_t)) {
        fclose(fp);
        fprintf(stderr, "rp_write_calib_params(): fwrite() failed, "
                "returned bytes: %d (should be :%d)\n", size, 
                sizeof(rp_calib_params_t));
        return -1;
    }
    fclose(fp);

    return 0;
}


/*----------------------------------------------------------------------------*/
/**
 * Initialize calibration parameters to default values.
 *
 * @param[out] calib_params  Pointer to target buffer to be initialized.

 * @retval     0             Success, could never fail.
 */
int rp_default_calib_params(rp_calib_params_t *calib_params)
{
	// ADC
    calib_params->fe_ch1_fs_g_hi        =  28101971;	/*  0.006543000 [V] @ 32 bit */				/* one step = 232.83064365e-12 V */
    calib_params->fe_ch2_fs_g_hi        =  28101971;	/*  0.006543000 [V] @ 32 bit */				/* one step = 232.83064365e-12 V */
    calib_params->fe_ch1_fs_g_lo        = 625682246;	/*  0.145678000 [V] @ 32 bit */				/* one step = 232.83064365e-12 V */
    calib_params->fe_ch2_fs_g_lo        = 625682246;	/*  0.145678000 [V] @ 32 bit */				/* one step = 232.83064365e-12 V */
    calib_params->fe_ch1_dc_offs        =       585;	/*  0.023362 HI [V] @ 14 bit */				/* treated as signed value for 14 bit ADC value */
    calib_params->fe_ch2_dc_offs        =       585;	/*  0.520152 LO [V] @ 14 bit */				/* treated as signed value for 14 bit ADC value */

    // DAC
    calib_params->be_ch1_fs             =  42949673;	/*  0.010000000 [V] @ 32 bit */				/* one step = 232.83064365e-12 V */
    calib_params->be_ch2_fs             =  42949673;	/*  0.010000000 [V] @ 32 bit */				/* one step = 232.83064365e-12 V */
    calib_params->be_ch1_dc_offs        =     16044;	/* -0.010376 DAC[V] @ 15 bit */				/* treated as unsigned mid-range value for 15 bit DAC value - assumption: Ref-Voltage = 1.000 V */
    calib_params->be_ch2_dc_offs        =     16044;	/* -0.010376 DAC[V] @ 15 bit */				/* treated as unsigned mid-range value for 15 bit DAC value - assumption: Ref-Voltage = 1.000 V */

    // internals
    calib_params->base_osc125mhz_realhz =   125e+6f;	/* 125 000 000 [Hz] of the DAC clock */

    return 0;
}

/*
 * Information about data representation
 * =====================================
 *
 * max_adc_v = 100.0f * fe_gain_fs     / ((float) (1ULL << 32) * probe_att_fact;  // having probe_att_fact = 1 or 10
 * max_dac_v = 100.0f * be_chX_dc_offs / ((float) (1ULL << 32));
 *
 */


/*----------------------------------------------------------------------------*/
/**
 * @brief Calculates maximum [V] in respect to calibration parameters
 *
 * Function is used to calculate the maximum voltage which can be applied on an ADC input.
 * This calculation is based on the calibrated front-end full scale gain setting and the
 * configured probe attenuation.
 *
 * @param[in] fe_gain_fs     Front End Full Scale Gain
 * @param[in] probe_att      Probe attenuation
 * @retval    float          Maximum voltage, expressed in [V]
 */
float rp_calib_calc_max_v(uint32_t fe_gain_fs, int probe_att)
{
    int probe_att_fact = (probe_att > 0) ?  10 : 1;

    return probe_att_fact * 100.0f * (fe_gain_fs / ((float) (1ULL << 32)));
}
