/**
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
#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "calib.h"
#include "fpga.h"
#include "cb_http.h"


/** @brief calibration data layout within the EEPROM device */
extern rp_calib_params_t    rp_main_calib_params;

/** @brief CallBack copy of params from the worker when requested */
extern rp_app_params_t*     rp_cb_out_params;
/** @brief Holds mutex to access on parameters from the worker thread to any other context */
extern pthread_mutex_t      rp_cb_out_params_mutex;

/** @brief The RadioBox memory file descriptor used to mmap() the FPGA space. */
extern int                  g_fpga_rb_mem_fd;
/** @brief The RadioBox memory layout of the FPGA registers. */
extern fpga_rb_reg_mem_t*   g_fpga_rb_reg_mem;


/*----------------------------------------------------------------------------*/
int fpga_rb_init(void)
{
    fprintf(stderr, "fpga_rb_init: BEGIN\n");

    /* make sure all previous data is vanished */
    fpga_rb_exit();

    /* init the RadioBox FPGA sub-module access */
    {
        const long page_size = sysconf(_SC_PAGESIZE);

        g_fpga_rb_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
        if (g_fpga_rb_mem_fd < 0) {
            fprintf(stderr, "ERROR - fpga_rb_init: open(/dev/mem) failed: %s\n", strerror(errno));
            fpga_exit();
            return -1;
        }
        long page_addr = FPGA_RB_BASE_ADDR & (~(page_size-1));
        long page_offs = FPGA_RB_BASE_ADDR - page_addr;

        void* page_ptr = mmap(NULL, FPGA_RB_BASE_SIZE, PROT_READ | PROT_WRITE,
                              MAP_SHARED, g_fpga_rb_mem_fd, page_addr);
        if (page_ptr == MAP_FAILED) {
            fprintf(stderr, "ERROR - fpga_rb_init: mmap() failed: %s\n", strerror(errno));
            fpga_exit();
            return -1;
        }
        g_fpga_rb_reg_mem = (fpga_rb_reg_mem_t*) page_ptr + page_offs;
    }

    // enable RadioBox sub-module
    fpga_rb_enable(1);

    fprintf(stderr, "fpga_rb_init: END\n");
    return 0;
}

/*----------------------------------------------------------------------------*/
int fpga_rb_exit(void)
{
    fprintf(stderr, "fpga_rb_exit: BEGIN\n");

    /* disable RadioBox sub-module */
    fpga_rb_enable(0);

    /* unmap the RadioBox sub-module */
    {
        if (g_fpga_rb_reg_mem) {
            if (munmap(g_fpga_rb_reg_mem, FPGA_RB_BASE_SIZE) < 0) {
                fprintf(stderr, "ERROR - fpga_rb_exit: munmap() failed: %s\n", strerror(errno));
                return -1;
            }
            g_fpga_rb_reg_mem = NULL;
        }

        if (g_fpga_rb_mem_fd >= 0) {
            close(g_fpga_rb_mem_fd);
            g_fpga_rb_mem_fd = -1;
        }
    }
    fprintf(stderr, "fpga_rb_exit: END\n");
    return 0;
}


