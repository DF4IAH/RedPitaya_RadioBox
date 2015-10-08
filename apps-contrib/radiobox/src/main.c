/**
 * $Id: main.c 881 2013-12-16 05:37:34Z rp_jmenart $
 *
 * @brief Red Pitaya Oscilloscope main module.
 *
 * @Author Jure Menart <juremenart@gmail.com>
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "version.h"
#include "worker.h"
#include "fpga.h"
#include "calib.h"
#include "cb_ws.h"

#include "main.h"


#ifndef VERSION
# define VERSION "(not set)"
#endif
#ifndef REVISION
# define REVISION "(not set)"
#endif


static int                g_hk_fpga_mem_fd = -1;
static int                g_rb_fpga_mem_fd = -1;
static hk_fpga_reg_mem_t *g_hk_fpga_reg_mem = NULL;
//static rb_fpga_reg_mem_t *g_rb_fpga_reg_mem = NULL;


/* Describe app. parameters with some info/limitations */
pthread_mutex_t rp_main_params_mutex = PTHREAD_MUTEX_INITIALIZER;
static rp_app_params_t rp_main_params[PARAMS_NUM+1] = {
    { /* Oscillator-1 frequency (Hz) */
        "osc1_qrg_i", 0, 1, 0, 0, 125000000 },
    { /* Oscillator-1 amplitude (µV) */
        "osc1_amp_i", 0, 1, 0, 0, 2048000 },
    { /* Oscillator-1 modulation source selector (0: none, 1: VCO2, 2: XADC0) */
        "osc1_modsrc_s", 0, 1, 0, 0, 2 },
    { /* Oscillator-1 modulation type selector (0: AM, 1: FM, 2: PM) */
        "osc1_modtyp_s", 0, 1, 0, 0, 2 },
    { /* Oscillator-2 frequency (Hz) */
        "osc2_qrg_i", 0, 1, 0, 0, 125000000 },
    { /* Oscillator-2 magnitude (AM:%, FM:Hz, PM:°) */
        "osc2_mag_i", 0, 1, 0, 0, 1000000 },

    { /* calculator register A */
        "rb_add_a_i", 0, 1, 0, 0, 1000 },
    { /* calculator register B */
        "rb_add_b_i", 0, 1, 0, 0, 1000 },
    { /* calculator sum result register */
        "rb_add_res_i", 0, 0, 1, 0, 10000 },
    { /* Must be last! */
        NULL, 0.0, -1, -1, 0.0, 0.0 }
};
/* params initialized */
static int params_init = 0;

/* AUTO set algorithm in progress flag */
int auto_in_progress = 0;

rp_calib_params_t rp_main_calib_params;


const char *rp_app_desc(void)
{
    return (const char *)"RedPitaya RadioBox application.\n";
}

int rp_app_init(void)
{
    fprintf(stderr, "Loading radiobox version %s-%s.\n", VERSION, REVISION);

    // Debugging
    set_leds(0, 0xff, 0x01);

    /*
    rp_default_calib_params(&rp_main_calib_params);
    if (rp_read_calib_params(&rp_main_calib_params) < 0) {
        fprintf(stderr, "rp_read_calib_params() failed, using default"
                " parameters\n");
    }
    */

    if (rp_osc_worker_init(&rp_main_params[0], PARAMS_NUM,
                           &rp_main_calib_params) < 0) {
        return -1;
    }

    rp_set_params(&rp_main_params[0], PARAMS_NUM, 0);

    return 0;
}

int rp_app_exit(void)
{
    fprintf(stderr, "Unloading radiobox version %s-%s.\n", VERSION, REVISION);

    rp_osc_worker_exit();

    // Debugging
    set_leds(0, 0xff, 0x00);

    return 0;
}

int time_range_to_time_unit(int range)
{
    int unit = 2;

    switch (range) {
    case 0:
    case 1:
        unit = 0;
        break;
    case 2:
    case 3:
        unit = 1;
        break;
    default:
        unit = 2;
    }

    return unit;
}

