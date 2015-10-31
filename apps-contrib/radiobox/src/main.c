/**
 * @brief Red Pitaya RadioBox main module.
 *
 * @author Ulrich Habel (DF4IAH) <espero7757@gmx.net>
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
#include "cb_http.h"
#include "cb_ws.h"

#include "main.h"


#ifndef VERSION
# define VERSION "(not set)"
#endif
#ifndef REVISION
# define REVISION "(not set)"
#endif


/** @brief calibration data layout within the EEPROM device */
rp_calib_params_t               rp_main_calib_params;

/** @brief HouseKeeping memory file descriptor used to mmap() the FPGA space */
int                             g_fpga_hk_mem_fd = -1;
/** @brief HouseKeeping memory layout of the FPGA registers */
fpga_hk_reg_mem_t*              g_fpga_hk_reg_mem = NULL;

/** @brief RadioBox memory file descriptor used to mmap() the FPGA space */
int                             g_fpga_rb_mem_fd = -1;
/** @brief RadioBox memory layout of the FPGA registers */
fpga_rb_reg_mem_t*              g_fpga_rb_reg_mem = NULL;

#if 0
/** @brief Describes app. parameters with some info/limitations */
const rp_app_params_t rp_default_params[RB_PARAMS_NUM + 1] = {
    { /* Running mode */
        "rb_run",           0.0f,  1, 0, 0.0f,      1.0f },

    { /* Oscillator-1 modulation source selector
       * ( 0: none,
       *   1: RF Input 1,
       *   2: RF Input 2,
       *   4: EXT AI0,
       *   5: EXT AI1,
       *   6: EXT AI2,
       *   7: EXT AI3,
       *  15: OSC2
       * )
       **/
        "osc1_modsrc_s",    0.0f,  1,  0, 0.0f,    15.0f },

    { /* Oscillator-1 modulation type selector (0: AM, 1: FM, 2: PM) */
        "osc1_modtyp_s",    0.0f,  1,  0, 0.0f,     2.0f },

    { /* Oscillator-1 frequency (Hz) */
        "osc1_qrg_f",       0.0f,  1,  0, 0.0f, 62.5e+9f },

    { /* Oscillator-2 frequency (Hz) */
        "osc2_qrg_f",       0.0f,  1,  0, 0.0f, 62.5e+9f },

    { /* Oscillator-1 amplitude (mV) */
        "osc1_amp_f",       0.0f,  1,  0, 0.0f, 2047e+3f },

    { /* Oscillator-2 magnitude (AM:%, FM:Hz, PM:°) */
        "osc2_mag_f",       0.0f,  1,  0, 0.0f,    1e+9f },

    { /* Must be last! */
        NULL,               0.0f, -1, -1, 0.0f,     0.0f }
};
#endif

/** @brief Describes app. parameters with some info/limitations in high definition */
const rb_app_params_t rb_default_params[RB_PARAMS_NUM + 1] = {
    { /* Running mode */
        "rb_run",           1.0,   1, 0, 0.0,       1.0  },

    { /* Oscillator-1 modulation source selector
       * ( 0: none,
       *   1: RF Input 1,
       *   2: RF Input 2,
       *   4: EXT AI0,
       *   5: EXT AI1,
       *   6: EXT AI2,
       *   7: EXT AI3,
       *  15: OSC2
       * )
       **/
        "osc1_modsrc_s",    0.0,   1,  0, 0.0,     15.0  },

    { /* Oscillator-1 modulation type selector (0: AM, 1: FM, 2: PM) */
        "osc1_modtyp_s",    0.0,   1,  0, 0.0,      2.0  },

    { /* Oscillator-1 frequency (Hz) */
        "osc1_qrg_f",       0.0,   1,  0, 0.0,  62.5e+9  },

    { /* Oscillator-2 frequency (Hz) */
        "osc2_qrg_f",       0.0,   1,  0, 0.0,  62.5e+9  },

    { /* Oscillator-1 amplitude (mV) */
        "osc1_amp_f",       0.0,   1,  0, 0.0,  2047e+3  },

    { /* Oscillator-2 magnitude (AM:%, FM:Hz, PM:°) */
        "osc2_mag_f",       0.0,   1,  0, 0.0,     1e+9  },

    { /* Must be last! */
        NULL,               0.0,  -1, -1, 0.0,      0.0  }
};