/*----------------------------------------------------------------------------*/
int fpga_rb_update_all_params(rp_app_params_t* p)
{
    int   loc_rb_run   = 0;
    int   loc_modsrc   = 0;
    int   loc_modtyp   = 0;
    float loc_osc1_qrg = 0.0f;
    float loc_osc2_qrg = 0.0f;
    float loc_osc1_amp = 0.0f;
    float loc_osc2_mag = 0.0f;

    fprintf(stderr, "fpga_rb_update_all_params: BEGIN\n");

    if (!g_fpga_rb_reg_mem || !p) {
        fprintf(stderr, "ERROR - fpga_rb_update_all_params: bad parameter (p=%p) or not init'ed(g=%p)\n", p, g_fpga_rb_reg_mem);
        return -1;
    }

    int idx = 0;
    while (1) {
        if (!(p[idx].name)) {
            break;  // end of list
        }

        if (!(p[idx].fpga_update & 0x80)) {  // MARKer set?
            fprintf(stderr, "INFO - fpga_rb_update_all_params: skipped not modified parameter (name=%s)\n", p[idx].name);
            idx++;
            continue;  // this value is not marked to update the FPGA
        }
        fprintf(stderr, "INFO - fpga_rb_update_all_params: this parameter has to update the FPGA (name=%s)\n", p[idx].name);

        /* Remove the marker */
        p[idx].fpga_update &= ~0x80;


        /* Get current parameters from the worker */
        {
            fprintf(stderr, "INFO - fpga_rb_update_all_params: waiting for cb_out_params ...\n");
            pthread_mutex_lock(&rp_cb_out_params_mutex);
            if (rp_cb_out_params) {
                loc_rb_run   = (int) rp_cb_out_params[RB_RUN].value;
                loc_modsrc   = (int) rp_cb_out_params[RB_OSC1_MODSRC].value;
                loc_modtyp   = (int) rp_cb_out_params[RB_OSC1_MODTYP].value;
                loc_osc1_qrg = rp_cb_out_params[RB_OSC1_QRG].value;
                loc_osc2_qrg = rp_cb_out_params[RB_OSC2_QRG].value;
                loc_osc1_amp = rp_cb_out_params[RB_OSC1_AMP].value;
                loc_osc2_mag = rp_cb_out_params[RB_OSC2_MAG].value;
            }
            pthread_mutex_unlock(&rp_cb_out_params_mutex);
            fprintf(stderr, "INFO - fpga_rb_update_all_params: ... done\n");
        }

        /* Since here process on each known parameter accordingly */

        if (!strcmp("rb_run", p[idx].name)) {
            fprintf(stderr, "INFO - fpga_rb_update_all_params: #got rb_run = %d\n", (int) (p[idx].value));
            fpga_rb_enable((int) (p[idx].value));
               fpga_rb_set_ctrl((int) p[idx].value, loc_modsrc, loc_modtyp, loc_osc1_qrg, loc_osc2_qrg, loc_osc1_amp, loc_osc2_mag);

        } else if (!strcmp("osc1_modsrc_s", p[idx].name)) {
            fprintf(stderr, "INFO - fpga_rb_update_all_params: #got osc1_modsrc_s = %d\n", (int) (p[idx].value));
            fpga_rb_set_ctrl(loc_rb_run, (int) (p[idx].value), loc_modtyp, loc_osc1_qrg, loc_osc2_qrg, loc_osc1_amp, loc_osc2_mag);

        } else if (!strcmp("osc1_modtyp_s", p[idx].name)) {
            fprintf(stderr, "INFO - fpga_rb_update_all_params: #got osc1_modtyp_s = %d\n", (int) (p[idx].value));
            fpga_rb_set_ctrl(loc_rb_run, loc_modsrc, (int) (p[idx].value), loc_osc1_qrg, loc_osc2_qrg, loc_osc1_amp, loc_osc2_mag);

        } else if (!strcmp("osc1_qrg_imil", p[idx].name)) {
            fprintf(stderr, "INFO - fpga_rb_update_all_params: #got osc1_qrg_imil = %f\n", p[idx].value);
            fpga_rb_set_ctrl(loc_rb_run, loc_modsrc, loc_modtyp, p[idx].value, loc_osc2_qrg, loc_osc1_amp, loc_osc2_mag);

        } else if (!strcmp("osc2_qrg_imil", p[idx].name)) {
            fprintf(stderr, "INFO - fpga_rb_update_all_params: #got osc2_qrg_imil = %f\n", p[idx].value);
            fpga_rb_set_ctrl(loc_rb_run, loc_modsrc, loc_modtyp, loc_osc1_qrg, p[idx].value, loc_osc1_amp, loc_osc2_mag);

        } else if (!strcmp("osc1_amp_imil", p[idx].name)) {
            fprintf(stderr, "INFO - fpga_rb_update_all_params: #got osc1_amp_imil = %f\n", p[idx].value);
            fpga_rb_set_ctrl(loc_rb_run, loc_modsrc, loc_modtyp, loc_osc1_qrg, loc_osc2_qrg, p[idx].value, loc_osc2_mag);

        } else if (!strcmp("osc2_mag_imil", p[idx].name)) {
            fprintf(stderr, "INFO - fpga_rb_update_all_params: #got osc2_mag_imil = %f\n", p[idx].value);
            fpga_rb_set_ctrl(loc_rb_run, loc_modsrc, loc_modtyp, loc_osc1_qrg, loc_osc2_qrg, loc_osc1_amp, p[idx].value);
        }  // else if ()

        idx++;
    }  // while (1)

    fprintf(stderr, "fpga_rb_update_all_params: END\n");
    return 0;
}