int rp_set_params(rp_app_params_t *p, int len, int requesterIsServer)
{
    int i;
    int params_change = 0;
    int fpga_update = 1;

    fprintf(stderr, "rp_set_params: BEGIN\n");
    TRACE("%s()\n", __FUNCTION__);

    // Debugging
    set_leds(1, 0x02, 0);

    if (len > PARAMS_NUM) {
        fprintf(stderr, "Too many parameters: len=%d (max:%d)\n", len, PARAMS_NUM);
        return -1;
    }

    pthread_mutex_lock(&rp_main_params_mutex);
    for (i = 0; i < len || p[i].name != NULL; i++) {
        int p_idx = -1;
        int j = 0;

        /* Search for correct parameter name in defined parameters */
        while (rp_main_params[j].name != NULL) {
            fprintf(stderr, "rp_set_params: next param name = %s\n", p[i].name);

            int p_strlen = strlen(p[i].name);

            if (p_strlen != strlen(rp_main_params[j].name)) {
                j++;
                continue;
            }
            if (!strncmp(p[i].name, rp_main_params[j].name, p_strlen)) {
                p_idx = j;
                break;
            }
            j++;
        }

        if (p_idx == -1) {
            fprintf(stderr, "Parameter %s not found, ignoring it\n", p[i].name);
            continue;
        }

        if (rp_main_params[p_idx].read_only)
            continue;

        if (rp_main_params[p_idx].value != p[i].value) {
            params_change = 1;
            if (rp_main_params[p_idx].fpga_update) {
                fpga_update = 1;
            }
        }

        if (rp_main_params[p_idx].min_val > p[i].value) {
            fprintf(stderr, "Incorrect parameters value: %f (min:%f), "
                    " correcting it\n", p[i].value, rp_main_params[p_idx].min_val);
            p[i].value = rp_main_params[p_idx].min_val;

        } else if (rp_main_params[p_idx].max_val < p[i].value) {
            fprintf(stderr, "Incorrect parameters value: %f (max:%f), "
                    " correcting it\n", p[i].value, rp_main_params[p_idx].max_val);
            p[i].value = rp_main_params[p_idx].max_val;
        }
        rp_main_params[p_idx].value = p[i].value;
    }
    pthread_mutex_unlock(&rp_main_params_mutex);

    // DF4IAH  v
    if (params_change || (params_init == 0)) {
        (void) fpga_update;

        // Debugging
        set_leds(1, 0x04, 0);

        rb_set_fpga(0x00, rp_main_params[RB_ADD_A].value);
        rb_set_fpga(0x04, rp_main_params[RB_ADD_B].value);
        rp_main_params[RB_ADD_RES].value = rb_get_fpga(0x08);
    }
    // DF4IAH  ^

    fprintf(stderr, "rp_set_params: END\n\n");
    return 0;
}

/* Returned vector must be free'd externally! */
int rp_get_params(rp_app_params_t **p)
{
    fprintf(stderr, "rp_get_params: BEGIN\n");

    rp_app_params_t *p_copy = NULL;
    int i;

    // Debugging
    set_leds(1, 0x08, 0);

    p_copy = (rp_app_params_t *)malloc((PARAMS_NUM+1) * sizeof(rp_app_params_t));
    if (p_copy == NULL)
        return -1;

    pthread_mutex_lock(&rp_main_params_mutex);
    for (i = 0; i < PARAMS_NUM; i++) {
        int p_strlen = strlen(rp_main_params[i].name);

        p_copy[i].name = (char *)malloc(p_strlen+1);
        strncpy((char *)&p_copy[i].name[0], &rp_main_params[i].name[0],
                p_strlen);
        p_copy[i].name[p_strlen]='\0';

        p_copy[i].value       = rp_main_params[i].value;
        p_copy[i].fpga_update = rp_main_params[i].fpga_update;
        p_copy[i].read_only   = rp_main_params[i].read_only;
        p_copy[i].min_val     = rp_main_params[i].min_val;
        p_copy[i].max_val     = rp_main_params[i].max_val;
    }
    pthread_mutex_unlock(&rp_main_params_mutex);
    p_copy[PARAMS_NUM].name = NULL;

    *p = p_copy;
    fprintf(stderr, "rp_get_params: END\n\n");
    return PARAMS_NUM;
}

int rp_get_signals(float ***s, int *sig_num, int *sig_len)
{
    int ret_val = 0;

    fprintf(stderr, "rp_get_signals: BEGIN\n");

    // Debugging
    set_leds(1, 0x10, 0);

    if (*s == NULL)
        return -1;

    *sig_num = SIGNALS_NUM;
    *sig_len = SIGNAL_LENGTH;

    fprintf(stderr, "rp_get_signals: END\n\n");
    return ret_val;
}

int rp_create_signals(float ***a_signals)
{
    int i;
    float **s;

    fprintf(stderr, "rp_create_signals: BEGIN\n");

    // Debugging
    set_leds(1, 0x20, 0);

    s = (float **)malloc(SIGNALS_NUM * sizeof(float *));
    if (s == NULL) {
        return -1;
    }

    for (i = 0; i < SIGNALS_NUM; i++)
        s[i] = NULL;

    for (i = 0; i < SIGNALS_NUM; i++) {
        s[i] = (float *)malloc(SIGNAL_LENGTH * sizeof(float));
        if (s[i] == NULL) {
            rp_cleanup_signals(a_signals);
            return -1;
        }
        memset(&s[i][0], 0, SIGNAL_LENGTH * sizeof(float));
    }
    *a_signals = s;

    fprintf(stderr, "rp_create_signals: END\n\n");

    return 0;
}