/** @brief CallBack copy of params to inform the worker */
rp_app_params_t*                rp_cb_in_params = NULL;
/** @brief Holds mutex to access on parameters from outside to the worker thread */
pthread_mutex_t                 rp_cb_in_params_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief CallBack copy of params from the worker when requested */
rp_app_params_t*                rp_cb_out_params = NULL;
/** @brief Holds mutex to access on parameters from the worker thread to any other context */
pthread_mutex_t                 rp_cb_out_params_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief Current copy of params of the worker thread */
rb_app_params_t*                rb_info_worker_params = NULL;
/** @brief Holds mutex to access parameters from the worker thread to any other context */
pthread_mutex_t                 rb_info_worker_params_mutex = PTHREAD_MUTEX_INITIALIZER;


/** @brief params initialized */
int                             params_init_done = 0;  /* @see worker.c */

const int IEEE754_DOUBLE_EXP_BIAS = 1023;
const int IEEE754_DOUBLE_EXP_BITS = 12;
const int IEEE754_DOUBLE_MNT_BITS = 52;

const char cast_name_ext_se[]   = "SE_";
const char cast_name_ext_hi[]   = "HI_";
const char cast_name_ext_lo[]   = "LO_";
const int  cast_name_ext_len    = 3;


/*----------------------------------------------------------------------------------*/
double cast_3xbf_to_1xdouble(float f_se, float f_hi, float f_lo)
{
    unsigned long long ull = 0ULL;
    double*            dp  = (void*) &ull;

    if (!f_se && !f_hi && !f_lo) {
        //fprintf(stderr, "INFO cast_3xbf_to_1xdouble (zero) - out(d=%lf) <-- in(f_se=%f, f_hi=%f, f_lo=%f)\n", 0.0, f_se, f_hi, f_lo);
        return 0.0;
    }

    /* unsigned long long interpretation */
    ull  = (((uint64_t) f_se) & 0xfffULL    ) <<  IEEE754_DOUBLE_MNT_BITS;
    ull |= (((uint64_t) f_hi) & 0x3ffffffULL) << (IEEE754_DOUBLE_MNT_BITS >> 1);
    ull |=  ((uint64_t) f_lo) & 0x3ffffffULL;

    /* double interpretation */
    //fprintf(stderr, "INFO cast_3xbf_to_1xdouble (val)  - out(d=%lf) <-- in(f_se=%f, f_hi=%f, f_lo=%f)\n", *dp, f_se, f_hi, f_lo);
    return *dp;
}

/*----------------------------------------------------------------------------------*/
int cast_1xdouble_to_3xbf(float* f_se, float* f_hi, float* f_lo, double d)
{
    unsigned long long ull = 0;
    double*            dp  = (void*) &ull;

    if (!f_se || !f_hi || !f_lo) {
        return -1;
    }

    if (d == 0.0) {
        /* use unnormalized zero instead */
        *f_se = 0.0f;
        *f_hi = 0.0f;
        *f_lo = 0.0f;
        //fprintf(stderr, "INFO cast_1xdouble_to_3xbf (zero) - out(f_se=%f, f_hi=%f, f_lo=%f) <-- in(d=%lf)\n", *f_se, *f_hi, *f_lo, d);
        return 0;
    }

    /* double interpretation */
    *dp = d;

    /* unsigned long long interpretation */
    *f_se = (ull >>  IEEE754_DOUBLE_MNT_BITS      ) & 0xfff;
    *f_hi = (ull >> (IEEE754_DOUBLE_MNT_BITS >> 1)) & 0x3ffffffULL;
    *f_lo =  ull                                    & 0x3ffffffULL;

    //fprintf(stderr, "INFO cast_1xdouble_to_3xbf (val)  - out(f_se=%f, f_hi=%f, f_lo=%f) <-- in(d=%lf)\n", *f_se, *f_hi, *f_lo, d);
    return 0;
}