/*----------------------------------------------------------------------------*/
void fpga_rb_set_ctrl(int rb_run, int modsrc, int modtyp, float osc1_qrg_mil, float osc2_qrg_mil, float osc1_amp_mil, float osc2_mag_mil)
{
	float osc1_qrg = osc1_qrg_mil / 1000.0f;
	float osc2_qrg = osc2_qrg_mil / 1000.0f;
	float osc1_amp = osc1_amp_mil / 1000.0f;
	float osc2_mag = osc2_mag_mil / 1000.0f;

    if (rb_run) {
        fpga_rb_set_osc1_mod_none_am_pm(osc1_qrg);                                                      // OSC1 frequency
        fpga_rb_set_osc2_mod_am_fm_pm(osc2_qrg);                                                        // OSC2 frequency
        fpga_rb_set_osc1_mixer_mod_none_fm_pm(osc1_amp);                                                // OSC1 mixer

        switch (modsrc) {

        default:
        case RB_MODSRC_NONE: {
            g_fpga_rb_reg_mem->ctrl &= ~0x000000e0;                                                     // control: turn off all streams into OSC1 and OSC1 mixer
        }
        break;

        case RB_MODSRC_OSC2: {
            switch (modtyp) {

            case RB_MODTYP_AM: {
                fprintf(stderr, "INFO - fpga_rb_set_ctrl: setting FPGA for AM\n");

                g_fpga_rb_reg_mem->ctrl &= ~0x00000060;                                                 // control: turn off all other streams
                g_fpga_rb_reg_mem->ctrl |=  0x00000080;                                                 // control: AM by OSC1 mixer amplitude streaming
                fpga_rb_set_osc2_mixer_mod_am(osc1_amp, osc2_mag);                                      // AM by streaming in amplitude
            }
            break;

            case RB_MODTYP_FM: {
                fprintf(stderr, "INFO - fpga_rb_set_ctrl: setting FPGA for FM\n");

                g_fpga_rb_reg_mem->ctrl &= ~0x000000c0;                                                 // control: turn off all other streams
                g_fpga_rb_reg_mem->ctrl |=  0x00000020;                                                 // control: FM by OSC1 increment streaming
                fpga_rb_set_osc2_mixer_mod_fm(osc1_qrg, osc2_mag);                                      // FM by streaming in DDS increment
            }
            break;

            case RB_MODTYP_PM: {
                fprintf(stderr, "INFO - fpga_rb_set_ctrl: setting FPGA for PM\n");

                g_fpga_rb_reg_mem->ctrl &= ~0x000000a0;                                                 // control: turn off all other streams
                g_fpga_rb_reg_mem->ctrl |=  0x00000040;                                                 // control: PM by OSC1 offset streaming
                fpga_rb_set_osc2_mixer_mod_pm(osc1_qrg, osc2_mag);                                      // PM by streaming in DDS phase offset
            }
            break;

            }
        }
        break;

        case RB_MODSRC_RF_IN1: {
            // TODO to be defined
        }
        break;

        }  // switch ()

    } else {
        fpga_rb_set_osc1_mod_none_am_pm(0.0f);                                                          // OSC1 frequency
        fpga_rb_set_osc2_mod_am_fm_pm(0.0f);                                                            // OSC2 frequency
        fpga_rb_set_osc1_mixer_mod_none_fm_pm(0.0f);                                                    // OSC1 mixer
        fpga_rb_set_osc2_mixer_mod_fm(0.0f, 0.0f);                                                      // OSC2 mixer

        g_fpga_rb_reg_mem->ctrl &= ~0x000000e0;                                                         // control: turn off all streams into OSC1 and OSC1 mixer
    }
}

