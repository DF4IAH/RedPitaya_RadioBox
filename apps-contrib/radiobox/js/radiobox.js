/*
 * Red Pitaya RadioBox client
 *
 * Author: Ulrich Habel (DF4IAH) <espero@gmx.net>
 *         
 * (c) Red Pitaya  http://www.redpitaya.com
 *
*/

(function(){
    var originalAddClassMethod = jQuery.fn.addClass;
    var originalRemoveClassMethod = jQuery.fn.removeClass;
    $.fn.addClass = function(clss){
        var result = originalAddClassMethod.apply(this, arguments);
        $(this).trigger('activeChanged', 'add');
        return result;
    };
    $.fn.removeClass = function(clss){
        var result = originalRemoveClassMethod.apply(this, arguments);
        $(this).trigger('activeChanged', 'remove');
        return result;
    }
})();

(function(RB, $, undefined) {

  // App configuration
  RB.config = {};
  RB.config.app_id = 'radiobox';
  RB.config.server_ip = '';  // Leave empty on production, it is used for testing only
  RB.config.start_app_url = (RB.config.server_ip.length ? 'http://' + RB.config.server_ip : '') + '/bazaar?start=' + RB.config.app_id + '?' + location.search.substr(1);
  RB.config.stop_app_url = (RB.config.server_ip.length ? 'http://' + RB.config.server_ip : '') + '/bazaar?stop=' + RB.config.app_id;
  RB.config.socket_url = 'ws://' + (RB.config.server_ip.length ? RB.config.server_ip : window.location.hostname) + ':9002';  // WebSocket server URI
  RB.config.graph_colors = {
    'ch1' : '#f3ec1a',
    'ch2' : '#31b44b',
    'output1': '#9595ca',
    'output2': '#ee3739',
    'math': '#ab4d9d',
    'trig': '#75cede'
  };
  
  // Time scale steps in millisecods
  RB.time_steps = [
    // Nanoseconds
    100/1000000, 200/1000000, 500/1000000,
    // Microseconds
    1/1000, 2/1000, 5/1000, 10/1000, 20/1000, 50/1000, 100/1000, 200/1000, 500/1000,
    // Millisecods
    1, 2, 5, 10, 20, 50, 100, 200, 500,
    // Seconds
    1*1000, 2*1000, 5*1000, 10*1000, 20*1000, 50*1000
  ];
  
  // Voltage scale steps in volts
  RB.voltage_steps = [
    // Millivolts
    1/1000, 2/1000, 5/1000, 10/1000, 20/1000, 50/1000, 100/1000, 200/1000, 500/1000,
    // Volts
    1, 2, 5
  ];
  
  // Sampling rates
  RB.sample_rates = ['125M', '15.625M', '1.953M', '122.070k', '15.258k', '1.907k'];
  
  // App state
  RB.state = {
    socket_opened: false,
    processing: false,
    editing: false,
    trig_dragging: false,
    cursor_dragging: false,
    resized: false,
    sel_sig_name: null,
    fine: false,
	graph_grid_height: null,
	graph_grid_width: null,
	calib: 0	
  };
  
  // Params cache
  RB.params = { 
    orig: {}, 
    local: {}
  };

  // Other global variables
  RB.ws = null;
  RB.graphs = {};
  RB.touch = {};
  
  RB.connect_time;
  
  // Starts the oscilloscope application on server
  RB.startApp = function() {
    $.get(
      RB.config.start_app_url
    )
    .done(function(dresult) {
      if(dresult.status == 'OK') {
		 RB.connectWebSocket();
      }
      else if(dresult.status == 'ERROR') {
        console.log(dresult.reason ? dresult.reason : 'Could not start the application (ERR1)');
      }
      else {
        console.log('Could not start the application (ERR2)');
      }
    })
    .fail(function() {
      console.log('Could not start the application (ERR3)');
    });
  };
  
  // Creates a WebSocket connection with the web server  
  RB.connectWebSocket = function() {
    
    if(window.WebSocket) {
      RB.ws = new WebSocket(RB.config.socket_url);
    } 
    else if(window.MozWebSocket) {
      RB.ws = new MozWebSocket(RB.config.socket_url);
    } 
    else {
      console.log('Browser does not support WebSocket');
    }
    
    // Define WebSocket event listeners
    if(RB.ws) {
    
      RB.ws.onopen = function() {
        RB.state.socket_opened = true;
        console.log('Socket opened');
        
		RB.params.local['in_command'] = { value: 'send_all_params' };
		RB.ws.send(JSON.stringify({ parameters: RB.params.local }));
		RB.params.local = {};        
      };
      
      RB.ws.onclose = function() {
        RB.state.socket_opened = false;
        $('#graphs .plot').hide();  // Hide all graphs
        console.log('Socket closed');
      };
      
      RB.ws.onerror = function(ev) {
        console.log('Websocket error: ', ev);
      };
        
      RB.ws.onmessage = function(ev) {
        if(RB.state.processing) {
          return;
        }
        RB.state.processing = true;
        
        var receive = JSON.parse(ev.data);

        if(receive.parameters) {
          if((Object.keys(RB.params.orig).length == 0) && (Object.keys(receive.parameters).length == 0)) {
            RB.params.local['in_command'] = { value: 'send_all_params' };
            RB.ws.send(JSON.stringify({ parameters: RB.params.local }));
            RB.params.local = {};
          } else {
            RB.processParameters(receive.parameters);
          }
        }
        
        if(receive.signals) {
          RB.processSignals(receive.signals);
        }
        
        RB.state.processing = false;
      };
    }
  };

  // Processes newly received values for parameters
  RB.processParameters = function(new_params) {
    var old_params = $.extend(true, {}, RB.params.orig);
    
    var send_all_params = Object.keys(new_params).indexOf('send_all_params') != -1;
    for(var param_name in new_params) {
      
      // Save new parameter value
      RB.params.orig[param_name] = new_params[param_name];
      
	  if (param_name.indexOf('RB_MEAS_VAL') == 0) {
		  var orig_units = $("#"+param_name).parent().children("#RB_MEAS_ORIG_UNITS").text();
		  var orig_function = $("#"+param_name).parent().children("#RB_MEAS_ORIG_FOO").text();
		  var orig_source = $("#"+param_name).parent().children("#RB_MEAS_ORIG_SIGNAME").text();
		  var y = new_params[param_name].value;
		  var z = y;
		  var factor = '';
					  
		  if (orig_function == "PERIOD")
		  {
			  y /= 1000; // Now in seconds and not ms
			  z = y;
			  orig_units = 's';
			  if (y < 0.000000010)
				new_params[param_name].value = 'OVER RANGE';
			  else if (y >= 0.000000010 && y <= 0.00000099990)
			  {
				  z*=1e9; factor = 'n';
				  new_params[param_name].value = z.toFixed(0);
			  }
			  else if (y > 0.00000099990 && y <= 0.00099990)
			  {
				  z*=1e6; factor = 'µ';
				  new_params[param_name].value = z.toFixed(1);
			  }
			  else if (y > 0.00099990 && y <= 0.99990)
			  {
				  z*=1e3; factor = 'm';
				  new_params[param_name].value = z.toFixed(2);
			  }
			  else if (y > 0.99990 && y <= 8.5901)
			  {
				  new_params[param_name].value = z.toFixed(3);
			  } else 
				  new_params[param_name].value = 'NO EDGES';
				  
		  } else if (orig_function == "FREQ")
		  {
			  if (y < 0.12)
				new_params[param_name].value = 'NO EDGES';
			  else if (y >= 0.12 && y <= 0.99990)
			  {
				  z*=1e3; factor = 'm';
				  new_params[param_name].value = z.toFixed(0);
			  }
			  else if (y > 0.99990 && y <= 999.990)
			  {
				  new_params[param_name].value = z.toFixed(2);
			  } else if (y > 999.990 && y <= 999900.0)
			  {
				  z/=1e3; factor = 'k';
				  new_params[param_name].value = z.toFixed(2);
			  } else if (y > 999900.0 && y <= 9999900.0)
			  {
				  z/=1e6; factor = 'M';
				  new_params[param_name].value = z.toFixed(3);
			  } else if (y > 9999900.0 && y <= 50000000.0)
			  {
				  z/=1e6; factor = 'M';
				  new_params[param_name].value = z.toFixed(2);
			  } else 
				  new_params[param_name].value = 'OVER RANGE';
		  } else if (orig_function == "DUTY CYCLE")
		  {
			  if (y < 0 || y > 100)
				new_params[param_name].value = 'ERROR';
			  else 
				new_params[param_name].value = z.toFixed(1);
				
		  } else // P2P, MEAN, MAX, MIN, RMS
		  {
			  y = Math.abs(y);
			  if(orig_source == "MATH") 
			  {
				  if(y < 0.00000000000010)
						new_params[param_name].value = 'No signal';
				  else if(y > 0.00000000000010 && y <= 0.000000999990)
				  {	
					  z*=1e9; factor = 'n';
					  if(y > 0.00000000000010 && y <= 0.00000000999990)
						new_params[param_name].value = z.toFixed(4);
					  else if(y > 0.00000000999990 && y <= 0.0000000999990)
						new_params[param_name].value = z.toFixed(3);
					  else if(y > 0.0000000999990 && y <= 0.000000999990)
						new_params[param_name].value = z.toFixed(2);
				  }
				  else if(y > 0.000000999990 && y <= 0.000999990)
				  {	
					  z*=1e6; factor = 'u';
					  if(y > 0.000000999990 && y <= 0.00000999990)
						new_params[param_name].value = z.toFixed(4);
					  else if(y > 0.00000999990 && y <= 0.0000999990)
						new_params[param_name].value = z.toFixed(3);
					  else if(y > 0.0000999990 && y <= 0.000999990)
						new_params[param_name].value = z.toFixed(2);
				  }
				  else if(y > 0.000999990 && y <= 0.999990)
				  {	
					  z*=1e3; factor = 'm';
					  if(y > 0.000999990 && y <= 0.00999990)
						new_params[param_name].value = z.toFixed(4);
					  else if(y > 0.00999990 && y <= 0.0999990)
						new_params[param_name].value = z.toFixed(3);
					  else if(y > 0.0999990 && y <= 0.999990)
						new_params[param_name].value = z.toFixed(2);
				  }
				  else if(y > 0.999990 && y <= 9999.90)
				  {	
					  if(y > 0.999990 && y <= 9.99990)
						new_params[param_name].value = z.toFixed(4);
					  else if(y > 9.99990 && y <= 99.9990)
						new_params[param_name].value = z.toFixed(3);
					  else if(y > 99.9990 && y <= 999.990)
						new_params[param_name].value = z.toFixed(2);
					  else if(y > 999.990 && y <= 9999.90)
						new_params[param_name].value = z.toFixed(1);
				  }
				  else if(y > 9999.90 && y <= 999990.0)
				  {	
					  z/=1e3;  factor = 'k';
					  if(y > 9999.90 && y <= 99999.0)
						new_params[param_name].value = z.toFixed(3);
					  else if(y > 99999.0 && y <= 999990.0)
						new_params[param_name].value = z.toFixed(2);
				  }
				  else if(y > 999990.0 && y <= 999990000.0)
				  {	
					  z/=1e6; factor = 'M';
					  if(y > 999990.0 && y <= 9999900.0)
						new_params[param_name].value = z.toFixed(4);
					  else if(y > 9999900.0 && y <= 99999000.0)
						new_params[param_name].value = z.toFixed(3);
					  else if(y > 99999000.0 && y <= 999990000.0)
						new_params[param_name].value = z.toFixed(2);
				  }
			  } else { // CH1 or CH2
				  if (y < 0.00010)
					new_params[param_name].value = 'LOW SIGNAL';
				  else if (y >= 0.00010 && y <= 0.99990)
				  {
					  z*=1e3; factor = 'm';
					  new_params[param_name].value = z.toFixed(1);
				  } else if (y > 0.99990 && y <= 9.9990)
				  {
					  new_params[param_name].value = z.toFixed(3);
				  } else if (y > 9.9990 && y <= 99.990)
				  {
					  new_params[param_name].value = z.toFixed(2);
				  } else if (y > 99.990 && y <= 999.90)
				  {
					  new_params[param_name].value = z.toFixed(1);
				  } else if (y > 999.90 && y <= 4000.0)
				  {
					  z/=1e3; factor = 'k';
					  new_params[param_name].value = z.toFixed(1);
				  } else 
				  {
					  new_params[param_name].value = "OVER RANGE";
				  }
			  }
		  }
		  $("#"+param_name).parent().children("#RB_MEAS_UNITS").text(factor + orig_units);
	  }

      // Run/Stop button
      if(param_name == 'RB_RUN') {
        if(new_params[param_name].value === true) {
          $('#RB_RUN').hide();
          $('#RB_STOP').css('display','block');
        }
        else {
          $('#RB_STOP').hide();
          $('#RB_RUN').show();
        }
      }
      // Buffer size parameter
      else if(param_name == 'RB_VIEV_PART') {
        var full_width = $('#buffer').width() - 4;
        var visible_width = full_width * new_params['RB_VIEV_PART'].value;
        
        $('#buffer .buf-red-line').width(visible_width).show();
        $('#buffer .buf-red-line-holder').css('left', full_width / 2 - visible_width / 2);
      }
      // Sampling rate
      else if(param_name == 'RB_SAMPL_RATE') {
        $('#' + param_name).html(RB.sample_rates[new_params[param_name].value] + 'S/s');
      }
      // All other parameters
      else {
		if (['CALIB_RESET', 'CALIB_FE_OFF', 'CALIB_FE_SCALE_LV', 'CALIB_FE_SCALE_HV', 'CALIB_BE'].indexOf(param_name) != -1 && !send_all_params) {
			if (new_params[param_name].value == -1) {
				++RB.state.calib;
				RB.setCalibState(RB.state.calib);
				
				$('#calib-2').children().removeAttr('disabled');
				$('#calib-3').children().removeAttr('disabled');
			} else if (new_params[param_name].value == 0) {				
				$('#modal-warning').show();
				$('#calib-4').show();
				$('#calib-5').show();
				$('#calib-2').children().removeAttr('disabled');
				$('#calib-3').children().removeAttr('disabled');
			}
			
			new_params[param_name].value = -2;
		}
		if (param_name == 'is_demo' && new_params['is_demo'].value && RB.state.calib == 0) {
			RB.setCalibState(RB.state.calib);		
			$('#calib-2').children().attr('disabled', 'true');
			$('#calib-3').children().attr('disabled', 'true');
			$('#calib-text').html('Calibration is not available in demo mode');
		} else if (param_name == 'is_demo' && !new_params['is_demo'].value && RB.state.calib == 0) {
			$('#calib-text').html('Calibration of fast analog inputs and outputs is started. To proceed with calibration press CONTINUE. For factory calibration settings press DEFAULT.');
		}
		  
        if (param_name == 'RB_TRIG_INFO') {
			var idx = new_params['RB_TRIG_INFO'].value;
			var states = ['STOPPED', 'AUTO', 'TRIG\'D', 'WAITING'];
			var colors = ['red', 'green', 'green', 'yellow'];
			
			$('#triginfo').html(states[idx]);
			$('#triginfo').css('color', colors[idx]);
			$('#triginfo').css('display', '');
		}
        // Show/hide Y offset arrows
        if(param_name == 'RB_CH1_OFFSET' && new_params['CH1_SHOW']) {
          if(new_params['CH1_SHOW'].value) {
            
            // Change arrow position only if arrow is hidden or old/new values are not the same
            if(!$('#ch1_offset_arrow').is(':visible') 
                || old_params[param_name].value != new_params[param_name].value 
                || old_params['RB_CH1_SCALE'].value != new_params['RB_CH1_SCALE'].value
				|| (RB.state.graph_grid_height && RB.state.graph_grid_height !== $('#graph_grid').outerHeight())) {
              var volt_per_px = (new_params['RB_CH1_SCALE'].value * 10) / $('#graph_grid').outerHeight();
              var px_offset = -(new_params['RB_CH1_OFFSET'].value / volt_per_px - parseInt($('#ch1_offset_arrow').css('margin-top')) / 2);
			  RB.state.graph_grid_height = $('#graph_grid').outerHeight();
              $('#ch1_offset_arrow').css('top', ($('#graph_grid').outerHeight() + 7) / 2 + px_offset).show();
            }
          }
          else {
            $('#ch1_offset_arrow').hide();
          }
        }
        else if(param_name == 'RB_CH2_OFFSET' && new_params['CH2_SHOW']) {
          if(new_params['CH2_SHOW'].value) {
            
            // Change arrow position only if arrow is hidden or old/new values are not the same
            if(!$('#ch2_offset_arrow').is(':visible') 
				|| old_params[param_name].value != new_params[param_name].value
				|| (RB.state.graph_grid_height && RB.state.graph_grid_height !== $('#graph_grid').outerHeight())) {
              var volt_per_px = (new_params['RB_CH2_SCALE'].value * 10) / $('#graph_grid').outerHeight();
              var px_offset = -(new_params['RB_CH2_OFFSET'].value / volt_per_px - parseInt($('#ch2_offset_arrow').css('margin-top')) / 2);
			  RB.state.graph_grid_height = $('#graph_grid').outerHeight();
              $('#ch2_offset_arrow').css('top', ($('#graph_grid').outerHeight() + 7) / 2 + px_offset).show();
            }
          }
          else {
            $('#ch2_offset_arrow').hide();
          }
        }
		else if(param_name == 'SOUR1_VOLT_OFFS') {
			if((!RB.state.editing && (old_params[param_name] !== undefined && old_params[param_name].value == new_params[param_name].value))){
				var value = $('#SOUR1_VOLT_OFFS').val();
				if(value !== new_params[param_name].value){
					//$('#SOUR1_VOLT_OFFS').val(new_params[param_name].value);
					RB.setValue($('#SOUR1_VOLT_OFFS'), new_params[param_name].value);
				}
			}
		}
		else if(param_name == 'SOUR2_VOLT_OFFS') {
			if((!RB.state.editing && (old_params[param_name] !== undefined && old_params[param_name].value == new_params[param_name].value))){
				var value = $('#SOUR2_VOLT_OFFS').val();
				if(value !== new_params[param_name].value){
					//$('#SOUR2_VOLT_OFFS').val(new_params[param_name].value);
					RB.setValue($('#SOUR2_VOLT_OFFS'), new_params[param_name].value);
				}
			}
		}
	    else if(param_name == 'OUTPUT1_SHOW_OFF') {
          if(new_params['OUTPUT1_SHOW'].value && new_params['OUTPUT1_STATE'].value) {
            
            // Change arrow position only if arrow is hidden or old/new values are not the same
            if(!$('#output1_offset_arrow').is(':visible') 
				|| old_params[param_name].value != new_params[param_name].value
				|| (RB.state.graph_grid_height && RB.state.graph_grid_height !== $('#graph_grid').outerHeight())) {
              var graph_height = $('#graph_grid').outerHeight();
              var volt_per_px = 10 / graph_height;
              var px_offset = -(new_params['OUTPUT1_SHOW_OFF'].value / volt_per_px - parseInt($('#output1_offset_arrow').css('margin-top')) / 2);
			  RB.state.graph_grid_height = $('#graph_grid').outerHeight();
              $('#output1_offset_arrow').css('top', (graph_height + 7) / 2 + px_offset).show();
            }
          }
          else {
            $('#output1_offset_arrow').hide();
          }
        }
        else if(param_name == 'OUTPUT2_SHOW_OFF') {
          if(new_params['OUTPUT2_SHOW'].value && new_params['OUTPUT2_STATE'].value) {
            
            // Change arrow position only if arrow is hidden or old/new values are not the same
            if(!$('#output2_offset_arrow').is(':visible') 
				|| old_params[param_name].value != new_params[param_name].value
				|| (RB.state.graph_grid_height && RB.state.graph_grid_height !== $('#graph_grid').outerHeight())) {
              var graph_height = $('#graph_grid').outerHeight();
              var volt_per_px = 10 / graph_height;
              var px_offset = -(new_params['OUTPUT2_SHOW_OFF'].value / volt_per_px - parseInt($('#output2_offset_arrow').css('margin-top')) / 2);
			  RB.state.graph_grid_height = $('#graph_grid').outerHeight();
              $('#output2_offset_arrow').css('top', (graph_height + 7) / 2 + px_offset).show();
            }
          }
          else {
            $('#output2_offset_arrow').hide();
          }
        }
        else if(param_name == 'RB_MATH_OFFSET') {
          if(new_params['MATH_SHOW'].value) {
            
            // Change arrow position only if arrow is hidden or old/new values are not the same
            if(!$('#math_offset_arrow').is(':visible') 
                || old_params[param_name].value != new_params[param_name].value 
                || old_params['RB_MATH_SCALE'].value != new_params['RB_MATH_SCALE'].value
				|| (RB.state.graph_grid_height && RB.state.graph_grid_height !== $('#graph_grid').outerHeight())) {
              var volt_per_px = (new_params['RB_MATH_SCALE'].value * 10) / $('#graph_grid').outerHeight();
              var px_offset = -(new_params['RB_MATH_OFFSET'].value / volt_per_px - parseInt($('#math_offset_arrow').css('margin-top')) / 2);
			  RB.state.graph_grid_height = $('#graph_grid').outerHeight();
              $('#math_offset_arrow').css('top', ($('#graph_grid').outerHeight() + 7) / 2 + px_offset).show();
            }
          }
          else {
            $('#math_offset_arrow').hide();
          }
        }
        // Time offset arrow
        else if(param_name == 'RB_TIME_OFFSET') {
          
          // Change arrow position only if arrow is hidden or old/new values are not the same
          if(!$('#time_offset_arrow').is(':visible') 
			  || old_params[param_name].value != new_params[param_name].value
			  || (RB.state.graph_grid_width && RB.state.graph_grid_width !== $('#graph_grid').outerWidth())) {
            var graph_width = $('#graph_grid').outerWidth();
            var ms_per_px = (new_params['RB_TIME_SCALE'].value * 10) / graph_width;
            var px_offset = -(new_params['RB_TIME_OFFSET'].value / ms_per_px + $('#time_offset_arrow').width()/2 + 1);
            var arrow_left = (graph_width + 2) / 2 + px_offset;
            var buf_width = graph_width - 2;
            var ratio = buf_width / (buf_width * new_params['RB_VIEV_PART'].value);
            RB.state.graph_grid_width = graph_width;
            $('#time_offset_arrow').css('left', arrow_left).show();
            $('#buf_time_offset').css('left', buf_width / 2 - buf_width * new_params['RB_VIEV_PART'].value / 2 + arrow_left / ratio - 4).show();
          }
        }
        // Trigger level
        else if(param_name == 'RB_TRIG_LEVEL' || param_name == 'RB_TRIG_SOURCE') {
          if(! RB.state.trig_dragging) {
            
            // Trigger button is blured out and trigger level is hidden for source 'EXT'
            if(new_params['RB_TRIG_SOURCE'].value > 1) {
              $('#trigger_level, #trig_level_arrow').hide();
              $('#right_menu .menu-btn.trig').prop('disabled', true);
              $('#osc_trig_level_info').html('-');
            }
            else {
              var ref_scale = (new_params['RB_TRIG_SOURCE'].value == 0 ? 'RB_CH1_SCALE' : 'RB_CH2_SCALE');
              var source_offset = (new_params['RB_TRIG_SOURCE'].value == 0 ? new_params['RB_CH1_OFFSET'].value : new_params['RB_CH2_OFFSET'].value);
              var graph_height = $('#graph_grid').outerHeight();
              var volt_per_px = (new_params[ref_scale].value * 10) / graph_height;
              var px_offset = -((new_params['RB_TRIG_LEVEL'].value + source_offset) / volt_per_px - parseInt($('#trig_level_arrow').css('margin-top')) / 2);
              
              $('#trig_level_arrow, #trigger_level').css('top', (graph_height + 7) / 2 + px_offset).show();
              if(param_name == 'RB_TRIG_LEVEL') {
				$('#right_menu .menu-btn.trig').prop('disabled', false);
				$('#osc_trig_level_info').html(RB.convertVoltage(new_params['RB_TRIG_LEVEL'].value));
				
				if((!RB.state.editing && (old_params[param_name] !== undefined && old_params[param_name].value == new_params[param_name].value))){
					var value = $('#RB_TRIG_LEVEL').val();
					if(value !== new_params[param_name].value){
						
						var probeAttenuation = 1;
						var jumperSettings = 1;
						var ch="";
						if($("#RB_TRIG_SOURCE").parent().hasClass("active"))
							ch="CH1";
						else if ($("RB_TRIG_SOURCE2").parent().hasClass("active"))
							ch="CH2";
						else
						{
							probeAttenuation = 1;
						}
						
						if (ch == "CH1" || ch == "CH2")
						{
							probeAttenuation = parseInt($("#RB_"+ch+"_PROBE option:selected").text());
							jumperSettings = $("#RB_"+ch+"_IN_GAIN").parent().hasClass("active") ? 1 : 20;
						}
						//$('#RB_TRIG_LEVEL').val(new_params[param_name].value);
						RB.setValue($('#RB_TRIG_LEVEL'), RB.formatInputValue(new_params[param_name].value, probeAttenuation, false, jumperSettings == 20));
					}
				}
			  }
            }
          }
		   // Trigger source
		  if(param_name == 'RB_TRIG_SOURCE') {
			  var source = new_params['RB_TRIG_SOURCE'].value == 0 ? 'IN1' : (new_params['RB_TRIG_SOURCE'].value == 1 ? 'IN2' : 'EXT');
			$('#osc_trig_source_ch').html(source);
		  }
        }
        // Trigger edge/slope
        else if(param_name == 'RB_TRIG_SLOPE') {
          $('#osc_trig_edge_img').attr('src', (new_params[param_name].value == 1 ? 'img/trig-edge-up.png' : 'img/trig-edge-down.png'));
        }
        // Y cursors
        else if(param_name == 'RB_CURSOR_Y1' || param_name == 'RB_CURSOR_Y2') {
          if(! RB.state.cursor_dragging) {
            var y = (param_name == 'RB_CURSOR_Y1' ? 'y1' : 'y2');
            
            if(new_params[param_name].value) {
              var new_value = new_params[y == 'y1' ? 'RB_CUR1_V' : 'RB_CUR2_V'].value;
              var ref_scale = (new_params['RB_CURSOR_SRC'].value == 0 ? 'RB_CH1_SCALE' : (new_params['RB_CURSOR_SRC'].value == 1 ? 'RB_CH2_SCALE' : 'RB_MATH_SCALE'));
              var source_offset = new_params[new_params['RB_CURSOR_SRC'].value == 0 ? 'RB_CH1_OFFSET' : (new_params['RB_CURSOR_SRC'].value == 1 ? 'RB_CH2_OFFSET' : 'RB_MATH_OFFSET')].value;
              var graph_height = $('#graph_grid').height();
              var volt_per_px = (new_params[ref_scale].value * 10) / graph_height;
              var px_offset = -((new_params[y == 'y1' ? 'RB_CUR1_V' : 'RB_CUR2_V'].value + source_offset) / volt_per_px - parseInt($('#cur_' + y + '_arrow').css('margin-top')) / 2);
              var top = (graph_height + 7) / 2 + px_offset;
              var overflow = false;
              
              if (top < 0)
			  {
				top = 0;
				overflow = true;
			  }
			  if (top > graph_height)
			  {
				top = graph_height;
				overflow = true;
			  }
              
              $('#cur_' + y + '_arrow, #cur_' + y + ', #cur_' + y + '_info').css('top', top).show();
              $('#cur_' + y + '_info')
                .html(RB.convertVoltage(+new_value))
                .data('cleanval', +new_value)
                .css('margin-top', (top < 16 ? 3 : ''));
              if(overflow)
				$('#cur_' + y + '_info').hide();
            }
            else {
              $('#cur_' + y + '_arrow, #cur_' + y + ', #cur_' + y + '_info').hide();
            }
          }
        }
        // X cursors
        else if(param_name == 'RB_CURSOR_X1' || param_name == 'RB_CURSOR_X2') {
          if(! RB.state.cursor_dragging) {
            var x = (param_name == 'RB_CURSOR_X1' ? 'x1' : 'x2');
            
            if(new_params[param_name].value) {
              var new_value = new_params[x == 'x1' ? 'RB_CUR1_T' : 'RB_CUR2_T'].value;
              var graph_width = $('#graph_grid').width();
              var ms_per_px = (new_params['RB_TIME_SCALE'].value * 10) / graph_width;
              var px_offset = -((new_value + new_params['RB_TIME_OFFSET'].value) / ms_per_px - parseInt($('#cur_' + x + '_arrow').css('margin-left')) / 2 - 2.5);
              var msg_width = $('#cur_' + x + '_info').outerWidth();
              var left = (graph_width + 2) / 2 + px_offset;
              
              var overflow = false;
              if (left < 0)
			  {
				left = 0;
				overflow = true;
			  }
			  if (left > graph_width)
			  {
				left = graph_width;
				overflow = true;
			  }
              
              $('#cur_' + x + '_arrow, #cur_' + x + ', #cur_' + x + '_info').css('left', left).show();
              $('#cur_' + x + '_info')
                .html(RB.convertTime(-new_value))
                .data('cleanval', -new_value)
                .css('margin-left', (left + msg_width > graph_width - 2 ? -msg_width - 1 : ''));
                
              if (overflow)
				$('#cur_' + x + '_info').hide();
            }
            else {
              $('#cur_' + x + '_arrow, #cur_' + x + ', #cur_' + x + '_info').hide();
            }
          }
        }
        else if(param_name == 'SOUR1_VOLT') {
          $('#' + param_name + '_info').html(RB.convertVoltage(new_params['RB_OUTPUT1_SCALE'].value));
        }
        else if(param_name == 'SOUR2_VOLT') {
          $('#' + param_name + '_info').html(RB.convertVoltage(new_params['RB_OUTPUT2_SCALE'].value));
        }
        
        // Find the field having ID equal to current parameter name
        // TODO: Use classes instead of ids, to be able to use a param name in multiple fields and to loop through all fields to set new values
        var field = $('#' + param_name);
        
        // Do not change fields from dialogs when user is editing something or new parameter value is the same as the old one
        if(field.closest('.menu-content').length == 0 
            || (!RB.state.editing && (old_params[param_name] === undefined || old_params[param_name].value !== new_params[param_name].value))) {
          
          if(field.is('select') || (field.is('input') && !field.is('input:radio')) || field.is('input:text')) {
				if(param_name == "RB_CH1_OFFSET") 
				{
					var units;
					if (new_params["RB_CH1_SCALE"] != undefined)
					{
						if(Math.abs(new_params["RB_CH1_SCALE"].value) >= 1) {
							units = 'V';
						}
						else if(Math.abs(new_params["RB_CH1_SCALE"].value) >= 0.001) {
							units = 'mV';
						}
					}
					else 
						units = $('#RB_CH1_OFFSET_UNIT').html();
					var multiplier = units == "mV" ? 1000 : 1;
					field.val(RB.formatValue(new_params[param_name].value * multiplier));
				} else if (param_name == "RB_CH2_OFFSET")
				{
					var units;
					if (new_params["RB_CH2_SCALE"] != undefined)
					{
						if(Math.abs(new_params["RB_CH2_SCALE"].value) >= 1) {
							units = 'V';
						}
						else if(Math.abs(new_params["RB_CH2_SCALE"].value) >= 0.001) {
							units = 'mV';
						}
					}
					else 
						units = $('#RB_CH2_OFFSET_UNIT').html();
					var multiplier = units == "mV" ? 1000 : 1;
					field.val(RB.formatValue(new_params[param_name].value * multiplier));
				} else if (param_name == "RB_MATH_OFFSET")
				{
					field.val(RB.formatMathValue(new_params[param_name].value));
				}
				else if (param_name == "RB_TRIG_LEVEL")
				{
					var probeAttenuation = 1;
					var jumperSettings = 1;
					var ch="";
					if($("#RB_TRIG_SOURCE").parent().hasClass("active"))
						ch="CH1";
					else if ($("RB_TRIG_SOURCE2").parent().hasClass("active"))
						ch="CH2";
					else
					{
						probeAttenuation = 1;
					}
					
					if (ch == "CH1" || ch == "CH2")
					{
						probeAttenuation = parseInt($("#RB_"+ch+"_PROBE option:selected").text());
						jumperSettings = $("#RB_"+ch+"_IN_GAIN").parent().hasClass("active") ? 1 : 20;
					}
					field.val(formatInputValue(new_params[param_name].value, probeAttenuation, false, jumperSettings == 20));
				} 
				else if(['SOUR1_DCYC', 'SOUR2_DCYC'].indexOf(param_name) != -1)
				{
					field.val(new_params[param_name].value.toFixed(1));
				} 
				else if(['SOUR1_PHAS', 'SOUR2_PHAS'].indexOf(param_name) != -1)
				{
					field.val(new_params[param_name].value.toFixed(0));
				} else 
					field.val(new_params[param_name].value);
          }
          else if(field.is('button')) {
            field[new_params[param_name].value === true ? 'addClass' : 'removeClass' ]('active');
			//switch green light for output signals
			if(param_name == "OUTPUT1_STATE" || param_name == "OUTPUT2_STATE")
			{
				var sig_name = param_name == "OUTPUT1_STATE" ? 'output1' : 'output2';
				if(new_params[param_name].value === true)
				{
					if (RB.state.sel_sig_name)
						$('#right_menu .menu-btn.' + RB.state.sel_sig_name).removeClass('active');
					RB.state.sel_sig_name = sig_name;
					
					$('#right_menu .menu-btn.' + RB.state.sel_sig_name).addClass('active');		
					$('.y-offset-arrow').css('z-index', 10);
					$('#' + RB.state.sel_sig_name + '_offset_arrow').css('z-index', 11);
				} else 
				{
					if (RB.state.sel_sig_name == sig_name)
					{
						$('#right_menu .menu-btn.' + RB.state.sel_sig_name).removeClass('active');
						RB.state.sel_sig_name = null;
					}
				}
				
				var value = new_params[param_name].value === true ? 1 : 0;
				if(value == 1)
				{
					$('#'+param_name+'_ON').show();
					$('#'+param_name+'_ON').closest('.menu-btn').addClass('state-on');
				}
				else{
					$('#'+param_name+'_ON').hide();
					$('#'+param_name+'_ON').closest('.menu-btn').removeClass('state-on');
				}
			} else if (param_name == "MATH_SHOW")
			{
				var sig_name = "math";
				if(new_params[param_name].value === true)
				{
					if (RB.state.sel_sig_name)
						$('#right_menu .menu-btn.' + RB.state.sel_sig_name).removeClass('active');
					RB.state.sel_sig_name = sig_name;
					
					$('#right_menu .menu-btn.' + RB.state.sel_sig_name).addClass('active');		
					$('.y-offset-arrow').css('z-index', 10);
					$('#' + RB.state.sel_sig_name + '_offset_arrow').css('z-index', 11);
				} else 
				{
					if (RB.state.sel_sig_name == sig_name)
					{
						$('#right_menu .menu-btn.' + RB.state.sel_sig_name).removeClass('active');
						RB.state.sel_sig_name = null;
					}
				}
			}
          }
          else if(field.is('input:radio')) {
            var radios = $('input[name="' + param_name + '"]');
            
            radios.closest('.btn-group').children('.btn.active').removeClass('active');
            
            if(param_name == 'RB_TRIG_SLOPE') {
              if(new_params[param_name].value == 0) {
                $('#edge1').find('img').attr('src','img/edge1.png');
                $('#edge2').addClass('active').find('img').attr('src','img/edge2_active.png').end().find('#RB_TRIG_SLOPE1').prop('checked', true);
              }
              else {
                $('#edge1').addClass('active').find('img').attr('src','img/edge1_active.png').end().find('#RB_TRIG_SLOPE').prop('checked', true);
                $('#edge2').find('img').attr('src','img/edge2.png');
              }
            }
            else {
              radios.eq([+new_params[param_name].value]).prop('checked', true).parent().addClass('active');
            }
          }
          else if(field.is('span')) {
            if($.inArray(param_name, ['RB_TIME_OFFSET', 'RB_TIME_SCALE']) > -1) {
              field.html(RB.convertTime(new_params[param_name].value));
            }
            else if($.inArray(param_name, ['RB_CH1_SCALE', 'RB_CH2_SCALE', 'RB_MATH_SCALE', 'RB_OUTPUT1_SCALE', 'RB_OUTPUT2_SCALE']) > -1) {				                
				if (param_name == 'RB_MATH_SCALE' && new_params['RB_MATH_OP'] && $('#munit')) {			
					var value = new_params[param_name].value;
					var unit = 'V';
					RB.div = 1;
					if(Math.abs(value) <= 0.1) {
						value *= 1000;
						RB.div = 0.001;
						unit = 'mV';
					} else if (Math.abs(value) >= 1000000) {
						value /= 1000000;
						RB.div = 1000000;
						unit = 'MV';						
					} else if (Math.abs(value) >= 1000) {
						value /= 1000;
						RB.div = 1000;
						unit = 'kV';						
					}
					field.html(value);
					var units = ['', unit, unit, unit + '^2', '', unit, unit + '/s', unit + 's'];
					$('#munit').html(units[new_params['RB_MATH_OP'].value] + '/div');
					
					$('#RB_MATH_OFFSET_UNIT').html(units[new_params['RB_MATH_OP'].value]);	
					$('#RB_MATH_OFFSET').val(RB.formatMathValue(RB.params.orig['RB_MATH_OFFSET'].value/RB.div));
				}        
				else
				{
					var inp_units;
				    if(Math.abs(new_params[param_name].value) >= 1) {
						inp_units = 'V';
					}
					else if(Math.abs(new_params[param_name].value) >= 0.001) {
						inp_units = 'mV';
					}
					field.html(RB.convertVoltage(new_params[param_name].value)); 
					if (param_name == "RB_CH1_SCALE")
						$("#RB_CH1_OFFSET_UNIT").html(inp_units)
					else if (param_name == "RB_CH2_SCALE")
						$("#RB_CH2_OFFSET_UNIT").html(inp_units);
				}
            }
            else {
              field.html(new_params[param_name].value);
            }
          }
        } else {
			if(param_name == "RB_CH1_OFFSET" || param_name == "RB_CH2_OFFSET")
			{
				var ch = (param_name == "RB_CH1_OFFSET") ? "CH1" : "CH2";
				var units = $('#RB_'+ch+'_OFFSET_UNIT').html();
				var multiplier = units == "mV" ? 1000 : 1;
				
				var probeAttenuation = parseInt($("#RB_"+ch+"_PROBE option:selected").text());
				var jumperSettings = $("#RB_"+ch+"_IN_GAIN").parent().hasClass("active") ? 1 : 20;
				
				field.val(RB.formatInputValue(new_params[param_name].value * multiplier, probeAttenuation, units == "mV", jumperSettings == 20));
			}
			if (param_name == "RB_MATH_OFFSET")
				field.val(RB.formatMathValue(new_params[param_name].value));
		}
      }
    }
    
    // Resize double-headed arrows showing the difference between cursors
    RB.updateYCursorDiff();
    RB.updateXCursorDiff();
  };

  // Processes newly received data for signals
  RB.iterCnt = 0;
  RB.processSignals = function(new_signals) {
    var visible_btns = [];
    var visible_plots = [];
    var visible_info = '';
    var start = +new Date();
    
    // Do nothing if no parameters received yet
    if($.isEmptyObject(RB.params.orig)) {
      return;
    }
    
    // (Re)Draw every signal
    for(sig_name in new_signals) {
      
      // Ignore empty signals
      if(new_signals[sig_name].size == 0) {
        continue;
      }
      
      // Ignore disabled signals
      if(RB.params.orig[sig_name.toUpperCase() + '_SHOW'] && RB.params.orig[sig_name.toUpperCase() + '_SHOW'].value == false) {
        continue;
      }
      
      // Ignore math signal if no operator defined
      if(sig_name == 'math' && (!RB.params.orig['MATH_SHOW'] || RB.params.orig['MATH_SHOW'].value == false)) {
        continue;
      }
      
      var points = [];
      var sig_btn = $('#right_menu .menu-btn.' + sig_name);
      var color = RB.config.graph_colors[sig_name];
      

      if(RB.params.orig['RB_VIEW_START_POS'] && RB.params.orig['RB_VIEW_END_POS']) {
          if ((((sig_name == 'output1') || (sig_name == 'output2')) && RB.params.orig['RB_VIEW_END_POS'].value != 0) || !RB.graphs[sig_name]) {
              for(var i=0; i<new_signals[sig_name].size; i++) {
                  points.push([i, new_signals[sig_name].value[i]]);
              }
          } else {
              for(var i=RB.params.orig['RB_VIEW_START_POS'].value; i<RB.params.orig['RB_VIEW_END_POS'].value; i++) {
                points.push([i, new_signals[sig_name].value[i]]);
              }
          }
      } else {
          for(var i=0; i<new_signals[sig_name].size; i++) {
              points.push([i, new_signals[sig_name].value[i]]);
          }
      }

      if(RB.graphs[sig_name]) {
        RB.graphs[sig_name].elem.show();
        
        if(RB.state.resized) {
          RB.graphs[sig_name].plot.resize();
          RB.graphs[sig_name].plot.setupGrid();
        }
        
        RB.graphs[sig_name].plot.setData([points]);
        RB.graphs[sig_name].plot.draw();
      }
      else {
        RB.graphs[sig_name] = {};
        RB.graphs[sig_name].elem = $('<div class="plot" />').css($('#graph_grid').css(['height','width'])).appendTo('#graphs');
        RB.graphs[sig_name].plot = $.plot(RB.graphs[sig_name].elem, [points], {
          series: {
            shadowSize: 0,  // Drawing is faster without shadows
            color: color
          },
          yaxis: {
            min: -5,
            max: 5
          },
          xaxis: {
            min: 0
          },
          grid: {
            show: false
          }
        });
      }
      
      sig_btn.prop('disabled', false);
      visible_btns.push(sig_btn[0]);
      visible_plots.push(RB.graphs[sig_name].elem[0]);
      visible_info += (visible_info.length ? ',' : '') + '.' + sig_name;
      
      // By default first signal is selected
      if(! RB.state.sel_sig_name && !$('#right_menu .not-signal').hasClass('active')) {
        //RB.state.sel_sig_name = sig_name;
        $('#right_menu .menu-btn.' + RB.state.sel_sig_name).addClass('active');
      }
    }
    
    // Hide plots without signal
    $('#graphs .plot').not(visible_plots).hide();
    
    // Disable buttons related to inactive signals
    $('#right_menu .menu-btn').not(visible_btns).not('.not-signal').prop('disabled', true);
    
    // Show only information about active signals
    $('#info .info-title > div, #info .info-value > div').not(visible_info).hide();
    $('#info').find(visible_info).show();
    
    // Reset resize flag
    RB.state.resized = false;
    
    // Check if selected signal is still visible 
    if(RB.state.sel_sig_name && RB.graphs[RB.state.sel_sig_name] && !RB.graphs[RB.state.sel_sig_name].elem.is(':visible')) {
      $('#right_menu .menu-btn.active.' + RB.state.sel_sig_name).removeClass('active');
      //RB.state.sel_sig_name = null;
    }

    var fps = 1000/(+new Date() - start);

    if (RB.iterCnt++ >= 20 && RB.params.orig['DEBUG_SIGNAL_PERIOD']) {
		var new_period = 1100/fps < 25 ? 25 : 1100/fps;
		var period = {};
		period['DEBUG_SIGNAL_PERIOD'] = { value: new_period };
		RB.ws.send(JSON.stringify({ parameters: period }));
		RB.iterCnt = 0;
    }
  };

  // Exits from editing mode
  RB.exitEditing = function(noclose) {

	if($('#math_dialog').is(':visible')) {
		//for values == abs, dy/dt, ydt (5, 6, 7) deselect and disable signal2 buttons
		var radios = $('input[name="RB_MATH_SRC2"]');
		var field = $('#RB_MATH_OP');
		var value = field.val();
		if(value >= 5)
		{
			radios.closest('.btn-group').children('.btn').addClass('disabled');
		}
		else{
			radios.closest('.btn-group').children('.btn').removeClass('disabled');	
		}
	}

   for(var key in RB.params.orig) {
      var field = $('#' + key);
      var value = undefined;

      if(key == 'RB_RUN'){
        value = (field.is(':visible') ? 0 : 1);
      }
      else if(field.is('select') || (field.is('input') && !field.is('input:radio')) || field.is('input:text')) {
value = field.val();
      }
      else if(field.is('button')) {
        value = (field.hasClass('active') ? 1 : 0);
      }
      else if(field.is('input:radio')) {
        value = $('input[name="' + key + '"]:checked').val();
      }
	  
	  if (key == "RB_CH1_OFFSET")
	  {
		var units = $('#RB_CH1_OFFSET_UNIT').html();
		var divider = units == "mV" ? 1000 : 1;
		value /= divider;
	  }
	  
	  if (key == "RB_CH2_OFFSET")
	  {
		var units = $('#RB_CH2_OFFSET_UNIT').html();
		var divider = units == "mV" ? 1000 : 1;
		value /= divider;
	  }
	  
	  if (key == "RB_MATH_OFFSET")
	  {
		value = RB.convertMathUnitToValue();
	  }
      
      if(value !== undefined && value != RB.params.orig[key].value) {
        console.log(key + ' changed from ' + RB.params.orig[key].value + ' to ' + ($.type(RB.params.orig[key].value) == 'boolean' ? !!value : value));
        RB.params.local[key] = { value: ($.type(RB.params.orig[key].value) == 'boolean' ? !!value : value) };
      }
    }
    
    // Check changes in measurement list
    var mi_count = 0;
    $('#info-meas').empty();
//    $($('#meas_list .meas-item').get().reverse()).each(function(index, elem) {
    $('#meas_list .meas-item').each(function(index, elem) {
      var $elem = $(elem);
      var item_val = $elem.data('value');
      
      if(item_val !== null) {
		++mi_count;
		var units = {'P2P': 'V', 'MEAN': 'V', 'MAX': 'V', 'MIN': 'V', 'RMS': 'V', 'DUTY CYCLE': '%', 'PERIOD': 'ms', 'FREQ': 'Hz'};
        RB.params.local['RB_MEAS_SEL' + mi_count] = { value: item_val };
		var sig_name = 'MATH';
		if ($elem.data('signal')[2] == '1')
			sig_name = 'IN1';
		else if ($elem.data('signal')[2] == '2')
			sig_name = 'IN2';
			
		// V/s or Vs unit for dy/dt and ydt
		if (sig_name == 'MATH') {
			if ($('#RB_MATH_OP').find(":selected").text() == 'dy/dt') {
				units['P2P'] = 'V/s';
				units['MEAN'] = 'V/s';
				units['MAX'] = 'V/s';
				units['MIN'] = 'V/s';
				units['RMS'] = 'V/s';
			} else if ($('#RB_MATH_OP').find(":selected").text() == 'ydt') {
				units['P2P'] = 'Vs';
				units['MEAN'] = 'Vs';
				units['MAX'] = 'Vs';
				units['MIN'] = 'Vs';
				units['RMS'] = 'Vs';
			} else if ($('#RB_MATH_OP').find(":selected").text() == '*') {
				units['P2P'] = 'V^2';
				units['MEAN'] = 'V^2';
				units['MAX'] = 'V^2';
				units['MIN'] = 'V^2';
				units['RMS'] = 'V^2';
			}
		}
		
		var u = '';
		if (RB.params.orig['RB_MEAS_VAL' + mi_count])
			u = RB.params.orig['RB_MEAS_VAL' + mi_count].value == 'No signal' ? '' : units[$elem.data('operator')];
        $('#info-meas').append(
          '<div>' + $elem.data('operator') + '(<span class="' + $elem.data('signal').toLowerCase() + '">' + sig_name + '</span>) <span id="RB_MEAS_VAL' + mi_count + '">-</span>&nbsp;<span id="RB_MEAS_UNITS">' + u + '</span><span id="RB_MEAS_ORIG_UNITS" style="display:none;">' + u + '</span><span id="RB_MEAS_ORIG_FOO" style="display:none;">' + $elem.data('operator') + '</span><span id="RB_MEAS_ORIG_SIGNAME" style="display:none;">' + sig_name + '</span></div>'
        );
      }
    });
    
    // Send params then reset editing state and hide dialog
    RB.sendParams();
    RB.state.editing = false;
    if (noclose) return;
    $('.dialog:visible').hide();
    $('#right_menu').show(); 
  };

  // Sends to server modified parameters
  RB.sendParams = function() {
    if($.isEmptyObject(RB.params.local)) {
      return false;
    }
    
    if(! RB.state.socket_opened) {
      console.log('ERROR: Cannot save changes, socket not opened');
      return false;
    }
    
    RB.setDefCursorVals();
    
    // TEMP TEST
    // TODO: Set the update period depending on device type
    //RB.params.local['DEBUG_PARAM_PERIOD'] = { value: 200 };
    //RB.params.local['DEBUG_SIGNAL_PERIOD'] = { value: 100 };
    
    RB.params.local['in_command'] = { value: 'send_all_params' };
    // Send new values and reset the local params object
//    if (RB.params.local['RB_MATH_OFFSET'])
//		RB.params.local['RB_MATH_OFFSET'].value *= RB.div;
    RB.ws.send(JSON.stringify({ parameters: RB.params.local }));
    RB.params.local = {};
    
    return true;
  };

  // Draws the grid on the lowest canvas layer
  RB.drawGraphGrid = function() {
    var canvas_width = $('#graphs').width() - 2;
    var canvas_height = Math.round(canvas_width / 2);
    
    var center_x = canvas_width / 2;
    var center_y = canvas_height / 2;
    
    var ctx = $('#graph_grid')[0].getContext('2d');
    
    var x_offset = 0;
    var y_offset = 0;
    
    // Set canvas size
    ctx.canvas.width = canvas_width;
    ctx.canvas.height = canvas_height;
    
    // Set draw options
    ctx.beginPath();
    ctx.lineWidth = 1;
    ctx.strokeStyle = '#5d5d5c';

    // Draw ticks
    for(var i = 1; i < 50; i++) {
      x_offset = x_offset + (canvas_width / 50);
      y_offset = y_offset + (canvas_height / 50);
      
      if(i == 25) {
        continue;
      }
      
      ctx.moveTo(x_offset, canvas_height - 3);
      ctx.lineTo(x_offset, canvas_height);
      
      ctx.moveTo(0, y_offset);
      ctx.lineTo(3, y_offset);
    }

    // Draw lines
    x_offset = 0;
    y_offset = 0;
    
    for(var i = 1; i < 10; i++){
      x_offset = x_offset + (canvas_height / 10);
      y_offset = y_offset + (canvas_width / 10);
      
      if(i == 5) {
        continue;
      }
      
      ctx.moveTo(y_offset, 0);
      ctx.lineTo(y_offset, canvas_height);
      
      ctx.moveTo(0, x_offset);
      ctx.lineTo(canvas_width, x_offset);
    } 
    
    ctx.stroke();
    
    // Draw central cross
    ctx.beginPath();
    ctx.lineWidth = 1;
    ctx.strokeStyle = '#999';
    
    ctx.moveTo(center_x, 0);
    ctx.lineTo(center_x, canvas_height);
    
    ctx.moveTo(0, center_y);
    ctx.lineTo(canvas_width, center_y);
    
    ctx.stroke();
  };

  // Changes Y zoom/scale for the selected signal
  RB.changeYZoom = function(direction, curr_scale, send_changes) {
    
    // Output 1/2 signals do not have zoom
    if($.inArray(RB.state.sel_sig_name, ['ch1', 'ch2', 'math', 'output1', 'output2']) < 0) {
      return;
    }
    
    var mult = 1;
    if(RB.state.sel_sig_name.toUpperCase() === 'MATH') {
        mult = RB.params.orig['RB_MATH_SCALE_MULT'].value;
    }
    if(RB.state.sel_sig_name.toUpperCase() === 'CH1')
    {
		var probeAttenuation = parseInt($("#RB_CH1_PROBE option:selected").text());
		var jumperSettings = $("#RB_CH1_IN_GAIN").parent().hasClass("active") ? 1 : 10;
		mult = probeAttenuation * jumperSettings;
	}
	
    if(RB.state.sel_sig_name.toUpperCase() === 'CH2')
    {
		var probeAttenuation = parseInt($("#RB_CH2_PROBE option:selected").text());
		var jumperSettings = $("#RB_CH2_IN_GAIN").parent().hasClass("active") ? 1 : 10;
		mult = probeAttenuation * jumperSettings;
	}

      
    var curr_scale = (curr_scale === undefined ? RB.params.orig['RB_' + RB.state.sel_sig_name.toUpperCase() + '_SCALE'].value : curr_scale) / mult;
    var new_scale;
    
    for(var i=0; i < RB.voltage_steps.length - 1; i++) {
      
      if(RB.state.fine && (curr_scale == RB.voltage_steps[i] 
          || (curr_scale > RB.voltage_steps[i] && curr_scale < RB.voltage_steps[i + 1])
          || (curr_scale == RB.voltage_steps[i + 1] && direction == '-'))) {
        
        new_scale = curr_scale + (RB.voltage_steps[i + 1] / 100) * (direction == '-' ? -1 : 1);
        
        // Do not allow values smaller than the lowest possible one
        if(new_scale < RB.voltage_steps[0]) {
          new_scale = RB.voltage_steps[0];
        }
        
        break;
      }
      
      if(!RB.state.fine && curr_scale == RB.voltage_steps[i]) {
        new_scale = RB.voltage_steps[direction == '-' ? (i > 0 ? i - 1 : 0) : i + 1];
        break;
      }
      else if(!RB.state.fine && ((curr_scale > RB.voltage_steps[i] && curr_scale < RB.voltage_steps[i + 1]) || (curr_scale == RB.voltage_steps[i + 1] && i == RB.voltage_steps.length - 2))) {
        new_scale = RB.voltage_steps[direction == '-' ? i : i + 1];
        break;
      }
    }
    
    if(new_scale !== undefined && new_scale > 0 && new_scale != curr_scale) {
      new_scale *= mult;
      // Fix float length
//      new_scale = parseFloat(new_scale.toFixed(RB.state.fine ? 5 : 3));
      if(send_changes !== false) {
        RB.params.local['RB_' + RB.state.sel_sig_name.toUpperCase() + '_SCALE'] = { value: new_scale };
        if (RB.params.orig['RB_' + RB.state.sel_sig_name.toUpperCase() + '_OFFSET']!=undefined)
        {
			var cur_offset = RB.params.orig['RB_' + RB.state.sel_sig_name.toUpperCase() + '_OFFSET'].value;
			var new_offset = (cur_offset / curr_scale) * (new_scale / mult);
			RB.params.local['RB_' + RB.state.sel_sig_name.toUpperCase() + '_OFFSET'] = {value: new_offset};
		}
        RB.sendParams();
      }
      return new_scale;
    }
    
    return null;
  };

  // Changes X zoom/scale for all signals
  RB.changeXZoom = function(direction, curr_scale, send_changes) {
    var curr_scale = (curr_scale === undefined ? RB.params.orig['RB_TIME_SCALE'].value : curr_scale);
    var new_scale;
    
    for(var i=0; i < RB.time_steps.length - 1; i++) {
      
      if(RB.state.fine && (curr_scale == RB.time_steps[i] 
          || (curr_scale > RB.time_steps[i] && curr_scale < RB.time_steps[i + 1])
          || (curr_scale == RB.time_steps[i + 1] && direction == '-'))) {
        
        new_scale = curr_scale + (RB.time_steps[i + 1] / 100) * (direction == '-' ? -1 : 1);
        
        // Do not allow values smaller than the lowest possible one
        if(new_scale < RB.time_steps[0]) {
          new_scale = RB.time_steps[0];
        }
        
        break;
      }
      
      if(!RB.state.fine && curr_scale == RB.time_steps[i]) {
        new_scale = RB.time_steps[direction == '-' ? (i > 0 ? i - 1 : 0) : i + 1];
        break;
      }
      else if(!RB.state.fine && ((curr_scale > RB.time_steps[i] && curr_scale < RB.time_steps[i + 1]) || (curr_scale == RB.time_steps[i + 1] && i == RB.time_steps.length - 2))) {
        new_scale = RB.time_steps[direction == '-' ? i : i + 1]
        break;
      }
    }
    
    if(new_scale !== undefined && new_scale > 0 && new_scale != curr_scale) {
      
      // Fix float length
      new_scale = parseFloat(new_scale.toFixed(RB.state.fine ? 8 : 6));
      
      if(send_changes !== false) {
        var new_offset = RB.params.orig['RB_TIME_OFFSET'].value * new_scale / RB.params.orig['RB_TIME_SCALE'].value;
        RB.params.local['RB_TIME_OFFSET'] = { value: new_offset };
        RB.params.local['RB_TIME_SCALE'] = { value: new_scale };
        RB.sendParams();
      }
      return new_scale;
    }
    
    return null;
  };

  // Sets default values for cursors, if values not yet defined
  RB.setDefCursorVals = function() {
    var graph_height = $('#graph_grid').height();
    var graph_width = $('#graph_grid').width();
    
    var source = (RB.params.local['RB_CURSOR_SRC'] ? RB.params.local['RB_CURSOR_SRC'].value : RB.params.orig['RB_CURSOR_SRC'].value);
    var ref_scale = (source == 0 ? 'RB_CH1_SCALE' : (source == 1 ? 'RB_CH2_SCALE' : 'RB_MATH_SCALE'));
    var volt_per_px = (RB.params.orig[ref_scale].value * 10) / graph_height;
    
    // Default value for Y1 cursor is 1/4 from graph height
    if(RB.params.local['RB_CURSOR_Y1'] && RB.params.local['RB_CURSOR_Y1'].value && RB.params.local['RB_CUR1_V'] === undefined && $('#cur_y1').data('init') === undefined) {
      var cur_arrow = $('#cur_y1_arrow');
      var top = (graph_height + 7) * 0.25;
      
      RB.params.local['RB_CUR1_V'] = { value: (graph_height / 2 - top - (cur_arrow.height() - 2) / 2 - parseInt(cur_arrow.css('margin-top'))) * volt_per_px };
      
      $('#cur_y1_arrow, #cur_y1').css('top', top).show();
      $('#cur_y1').data('init', true);
    }
    
    // Default value for Y2 cursor is 1/3 from graph height
    if(RB.params.local['RB_CURSOR_Y2'] && RB.params.local['RB_CURSOR_Y2'].value && RB.params.local['RB_CUR2_V'] === undefined && $('#cur_y2').data('init') === undefined) {
      var cur_arrow = $('#cur_y2_arrow');
      var top = (graph_height + 7) * 0.33;
      
      RB.params.local['RB_CUR2_V'] = { value: (graph_height / 2 - top - (cur_arrow.height() - 2) / 2 - parseInt(cur_arrow.css('margin-top'))) * volt_per_px };
      
      $('#cur_y2_arrow, #cur_y2').css('top', top).show();
      $('#cur_y2').data('init', true);
    }
    
    // Default value for X1 cursor is 1/4 from graph width
    if(RB.params.local['RB_CURSOR_X1'] && RB.params.local['RB_CURSOR_X1'].value && RB.params.local['RB_CUR1_T'] === undefined && $('#cur_x1').data('init') === undefined) {
      var cur_arrow = $('#cur_x1_arrow');
      var left = graph_width * 0.25;
      var ms_per_px = (RB.params.orig['RB_TIME_SCALE'].value * 10) / graph_width;
      
      RB.params.local['RB_CUR1_T'] = { value: (graph_width / 2 - left - (cur_arrow.width() - 2) / 2 - parseInt(cur_arrow.css('margin-left'))) * ms_per_px };
      
      $('#cur_x1_arrow, #cur_x1').css('left', left).show();
      $('#cur_x1').data('init', true);
    }
    
    // Default value for X2 cursor is 1/3 from graph width
    if(RB.params.local['RB_CURSOR_X2'] && RB.params.local['RB_CURSOR_X2'].value && RB.params.local['RB_CUR2_T'] === undefined && $('#cur_x2').data('init') === undefined) {
      var cur_arrow = $('#cur_x2_arrow');
      var left = graph_width * 0.33;
      var ms_per_px = (RB.params.orig['RB_TIME_SCALE'].value * 10) / graph_width;
      
      RB.params.local['RB_CUR2_T'] = { value: (graph_width / 2 - left - (cur_arrow.width() - 2) / 2 - parseInt(cur_arrow.css('margin-left'))) * ms_per_px };
      
      $('#cur_x2_arrow, #cur_x2').css('left', left).show();
      $('#cur_x2').data('init', true);
    }
  };
  
  // Updates all elements related to a Y cursor
  RB.updateYCursorElems = function(ui, save) {
    var y = (ui.helper[0].id == 'cur_y1_arrow' ? 'y1' : 'y2');
    var ref_scale = (RB.params.orig['RB_CURSOR_SRC'].value == 0 ? 'RB_CH1_SCALE' : (RB.params.orig['RB_CURSOR_SRC'].value == 1 ? 'RB_CH2_SCALE' : 'RB_MATH_SCALE'));
    var source_offset = RB.params.orig[RB.params.orig['RB_CURSOR_SRC'].value == 0 ? 'RB_CH1_OFFSET' : (RB.params.orig['RB_CURSOR_SRC'].value == 1 ? 'RB_CH2_OFFSET' : 'RB_MATH_OFFSET')].value;
    var graph_height = $('#graph_grid').height();
    var volt_per_px = (RB.params.orig[ref_scale].value * 10) / graph_height;
    var new_value = (graph_height / 2 - ui.position.top - (ui.helper.height() - 2) / 2 - parseInt(ui.helper.css('margin-top'))) * volt_per_px - source_offset;
    
	$('#cur_' + y + '_arrow, #cur_' + y + ', #cur_' + y + '_info').show();    
    $('#cur_' + y + ', #cur_' + y + '_info').css('top', ui.position.top);
    $('#cur_' + y + '_info')
      .html(RB.convertVoltage(+new_value))
      .data('cleanval', +new_value)
      .css('margin-top', (ui.position.top < 16 ? 3 : ''));
    
    RB.updateYCursorDiff();
    
    if(save) {
      RB.params.local[y == 'y1' ? 'RB_CUR1_V' : 'RB_CUR2_V'] = { value: new_value };
      RB.sendParams();
    }
  };
  
  // Updates all elements related to a X cursor
  RB.updateXCursorElems = function(ui, save) {
    var x = (ui.helper[0].id == 'cur_x1_arrow' ? 'x1' : 'x2');
    var graph_width = $('#graph_grid').width();
    var ms_per_px = (RB.params.orig['RB_TIME_SCALE'].value * 10) / graph_width;
    var msg_width = $('#cur_' + x + '_info').outerWidth();
    var new_value = (graph_width / 2 - ui.position.left - (ui.helper.width() - 2) / 2 - parseInt(ui.helper.css('margin-left'))) * ms_per_px - RB.params.orig['RB_TIME_OFFSET'].value;
    
	$('#cur_' + x + '_arrow, #cur_' + x + ', #cur_' + x + '_info').show(); 
    $('#cur_' + x + ', #cur_' + x + '_info').css('left', ui.position.left);
    $('#cur_' + x + '_info')
      .html(RB.convertTime(-new_value))
      .data('cleanval', -new_value)
      .css('margin-left', (ui.position.left + msg_width > graph_width - 2 ? -msg_width - 1 : ''));
    
    RB.updateXCursorDiff();
    
    if(save) {
      RB.params.local[x == 'x1' ? 'RB_CUR1_T' : 'RB_CUR2_T'] = { value: new_value };
      RB.sendParams();
    }
  };
  
  // Resizes double-headed arrow showing the difference between Y cursors
  RB.updateYCursorDiff = function() {
    var y1 = $('#cur_y1_info');
    var y2 = $('#cur_y2_info');
    var y1_top = parseInt(y1.css('top'));
    var y2_top = parseInt(y2.css('top'));
    var diff_px = Math.abs(y1_top - y2_top) - 6;
    
    if(y1.is(':visible') && y2.is(':visible') && diff_px > 12) {
      var top = Math.min(y1_top, y2_top);
      var value = $('#cur_y1_info').data('cleanval') - $('#cur_y2_info').data('cleanval');
      
      $('#cur_y_diff')
        .css('top', top + 5)
        .height(diff_px)
        .show();
      $('#cur_y_diff_info')
        .html(RB.convertVoltage(Math.abs(value)))
        .css('top', top + diff_px/2 - 2)
        .show();
    }
    else {
      $('#cur_y_diff, #cur_y_diff_info').hide();
    }
  };
  
  // Resizes double-headed arrow showing the difference between X cursors
  RB.updateXCursorDiff = function() {
    var x1 = $('#cur_x1_info');
    var x2 = $('#cur_x2_info');
    var x1_left = parseInt(x1.css('left'));
    var x2_left = parseInt(x2.css('left'));
    var diff_px = Math.abs(x1_left - x2_left) - 9;
    
    if(x1.is(':visible') && x2.is(':visible') && diff_px > 12) {
      var left = Math.min(x1_left, x2_left);
      var value = $('#cur_x1_info').data('cleanval') - $('#cur_x2_info').data('cleanval');
      
      $('#cur_x_diff')
        .css('left', left + 1)
        .width(diff_px)
        .show();
      $('#cur_x_diff_info')
        .html(RB.convertTime(Math.abs(value)))
        .show()
        .css('left', left + diff_px/2 - $('#cur_x_diff_info').width()/2 + 3);
    }
    else {
      $('#cur_x_diff, #cur_x_diff_info').hide();
    }
  };
  
  // Updates Y offset in the signal config dialog, if opened, or saves new value
  RB.updateYOffset = function(ui, save) {
    var graph_height = $('#graph_grid').outerHeight();
    var zero_pos = (graph_height + 7) / 2;
    var new_value;
    
    if(ui.helper[0].id == 'ch1_offset_arrow') {
      var volt_per_px = (RB.params.orig['RB_CH1_SCALE'].value * 10) / graph_height;
      
      new_value = (zero_pos - ui.position.top + parseInt(ui.helper.css('margin-top')) / 2) * volt_per_px;
      $('#info_box').html('IN1 zero offset ' + RB.convertVoltage(new_value));
      
      if($('#in1_dialog').is(':visible')) {
        //$('#RB_CH1_OFFSET').val(+(new_value));
		//$('#RB_CH1_OFFSET').change();
		var units = $('#RB_CH1_OFFSET_UNIT').html();
		var multiplier = units == "mV" ? 1000 : 1;
		
		var probeAttenuation = parseInt($("#RB_CH1_PROBE option:selected").text());
		var jumperSettings = $("#RB_CH1_IN_GAIN").parent().hasClass("active") ? 1 : 20;
		RB.setValue($('#RB_CH1_OFFSET'), RB.formatInputValue(new_value * multiplier, probeAttenuation, units == "mV", jumperSettings == 20));
      }

      //else if(save) {
        RB.params.local['RB_CH1_OFFSET'] = { value: new_value };
      //}
    }
    else if(ui.helper[0].id == 'ch2_offset_arrow') {
      var volt_per_px = (RB.params.orig['RB_CH2_SCALE'].value * 10) / graph_height;
      
      new_value = (zero_pos - ui.position.top + parseInt(ui.helper.css('margin-top')) / 2) * volt_per_px;
      $('#info_box').html('IN2 zero offset ' + RB.convertVoltage(new_value));
      
      if($('#in2_dialog').is(':visible')) {
        //$('#RB_CH2_OFFSET').val(+(new_value));
		//$('#RB_CH2_OFFSET').change();
		var units = $('#RB_CH2_OFFSET_UNIT').html();
		var multiplier = units == "mV" ? 1000 : 1;
		
		var probeAttenuation = parseInt($("#RB_CH2_PROBE option:selected").text());
		var jumperSettings = $("#RB_CH2_IN_GAIN").parent().hasClass("active") ? 1 : 20;
		RB.setValue($('#RB_CH2_OFFSET'), RB.formatInputValue(new_value * multiplier, probeAttenuation, units == "mV", jumperSettings == 20));
      }
      //else if(save) {
        RB.params.local['RB_CH2_OFFSET'] = { value: new_value };
      //}
    }
    else if(ui.helper[0].id == 'output1_offset_arrow') {
      var volt_per_px =  10 / graph_height;
      
      new_value = (zero_pos - ui.position.top + parseInt(ui.helper.css('margin-top')) / 2) * volt_per_px;
      $('#info_box').html('OUT1 zero offset ' + RB.convertVoltage(new_value));
      if(save) {
        RB.params.local['OUTPUT1_SHOW_OFF'] = { value: new_value };
      }
    }
    else if(ui.helper[0].id == 'output2_offset_arrow') {
      var volt_per_px =  10 / graph_height;
      
      new_value = (zero_pos - ui.position.top + parseInt(ui.helper.css('margin-top')) / 2) * volt_per_px;
      $('#info_box').html('OUT2 zero offset ' + RB.convertVoltage(new_value));
      if(save) {
        RB.params.local['OUTPUT2_SHOW_OFF'] = { value: new_value };
      }
    }
    else if(ui.helper[0].id == 'math_offset_arrow') {
      var volt_per_px = (RB.params.orig['RB_MATH_SCALE'].value * 10) / graph_height;
      
      new_value = (zero_pos - ui.position.top + parseInt(ui.helper.css('margin-top')) / 2) * volt_per_px;
      $('#info_box').html('MATH zero offset ' + RB.convertVoltage(new_value));
      
      if($('#math_dialog').is(':visible')) {
        RB.convertValueToMathUnit(new_value);
      }
      //else if(save) {
        RB.params.local['RB_MATH_OFFSET'] = { value: new_value };
      //}
    }
    
    if(new_value !== undefined && save) {
      RB.sendParams();
    }
  };
  
  
  	RB.formatInputValue = function(oldValue, attenuation, is_milis, is_hv){
		var z = oldValue;
		if (is_milis)
			return z.toFixed(0);
		if(is_hv) 
		{
			switch(attenuation)
			{
				case 1: 
					return z.toFixed(2);
					break;
				case 10:
					return z.toFixed(1);
					break;
				case 100:
					return z.toFixed(0);
					break;
			}
		} else 
		{
			switch(attenuation)
			{
				case 1: 
					return z.toFixed(3);
					break;
				case 10:
					return z.toFixed(2);
					break;
				case 100:
					return z.toFixed(1);
					break;
			}
		}
		return z;
	}
  
   RB.formatValue = function (oldValue){
		var z = oldValue;
/*
		if (z > 0)
		{
			if(z < 9.99990)
				return z.toFixed(3);
			else if(z < 99.9990)
				return z.toFixed(2);
			else if(z < 999.990)
				return z.toFixed(1);		
			else 
				return z.toFixed(0);					
		} else 
		{
			if(z > -9.99990)
				return z.toFixed(3);
			else if(z > -99.9990)
				return z.toFixed(2);
			else if(z > -999.990)
				return z.toFixed(1);
			else 
				return z.toFixed(0);				
		}
*/		
		return z;
   };
  
  // Updates trigger level in the trigger config dialog, if opened, or saves new value
  RB.updateTrigLevel = function(ui, save) {
  
    $('#trigger_level').css('top', ui.position.top);
    
    if(RB.params.orig['RB_TRIG_SOURCE'] !== undefined) {
      
      if(RB.params.orig['RB_TRIG_SOURCE'].value < 2) {
        var ref_scale = (RB.params.orig['RB_TRIG_SOURCE'].value == 0 ? 'RB_CH1_SCALE' : 'RB_CH2_SCALE');
        var source_offset = (RB.params.orig['RB_TRIG_SOURCE'].value == 0 ? RB.params.orig['RB_CH1_OFFSET'].value : RB.params.orig['RB_CH2_OFFSET'].value);
        
        if(RB.params.orig[ref_scale] !== undefined) {
          var graph_height = $('#graph_grid').height();
          var volt_per_px = (RB.params.orig[ref_scale].value * 10) / graph_height;
          var new_value = (graph_height / 2 - ui.position.top - (ui.helper.height() - 2) / 2 - parseInt(ui.helper.css('margin-top'))) * volt_per_px - source_offset;

		  if(RB.params.orig['RB_TRIG_LIMIT'] !== undefined && (new_value > RB.params.orig['RB_TRIG_LIMIT'].value || new_value < -RB.params.orig['RB_TRIG_LIMIT'].value)) {
			$('#info_box').html('Trigger at its limit');
			if(new_value > RB.params.orig['RB_TRIG_LIMIT'].value)
				new_value = RB.params.orig['RB_TRIG_LIMIT'].value
			if(new_value < -RB.params.orig['RB_TRIG_LIMIT'].value)
				new_value = -RB.params.orig['RB_TRIG_LIMIT'].value
		  }
		  else{
			$('#info_box').html('Trigger level ' + RB.convertVoltage(new_value));
		  }
          
          if($('#trig_dialog').is(':visible')) {
            //$('#RB_TRIG_LEVEL').val(+(new_value));
			//$('#RB_TRIG_LEVEL').change();
			var probeAttenuation = 1;
			var jumperSettings = 1;
			var ch="";
			if($("#RB_TRIG_SOURCE").parent().hasClass("active"))
				ch="CH1";
			else if ($("RB_TRIG_SOURCE2").parent().hasClass("active"))
				ch="CH2";
			else
			{
				probeAttenuation = 1;
			}
			
			if (ch == "CH1" || ch == "CH2")
			{
				probeAttenuation = parseInt($("#RB_"+ch+"_PROBE option:selected").text());
				jumperSettings = $("#RB_"+ch+"_IN_GAIN").parent().hasClass("active") ? 1 : 20;
			}
			
			RB.setValue($('#RB_TRIG_LEVEL'), RB.formatInputValue(new_value, probeAttenuation, false, jumperSettings == 20));
			$('#RB_TRIG_LEVEL').change();
          }
          if(save) {
            RB.params.local['RB_TRIG_LEVEL'] = { value: new_value };
            RB.sendParams();
          }
        }
      }
      else {
        console.log('Trigger level for source ' + RB.params.orig['RB_TRIG_SOURCE'].value + ' not yet supported');
      }
    }
  };
  
  // Converts time from milliseconds to a more 'user friendly' time unit; returned value includes units
  RB.convertTime = function(t) {
    var abs_t = Math.abs(t);
    var unit = 'ms';
    
    if(abs_t >= 1000) {
      t = t / 1000;
      unit = 's';
    } 
    else if(abs_t >= 1) {
      t = t * 1;
      unit = 'ms';
    }
    else if(abs_t >= 0.001) {
      t = t * 1000;
      unit = 'μs';
    }
    else if(abs_t >= 0.000001) {
      t = t * 1000000;
      unit = ' ns';
    }
    
    return +(t.toFixed(2)) + ' ' + unit;
  };
  
  // Converts voltage from volts to a more 'user friendly' unit; returned value includes units
  RB.convertVoltage = function(v) {
    var abs_v = Math.abs(v);
    var unit = 'V';
    
    if(abs_v >= 1) {
      v = v * 1;
      unit = 'V';
    }
    else if(abs_v >= 0.001) {
      v = v * 1000;
      unit = 'mV';
    }
    
    return +(v.toFixed(2)) + ' ' + unit;
  };
  
   	RB.formatMathValue = function(oldValue){
		var z = oldValue;
		var precision = 2;
		var munit = $('#munit').html().charAt(0);
		var scale_val = $("#RB_MATH_SCALE").text();
		var math_vdiv = parseFloat(scale_val);
		if(munit == 'm') 
			precision = 0;
		if (math_vdiv < 1)
			precision = 3;
		
		return z.toFixed(precision);
	}
  
  RB.convertValueToMathUnit = function(v) {
    var value = v;
	var unit = 'V';
	var precision = 2;
	var munit = $('#munit').html().charAt(0);
	var scale_val = $("#RB_MATH_SCALE").text();
	var math_vdiv = parseFloat(scale_val);
	
	if(RB.params.orig['RB_MATH_OP']){
		if(munit == 'm') {
			value *= 1000;
			unit = 'mV';
			precision = 0;
		} else if (munit == 'M') {
			value /= 1000000;
			unit = 'MV';	
		} else if (munit == 'k') {
			value /= 1000;
			unit = 'kV';
		}
		if (math_vdiv < 1)
			precision = 3;
						
		var units = ['', unit, unit, unit + '^2', '', unit, unit + '/s', unit + 's'];
		$('#RB_MATH_OFFSET_UNIT').html(units[RB.params.orig['RB_MATH_OP'].value]);		
	}
	var value_holder = $('#RB_MATH_OFFSET');
	value_holder.val(RB.formatMathValue(value));
	value_holder.change();
  };
  
  RB.convertMathUnitToValue = function() {
    var value = parseFloat($('#RB_MATH_OFFSET').val());
	var unit = $('#RB_MATH_OFFSET_UNIT').html().charAt(0);
	var precision = 3;
	if(unit === 'm') {
		value /= 1000;

	} else if (unit === 'M') {
		value *= 1000000;
							
	} else if (unit === 'k') {
		value *= 1000;			
	}
	
	value = RB.formatValue(value);
	return value;
  };
  
  RB.setValue = function(input, value) {
    input.val(value);
	//input.change();
  };
  
  
}(window.RB = window.RB || {}, jQuery));