/*----------------------------------------------------------------------------------*/
const char* rp_app_desc(void)
{
    return (const char *)"RedPitaya RadioBox application by DF4IAH and DD8UU.\n";
}


/*----------------------------------------------------------------------------------*/
int rp_create_traces(float** a_traces[TRACE_NUM])
{
    int i;
    float** trc;

    fprintf(stderr, "rp_create_traces: BEGIN\n");

    trc = malloc(TRACE_NUM * sizeof(float*));
    if (!trc) {
        return -1;
    }

    // First step - void all pointers in case of early return
    for (i = 0; i < TRACE_NUM; i++)
        trc[i] = NULL;

    // Set-up for each trace
    for (i = 0; i < TRACE_NUM; i++) {
        trc[i] = (float*) malloc(TRACE_LENGTH * sizeof(float));
        if (!(trc[i])) {
            // de-allocate all memory as much as already taken
            rp_free_traces(a_traces);
            return -2;
        }
        // wipe-out all random data
        memset(trc[i], 0, TRACE_LENGTH * sizeof(float));
    }

    // Success - assign to in/out parameter
    *a_traces = trc;
    fprintf(stderr, "rp_create_traces: END\n\n");

    return 0;
}

/*----------------------------------------------------------------------------------*/
void rp_free_traces(float** a_traces[TRACE_NUM])
{
    fprintf(stderr, "rp_free_traces: BEGIN\n");

    if (!a_traces) {
        return;
    }

    float** trc = *a_traces;

    if (trc) {
        int i;
        for (i = 0; i < TRACE_NUM; i++) {
            if (trc[i]) {
                free(trc[i]);
                trc[i] = NULL;
            }
        }
        free(trc);
    }
    *a_traces = NULL;

    fprintf(stderr, "rp_free_traces: END\n\n");
}


/*----------------------------------------------------------------------------------*/
int rp_find_parms_index(const rp_app_params_t* src, const char* name)
{
    if (!src || !name) {
        fprintf(stderr, "ERROR find_parms_index - Bad function arguments received.\n");
        return -2;
    }

    int i = 0;
    while (src[i].name) {
        if (!strcmp(src[i].name, name)) {
            return i;
        }
        ++i;
    }
    return -1;
}

/*----------------------------------------------------------------------------------*/
int rb_find_parms_index(const rb_app_params_t* src, const char* name)
{
    if (!src || !name) {
        fprintf(stderr, "ERROR find_parms_index - Bad function arguments received.\n");
        return -2;
    }

    int i = 0;
    while (src[i].name) {
        if (!strcmp(src[i].name, name)) {
            return i;
        }
        ++i;
    }
    return -1;
}


/*----------------------------------------------------------------------------------*/
void rp2rb_params_value_copy(rb_app_params_t* dst_line, const rp_app_params_t src_line_se, const rp_app_params_t src_line_hi, const rp_app_params_t src_line_lo)
{
    dst_line->value          = cast_3xbf_to_1xdouble(src_line_se.value, src_line_hi.value, src_line_lo.value);

    dst_line->fpga_update    = src_line_lo.fpga_update;
    dst_line->read_only      = src_line_lo.read_only;

//  dst_line->min_val        = cast_3xbf_to_1xdouble(src_line_se.min_val, src_line_hi.min_val, src_line_lo.min_val);
//  dst_line->max_val        = cast_3xbf_to_1xdouble(src_line_se.max_val, src_line_hi.max_val, src_line_lo.max_val);
}