/*----------------------------------------------------------------------------*/
void fpga_rb_set_osc1_mod_none_am_pm(float osc1_qrg)
{
    double qrg1 = 0.5 + ((double) (1ULL << 48)) * (osc1_qrg / rp_main_calib_params.base_osc125mhz_realhz);

    g_fpga_rb_reg_mem->osc1_inc_lo = (uint32_t) (((uint64_t) qrg1) & 0xffffffff);
    g_fpga_rb_reg_mem->osc1_inc_hi = (uint32_t) (((uint64_t) qrg1) >> 32);
    g_fpga_rb_reg_mem->osc1_ofs_lo = 0;                                                                 // no carrier phase offset
    g_fpga_rb_reg_mem->osc1_ofs_hi = 0;                                                                 // no carrier phase offset
}

/*----------------------------------------------------------------------------*/
void fpga_rb_set_osc1_mixer_mod_none_fm_pm(float osc1_amp)
{
    double gain1 = 0.5 + ((double) (1ULL << 31)) * (osc1_amp / 2048.0);                                 // TODO: DAC amplitude correction goes into here
    double ofs1  = 0.0f;                                                                                // TODO: DAC offset correction goes into here
//  double ofs1  = (1ULL << 46);                                                                        // TODO: DAC offset correction goes into here

    g_fpga_rb_reg_mem->osc1_mix_gain = (uint32_t) (((uint64_t) gain1) & 0xffffffff);
    g_fpga_rb_reg_mem->osc1_mix_ofs_lo = (uint32_t) (((uint64_t) ofs1)  & 0xffffffff);
    g_fpga_rb_reg_mem->osc1_mix_ofs_hi = (uint32_t) (((uint64_t) ofs1) >> 32);
}

/*----------------------------------------------------------------------------*/
void fpga_rb_set_osc2_mod_am_fm_pm(float osc2_qrg)
{
    double qrg2 = 0.5 + ((double) (1ULL << 48)) * (osc2_qrg / rp_main_calib_params.base_osc125mhz_realhz);

    g_fpga_rb_reg_mem->osc2_inc_lo = (uint32_t) (((uint64_t) qrg2) & 0xffffffff);
    g_fpga_rb_reg_mem->osc2_inc_hi = (uint32_t) (((uint64_t) qrg2) >> 32);
    g_fpga_rb_reg_mem->osc2_ofs_lo = 0;                                                                 // no carrier phase offset
    g_fpga_rb_reg_mem->osc2_ofs_hi = 0;                                                                 // no carrier phase offset
}

/*----------------------------------------------------------------------------*/
void fpga_rb_set_osc2_mixer_mod_am(float osc1_amp, float osc2_mag)
{
    double gain2 = 0.5 +  ((double) (1ULL << 30))                           * (osc2_mag / 100.0)  * (osc1_amp / 2048.0);
    double ofs2  = 0.5 + (((double) (1ULL << 47)) - ((double) (1ULL << 46)) * (osc2_mag / 100.0)) * (osc1_amp / 2048.0);

    g_fpga_rb_reg_mem->osc2_mix_gain   = (uint32_t) (((uint64_t) gain2) & 0xffffffff);
    g_fpga_rb_reg_mem->osc2_mix_ofs_lo = (uint32_t) (((uint64_t) ofs2)  & 0xffffffff);
    g_fpga_rb_reg_mem->osc2_mix_ofs_hi = (uint32_t) (((uint64_t) ofs2) >> 32);
}

/*----------------------------------------------------------------------------*/
void fpga_rb_set_osc2_mixer_mod_fm(float osc1_qrg, float osc2_mag)
{
    double gain2 = 0.5 + ((double) (1ULL << 32)) * (osc2_mag / rp_main_calib_params.base_osc125mhz_realhz);
    double ofs2  = 0.5 + ((double) (1ULL << 48)) * (osc1_qrg / rp_main_calib_params.base_osc125mhz_realhz);

    g_fpga_rb_reg_mem->osc2_mix_gain   = (uint32_t) (((uint64_t) gain2) & 0xffffffff);
    g_fpga_rb_reg_mem->osc2_mix_ofs_lo = (uint32_t) (((uint64_t) ofs2)  & 0xffffffff);
    g_fpga_rb_reg_mem->osc2_mix_ofs_hi = (uint32_t) (((uint64_t) ofs2) >> 32);
}

