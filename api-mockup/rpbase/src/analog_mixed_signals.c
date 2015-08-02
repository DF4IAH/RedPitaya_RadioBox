/**
 * $Id: $
 *
 * @brief Red Pitaya library Analog Mixed Signals (AMS) module interface
 *
 * @Author Red Pitaya
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */


#include <stdio.h>

#include "common.h"
#include "analog_mixed_signals.h"


// Base Analog Mixed Signals address
static const int ANALOG_MIXED_SIGNALS_BASE_ADDR = 0x40400000;
static const int ANALOG_MIXED_SIGNALS_BASE_SIZE = 0x30;

typedef struct analog_mixed_signals_control_s {
    uint32_t aif0;
    uint32_t aif1;
    uint32_t aif2;
    uint32_t aif3;
    uint32_t reserved[4];
    uint32_t dac0;
    uint32_t dac1;
    uint32_t dac2;
    uint32_t dac3;
} analog_mixed_signals_control_t;


static const uint32_t ANALOG_OUT_MASK            = 0xFF;
static const uint32_t ANALOG_OUT_BITS            = 16;
static const uint32_t ANALOG_IN_MASK             = 0xFFF;

static const float ANALOG_IN_MAX_VAL             = 3.5;
static const float ANALOG_IN_MIN_VAL             = 0.0;
static const uint32_t ANALOG_IN_MAX_VAL_INTEGER  = 0x7FF;
static const float ANALOG_OUT_MAX_VAL            = 1.8;
static const float ANALOG_OUT_MIN_VAL            = 0.0;
static const uint32_t ANALOG_OUT_MAX_VAL_INTEGER = 156;


static volatile analog_mixed_signals_control_t *ams = NULL;


int ams_Init()
{
//    ECHECK(cmn_Init());
    ECHECK(cmn_Map(ANALOG_MIXED_SIGNALS_BASE_SIZE, ANALOG_MIXED_SIGNALS_BASE_ADDR, (void**)&ams));
    return RP_OK;
}

int ams_Release()
{
    ECHECK(cmn_Unmap(ANALOG_MIXED_SIGNALS_BASE_SIZE, (void**)&ams));
//    ECHECK(cmn_Release());
    return RP_OK;
}

int ams_SetValueDAC0(uint32_t value)
{
    if (value > ANALOG_OUT_MAX_VAL_INTEGER) {
        return RP_EOOR;
    }
    return cmn_SetShiftedValue(&ams->dac0, value, ANALOG_OUT_MASK, ANALOG_OUT_BITS);
    return RP_OK;
}

int ams_SetValueDAC1(uint32_t value)
{
    if (value > ANALOG_OUT_MAX_VAL_INTEGER) {
        return RP_EOOR;
    }
    return cmn_SetShiftedValue(&ams->dac1, value, ANALOG_OUT_MASK, ANALOG_OUT_BITS);
    return RP_OK;
}

int ams_SetValueDAC2(uint32_t value)
{
    if (value > ANALOG_OUT_MAX_VAL_INTEGER) {
        return RP_EOOR;
    }
    return cmn_SetShiftedValue(&ams->dac2, value, ANALOG_OUT_MASK, ANALOG_OUT_BITS);
    return RP_OK;
}

int ams_SetValueDAC3(uint32_t value)
{
    if (value > ANALOG_OUT_MAX_VAL_INTEGER) {
        return RP_EOOR;
    }
    return cmn_SetShiftedValue(&ams->dac3, value, ANALOG_OUT_MASK, ANALOG_OUT_BITS);
    return RP_OK;
}

int ams_GetValueADC0(uint32_t* value)
{
    file fp;
    fopen (fp, "/sys/devices/soc0/amba_pl/83c00000.xadc_wiz/", 'r');
    int r=fscanf (fp, "%d", value);
    return r;
}

int ams_GetValueADC1(uint32_t* value)
{
    int r=cmn_GetValue(&ams->aif1, value, ANALOG_IN_MASK);
    if(*value>ANALOG_IN_MAX_VAL_INTEGER){
    	*value=0;
    }
    return r;
}

int ams_GetValueADC2(uint32_t* value)
{
    int r=cmn_GetValue(&ams->aif2, value, ANALOG_IN_MASK);
    if(*value>ANALOG_IN_MAX_VAL_INTEGER){
    	*value=0;
    }
    return r;
}

int ams_GetValueADC3(uint32_t* value)
{
    int r=cmn_GetValue(&ams->aif3, value, ANALOG_IN_MASK);
    if(*value>ANALOG_IN_MAX_VAL_INTEGER){
    	*value=0;
    }
    return r;
}

int ams_GetValueDAC0(uint32_t* value)
{
    return cmn_GetShiftedValue(&ams->dac0, value, ANALOG_IN_MASK, ANALOG_OUT_BITS);
}

int ams_GetValueDAC1(uint32_t* value)
{
    return cmn_GetShiftedValue(&ams->dac1, value, ANALOG_IN_MASK, ANALOG_OUT_BITS);
}

int ams_GetValueDAC2(uint32_t* value)
{
    return cmn_GetShiftedValue(&ams->dac2, value, ANALOG_IN_MASK, ANALOG_OUT_BITS);
}

int ams_GetValueDAC3(uint32_t* value)
{
    return cmn_GetShiftedValue(&ams->dac3, value, ANALOG_IN_MASK, ANALOG_OUT_BITS);
}

int ams_GetRangeInput(float *min_val, float *max_val, uint32_t *int_max_val) {
    *min_val = ANALOG_IN_MIN_VAL;
    *max_val = ANALOG_IN_MAX_VAL;
    *int_max_val = ANALOG_IN_MAX_VAL_INTEGER;
    return RP_OK;
}

int ams_GetRangeOutput(float *min_val, float *max_val, uint32_t *int_max_val) {
    *min_val = ANALOG_OUT_MIN_VAL;
    *max_val = ANALOG_OUT_MAX_VAL;
    *int_max_val = ANALOG_OUT_MAX_VAL_INTEGER;
    return RP_OK;
}