/*----------------------------------------------------------------------------------*/
void rb2rp_params_value_copy(rp_app_params_t* dst_line_se, rp_app_params_t* dst_line_hi, rp_app_params_t* dst_line_lo, const rb_app_params_t src_line)
{
    cast_1xdouble_to_3xbf(&(dst_line_se->value), &(dst_line_hi->value), &(dst_line_lo->value), src_line.value);

    dst_line_se->fpga_update = 0;
    dst_line_hi->fpga_update = 0;
    dst_line_lo->fpga_update = src_line.fpga_update;

    dst_line_se->read_only = 0;
    dst_line_hi->read_only = 0;
    dst_line_lo->read_only = src_line.read_only;

    cast_1xdouble_to_3xbf(&(dst_line_se->min_val), &(dst_line_hi->min_val), &(dst_line_lo->min_val), src_line.min_val);
    cast_1xdouble_to_3xbf(&(dst_line_se->max_val), &(dst_line_hi->max_val), &(dst_line_lo->max_val), src_line.max_val);
}


/*----------------------------------------------------------------------------------*/
int rp_copy_params(rp_app_params_t** dst, const rp_app_params_t src[], int len, int do_copy_all_attr)
{
    const rp_app_params_t* s = src;
    int i, j, num_params;

    /* check arguments */
    if (!s || !dst) {
        fprintf(stderr, "ERROR rp_copy_params - Internal error, the destination Application parameters variable is not set.\n");
        return -1;
    }

    /* check if destination buffer is allocated already */
    rp_app_params_t* p_dst = *dst;
    if (p_dst) {
        //fprintf(stderr, "INFO rp_copy_params - dst exists - updating into dst vector.\n");
        /* destination buffer exists */
        i = 0;
        while (s[i].name) {
            //fprintf(stderr, "INFO rp_copy_params - processing name = %s\n", s[i].name);
            /* process each parameter entry of the list */

            if (!strcmp(p_dst[i].name, s[i].name)) {  // direct mapping found - just copy the value
                //fprintf(stderr, "INFO rp_copy_params - direct mapping used\n");
                p_dst[i].value = s[i].value;
                if (s[i].fpga_update & 0x80) {
                    p_dst[i].fpga_update |=  0x80;  // transfer FPGA update marker in case it is present
                } else {
                    p_dst[i].fpga_update &= ~0x80;  // or remove it when it is not set
                }

                if (do_copy_all_attr) {  // if default parameters are taken, use all attributes
                    p_dst[i].fpga_update    = s[i].fpga_update;
                    p_dst[i].read_only      = s[i].read_only;
                    p_dst[i].min_val        = s[i].min_val;
                    p_dst[i].max_val        = s[i].max_val;
                }

            } else {
                //fprintf(stderr, "INFO rp_copy_params - iterative searching ...\n");
                j = 0;
                while (p_dst[j].name) {  // scanning the complete list
                    if (j == i) {  // do a short-cut here
                        j++;
                        continue;
                    }

                    if (!strcmp(p_dst[j].name, s[i].name)) {
                        p_dst[j].value = s[i].value;
                        if (s[i].fpga_update & 0x80) {
                            p_dst[i].fpga_update |=  0x80;  // transfer FPGA update marker in case it is present
                        } else {
                            p_dst[i].fpga_update &= ~0x80;  // or remove it when it is not set
                        }

                        if (do_copy_all_attr) {  // if default parameters are taken, use all attributes
                            p_dst[i].fpga_update    = s[i].fpga_update;  // copy FPGA update marker in case it is present
                            p_dst[i].read_only      = s[i].read_only;
                            p_dst[i].min_val        = s[i].min_val;
                            p_dst[i].max_val        = s[i].max_val;
                        }
                        break;
                    }
                    j++;
                }  // while (p_new[j].name)
            }  // if () else
            i++;
        }  // while (src[i].name)

    } else {
        /* destination buffer has to be allocated, create a new parameter list */

        if (len >= 0) {
            num_params = len;

        } else {
            /* retrieve the number of source parameters */
            i = 0;
            num_params = 0;
            while (s[i++].name) {
                num_params++;
            }
        }

        /* allocate array of parameter entries, parameter names must be allocated separately */
        p_dst = (rp_app_params_t*) malloc(sizeof(rp_app_params_t) * (num_params + 1));
        if (!p_dst) {
            fprintf(stderr, "ERROR rp_copy_params - memory problem, the destination buffer could not be allocated (1).\n");
            return -3;
        }
        /* prepare a copy for built-in attributes. Strings have to be handled on their own way */
        memcpy(p_dst, s, (num_params + 1) * sizeof(rp_app_params_t));

        /* allocate memory and copy character strings for params names */
        i = 0;
        while (s[i].name) {
            int slen = strlen(s[i].name);
            p_dst[i].name = (char*) malloc(slen + 1);  // old pointer to name does not belong to us and has to be discarded
            if (!(p_dst[i].name)) {
                fprintf(stderr, "ERROR rp_copy_params - memory problem, the destination buffer could not be allocated (2).\n");
                return -4;
            }
            strncpy(p_dst[i].name, s[i].name, slen);
            p_dst[i].name[slen] = '\0';

            i++;
        }

        /* mark last one as final entry */
        p_dst[num_params].name = NULL;
        p_dst[num_params].value = -1;
    }
    *dst = p_dst;

    return 0;
}

