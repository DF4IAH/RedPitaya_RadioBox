/**
 * $Id: worker.c 881 2013-12-16 05:37:34Z rp_jmenart $
 *
 * @brief Red Pitaya Oscilloscope worker.
 *
 * @Author Jure Menart <juremenart@gmail.com>
 *         
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */

#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>

#include "cb_http.h"
#include "fpga.h"

#include "worker.h"

pthread_t *rp_osc_thread_handler = NULL;
void *rp_osc_worker_thread(void *args);

pthread_mutex_t       rp_osc_ctrl_mutex = PTHREAD_MUTEX_INITIALIZER;
rp_osc_worker_state_t rp_osc_ctrl;
rp_app_params_t       *rp_osc_params = NULL;
int                   rp_osc_params_dirty;
int                   rp_osc_params_fpga_update;

pthread_mutex_t       rp_osc_sig_mutex = PTHREAD_MUTEX_INITIALIZER;
float               **rp_osc_signals;
int                   rp_osc_signals_dirty = 0;
int                   rp_osc_sig_last_idx = 0;
float               **rp_tmp_signals; /* used for calculation, only from worker */

/* Signals directly pointing at the FPGA mem space */
int                  *rp_fpga_cha_signal, *rp_fpga_chb_signal;

/* Calibration parameters read from EEPROM */
rp_calib_params_t *rp_calib_params = NULL;