/*----------------------------------------------------------------------------*/
void fpga_rb_set_osc2_mixer_mod_pm(float osc1_qrg, float osc2_mag)
{
    double gain2 = 0.5 + ((double) (1ULL << 31)) * (osc2_mag / 360.0);

    g_fpga_rb_reg_mem->osc2_mix_gain   = (uint32_t) (((uint64_t) gain2) & 0xffffffff);
    g_fpga_rb_reg_mem->osc2_mix_ofs_lo = (uint32_t) 0;
    g_fpga_rb_reg_mem->osc2_mix_ofs_hi = (uint32_t) 0;
}


/*----------------------------------------------------------------------------*/
void fpga_rb_enable(int enable)
{
    if (!g_fpga_rb_reg_mem) {
        return;
    }

    fprintf(stderr, "fpga_rb_enable(%d): BEGIN\n", enable);

    if (enable) {
        // enable RadioBox
        g_fpga_rb_reg_mem->ctrl        = 0x00000001;    // enable RB sub-module
        fpga_rb_reset();
#if 1
        g_fpga_rb_reg_mem->led_ctrl    = 0x00000002;    // show OSC1 mixer output at RB LEDs
#else
        g_fpga_rb_reg_mem->led_ctrl    = 0x00000003;    // show OSC1 output at RB LEDs
#endif

    } else {
        fprintf(stderr, "fpga_rb_enable: turning off RB LEDs\n");
        g_fpga_rb_reg_mem->led_ctrl    = 0x00000000;    // disable RB LEDs

        // disable RadioBox
        fprintf(stderr, "fpga_rb_enable: disabling RB sub-module\n");
        g_fpga_rb_reg_mem->ctrl        = 0x00000000;    // disable RB sub-module
    }

    fprintf(stderr, "fpga_rb_enable(%d): END\n", enable);
}

/*----------------------------------------------------------------------------*/
void fpga_rb_reset(void)
{
    if (!g_fpga_rb_reg_mem) {
        return;
    }

    // send resync to OSC1 and OSC2
    g_fpga_rb_reg_mem->ctrl = 0x00001011;

    // send resync and reset to OSC1 and OSC2
    g_fpga_rb_reg_mem->ctrl = 0x00001017;

    // send resync to OSC1 and OSC2
    g_fpga_rb_reg_mem->ctrl = 0x00001011;

    // run mode of both oscillators
    g_fpga_rb_reg_mem->ctrl = 0x00000001;
}


#if 0
/*----------------------------------------------------------------------------*/
/**
 * @brief Reads value from the specific RadioBox sub-module register
 *
 * @param[in] rb_reg_ofs  offset value for the RadioBox base address to be written to.
 *
 * @retval  value of the specified register.
 */
uint32_t fpga_rb_read_register(unsigned int rb_reg_ofs)
{
    fprintf(stderr, "fpga_rb_read_register: BEGIN\n");
    if (!g_fpga_rb_reg_mem) {
        return -1;
    }

    uint32_t value = *((uint32_t*) ((void*) g_fpga_rb_reg_mem) + rb_reg_ofs);
    fprintf(stderr, "fpga_rb_read_register: ofs=0x%06x --> read=0x%08x\n", rb_reg_ofs, value);
    fprintf(stderr, "fpga_rb_read_register: END\n");
    return value;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Writes value to the specific RadioBox sub-module register
 *
 * @param[in] rb_reg_ofs  offset value for the RadioBox base address to be written to.
 * @param[in] value  value that is written to the specified register.
 *
 * @retval  0 Success
 * @retval -1 Failure, error message is output on standard error device
 */
int fpga_rb_write_register(unsigned int rb_reg_ofs, uint32_t value)
{
    fprintf(stderr, "fpga_rb_write_register: BEGIN\n");

    if (!g_fpga_rb_reg_mem) {
        return -1;
    }

    //fprintf(stderr, "INFO fpga_rb_write_register: Compare LED access: %p, calced=%p\n", &(g_fpga_rb_reg_mem->led_ctrl), ((void*) g_fpga_rb_reg_mem) + rb_reg_ofs);

    fprintf(stderr, "fpga_rb_write_register: ofs=0x%06x <-- write=0x%08x\n", rb_reg_ofs, value);
    *((uint32_t*) ((void*) g_fpga_rb_reg_mem) + rb_reg_ofs) = value;

    fprintf(stderr, "fpga_rb_write_register: END\n");
    return 0;
}
#endif
