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
//#include <math.h>
#include <errno.h>
#include <pthread.h>
//#include <sys/types.h>
#include <sys/mman.h>

#include "version.h"
#include "worker.h"
#include "fpga.h"
#include "calib.h"
#include "cb_http.h"
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

//extern pthread_mutex_t rp_main_params_mutex;
//extern rp_app_params_t rp_main_params[];
//extern rp_calib_params_t rp_main_calib_params;
//extern int params_init;


const char *rp_app_desc(void)
{
    return (const char *)"RedPitaya RadioBox application.\n";
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