/*----------------------------------------------------------------------------------*/
int rp_osc_worker_init(rp_app_params_t *params, int params_len,
                       rp_calib_params_t *calib_params)
{
    int ret_val;

    rp_osc_ctrl               = rp_osc_idle_state;
    rp_osc_params_dirty       = 0;
    rp_osc_params_fpga_update = 0;

    rp_copy_params(params, (rp_app_params_t **)&rp_osc_params);

    rp_cleanup_signals(&rp_osc_signals);
    if(rp_create_signals(&rp_osc_signals) < 0)
        return -1;

    rp_cleanup_signals(&rp_tmp_signals);
    if(rp_create_signals(&rp_tmp_signals) < 0) {
        rp_cleanup_signals(&rp_osc_signals);
        return -1;
    }

    if(osc_fpga_init() < 0) {
        rp_cleanup_signals(&rp_osc_signals);
        rp_cleanup_signals(&rp_tmp_signals);
        return -1;
    }

    rp_calib_params = calib_params;

    osc_fpga_get_sig_ptr(&rp_fpga_cha_signal, &rp_fpga_chb_signal);

    rp_osc_thread_handler = (pthread_t *)malloc(sizeof(pthread_t));
    if(rp_osc_thread_handler == NULL) {
        rp_cleanup_signals(&rp_osc_signals);
        rp_cleanup_signals(&rp_tmp_signals);
        return -1;
    }
    ret_val = 
        pthread_create(rp_osc_thread_handler, NULL, rp_osc_worker_thread, NULL);
    if(ret_val != 0) {
        osc_fpga_exit();

        rp_cleanup_signals(&rp_osc_signals);
        rp_cleanup_signals(&rp_tmp_signals);
        fprintf(stderr, "pthread_create() failed: %s\n", 
                strerror(errno));
        return -1;
    }

    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_worker_exit(void)
{
    int ret_val = 0; 

    rp_osc_worker_change_state(rp_osc_quit_state);
    if(rp_osc_thread_handler) {
        ret_val = pthread_join(*rp_osc_thread_handler, NULL);
        free(rp_osc_thread_handler);
        rp_osc_thread_handler = NULL;
    }
    if(ret_val != 0) {
        fprintf(stderr, "pthread_join() failed: %s\n", 
                strerror(errno));
    }
    osc_fpga_exit();

    rp_cleanup_signals(&rp_osc_signals);
    rp_cleanup_signals(&rp_tmp_signals);

    rp_clean_params(rp_osc_params);

    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_worker_change_state(rp_osc_worker_state_t new_state)
{
    if(new_state >= rp_osc_nonexisting_state)
        return -1;
    pthread_mutex_lock(&rp_osc_ctrl_mutex);
    rp_osc_ctrl = new_state;
    pthread_mutex_unlock(&rp_osc_ctrl_mutex);
    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_worker_get_state(rp_osc_worker_state_t *state)
{
    pthread_mutex_lock(&rp_osc_ctrl_mutex);
    *state = rp_osc_ctrl;
    pthread_mutex_unlock(&rp_osc_ctrl_mutex);
    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_worker_update_params(rp_app_params_t *params, int fpga_update)
{
    pthread_mutex_lock(&rp_osc_ctrl_mutex);
    rp_copy_params(params, (rp_app_params_t **)&rp_osc_params);
    rp_osc_params_dirty       = 1;
    rp_osc_params_fpga_update = fpga_update;
    rp_osc_params[PARAMS_NUM].name = NULL;
    rp_osc_params[PARAMS_NUM].value = -1;

    pthread_mutex_unlock(&rp_osc_ctrl_mutex);
    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_clean_signals(void)
{
    pthread_mutex_lock(&rp_osc_sig_mutex);
    rp_osc_signals_dirty = 0;
    pthread_mutex_unlock(&rp_osc_sig_mutex);
    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_get_signals(float ***signals, int *sig_idx)
{
    float **s = *signals;
    pthread_mutex_lock(&rp_osc_sig_mutex);
    if(rp_osc_signals_dirty == 0) {
        *sig_idx = rp_osc_sig_last_idx;
        pthread_mutex_unlock(&rp_osc_sig_mutex);
        return -1;
    }

    memcpy(&s[0][0], &rp_osc_signals[0][0], sizeof(float)*SIGNAL_LENGTH);
    memcpy(&s[1][0], &rp_osc_signals[1][0], sizeof(float)*SIGNAL_LENGTH);
    memcpy(&s[2][0], &rp_osc_signals[2][0], sizeof(float)*SIGNAL_LENGTH);

    *sig_idx = rp_osc_sig_last_idx;

    rp_osc_signals_dirty = 0;
    pthread_mutex_unlock(&rp_osc_sig_mutex);
    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_set_signals(float **source, int index)
{
    pthread_mutex_lock(&rp_osc_sig_mutex);

    memcpy(&rp_osc_signals[0][0], &source[0][0], sizeof(float)*SIGNAL_LENGTH);
    memcpy(&rp_osc_signals[1][0], &source[1][0], sizeof(float)*SIGNAL_LENGTH);
    memcpy(&rp_osc_signals[2][0], &source[2][0], sizeof(float)*SIGNAL_LENGTH);
    rp_osc_sig_last_idx = index;

    rp_osc_signals_dirty = 1;
    pthread_mutex_unlock(&rp_osc_sig_mutex);

    return 0;
}


/*----------------------------------------------------------------------------------*/
void *rp_osc_worker_thread(void *args)
{
    rp_osc_worker_state_t old_state, state;
    rp_app_params_t      *curr_params = NULL;
    int                   fpga_update = 0;
    int                   params_dirty = 0;
    int calout1_state=0;
      

    /* Long acquisition special function */
    int long_acq = 0; /* long_acq if acq_time > 1 [s] */
    int long_acq_idx = 0;

    //pthread_mutex_lock(&rp_osc_ctrl_mutex);
    //old_state = state = rp_osc_ctrl;
    //pthread_mutex_unlock(&rp_osc_ctrl_mutex);

    while(1) {
        /* update states - we save also old state to see if we need to reset
         * FPGA 
         */
        old_state = state;
        pthread_mutex_lock(&rp_osc_ctrl_mutex);
        //state = rp_osc_ctrl;
        if(rp_osc_params_dirty) {
            rp_copy_params(rp_osc_params, (rp_app_params_t **)&curr_params);
            fpga_update = rp_osc_params_fpga_update;

            rp_osc_params_dirty = 0;
        }
        pthread_mutex_unlock(&rp_osc_ctrl_mutex);

        /* request to stop worker thread, we will shut down */
        if(state == rp_osc_quit_state) {
            rp_clean_params(curr_params);
            return 0;
        }
        if(fpga_update) {
            osc_fpga_reset();
            //if(osc_fpga_update_params( ) {
            //    fprintf(stderr, "Setting of FPGA registers failed\n");
            //    rp_osc_worker_change_state(rp_osc_idle_state);
            //}

            fpga_update = 0;
        }

        if(state == rp_osc_idle_state) {
            usleep(10000);
            continue;
        }

        /* start working */
        pthread_mutex_lock(&rp_osc_ctrl_mutex);
        old_state = state = rp_osc_ctrl;
        pthread_mutex_unlock(&rp_osc_ctrl_mutex);
        if((state == rp_osc_idle_state) || (state == rp_osc_abort_state)) {
            continue;
        } else if(state == rp_osc_quit_state) {
            break;
        }

        if(long_acq_idx == 0) {
            /* polling until data is ready */
            while(1) {
                pthread_mutex_lock(&rp_osc_ctrl_mutex);
                state = rp_osc_ctrl;
                params_dirty = rp_osc_params_dirty;
                pthread_mutex_unlock(&rp_osc_ctrl_mutex);
                /* change in state, abort polling */
                if((state != old_state) || params_dirty) {
                    break;
                }
                
                if(!long_acq && osc_fpga_triggered()) {
                    /* for non-long acquisition wait for trigger */
                    break;
                } else if(long_acq) {
                    int trig_ptr, curr_ptr;
                    osc_fpga_get_wr_ptr(&curr_ptr, &trig_ptr);
                }
                usleep(1000);
            }
        }

        if((state != old_state) || params_dirty) {
            params_dirty = 0;
            continue;
        }
        if(long_acq) {
            /* Long acquisition - after trigger wait for a while to collect some
            * data
            */
            
            /* we are after trigger - so let's wait a while to collect some 
            * samples */
            usleep(200000); /* Sleep for 200 [ms] */
        }

        pthread_mutex_lock(&rp_osc_ctrl_mutex);
        state = rp_osc_ctrl;
        params_dirty = rp_osc_params_dirty;
        pthread_mutex_unlock(&rp_osc_ctrl_mutex);

        if((state != old_state) || params_dirty) {
            continue;

        }

        /* check again for change of state */
        pthread_mutex_lock(&rp_osc_ctrl_mutex);
        state = rp_osc_ctrl;
        pthread_mutex_unlock(&rp_osc_ctrl_mutex);
        
	    // Wait next acquisition when average ready
        if (calout1_state==2)
	    {
	      rp_update_main_params(curr_params); // Update signal generator with new calibration constants		 	      
	    }
	    
        /* do not loop too fast */
        usleep(10000);
    }

    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_prepare_time_vector(float **out_signal, int dec_factor,
                               float t_start, float t_stop, int time_unit)
{
    float smpl_period = c_osc_fpga_smpl_period * dec_factor;
    float t_step, t_curr;
    int   out_idx, in_idx;
    int   idx_step;
    int   t_unit_factor = rp_osc_get_time_unit_factor(time_unit);;

    float *s = *out_signal;

    if(t_stop <= t_start) {
        t_start = 0;
        t_stop  = OSC_FPGA_SIG_LEN * smpl_period;
    }

    t_step = (t_stop - t_start) / (SIGNAL_LENGTH-1);
    idx_step = (int)(ceil(t_step/smpl_period));
    if(idx_step > 8)
        idx_step = 8;

    for(out_idx = 0, in_idx = 0, t_curr=t_start; out_idx < SIGNAL_LENGTH; 
        out_idx++, t_curr += t_step, in_idx += idx_step) {
        s[out_idx] = t_curr * t_unit_factor;
    }
    
    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_decimate(float **cha_signal, int *in_cha_signal,
                    float **chb_signal, int *in_chb_signal,
                    float **time_signal, int dec_factor, 
                    float t_start, float t_stop, int time_unit,
                    rp_osc_meas_res_t *ch1_meas, rp_osc_meas_res_t *ch2_meas,
                    float ch1_max_adc_v, float ch2_max_adc_v,
                    float ch1_user_dc_off, float ch2_user_dc_off, int save_trace)
{
    int t_start_idx, t_stop_idx;
    float smpl_period = c_osc_fpga_smpl_period * dec_factor;
    int   t_unit_factor = rp_osc_get_time_unit_factor(time_unit);
    int t_step;
    int in_idx, out_idx, t_idx;
    int wr_ptr_curr, wr_ptr_trig;

    float *cha_s = *cha_signal;
    float *chb_s = *chb_signal;
    float *t = *time_signal;
    
    /* If illegal take whole frame */
    if(t_stop <= t_start) {
        t_start = 0;
        t_stop = (OSC_FPGA_SIG_LEN-1) * smpl_period;
    }
    
    /* convert time to samples */
    t_start_idx = round(t_start / smpl_period);
    t_stop_idx  = round(t_stop / smpl_period);

    if((((t_stop_idx-t_start_idx)/(float)(SIGNAL_LENGTH-1))) < 1)
        t_step = 1;
    else {
        /* ceil was used already in rp_osc_main() for parameters, so we can easily
         * use round() here 
         */
        t_step = round((t_stop_idx-t_start_idx)/(float)(SIGNAL_LENGTH-1));
    }
    osc_fpga_get_wr_ptr(&wr_ptr_curr, &wr_ptr_trig);
    in_idx = wr_ptr_trig + t_start_idx - 3;

    if(in_idx < 0) 
        in_idx = OSC_FPGA_SIG_LEN + in_idx;
    if(in_idx >= OSC_FPGA_SIG_LEN)
        in_idx = in_idx % OSC_FPGA_SIG_LEN;

    /* First perform measurements on non-decimated signal:
     *  - min, max - performed in the loop
     *  - avg, amp - performed after the loop
     *  - freq, period - performed in the next decimation loop
     */
    for(out_idx=0; out_idx < OSC_FPGA_SIG_LEN; out_idx++) {
        rp_osc_meas_min_max(ch1_meas, in_cha_signal[out_idx]);
        rp_osc_meas_min_max(ch2_meas, in_chb_signal[out_idx]);
    }

    // Saving data traces from 16k buffer if required (before decimation)
    if (save_trace==1)
    {
      int in_idx2,ix_cnt;
      
      float t_tmp,a_tmp,b_tmp;
      
      
      FILE * fp;
       
      fp=fopen("/tmp/traces.csv","w+");
      
      for (in_idx2=in_idx, ix_cnt=0; ix_cnt< OSC_FPGA_SIG_LEN;in_idx2++, ix_cnt++)
      {
        /* Wrap the pointer */
        if(in_idx2 >= OSC_FPGA_SIG_LEN)
            in_idx2 = in_idx2 % OSC_FPGA_SIG_LEN;

	// Converts ADC counts to Volts (including DC offset calibration)
	    
	a_tmp=osc_fpga_cnv_cnt_to_v(in_cha_signal[in_idx2], ch1_max_adc_v,
                                               rp_calib_params->fe_ch1_dc_offs,
                                               ch1_user_dc_off);
        b_tmp=osc_fpga_cnv_cnt_to_v(in_chb_signal[in_idx2], ch2_max_adc_v,
                                               rp_calib_params->fe_ch2_dc_offs,
                                               ch2_user_dc_off);
	t_tmp=t_start+ix_cnt*smpl_period;
	
	fprintf(fp, "%4.9f %4.6f %4.6f  \n",t_tmp, a_tmp, b_tmp);
	
      }
      fclose(fp);
      
    }
    
    
    
    
    
    
    for(out_idx=0, t_idx=0; out_idx < SIGNAL_LENGTH; 
        out_idx++, in_idx+=t_step, t_idx+=t_step) {
        /* Wrap the pointer */
        if(in_idx >= OSC_FPGA_SIG_LEN)
            in_idx = in_idx % OSC_FPGA_SIG_LEN;

        cha_s[out_idx] = osc_fpga_cnv_cnt_to_v(in_cha_signal[in_idx], ch1_max_adc_v,
                                               rp_calib_params->fe_ch1_dc_offs,
                                               ch1_user_dc_off);

        chb_s[out_idx] = osc_fpga_cnv_cnt_to_v(in_chb_signal[in_idx], ch2_max_adc_v,
                                               rp_calib_params->fe_ch2_dc_offs,
                                               ch2_user_dc_off);

        t[out_idx] = (t_start + (t_idx * smpl_period)) * t_unit_factor;

        /* A bug in FPGA? - Trig & write pointers not sample-accurate. */
        if ( (dec_factor > 64) && (out_idx == 1) ) {
            int i;
            for (i=0; i < out_idx; i++) {
                cha_s[i] = cha_s[out_idx];
                chb_s[i] = chb_s[out_idx];
            }
        }
    }

    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_decimate_partial(float **cha_out_signal, int *cha_in_signal, 
                            float **chb_out_signal, int *chb_in_signal,
                            float **time_out_signal, int *next_wr_ptr, 
                            int last_wr_ptr, int step_wr_ptr, int next_out_idx,
                            float t_start, int dec_factor, int time_unit,
                            rp_osc_meas_res_t *ch1_meas, 
                            rp_osc_meas_res_t *ch2_meas,
                            float ch1_max_adc_v, float ch2_max_adc_v,
                            float ch1_user_dc_off, float ch2_user_dc_off)
{
    float *cha_out = *cha_out_signal;
    float *chb_out = *chb_out_signal;
    float *t_out   = *time_out_signal;
    int    in_idx = *next_wr_ptr;

    float smpl_period = c_osc_fpga_smpl_period * dec_factor;
    int   t_unit_factor = rp_osc_get_time_unit_factor(time_unit);

    int curr_ptr;
    /* check if we have reached currently acquired signals in FPGA */
    osc_fpga_get_wr_ptr(&curr_ptr, NULL);

    for(; in_idx < curr_ptr; in_idx++) {
        if(in_idx >= OSC_FPGA_SIG_LEN)
            in_idx = in_idx % OSC_FPGA_SIG_LEN;
        rp_osc_meas_min_max(ch1_meas, cha_in_signal[in_idx]);
        rp_osc_meas_min_max(ch2_meas, chb_in_signal[in_idx]);
    }

    in_idx = *next_wr_ptr;

    for(; (next_out_idx < SIGNAL_LENGTH); next_out_idx++, 
            in_idx += step_wr_ptr) {
        int curr_ptr;
        int diff_ptr;
        /* check if we have reached currently acquired signals in FPGA */
        osc_fpga_get_wr_ptr(&curr_ptr, NULL);
        if(in_idx >= OSC_FPGA_SIG_LEN)
            in_idx = in_idx % OSC_FPGA_SIG_LEN;
        diff_ptr = (in_idx-curr_ptr);
        /* Check that we did not hit the curr ptr (and that pointer is not
         * wrapped 
         */
        if((in_idx >= curr_ptr) && (diff_ptr > 0) && (diff_ptr < 100))
            break;

        cha_out[next_out_idx] = 
            osc_fpga_cnv_cnt_to_v(cha_in_signal[in_idx], ch1_max_adc_v,
                                  rp_calib_params->fe_ch1_dc_offs,
                                  ch1_user_dc_off);

        chb_out[next_out_idx] = 
            osc_fpga_cnv_cnt_to_v(chb_in_signal[in_idx], ch2_max_adc_v,
                                  rp_calib_params->fe_ch1_dc_offs,
                                  ch2_user_dc_off);

        t_out[next_out_idx]   = 
            (t_start + ((next_out_idx*step_wr_ptr)*smpl_period))*t_unit_factor;

        /* A bug in FPGA? - Trig & write pointers not sample-accurate. */
         if ( (dec_factor > 64) && (next_out_idx == 2) ) {
             int i;
             for (i=0; i < next_out_idx; i++) {
                 cha_out[i] = cha_out[next_out_idx];
                 chb_out[i] = chb_out[next_out_idx];
             }
         }
    }

    *next_wr_ptr = in_idx;

    return next_out_idx;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_get_time_unit_factor(int time_unit)
{
    int t_unit_factor;

    switch(time_unit) {
    case 0:
        /* [us] */
        t_unit_factor = 1e6;
        break;
    case 1:
        /* [ms] */
        t_unit_factor = 1e3;
        break;
    case 2:
    default:
        /* [s] */
        t_unit_factor = 1;
        break;
    }

    return t_unit_factor;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_meas_clear(rp_osc_meas_res_t *ch_meas)
{
    ch_meas->min = 1e9;
    ch_meas->max = -1e9;
    ch_meas->amp = 0;
    ch_meas->avg = 0;
    ch_meas->freq = 0;
    ch_meas->period = 0;

    return 0;
}


/*----------------------------------------------------------------------------------*/
inline int rp_osc_adc_sign(int in_data)
{
    int s_data = in_data;
    if(s_data & (1<<(c_osc_fpga_adc_bits-1)))
        s_data = -1 * ((s_data ^ ((1<<c_osc_fpga_adc_bits)-1)) + 1);
    return s_data;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_meas_min_max(rp_osc_meas_res_t *ch_meas, int sig_data)
{
    int s_data = rp_osc_adc_sign(sig_data);

    if(ch_meas->min > s_data)
        ch_meas->min = s_data;
    if(ch_meas->max < s_data)
        ch_meas->max = s_data;

    ch_meas->avg += s_data;

    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_meas_avg_amp(rp_osc_meas_res_t *ch_meas, int avg_len)
{
    ch_meas->avg /= avg_len;
    ch_meas->amp = ch_meas->max - ch_meas->min;
    return 0;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_mean_dc_offset(rp_osc_meas_res_t *ch_meas)
{    
    return ch_meas->avg;
}


/*----------------------------------------------------------------------------------*/
int rp_osc_meas_period(rp_osc_meas_res_t *ch1_meas, rp_osc_meas_res_t *ch2_meas, 
                       int *in_cha_signal, int *in_chb_signal, int dec_factor)
{
    int wr_ptr_curr, wr_ptr_trig;

    // Checking where acquisition starts
    osc_fpga_get_wr_ptr(&wr_ptr_curr, &wr_ptr_trig);

    int min, max; // Ignored for measurement panel calculations
    meas_period(ch1_meas, in_cha_signal, wr_ptr_trig, dec_factor, &min, &max);
    meas_period(ch2_meas, in_chb_signal, wr_ptr_trig, dec_factor, &min, &max);

    return 0;
}

/*----------------------------------------------------------------------------------*/
int meas_period(rp_osc_meas_res_t *meas, int *in_signal, int wr_ptr_trig, int dec_factor,
                int *min, int *max)
{
    const float c_meas_freq_thr = 100;
    const int c_meas_time_thr = OSC_FPGA_SIG_LEN / 8;
    const float c_min_period = 19.6e-9; // 51 MHz

    float thr1, thr2, cen;
    int state = 0;
    int trig_t[2] = { 0, 0 };
    int trig_cnt = 0;
    int ix, ix_corr;

    float acq_dur=(float)(OSC_FPGA_SIG_LEN)/((float) c_osc_fpga_smpl_freq) * (float) dec_factor;

    cen = (meas->max + meas->min) / 2;

    thr1 = cen + 0.2 * (meas->min - cen);
    thr2 = cen + 0.2 * (meas->max - cen);

    meas->period = 0;
    *max = INT_MIN;
    *min = INT_MAX;

    for(ix = 0; ix < (OSC_FPGA_SIG_LEN); ix++) {
        ix_corr = ix + wr_ptr_trig;

        if (ix_corr >= OSC_FPGA_SIG_LEN) {
            ix_corr %= OSC_FPGA_SIG_LEN;
        }

        int sa = rp_osc_adc_sign(in_signal[ix_corr]);

        /* Another max, min calculation at lower rate to avoid evaluation errors on slower signals */
        if (sa > *max)
            *max = sa;
        if (sa < *min)
            *min = sa;

        /* Lower transitions */
        if((state == 0) && (ix_corr > 0) && (sa < thr1)) {
            state = 1;
        }

        /* Upper transitions - count them & store edge times. */
        if((state == 1) && (sa >= thr2) ) {
            state = 0;
            if (trig_cnt++ == 0) {
                trig_t[0] = ix;
            } else {
                trig_t[1] = ix;
            }
        }

        if ((trig_t[1] - trig_t[0]) > c_meas_time_thr) {
            break;
        }
    }

    /* Period calculation - taking into account at least meas_time_thr samples */
    if(trig_cnt >= 2) {
        meas->period = (trig_t[1] - trig_t[0]) /
            ((float)c_osc_fpga_smpl_freq * (trig_cnt - 1)) * dec_factor;
    }

    if( ((thr2 - thr1) < c_meas_freq_thr) ||
         (meas->period * 3 >= acq_dur)    ||
         (meas->period < c_min_period) )
    {
        meas->period = 0;
        meas->freq   = 0;
    } else {
        meas->freq = 1.0 / meas->period;
    }

    return 0;
}


/*----------------------------------------------------------------------------------*/
inline float rp_osc_meas_cnv_cnt(float data, float adc_max_v)
{
    return (data * adc_max_v / (float)(1<<(c_osc_fpga_adc_bits-1)));
}


/*----------------------------------------------------------------------------------*/
int rp_osc_meas_convert(rp_osc_meas_res_t *ch_meas, float adc_max_v, int32_t cal_dc_offs)
{
    ch_meas->min = rp_osc_meas_cnv_cnt(ch_meas->min+cal_dc_offs, adc_max_v);
    ch_meas->max = rp_osc_meas_cnv_cnt(ch_meas->max+cal_dc_offs, adc_max_v);
    ch_meas->amp = rp_osc_meas_cnv_cnt(ch_meas->amp, adc_max_v);
    ch_meas->avg = rp_osc_meas_cnv_cnt(ch_meas->avg+cal_dc_offs, adc_max_v);

    return 0;
}