/*----------------------------------------------------------------------------------*/
int rb_copy_params(rb_app_params_t** dst, const rb_app_params_t src[], int len, int do_copy_all_attr)
{
    const rb_app_params_t* s = src;
    int i, j, num_params;

    /* check arguments */
    if (!dst) {
        fprintf(stderr, "ERROR rb_copy_params - Internal error, the destination Application parameters variable is not set.\n");
        return -1;
    }
    if (!s) {
        //fprintf(stderr, "INFO rb_copy_params - no source parameter list given, taking default parameters instead.\n");
        s = rb_default_params;
    }

    /* check if destination buffer is allocated already */
    rb_app_params_t* p_dst = *dst;
    if (p_dst) {
        //fprintf(stderr, "INFO rb_copy_params - dst exists - updating into dst vector.\n");
        /* destination buffer exists */
        i = 0;
        while (s[i].name) {
            //fprintf(stderr, "INFO rb_copy_params - processing name = %s\n", s[i].name);
            /* process each parameter entry of the list */

            if (!strcmp(p_dst[i].name, s[i].name)) {  // direct mapping found - just copy the value
                //fprintf(stderr, "INFO rb_copy_params - direct mapping used\n");
                p_dst[i].value = s[i].value;
                if (s[i].fpga_update & 0x80) {
                    p_dst[i].fpga_update |=  0x80;  // transfer FPGA update marker in case it is present
                } else {
                    p_dst[i].fpga_update &= ~0x80;  // or remove it when it is not set
                }

                if (do_copy_all_attr) {  // if default parameters are taken, use all attributes
                    p_dst[i].fpga_update    = s[i].fpga_update;
                    p_dst[i].read_only      = s[i].read_only;
                    p_dst[i].min_val        = s[i].min_val;
                    p_dst[i].max_val        = s[i].max_val;
                }

            } else {
                //fprintf(stderr, "INFO rb_copy_params - iterative searching ...\n");
                j = 0;
                while (p_dst[j].name) {  // scanning the complete list
                    if (j == i) {  // do a short-cut here
                        j++;
                        continue;
                    }

                    if (!strcmp(p_dst[j].name, s[i].name)) {
                        p_dst[j].value = s[i].value;
                        if (s[i].fpga_update & 0x80) {
                            p_dst[i].fpga_update |=  0x80;  // transfer FPGA update marker in case it is present
                        } else {
                            p_dst[i].fpga_update &= ~0x80;  // or remove it when it is not set
                        }

                        if (do_copy_all_attr) {  // if default parameters are taken, use all attributes
                            p_dst[i].fpga_update    = s[i].fpga_update;  // copy FPGA update marker in case it is present
                            p_dst[i].read_only      = s[i].read_only;
                            p_dst[i].min_val        = s[i].min_val;
                            p_dst[i].max_val        = s[i].max_val;
                        }
                        break;
                    }
                    j++;
                }  // while (p_new[j].name)
            }  // if () else
            i++;
        }  // while (src[i].name)

    } else {
        /* destination buffer has to be allocated, create a new parameter list */

        if (len >= 0) {
            num_params = len;

        } else {
            /* retrieve the number of source parameters */
            i = 0;
            num_params = 0;
            while (s[i++].name) {
                num_params++;
            }
        }

        /* allocate array of parameter entries, parameter names must be allocated separately */
        p_dst = (rb_app_params_t*) malloc(sizeof(rb_app_params_t) * (num_params + 1));
        if (!p_dst) {
            fprintf(stderr, "ERROR rb_copy_params - memory problem, the destination buffer could not be allocated (1).\n");
            return -3;
        }
        /* prepare a copy for built-in attributes. Strings have to be handled on their own way */
        memcpy(p_dst, s, (num_params + 1) * sizeof(rb_app_params_t));

        /* allocate memory and copy character strings for params names */
        i = 0;
        while (s[i].name) {
            int slen = strlen(s[i].name);
            p_dst[i].name = (char*) malloc(slen + 1);  // old pointer to name does not belong to us and has to be discarded
            if (!(p_dst[i].name)) {
                fprintf(stderr, "ERROR rb_copy_params - memory problem, the destination buffer could not be allocated (2).\n");
                return -4;
            }
            strncpy(p_dst[i].name, s[i].name, slen);
            p_dst[i].name[slen] = '\0';

            i++;
        }

        /* mark last one as final entry */
        p_dst[num_params].name = NULL;
        p_dst[num_params].value = -1;
    }
    *dst = p_dst;

    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_copy_params_rb2rp(rp_app_params_t** dst, const rb_app_params_t src[], int len)
{
    int i, j, num_params;

    /* check arguments */
    if (!dst) {
        fprintf(stderr, "ERROR rp_copy_params_rp2rb - Internal error, the destination Application parameters vector variable is not set.\n");
        return -1;
    }
    if (!src) {
        fprintf(stderr, "ERROR rp_copy_params_rp2rb - Internal error, the source Application parameters vector variable is not set.\n");
        return -1;
    }

    /* check if destination buffer is allocated already */
    rp_app_params_t* p_dst = *dst;
    if (p_dst) {
        rp_free_params(dst);
    }

    /* destination buffer has to be allocated, create a new parameter list */
    {
        if (len >= 0) {
            num_params = len;

        } else {
            /* retrieve the number of source parameters */
            i = 0;
            num_params = 0;
            while (src[i++].name) {
                num_params++;
            }
        }

        /* allocate array of parameter entries, parameter names must be allocated separately */
        p_dst = (rp_app_params_t*) malloc(sizeof(rp_app_params_t) * ((3 * num_params) + 1));
        if (!p_dst) {
            fprintf(stderr, "ERROR rp_copy_params_rp2rb - memory problem, the destination buffer failed to be allocated (1).\n");
            return -3;
        }

        /* allocate memory and copy character strings for params names */
        i = 0;
        j = 0;
        while (src[i].name) {
            int slen = strlen(src[i].name);
            int j_se = j++;
            int j_hi = j++;
            int j_lo = j++;

            p_dst[j_se].name = (char*) malloc(cast_name_ext_len + slen + 1);  // old pointer to name does not belong to us and has to be discarded
            p_dst[j_hi].name = (char*) malloc(cast_name_ext_len + slen + 1);  // old pointer to name does not belong to us and has to be discarded
            p_dst[j_lo].name = (char*) malloc(cast_name_ext_len + slen + 1);  // old pointer to name does not belong to us and has to be discarded
            if (!(p_dst[j_se].name) || !(p_dst[j_hi].name) || !(p_dst[j_lo].name)) {
                fprintf(stderr, "ERROR rp_copy_params_rp2rb - memory problem, the destination buffers failed to be allocated (2).\n");
                return -4;
            }

            strncpy( p_dst[j_se].name,   cast_name_ext_se, cast_name_ext_len);
            strncpy((p_dst[j_se].name) + cast_name_ext_len, src[i].name, slen);
            p_dst[j_se].name[cast_name_ext_len + slen] = '\0';

            strncpy( p_dst[j_hi].name,   cast_name_ext_hi, cast_name_ext_len);
            strncpy((p_dst[j_hi].name) + cast_name_ext_len, src[i].name, slen);
            p_dst[j_hi].name[cast_name_ext_len + slen] = '\0';

            strncpy( p_dst[j_lo].name,   cast_name_ext_lo, cast_name_ext_len);
            strncpy((p_dst[j_lo].name) + cast_name_ext_len, src[i].name, slen);
            p_dst[j_lo].name[cast_name_ext_len + slen] = '\0';

            rb2rp_params_value_copy(&(p_dst[j_se]), &(p_dst[j_hi]), &(p_dst[j_lo]), src[i]);

            i++;
        }

        /* mark last one as final entry */
        p_dst[3 * num_params].name = NULL;
        p_dst[3 * num_params].value = -1;
    }
    *dst = p_dst;

    return 0;
}

/*----------------------------------------------------------------------------------*/
 int rp_copy_params_rp2rb(rb_app_params_t** dst, const rp_app_params_t src[])
{
    char name_se[256];
    char name_hi[256];
    int num_LO_params = 0;
    int i = 0;
    int j = 0;

    /* check arguments */

    if (!dst) {
        fprintf(stderr, "ERROR rp_copy_params_rp2rb - Internal error, the destination Application parameters vector variable is not set.\n");
        return -1;
    }
    if (!src) {
        fprintf(stderr, "ERROR rp_copy_params_rb2rp - Internal error, the source Application parameters vector variable is not set.\n");
        return -1;
    }

    /* get the number of LO params */
    {
        int i = 0;
        while (src[i].name) {
            if (src[i].name == strstr(src[i].name, cast_name_ext_lo)) {
                num_LO_params++;
            }
            i++;
        }
        //fprintf(stderr, "INFO rp_copy_params_rp2rb - num_LO_params = %d\n", num_LO_params);
    }

    /* check if destination buffer is allocated already */
    rb_app_params_t* p_dst = *dst;
    if (p_dst) {
        fprintf(stderr, "INFO rp_copy_params_rp2rb - dst vector is valid\n");
         for (i = 0, j = 0; src[i].name; i++) {
            if (src[i].name != strstr(src[i].name, cast_name_ext_lo)) {
                continue;  // skip all none LO_ params
            }

            /* prepare name variants */
            {
                strncpy(name_se, cast_name_ext_se, cast_name_ext_len);
                strncpy(name_se + cast_name_ext_len, src[i].name + cast_name_ext_len, sizeof(name_se) - cast_name_ext_len - 1);
                name_se[sizeof(name_se) - 1] = '\0';

                strncpy(name_hi, cast_name_ext_hi, cast_name_ext_len);
                strncpy(name_hi + cast_name_ext_len, src[i].name + cast_name_ext_len, sizeof(name_hi) - cast_name_ext_len - 1);
                name_hi[sizeof(name_hi) - 1] = '\0';
            }

            /* find all triple elements */
            int i_se = rp_find_parms_index(src, name_se);
            int i_hi = rp_find_parms_index(src, name_hi);
            int i_lo = i;
            if (i_se < 0 || i_hi < 0) {
                continue;  // no triple found, ignore uncomplete entries
            }

            j = rb_find_parms_index(p_dst, src[i].name + cast_name_ext_len);  // the extension is stripped away before the compare
            if (j < 0) {
                // discard new entry if not already known in target vector
                fprintf(stderr, "WARNING rp_copy_params_rp2rb - input element of vector is unknown - name = %s\n", src[i].name);
                continue;
            }

            //fprintf(stderr, "INFO rp_copy_params_rp2rb - in[%d, %d, %d] copied to out[%d] - name = %s\n", i_se, i_hi, i_lo, j, src[i].name + cast_name_ext_len);
            rp2rb_params_value_copy(&(p_dst[j]), src[i_se], src[i_hi], src[i_lo]);
        }  // for ()

        /* mark last one as final entry */
        p_dst[num_LO_params].name = NULL;
        p_dst[num_LO_params].value = -1;

    } else {
        //fprintf(stderr, "INFO rp_copy_params_rp2rb - creating new dst vector\n");
        /* destination buffer has to be allocated, create a new parameter list */

        /* allocate array of parameter entries, parameter names must be allocated separately */
        p_dst = (rb_app_params_t*) malloc(sizeof(rb_app_params_t) * (num_LO_params + 1));
        if (!p_dst) {
            fprintf(stderr, "ERROR rp_copy_params_rp2rb - memory problem, the destination buffer failed to be allocated (1).\n");
            return -3;
        }

        /* allocate memory and copy character strings for params names */
        for (i = 0, j = 0; src[i].name; i++) {
            if (src[i].name != strstr(src[i].name, cast_name_ext_lo)) {
                continue;  // skip all none LO_ params
            }

            /* prepare name variants */
            {
                strncpy(name_se, cast_name_ext_se, cast_name_ext_len);
                strncpy(name_se + cast_name_ext_len, src[i].name + cast_name_ext_len, sizeof(name_se) - cast_name_ext_len - 1);
                name_se[sizeof(name_se) - 1] = '\0';

                strncpy(name_hi, cast_name_ext_hi, cast_name_ext_len);
                strncpy(name_hi + cast_name_ext_len, src[i].name + cast_name_ext_len, sizeof(name_hi) - cast_name_ext_len - 1);
                name_hi[sizeof(name_hi) - 1] = '\0';
            }

            /* find all triple elements */
            int i_se = rp_find_parms_index(src, name_se);
            int i_hi = rp_find_parms_index(src, name_hi);
            int i_lo = i;
            if (i_se < 0 || i_hi < 0) {
                continue;
            }

            /* create for each valid "rp" input vector tuple a "rb" output vector entry */
            int slen = strlen(src[i].name) - cast_name_ext_len;
            p_dst[j].name = (char*) malloc(slen + 1);
            if (!p_dst[j].name) {
                fprintf(stderr, "ERROR rp_copy_params_rp2rb - memory problem, the destination buffers failed to be allocated (2).\n");
                return -4;
            }

            strncpy(p_dst[j].name, src[i_lo].name + cast_name_ext_len, slen);
            p_dst[j].name[slen] = '\0';

            //fprintf(stderr, "INFO rp_copy_params_rp2rb - in[%d,%d,%d] copied to out[%d] - name = %s\n", i_se, i_hi, i_lo, j, src[i].name + cast_name_ext_len);
            rp2rb_params_value_copy(&(p_dst[j]), src[i_se], src[i_hi], src[i_lo]);
            j++;
        }  // for ()

        /* mark last one as final entry */
        p_dst[num_LO_params].name = NULL;
        p_dst[num_LO_params].value = -1;
    }
    *dst = p_dst;

    return 0;
}


/*----------------------------------------------------------------------------------*/
int print_rb_params(rb_app_params_t* params)
{
    if (!params) {
        return -1;
    }

    int i = 0;
    while (params[i].name) {
        fprintf(stderr, "DEBUG print_rb_params: name=%s - value=%lf\n", params[i].name, params[i].value);
        i++;
    }

    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_free_params(rp_app_params_t** params)
{
    if (!params) {
        return -1;
    }

    /* free params structure */
    if (*params) {
        rp_app_params_t* p = *params;

        int i = 0;
        while (p[i].name) {
            free(p[i].name);
            p[i].name = NULL;
            i++;
        }

        free(*params);
        *params = NULL;
    }
    return 0;
}

/*----------------------------------------------------------------------------------*/
int rb_free_params(rb_app_params_t** params)
{
    if (!params) {
        return -1;
    }

    /* free params structure */
    if (*params) {
        rb_app_params_t* p = *params;

        int i = 0;
        while (p[i].name) {
            free(p[i].name);
            p[i].name = NULL;
            i++;
        }

        free(p);
        *params = NULL;
    }
    return 0;
}