// Page onload event handler
$(function() {
	$('#calib-input').hide();
	$('#calib-input-text').hide();
	$('#modal-warning').hide();
	
    $('button').bind('activeChanged', function(){
        RB.exitEditing(true);
    });
    $('select, input').on('change', function(){RB.exitEditing(true);});
  
  // Initialize FastClick to remove the 300ms delay between a physical tap and the firing of a click event on mobile browsers
  //new FastClick(document.body);
  
	$(".dbl").on('dblclick', function() {
	  var cls = $(this).attr('class');
	  if (cls.indexOf('ch1') != -1)
		$('#RB_CH1_OFFSET').val(0);
	  if (cls.indexOf('ch2') != -1)
		$('#RB_CH2_OFFSET').val(0);
	  if (cls.indexOf('math') != -1)
		$('#RB_MATH_OFFSET').val(0);
	  if (cls.indexOf('trig') != -1)
		$('#RB_TRIG_LEVEL').val(0);
	  RB.exitEditing(true);
	});  
  
  // Process clicks on top menu buttons
//  $('#RB_RUN').on('click touchstart', function(ev) {
  $('#RB_RUN').on('click', function(ev) {
    ev.preventDefault();
    $('#RB_RUN').hide();
    $('#RB_STOP').css('display','block');
    RB.params.local['RB_RUN'] = { value: true };
    RB.sendParams();
  }); 
  
//  $('#RB_STOP').on('click touchstart', function(ev) {
  $('#RB_STOP').on('click', function(ev) {
    ev.preventDefault();
    $('#RB_STOP').hide();
    $('#RB_RUN').show(); 
    RB.params.local['RB_RUN'] = { value: false };
    RB.sendParams();
  });
  
//  $('#RB_SINGLE').on('click touchstart', function(ev) {
  $('#RB_SINGLE').on('click', function(ev) {
    ev.preventDefault();
    RB.params.local['RB_SINGLE'] = { value: true };
    RB.sendParams();
  });
  
//  $('#RB_AUTRBALE').on('click touchstart', function(ev) {
  $('#RB_AUTRBALE').on('click', function(ev) {
    ev.preventDefault();
    RB.params.local['RB_AUTRBALE'] = { value: true };
    RB.sendParams();
  });
  
  // Selecting active signal
//  $('.menu-btn').on('click touchstart', function() {
  $('.menu-btn').on('click', function() {
    $('#right_menu .menu-btn').not(this).removeClass('active');
    if (!$(this).hasClass('active'))
		RB.state.sel_sig_name = $(this).data('signal');
	else 
		RB.state.sel_sig_name = null;
    $('.y-offset-arrow').css('z-index', 10);
    $('#' + RB.state.sel_sig_name + '_offset_arrow').css('z-index', 11);
  });

  // Opening a dialog for changing parameters
//  $('.edit-mode').on('click touchstart', function() {
  $('.edit-mode').on('click', function() {
    RB.state.editing = true;
    $('#right_menu').hide();
    $('#' + $(this).attr('id') + '_dialog').show();
    
    if($.inArray($(this).data('signal'), ['ch1', 'ch2', 'math', 'output1', 'output2']) >= 0) {
		if (RB.state.sel_sig_name)
			$('#right_menu .menu-btn.' + RB.state.sel_sig_name).removeClass('active');
		if ($(this).data('signal') == 'output1' || $(this).data('signal') == 'output2' || $(this).data('signal') == 'math')
		{
			var out_enabled = $(this).data('signal') == 'output1' ? RB.params.orig["OUTPUT1_STATE"].value
							: $(this).data('signal') == 'output2' ? RB.params.orig["OUTPUT2_STATE"].value : RB.params.orig["MATH_SHOW"].value;
			if (out_enabled)
			{
				RB.state.sel_sig_name = $(this).data('signal');
				$('#right_menu .menu-btn.' + RB.state.sel_sig_name).addClass('active');
				$('.y-offset-arrow').css('z-index', 10);
				$('#' + RB.state.sel_sig_name + '_offset_arrow').css('z-index', 11);
			} else 
				RB.state.sel_sig_name = null;
		} else 
		{
			RB.state.sel_sig_name = $(this).data('signal');
			
			$('#right_menu .menu-btn.' + RB.state.sel_sig_name).addClass('active');
			$('.y-offset-arrow').css('z-index', 10);
			$('#' + RB.state.sel_sig_name + '_offset_arrow').css('z-index', 11);
		}
    }
  });
  
  // Close parameters dialog after Enter key is pressed
  $('input').keyup(function(event){
    if(event.keyCode == 13){
      RB.exitEditing(true);
    }
  });
  
  // Close parameters dialog on close button click
//  $('.close-dialog').on('click touchstart', function() {
  $('.close-dialog').on('click', function() {
    RB.exitEditing();
  });
  
  // Measurement dialog       
  $('#meas_done').on('click', function() {              
    var meas_signal = $('#meas_dialog input[name="meas_signal"]:checked');
    
    if(meas_signal.length) {
      var operator_name = $('#meas_operator option:selected').html();
      var operator_val = parseInt($('#meas_operator').val());
      var signal_name = meas_signal.val();
      var item_id = 'meas_' + operator_name + '_' + signal_name;
      
      // Check if the item already exists
      if($('#' + item_id).length > 0) {
        return;
      }

	var sig_text = 'MATH';
	if (signal_name == 'CH1')
		sig_text = 'IN1';
	else if (signal_name == 'CH2')
		sig_text = 'IN2';
      
      // Add new item
      $('<div id="' + item_id + '" class="meas-item">' + operator_name + ' (' + sig_text + ')</div>').data({
        value: (signal_name == 'CH1' ? operator_val : (signal_name == 'CH2' ? operator_val + 1 : operator_val + 2)),
        operator: operator_name,
        signal: signal_name
      }).appendTo('#meas_list');
    }
	RB.exitEditing(true);
  });

  $(document).on('click', '.meas-item', function() {
    $(this).remove();
	RB.exitEditing(true);
  });
  
  // Process events from other controls in parameters dialogs
//  $('#edge1').on('click touchstart', function() {
  $('#edge1').on('click', function() {
    $('#edge1').find('img').attr('src','img/edge1_active.png');
    $('#edge2').find('img').attr('src','img/edge2.png');
  });
  
//  $('#edge2').on('click touchstart', function() {
  $('#edge2').on('click', function() {
    $('#edge2').find('img').attr('src','img/edge2_active.png');
    $('#edge1').find('img').attr('src','img/edge1.png');
  });
  
  // Joystick events
  $('#jtk_up').on('mousedown touchstart', function() { $('#jtk_btns').attr('src','img/node_up.png'); });
  $('#jtk_left').on('mousedown touchstart', function() { $('#jtk_btns').attr('src','img/node_left.png'); });
  $('#jtk_right').on('mousedown touchstart', function() { $('#jtk_btns').attr('src','img/node_right.png'); });
  $('#jtk_down').on('mousedown touchstart', function() { $('#jtk_btns').attr('src','img/node_down.png'); });
  
//  $('#jtk_fine').on('click touchstart', function(ev){
  $('#jtk_fine').on('click', function(ev){
    var img = $('#jtk_fine');
    
    if(img.attr('src') == 'img/fine.png') {
      img.attr('src', 'img/fine_active.png');
      RB.state.fine = true;
    }
    else {
      img.attr('src', 'img/fine.png');
      RB.state.fine = false;
    }
    
    ev.preventDefault();
    ev.stopPropagation();
  });

  $(document).on('mouseup touchend', function(){ 
    $('#jtk_btns').attr('src','img/node_fine.png'); 
  });
  
//  $('#jtk_up, #jtk_down').on('click touchstart', function(ev) {
  $('#jtk_up, #jtk_down').on('click', function(ev) {
    ev.preventDefault();
    ev.stopPropagation();
    RB.changeYZoom(ev.target.id == 'jtk_down' ? '+' : '-');
  });
  
//  $('#jtk_left, #jtk_right').on('click touchstart', function(ev) {
  $('#jtk_left, #jtk_right').on('click', function(ev) {
    ev.preventDefault();
    ev.stopPropagation();
    RB.changeXZoom(ev.target.id == 'jtk_left' ? '+' : '-');
  });
  
  // Voltage offset arrow dragging
  $('.y-offset-arrow').draggable({
    axis: 'y',
    containment: 'parent',
    drag: function(ev, ui) {
      var margin_top = parseInt(ui.helper.css('marginTop'));
      var min_top = ((ui.helper.height() / 2) + margin_top) * -1;
      var max_top = $('#graphs').height() - margin_top;
      
      if(ui.position.top < min_top) {
        ui.position.top = min_top;
      }
      else if(ui.position.top > max_top) {
        ui.position.top = max_top;
      }
      
      RB.updateYOffset(ui, false);
    },
    stop: function(ev, ui) {
      if(! RB.state.simulated_drag) {
        RB.updateYOffset(ui, true);
        $('#info_box').empty();
      }
    }
  });
  
  // Time offset arrow dragging
  $('#time_offset_arrow').draggable({
    axis: 'x',
    containment: 'parent',
    drag: function(ev, ui) {
      var graph_width = $('#graph_grid').outerWidth();
      var zero_pos = (graph_width + 2) / 2;
      var ms_per_px = (RB.params.orig['RB_TIME_SCALE'].value * 10) / graph_width;
      var new_value = +(((zero_pos - ui.position.left - ui.helper.width() / 2 - 1) * ms_per_px).toFixed(2));
      var buf_width = graph_width - 2;
      var ratio = buf_width / (buf_width * RB.params.orig['RB_VIEV_PART'].value);
      
      $('#info_box').html('Time offset ' + RB.convertTime(new_value));
      $('#buf_time_offset').css('left', buf_width / 2 - buf_width * RB.params.orig['RB_VIEV_PART'].value / 2 + ui.position.left / ratio - 4).show();
    },
    stop: function(ev, ui) {
      if(! RB.state.simulated_drag) {
        var graph_width = $('#graph_grid').outerWidth();
        var zero_pos = (graph_width + 2) / 2;
        var ms_per_px = (RB.params.orig['RB_TIME_SCALE'].value * 10) / graph_width;
        
        RB.params.local['RB_TIME_OFFSET'] = { value: (zero_pos - ui.position.left - ui.helper.width() / 2 - 1) * ms_per_px };
        RB.sendParams();
        $('#info_box').empty();
      }
    }
  });
  
  // Time offset rectangle dragging
  $('#buf_time_offset').draggable({
    axis: 'x',
    containment: 'parent',
    drag: function(ev, ui) {
      var buf_width = $('#buffer').width();
      var zero_pos = (buf_width + 2) / 2;
      var ms_per_px = (RB.params.orig['RB_TIME_SCALE'].value * 10) / buf_width;
      var ratio = buf_width / (buf_width * RB.params.orig['RB_VIEV_PART'].value);
      var new_value = +(((zero_pos - ui.position.left - ui.helper.width() / 2 - 1) * ms_per_px * ratio).toFixed(2));
      var px_offset = -(new_value / ms_per_px + $('#time_offset_arrow').width() / 2 + 1);
      
      $('#info_box').html('Time offset ' + RB.convertTime(new_value));
      $('#time_offset_arrow').css('left', (buf_width + 2) / 2 + px_offset);
    },
    stop: function(ev, ui) {
      if(! RB.state.simulated_drag) {
        var buf_width = $('#buffer').width();
        var zero_pos = (buf_width + 2) / 2;
        var ms_per_px = (RB.params.orig['RB_TIME_SCALE'].value * 10) / buf_width;
        var ratio = buf_width / (buf_width * RB.params.orig['RB_VIEV_PART'].value);
        
        RB.params.local['RB_TIME_OFFSET'] = { value: (zero_pos - ui.position.left - ui.helper.width() / 2 - 1) * ms_per_px * ratio };
        RB.sendParams();
        $('#info_box').empty();
      }
    }
  });
  
  // Trigger level arrow dragging
  $('#trig_level_arrow').draggable({
    axis: 'y',
    containment: 'parent',
    start: function(ev, ui) {
      RB.state.trig_dragging = true;
    },
    drag: function(ev, ui) {
      RB.updateTrigLevel(ui, false);
    },
    stop: function(ev, ui) {
      RB.updateTrigLevel(ui, true);
      RB.state.trig_dragging = false;
      $('#info_box').empty();
    }
  });
  
  // Y cursor arrows dragging
  $('#cur_y1_arrow, #cur_y2_arrow').draggable({
    axis: 'y',
    containment: 'parent',
    start: function(ev, ui) {
      RB.state.cursor_dragging = true;
    },
    drag: function(ev, ui) {
      RB.updateYCursorElems(ui, false);
    },
    stop: function(ev, ui) {
      RB.updateYCursorElems(ui, true);
      RB.state.cursor_dragging = false;
    }
  });
  
  // X cursor arrows dragging
  $('#cur_x1_arrow, #cur_x2_arrow').draggable({
    axis: 'x',
    containment: 'parent',
    start: function(ev, ui) {
      RB.state.cursor_dragging = true;
    },
    drag: function(ev, ui) {
      RB.updateXCursorElems(ui, false);
    },
    stop: function(ev, ui) {
      RB.updateXCursorElems(ui, true);
      RB.state.cursor_dragging = false;
    }
  });
  
  // Touch events
  $(document).on('touchstart', '.plot', function(ev) {
    ev.preventDefault();
    
    // Multi-touch is used for zooming
    if(!RB.touch.start && ev.originalEvent.touches.length > 1) {
      RB.touch.zoom_axis = null;
      RB.touch.start = [
        { clientX: ev.originalEvent.touches[0].clientX, clientY: ev.originalEvent.touches[0].clientY }, 
        { clientX: ev.originalEvent.touches[1].clientX, clientY: ev.originalEvent.touches[1].clientY }
      ];
    }
    // Single touch is used for changing offset
    else if(! RB.state.simulated_drag) {
      RB.state.simulated_drag = true;
      RB.touch.offset_axis = null;
      RB.touch.start = [
        { clientX: ev.originalEvent.touches[0].clientX, clientY: ev.originalEvent.touches[0].clientY }
      ];
    }
  });
  
  $(document).on('touchmove', '.plot', function(ev) {
    ev.preventDefault();
    
    // Multi-touch is used for zooming
    if(ev.originalEvent.touches.length > 1) {

      RB.touch.curr = [
        { clientX: ev.originalEvent.touches[0].clientX, clientY: ev.originalEvent.touches[0].clientY }, 
        { clientX: ev.originalEvent.touches[1].clientX, clientY: ev.originalEvent.touches[1].clientY }
      ];
      
      // Find zoom axis
      if(! RB.touch.zoom_axis) {
        var delta_x = Math.abs(RB.touch.curr[0].clientX - RB.touch.curr[1].clientX);
        var delta_y = Math.abs(RB.touch.curr[0].clientY - RB.touch.curr[1].clientY);
        
        if(Math.abs(delta_x - delta_y) > 10) {
          if(delta_x > delta_y) {
            RB.touch.zoom_axis = 'x';
          }
          else if(delta_y > delta_x) {
            RB.touch.zoom_axis = 'y';
          }
        }
      }
      
      // Skip first touch event
      if(RB.touch.prev) {
        
        // Time zoom
        if(RB.touch.zoom_axis == 'x') {
          var prev_delta_x = Math.abs(RB.touch.prev[0].clientX - RB.touch.prev[1].clientX);
          var curr_delta_x = Math.abs(RB.touch.curr[0].clientX - RB.touch.curr[1].clientX);
          
          if(RB.state.fine || Math.abs(curr_delta_x - prev_delta_x) > $(this).width() * 0.9 / RB.time_steps.length) {
            var new_scale = RB.changeXZoom((curr_delta_x < prev_delta_x ? '+' : '-'), RB.touch.new_scale_x, true);
            
            if(new_scale !== null) {
              RB.touch.new_scale_x = new_scale;
              $('#info_box').html('Time scale ' + RB.convertTime(new_scale) + '/div');
            }
            
            RB.touch.prev = RB.touch.curr;
          }
        }
        // Voltage zoom
        else if(RB.touch.zoom_axis == 'y' && RB.state.sel_sig_name) {
          var prev_delta_y = Math.abs(RB.touch.prev[0].clientY - RB.touch.prev[1].clientY);
          var curr_delta_y = Math.abs(RB.touch.curr[0].clientY - RB.touch.curr[1].clientY);
          
          if(RB.state.fine || Math.abs(curr_delta_y - prev_delta_y) > $(this).height() * 0.9 / RB.voltage_steps.length) {
            var new_scale = RB.changeYZoom((curr_delta_y < prev_delta_y ? '+' : '-'), RB.touch.new_scale_y, true);
            
            if(new_scale !== null) {
              RB.touch.new_scale_y = new_scale;
              $('#info_box').html('Vertical scale ' + RB.convertVoltage(new_scale) + '/div');
            }
            
            RB.touch.prev = RB.touch.curr;
          }
        }
      }
      else if(RB.touch.prev === undefined) {
        RB.touch.prev = RB.touch.curr;
      }
    }
    // Single touch is used for changing offset
    else if(RB.state.simulated_drag) {
      
      // Find offset axis
      if(! RB.touch.offset_axis) {
        var delta_x = Math.abs(RB.touch.start[0].clientX - ev.originalEvent.touches[0].clientX);
        var delta_y = Math.abs(RB.touch.start[0].clientY - ev.originalEvent.touches[0].clientY);
        
        if(delta_x > 5 || delta_y > 5) {
          if(delta_x > delta_y) {
            RB.touch.offset_axis = 'x';
          }
          else if(delta_y > delta_x) {
            RB.touch.offset_axis = 'y';
          }
        }
      }
      
      if(RB.touch.prev) {
        
        // Time offset
        if(RB.touch.offset_axis == 'x') {
          var delta_x = ev.originalEvent.touches[0].clientX - RB.touch.prev[0].clientX;
          
          if(delta_x != 0) {
            //$('#time_offset_arrow').simulate('drag', { dx: delta_x, dy: 0 });
  			$('#time_offset_arrow').simulate('drag',{ dx: delta_x, dy: 0 });
          }
        }
        // Voltage offset
        else if(RB.touch.offset_axis == 'y' && RB.state.sel_sig_name) {
          var delta_y = ev.originalEvent.touches[0].clientY - RB.touch.prev[0].clientY;
          
          if(delta_y != 0) {
            $('#' + RB.state.sel_sig_name + '_offset_arrow').simulate('drag', { dx: 0, dy: delta_y });
          }
        }
        
      }
      
      RB.touch.prev = [
        { clientX: ev.originalEvent.touches[0].clientX, clientY: ev.originalEvent.touches[0].clientY }
      ];
    }
  });
  
  $(document).on('touchend', '.plot', function(ev) {
    ev.preventDefault();
    
    if(RB.state.simulated_drag) {
      RB.state.simulated_drag = false;
      
      if(RB.touch.offset_axis == 'x') {
        //$('#time_offset_arrow').simulate('drag', { dx: 0, dy: 0 });
        $('#buf_time_offset').simulate('drag', { dx: 0, dy: 0 });
      }
      else if(RB.touch.offset_axis == 'y' && RB.state.sel_sig_name) {
        $('#' + RB.state.sel_sig_name + '_offset_arrow').simulate('drag', { dx: 0, dy: 0 });
      }
      
      delete RB.touch.start;
      delete RB.touch.prev;
    }
    else {
      // Send new scale
      if(RB.touch.new_scale_y !== undefined) {
        RB.params.local['RB_' + RB.state.sel_sig_name.toUpperCase() + '_SCALE'] = { value: RB.touch.new_scale_y };
        RB.sendParams();
      }
      else if(RB.touch.new_scale_x !== undefined) {
        RB.params.local['RB_TIME_SCALE'] = { value: RB.touch.new_scale_x };
        RB.sendParams();
      }
    }
    
    // Reset touch information
    RB.touch = {};
    $('#info_box').empty();
  });

  // Prevent native touch activity like scrolling
/*
  $('html, body').on('touchstart touchmove', function(ev) {
    ev.preventDefault();
  });
  */

  // Preload images which are not visible at the beginning
  $.preloadImages = function() {
    for(var i = 0; i < arguments.length; i++) {
      $('<img />').attr('src', 'img/' + arguments[i]);
    }
  }
  $.preloadImages(
    'edge1_active.png',
    'edge2_active.png',
    'node_up.png',
    'node_left.png',
    'node_right.png',
    'node_down.png',
    'fine_active.png',
    'trig-edge-up.png',
    'trig-edge-down.png'
  );
  RB.drawGraphGrid();
  // Bind to the window resize event to redraw the graph; trigger that event to do the first drawing
  $(window).resize(function() {
    
    // Redraw the grid (it is important to do this before resizing graph holders)
    RB.drawGraphGrid();
    
    // Resize the graph holders
    $('.plot').css($('#graph_grid').css(['height','width']));
    
    // Hide all graphs, they will be shown next time signal data is received
    $('#graphs .plot').hide();
    
    // Hide offset arrows, trigger level line and arrow
    $('.y-offset-arrow, #time_offset_arrow, #buf_time_offset, #trig_level_arrow, #trigger_level').hide();

	if (RB.ws) {
            RB.params.local['in_command'] = { value: 'send_all_params' };
            RB.ws.send(JSON.stringify({ parameters: RB.params.local }));
            RB.params.local = {};
    }
    // Reset left position for trigger level arrow, it is added by jQ UI draggable
    $('#trig_level_arrow').css('left', '');
	//$('#graphs').height($('#graph_grid').height() - 5);
    // Set the resized flag
    RB.state.resized = true;
    
  }).resize();
  
  // Stop the application when page is unloaded
  window.onbeforeunload = function() {
    $.ajax({
      url: RB.config.stop_app_url,
      async: false
    });
  };
  
  // Everything prepared, start application
  RB.startApp();
  
	RB.calib_texts =  	['Calibration of fast analog inputs and outputs is started. To proceed with calibration press CONTINUE. For factory calibration settings press DEFAULT.',
						'To calibrate inputs DC offset, <b>shortcut</b> IN1 and IN2 and press CALIBRATE.',
						'DC offset calibration is done. For finishing DC offset calibration press DONE. To continue with gains calibration press CONTINUE.',
						'To calibrate inputs low gains set the jumpers to LV settings and connect IN1 and IN2 to the reference voltage source. Notice: <p>Max.</p> reference voltage on LV ' + 'jumper settings is <b>1 V</b> ! To continue, input reference voltage value and press CALIBRATE.',
						'LOW gains calibration is done. To finish press DONE to continue with high gain calibration press CONTINUE.',
						'To calibrate inputs high gains set the jumpers to HV settings and connect IN1 and IN2 to the reference voltage source. Notice: <p>Max.</p> reference voltage ' +
						'on HV jumper settings is <b>20 V</b> ! To continue, input reference voltage value and press CALIBRATE.',
						'High gains calibration is done. To finish press DONE, to continue with outputs calibration connect OUT1 to IN1 OUT2 to IN2 and set the jumpers to LV settings and press CONTINUE.',
						'Calibration of outputs is done. For finishing press DONE',
						'Something went wrong, try again!'];
						
	RB.calib_buttons = [['CANCEL', 'DEFAULT',	'CONTINUE'], 
						 ['CANCEL', null, 		'CALIBRARTE'],
						 [null,		'DONE', 	'CONTINUE'],
						 ['CANCEL', 'input', 	'CALIBRARTE'],
						 ['CANCEL', 'DONE', 	'CONTINUE'],
						 ['CANCEL', 'input', 	'CALIBRARTE'],
						 ['CANCEL', 'DONE', 	'CALIBRARTE'],
						 ['CANCEL', 'DONE', 	null],
						 ['EXIT', 	null, 		null]];
						 
	RB.calib_params =	['CALIB_RESET', 'CALIB_FE_OFF', null, 'CALIB_FE_SCALE_LV', null, 'CALIB_FE_SCALE_HV', 'CALIB_BE', null, null];

	RB.setCalibState = function(state) {
		var i = 0;
		var with_input = false;
		$('.calib-button').each(function() {
			if (RB.calib_buttons[state][i] && RB.calib_buttons[state][i] != 'input') { // button
				$(this).children().html(RB.calib_buttons[state][i]);
				$(this).show();
			}
			else if (RB.calib_buttons[state][i] && RB.calib_buttons[state][i] == 'input') { // input
				$('#calib-input').show();
				$('#calib-input-text').show();
				$(this).hide();
				with_input = true;
			} else if (RB.calib_buttons[state][i] == null) { // null
				$(this).hide();
			}
			++i;
		});		
		
		if (!with_input) {
			$('#calib-input').hide();
			$('#calib-input-text').hide();
		}
		
		// text
		if (RB.calib_texts[state])
			$('#calib-text').html(RB.calib_texts[state]);
			
		if (state > 3) {
			$('#calib-input').attr('max', '19');
			$('#calib-input').attr('min', '9');
			$('#calib-input').val(9);
		} else {
			$('#calib-input').attr('max', '0.9');
			$('#calib-input').attr('min', '0.1');
		}
	}

	$('#calib-1').click(function() {
		if (RB.params.orig['is_demo'] && RB.params.orig['is_demo'].value == false) {
			$('#calib-2').children().removeAttr('disabled');
			$('#calib-3').children().removeAttr('disabled');
		}		
		if (RB.state.calib == 0) {
			return;
		}
		
		RB.state.calib = 0;
		RB.setCalibState(RB.state.calib);
		
		var local = {};
		local['CALIB_CANCEL'] = {value: 1};
		RB.ws.send(JSON.stringify({ parameters: local }));
		location.reload();		
	});  
	  
	$('#calib-2').click(function() {
		if (RB.params.orig['is_demo'] && RB.params.orig['is_demo'].value)
			return;
		
		if (RB.state.calib == 0 && RB.calib_params[RB.state.calib]) {
			var local = {};
			local[RB.calib_params[RB.state.calib]] = {value: 1};
			RB.ws.send(JSON.stringify({ parameters: local }));	
		}
				
		RB.state.calib = 0;
		RB.setCalibState(RB.state.calib);
		
		$('#myModal').modal('hide');
		location.reload();
	});

	$('#calib-3').click(function() {
		if (RB.params.orig['is_demo'] && RB.params.orig['is_demo'].value)
			return;
					
		if (RB.calib_params[RB.state.calib]) {
			var local = {};
			local[RB.calib_params[RB.state.calib]] = {value: 1};
			if ($('#calib-input'))
				local['CALIB_VALUE'] = {value: $('#calib-input').val()};
			RB.ws.send(JSON.stringify({ parameters: local }));	
		}
		
		if ($('#calib-3').children().html() != 'CALIBRARTE') {
			++RB.state.calib;
			RB.setCalibState(RB.state.calib);
		} else {
			$('#calib-2').children().attr('disabled', 'true');
			$('#calib-3').children().attr('disabled', 'true');
		}
	});
	
	$('#calib-4').click(function() {
		$('#modal-warning').hide();
	});  
	$('#calib-5').click(function() {	
		var local = {};
		local['CALIB_WRITE'] = {value: true};
		RB.ws.send(JSON.stringify({ parameters: local }));
		$('#modal-warning').hide();
		++RB.state.calib;
		RB.setCalibState(RB.state.calib);
	});

});
