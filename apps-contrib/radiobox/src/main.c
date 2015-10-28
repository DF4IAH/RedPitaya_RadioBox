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
        "osc1_qrg_imil",    0.0f,  1,  0, 0.0f, 62.5e+9f },

    { /* Oscillator-2 frequency (Hz) */
        "osc2_qrg_imil",    0.0f,  1,  0, 0.0f, 62.5e+9f },

    { /* Oscillator-1 amplitude (µV) */
        "osc1_amp_imil",    0.0f,  1,  0, 0.0f, 2047e+3f },

    { /* Oscillator-2 magnitude (AM:%, FM:Hz, PM:°) */
        "osc2_mag_imil",    0.0f,  1,  0, 0.0f,    1e+9f },

    { /* Must be last! */
        NULL,               0.0f, -1, -1, 0.0f,     0.0f }
};
#endif

/** @brief Describes app. parameters with some info/limitations in high definition */
const rb_app_params_t rb_default_params[RB_PARAMS_NUM + 1] = {
    { /* Running mode */
        "rb_run",           0.0,   1, 0, 0.0,       1.0  },

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
        "osc1_qrg_imil",    0.0,   1,  0, 0.0,  62.5e+9  },

    { /* Oscillator-2 frequency (Hz) */
        "osc2_qrg_imil",    0.0,   1,  0, 0.0,  62.5e+9  },

    { /* Oscillator-1 amplitude (µV) */
        "osc1_amp_imil",    0.0,   1,  0, 0.0,  2047e+3  },

    { /* Oscillator-2 magnitude (AM:%, FM:Hz, PM:°) */
        "osc2_mag_imil",    0.0,   1,  0, 0.0,     1e+9  },

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

/** @brief params initialized */
int                             params_init_done = 0;  /* @see worker.c */


/** @brief offset value for the double <--> 2x float conversion system using a shared residue float */
const uint32_t cast_residue_bias = 512;

const char cast_name_ext_lo[]   = "LO_";
const char cast_name_ext_hi[]   = "HI_";
const char cast_name_ext_rs[]   = "SE_";
const int  cast_name_ext_len    = 3;

/** @brief Structure holds three bitfield members that reconstruct the IEEE754 double data format */
typedef struct cast_3xbf_s {
	/** @brief Lower 26 bits of the mantissa */
    uint32_t ui32_lo : 26;

	/** @brief Higher 26 bits of the mantissa */
    uint32_t ui32_hi : 26;

	/** @brief 1 Sign and 11 exponent bits */
    uint32_t ui32_se : 12;
} cast_3xbf_s_t;

/** @brief Structure holds an IEEE754 double member */
typedef struct cast_1xdouble_s {
	/** @brief The double member */
    double d;
} cast_1xdouble_s_t;

/** @brief Union gives two representations for a IEEE754 double variable */
typedef union cast_3xbf_1xdouble_u {
	/** @brief Union access to the bitfield members */
    cast_3xbf_s_t      bf;

	/** @brief Union access to the double member */
    cast_1xdouble_s_t  d;
} cast_3xbf_1xdouble_u_t;


/*----------------------------------------------------------------------------------*/
double cast_3xbf_to_1xdouble(float f_se, float f_hi, float f_lo)
{
	cast_3xbf_1xdouble_u_t u;

    /* union face: 3x float/bitfield */
    u.bf.ui32_lo = ((uint32_t) f_lo) & 0x3ffffff;
    u.bf.ui32_hi = ((uint32_t) f_hi) & 0x3ffffff;
    u.bf.ui32_se = ((uint32_t) f_se) & 0xfff;

    /* union face: 1x double */
    return u.d.d;
}