void rp_cleanup_signals(float ***a_signals)
{
    int i;
    float **s = *a_signals;

    fprintf(stderr, "rp_cleanup_signals: BEGIN\n");

    // Debugging
    set_leds(1, 0x40, 0);

    if (s) {
        for (i = 0; i < SIGNALS_NUM; i++) {
            if (s[i]) {
                free(s[i]);
                s[i] = NULL;
            }
        }
        free(s);
        *a_signals = NULL;
    }

    fprintf(stderr, "rp_cleanup_signals: END\n\n");
}

/*----------------------------------------------------------------------------------*/
/**
 * @brief Make a copy of Application parameters
 *
 * Function copies actual Application parameters to the specified destination
 * buffer. This action was intended to prepare two parameter instances, where the first
 * one can be further modified from the user side, while the second one is processed by
 * the worker thread.
 * In case the destination buffer is not allocated yet, it is allocated internally and must
 * be freed outside of the function scope by calling rp_clean_params() function. Note that
 * if function returns failure, the destination buffer could be partially allocated and must
 * be freed in the same way.
 * If the specified destination buffer is already allocated, it is assumed the number of table
 * entries is the same as in the source table. No special check is made internally if this is really
 * the case.
 *
 * @param[in]   src  Source application parameters
 * @param[out]  dst  Destination application parameters
 * @retval      -1   Failure, error message is output on standard error
 * @retval      0    Successful operation
 */
int rp_copy_params(rp_app_params_t *src, rp_app_params_t **dst)
{
    rp_app_params_t *p_new = *dst;
    int i, num_params;

    /* check arguments */
    if (src == NULL) {
        fprintf(stderr, "Internal error, the source Application parameters are not specified.\n");
        return -1;
    }

    /* check if destination buffer is allocated or not */
    if (p_new == NULL) {
        i = 0;

        /* retrieve the number of source parameters */
        num_params=0;
        while (src[i++].name != NULL)
            num_params++;

        /* allocate array of parameter entries, parameter names must be allocated separately */
        p_new = (rp_app_params_t *)malloc(sizeof(rp_app_params_t) * (num_params+1));
        if (p_new == NULL) {
            fprintf(stderr, "Memory problem, the destination buffer could not be allocated.\n");
            return -1;
        }

        /* scan source parameters, allocate memory space for parameter names and copy values */
        i = 0;
        while (src[i].name != NULL) {
            p_new[i].name = (char *)malloc(strlen(src[i].name)+1);
            if (p_new[i].name == NULL)
                return -1;

            strncpy(p_new[i].name, src[i].name, strlen(src[i].name));
            p_new[i].name[strlen(src[i].name)]='\0';
            p_new[i].value = src[i].value;
            i++;
        }

        /* mark last one */
        p_new[num_params].name = NULL;
        p_new[num_params].value = -1;

    } else {
        /* destination buffer is already allocated, just copy values */
        i = 0;
        while (src[i].name != NULL) {
            p_new[i].value = src[i].value;
            i++;
        }
    }

    *dst = p_new;
    return 0;
}


/*----------------------------------------------------------------------------------*/
/**
 * @brief Deallocate the specified buffer of Application parameters
 *
 * Function is used to deallocate the specified buffers, which were previously
 * allocated by calling rp_copy_params() function.
 *
 * @param[in]   params  Application parameters to be deallocated
 * @retval      0       Success, never fails
 */
int rp_clean_params(rp_app_params_t *params)
{
    int i = 0;
    /* cleanup params structure */
    if (params) {
        while (params[i].name != NULL) {
            if (params[i].name)
                free(params[i].name);
            params[i].name = NULL;
            i++;
        }
        free(params);
        params = NULL;
    }
    return 0;
}

int rp_update_main_params(rp_app_params_t *params)
{
    int i = 0;
    if (params == NULL)
        return -1;

    pthread_mutex_lock(&rp_main_params_mutex);
    while (params[i].name != NULL) {
        rp_main_params[i].value = params[i].value;
        i++;
    }
    pthread_mutex_unlock(&rp_main_params_mutex);
    params_init = 0;
    rp_set_params(&rp_main_params[0], PARAMS_NUM, 1);

    return 0;
}


void write_cal_eeprom( void)
{
    if (rp_write_calib_params(&rp_main_calib_params) < 0) {
        fprintf(stderr, "rp_write_calib_params() failed. \n");
    }
}