/*----------------------------------------------------------------------------------*/
int cast_1xdouble_to_3xbf(float* f_se, float* f_hi, float* f_lo, double d)
{
	cast_3xbf_1xdouble_u_t u;

    if (!f_se || !f_hi || !f_lo) {
        return -1;
    }

    /* union face: 1x double */
    u.d.d = d;

    /* union face: 3x uint32/bitfield, float cast simulates the data transport through the webserver-interface */
    *f_se = (float) u.bf.ui32_se;    // sign/exponent part
    *f_hi = (float) u.bf.ui32_hi;    // MSB part of the mantissa
    *f_lo = (float) u.bf.ui32_lo;    // LSB part of the mantissa

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
void rp2rb_params_value_copy(rb_app_params_t* dst_line, const rp_app_params_t src_line_rs, const rp_app_params_t src_line_lo, const rp_app_params_t src_line_hi)
{
    dst_line->value          = cast_3xbf_to_1xdouble(src_line_rs.value, src_line_lo.value, src_line_hi.value);
    dst_line->fpga_update    = src_line_lo.fpga_update;
    dst_line->read_only      = src_line_lo.read_only;
    dst_line->min_val        = cast_3xbf_to_1xdouble(src_line_rs.value, src_line_lo.min_val, src_line_hi.min_val);
    dst_line->max_val        = cast_3xbf_to_1xdouble(src_line_rs.value, src_line_lo.max_val, src_line_hi.max_val);
}

/*----------------------------------------------------------------------------------*/
void rb2rp_params_value_copy(rp_app_params_t* dst_line_rs, rp_app_params_t* dst_line_lo, rp_app_params_t* dst_line_hi, const rb_app_params_t src_line)
{
    cast_1xdouble_to_3xbf(&(dst_line_rs->value), &(dst_line_lo->value), &(dst_line_hi->value), src_line.value);
    dst_line_rs->fpga_update = 0.0f;
    dst_line_lo->fpga_update = src_line.fpga_update;
    dst_line_hi->fpga_update = 0.0f;
    dst_line_rs->read_only   = 0.0f;
    dst_line_lo->read_only   = src_line.read_only;
    dst_line_hi->read_only   = 0.0f;
    cast_1xdouble_to_3xbf(&(dst_line_rs->min_val), &(dst_line_lo->min_val), &(dst_line_hi->min_val), src_line.min_val);
    cast_1xdouble_to_3xbf(&(dst_line_rs->max_val), &(dst_line_lo->max_val), &(dst_line_hi->max_val), src_line.max_val);
}


/*----------------------------------------------------------------------------------*/
int rb_copy_params(rb_app_params_t** dst, const rb_app_params_t src[], int len, int do_copy_all_attr)
{
    const rb_app_params_t* s = src;
    int i, j, num_params;

    /* check arguments */
    if (!dst) {
        fprintf(stderr, "ERROR rp_copy_params - Internal error, the destination Application parameters variable is not set.\n");
        return -1;
    }
    if (!s) {
        //fprintf(stderr, "INFO rp_copy_params - no source parameter list given, taking default parameters instead.\n");
        s = rb_default_params;
    }

    /* check if destination buffer is allocated already */
    rb_app_params_t* p_dst = *dst;
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
            int j_rs = j++;
            int j_lo = j++;
            int j_hi = j++;

            p_dst[j_rs].name = (char*) malloc(cast_name_ext_len + slen + 1);  // old pointer to name does not belong to us and has to be discarded
            p_dst[j_lo].name = (char*) malloc(cast_name_ext_len + slen + 1);  // old pointer to name does not belong to us and has to be discarded
            p_dst[j_hi].name = (char*) malloc(cast_name_ext_len + slen + 1);  // old pointer to name does not belong to us and has to be discarded
            if (!(p_dst[j_rs].name) || !(p_dst[j_lo].name) || !(p_dst[j_hi].name)) {
                fprintf(stderr, "ERROR rp_copy_params_rp2rb - memory problem, the destination buffers failed to be allocated (2).\n");
                return -4;
            }

            strncpy( p_dst[j_rs].name,   cast_name_ext_rs, cast_name_ext_len);
            strncpy((p_dst[j_rs].name) + cast_name_ext_len, src[i].name, slen);
            p_dst[j_rs].name[cast_name_ext_len + slen] = '\0';

            strncpy( p_dst[j_lo].name,   cast_name_ext_lo, cast_name_ext_len);
            strncpy((p_dst[j_lo].name) + cast_name_ext_len, src[i].name, slen);
            p_dst[j_lo].name[cast_name_ext_len + slen] = '\0';

            strncpy( p_dst[j_hi].name,   cast_name_ext_hi, cast_name_ext_len);
            strncpy((p_dst[j_hi].name) + cast_name_ext_len, src[i].name, slen);
            p_dst[j_hi].name[cast_name_ext_len + slen] = '\0';

            rb2rp_params_value_copy(&(p_dst[j_rs]), &(p_dst[j_lo]), &(p_dst[j_hi]), src[i]);

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
    char name_rs[256];
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
        fprintf(stderr, "INFO rp_copy_params_rp2rb - num_LO_params = %d\n", num_LO_params);
    }

    /* check if destination buffer is allocated already */
    rb_app_params_t* p_dst = *dst;
    if (p_dst) {
         for (i = 0, j = 0; src[i].name; i++) {
            if (src[i].name != strstr(src[i].name, cast_name_ext_lo)) {
                continue;  // skip all none LO_ params
            }

            /* prepare name variants */
            {
                strncpy(name_rs, cast_name_ext_rs, cast_name_ext_len);
                strncpy(name_rs + cast_name_ext_len, src[i].name + cast_name_ext_len, sizeof(name_rs) - cast_name_ext_len - 1);
                name_rs[sizeof(name_rs) - 1] = '\0';

                strncpy(name_hi, cast_name_ext_hi, cast_name_ext_len);
                strncpy(name_hi + cast_name_ext_len, src[i].name + cast_name_ext_len, sizeof(name_hi) - cast_name_ext_len - 1);
                name_hi[sizeof(name_hi) - 1] = '\0';
            }

            /* find all triple elements */
            int i_rs = rp_find_parms_index(src, name_rs);
            int i_lo = i;
            int i_hi = rp_find_parms_index(src, name_hi);
            if (i_rs < 0 || i_hi < 0) {
                continue;  // no triple found, ignore uncomplete entries
            }

            j = rb_find_parms_index(p_dst, src[i].name + cast_name_ext_len);  // the extension is stripped away before the compare
            if (j < 0) {
                // discard new entry if not already known in target vector
                continue;
            }

            rp2rb_params_value_copy(&(p_dst[j]), src[i_rs], src[i_lo], src[i_hi]);
        }  // for ()

        /* mark last one as final entry */
        p_dst[num_LO_params].name = NULL;
        p_dst[num_LO_params].value = -1;

    } else {
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
                strncpy(name_rs, cast_name_ext_rs, cast_name_ext_len);
                strncpy(name_rs + cast_name_ext_len, src[i].name + cast_name_ext_len, sizeof(name_rs) - cast_name_ext_len - 1);
                name_rs[sizeof(name_rs) - 1] = '\0';

                strncpy(name_hi, cast_name_ext_hi, cast_name_ext_len);
                strncpy(name_hi + cast_name_ext_len, src[i].name + cast_name_ext_len, sizeof(name_hi) - cast_name_ext_len - 1);
                name_hi[sizeof(name_hi) - 1] = '\0';
            }

            /* find all triple elements */
            int i_rs = rp_find_parms_index(src, name_rs);
            int i_lo = i;
            int i_hi = rp_find_parms_index(src, name_hi);
            if (i_rs < 0 || i_hi < 0) {
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

            rp2rb_params_value_copy(&(p_dst[j]), src[i_rs], src[i_lo], src[i_hi]);
        }  // for ()

        /* mark last one as final entry */
        p_dst[num_LO_params].name = NULL;
        p_dst[num_LO_params].value = -1;
    }
    *dst = p_dst;

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

        free(p);
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