void set_leds(unsigned char doToggle, unsigned char mask, unsigned char leds)
{
    // constructor - part
    g_hk_fpga_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (g_hk_fpga_mem_fd < 0) {
        fprintf(stderr, "open(/dev/mem) failed: %s\n", strerror(errno));
        return;  // -1;
    }

    const long page_size = sysconf(_SC_PAGESIZE);
    long page_addr = HK_FPGA_BASE_ADDR & (~(page_size-1));
    long page_off  = HK_FPGA_BASE_ADDR - page_addr;

    void* page_ptr = mmap(NULL, HK_FPGA_BASE_SIZE, PROT_READ | PROT_WRITE,
                          MAP_SHARED, g_hk_fpga_mem_fd, page_addr);
    if ((void *)page_ptr == MAP_FAILED) {
        fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
        // call destructor     __osc_fpga_cleanup_mem();
        return;  // -1;
    }
    g_hk_fpga_reg_mem = page_ptr + page_off;

    // setting LEDs
    if (doToggle) {
        g_hk_fpga_reg_mem->leds = (g_hk_fpga_reg_mem->leds          ^ (mask & 0xfe));
    } else {
        g_hk_fpga_reg_mem->leds = (g_hk_fpga_reg_mem->leds & ~mask) | (mask & 0xfe & leds);
    }

    // destructor - part
    if (munmap(g_hk_fpga_reg_mem, HK_FPGA_BASE_SIZE) < 0) {
        fprintf(stderr, "munmap() failed: %s\n", strerror(errno));
        return;
    }
    g_hk_fpga_reg_mem = NULL;

    if (g_hk_fpga_mem_fd >= 0) {
        close(g_hk_fpga_mem_fd);
        g_hk_fpga_mem_fd = -1;
    }

}

void rb_set_fpga(unsigned int base_offs, unsigned int value)
{
    // constructor - part
    g_rb_fpga_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (g_rb_fpga_mem_fd < 0) {
        fprintf(stderr, "open(/dev/mem) failed: %s\n", strerror(errno));
        return;  // -1;
    }

    const long page_size = sysconf(_SC_PAGESIZE);
    long page_addr = RB_FPGA_BASE_ADDR & (~(page_size-1));
    long page_off  = RB_FPGA_BASE_ADDR - page_addr;

    void* page_ptr = mmap(NULL, RB_FPGA_BASE_SIZE, PROT_READ | PROT_WRITE,
                          MAP_SHARED, g_rb_fpga_mem_fd, page_addr);
    if ((void *)page_ptr == MAP_FAILED) {
        fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
        // call destructor     __rb_fpga_cleanup_mem();
        return;  // -1;
    }
    //g_rb_fpga_reg_mem = page_ptr + page_off;
    unsigned int* data_ptr = (unsigned int*) (page_ptr + page_off + base_offs);

    // setting value to offset address
    //*(g_rb_fpga_reg_mem + base_offs) = value;
    *data_ptr = value;

    // destructor - part
    if (munmap(page_ptr, RB_FPGA_BASE_SIZE) < 0) {
        fprintf(stderr, "munmap() failed: %s\n", strerror(errno));
        return;
    }
    page_ptr = NULL;

    if (g_rb_fpga_mem_fd >= 0) {
        close(g_rb_fpga_mem_fd);
        g_rb_fpga_mem_fd = -1;
    }
}


unsigned int rb_get_fpga(unsigned int base_offs)
{
    unsigned int retval = 0;

    // constructor - part
    g_rb_fpga_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (g_rb_fpga_mem_fd < 0) {
        fprintf(stderr, "open(/dev/mem) failed: %s\n", strerror(errno));
        return 0;
    }

    const long page_size = sysconf(_SC_PAGESIZE);
    long page_addr = RB_FPGA_BASE_ADDR & (~(page_size-1));
    long page_off  = RB_FPGA_BASE_ADDR - page_addr;

    void* page_ptr = mmap(NULL, RB_FPGA_BASE_SIZE, PROT_READ | PROT_WRITE,
                          MAP_SHARED, g_rb_fpga_mem_fd, page_addr);
    if ((void *)page_ptr == MAP_FAILED) {
        fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
        // call destructor     __rb_fpga_cleanup_mem();
        return 0;
    }
    //g_rb_fpga_reg_mem = page_ptr + page_off;
    unsigned int* data_ptr = (unsigned int*) (page_ptr + page_off + base_offs);

    // getting value from offset address
    //retval = *(g_rb_fpga_reg_mem + base_offs);
    retval = *data_ptr;

    // destructor - part
    if (munmap(page_ptr, RB_FPGA_BASE_SIZE) < 0) {
        fprintf(stderr, "munmap() failed: %s\n", strerror(errno));
        return 0;
    }
    page_ptr = NULL;

    if (g_rb_fpga_mem_fd >= 0) {
        close(g_rb_fpga_mem_fd);
        g_rb_fpga_mem_fd = -1;
    }

    return retval;
}
