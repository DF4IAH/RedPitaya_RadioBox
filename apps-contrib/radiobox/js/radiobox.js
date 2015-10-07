/*
 * Red Pitaya RadioBox client
 *
 * Author: Ulrich Habel (DF4IAH) <espero7757@gmx.net>
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
*/

(function() {
    var originalAddClassMethod = jQuery.fn.addClass;
    var originalRemoveClassMethod = jQuery.fn.removeClass;
    $.fn.addClass = function(clss) {
        var result = originalAddClassMethod.apply(this, arguments);
        $(this).trigger('activeChanged', 'add');
        return result;
    };
    $.fn.removeClass = function(clss) {
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
  var root_url = (RB.config.server_ip.length ? 'http://' + RB.config.server_ip : '');
  RB.config.start_app_url = root_url + '/bazaar?start=' + RB.config.app_id + '?' + location.search.substr(1);
  RB.config.stop_app_url = root_url + '/bazaar?stop=' + RB.config.app_id;
  RB.config.get_url = root_url + '/data';
  RB.config.post_url = root_url + '/data';
 //RB.config.socket_url = 'ws://' + (RB.config.server_ip.length ? RB.config.server_ip : window.location.hostname) + ':9002';  // WebSocket server URI
  RB.config.request_timeout = 3000;

  // App state
  RB.state = {
    app_started: false,
    socket_opened: false,
    sending: false,
    send_que: false,
    processing: false,
    editing: false,
    resized: false,
  };

  // Params cache
  RB.params = {
    orig: {},
    local: {},
    init: {}
  };
  RB.params.init = {
    osc1_qrg_i: { value: 0 },
    osc1_amp_i: { value: 0 },
    osc1_modsrc: { value: 0 },
    osc1_modtyp_s: { value: 0 },
    osc2_qrg_i: { value: 0 },
    osc1_amp_i: { value: 0 },

    rb_add_a_i: { value: 0 },  // TODO: remove me
    rb_add_b_i: { value: 0 },  // TODO: remove me

    'in_command': { value: 'send_all_params' }
  };


  // Other global variables
  RB.ac = null;
  //RB.ws = null;
  RB.touch = {};

  RB.connect_time;

  // Starts the application on server
  RB.startApp = function() {
    $.get(
      RB.config.start_app_url
    )
    .done(function(dresult) {
      if (dresult.status == 'OK') {
        RB.ac();
        //RB.connectWebSocket();
      }
      else if (dresult.status == 'ERROR') {
        console.log(dresult.reason ? dresult.reason : 'Failure returned when connecting the web-server - can not start the application (ERR1)');
      }
      else {
        console.log('Unknown connection state - can not start the application (ERR2)');
      }
    })
    .fail(function() {
      console.log('Can not connect the web-server (ERR3)');
    });
  };

  function showModalError(err_msg, retry_btn, restart_btn, ignore_btn) {
    var err_modal = $('#modal_err');
    err_modal.find('#btn_retry_get')[retry_btn ? 'show' : 'hide']();
    err_modal.find('.btn-app-restart')[restart_btn ? 'show' : 'hide']();
    err_modal.find('#btn_ignore')[ignore_btn ? 'show' : 'hide']();
    err_modal.find('.modal-body').html(err_msg);
    err_modal.modal('show');
  }

  // Initial Ajax Connection set-up
  RB.ac = function() {
    $.post(
      RB.config.post_url,
      JSON.stringify({ datasets: { params: RB.params.init } })
    )
    .done(function(dresult) {
      RB.state.socket_opened = true;
      RB.state.app_started = true;
      RB.parsePacket(dresult);
    })
    .fail(function() {
      showModalError('Can not initialize the application with the default parameters.', false, true);
    });
  }

  /*
  // Creates a WebSocket connection with the web server
  RB.connectWebSocket = function() {

    if (window.WebSocket) {
      RB.ws = new WebSocket(RB.config.socket_url);
    }
    else if (window.MozWebSocket) {
      RB.ws = new MozWebSocket(RB.config.socket_url);
    }
    else {
      console.log('Browser does not support WebSocket');
    }

    // Define WebSocket event listeners
    if (RB.ws) {
      RB.ws.onopen = function() {
        RB.state.socket_opened = true;
        console.log('Socket opened');

        RB.params.local['in_command'] = { value: 'send_all_params' };
        RB.ws.send(JSON.stringify({ parameters: RB.params.local }));
        RB.params.local = {};
      };

      RB.ws.onclose = function() {
        RB.state.socket_opened = false;
        console.log('Socket closed');
      };

      RB.ws.onerror = function(ev) {
        console.log('Websocket error: ', ev);
      };

      RB.ws.onmessage = function(ev) {
        if (RB.state.processing) {
          return;
        }
        RB.state.processing = true;

        var receive = JSON.parse(ev.data);

        if (receive.parameters) {
          if ((Object.keys(RB.params.orig).length == 0) && (Object.keys(receive.parameters).length == 0)) {
            RB.params.local['in_command'] = { value: 'send_all_params' };
            RB.ws.send(JSON.stringify({ parameters: RB.params.local }));
            RB.params.local = {};
          } else {
            RB.processParameters(receive.parameters);
          }
        }

        if (receive.signals) {
          RB.processSignals(receive.signals);
        }

        RB.state.processing = false;
      };
    }
  };
  */


  /* Server to front-end communication */

  // Parse returned data result from last POST transfer
  RB.parsePacket = function(dresult) {
    if (dresult.datasets !== undefined) {
      if (dresult.datasets.params !== undefined) {
        RB.processParameters(dresult.datasets.params);
      }

      if (dresult.datasets.signals !== undefined) {
        RB.processSignals(dresult.datasets.signals);
      }
    }
  };

  // Processes newly received values for parameters
  RB.processParameters = function(new_params) {
    var old_params = $.extend(true, {}, RB.params.orig);
    var send_all_params = Object.keys(new_params).indexOf('send_all_params') != -1;

    for (var param_name in new_params) {
      // Save new parameter value
      RB.params.orig[param_name] = new_params[param_name];

      if (param_name.indexOf('RB_ADD_RES') == 0) {
        //$("#"+param_name).parent().children("#RB_ADD_RES").text(new_params[param_name].value);
        $("#osc2_qrg_i").text(new_params[param_name].value);
      }

      /*
      if (param_name.indexOf('RB_MEAS_VAL') == 0) {
          var orig_units = $("#"+param_name).parent().children("#RB_MEAS_ORIG_UNITS").text();
          var orig_function = $("#"+param_name).parent().children("#RB_MEAS_ORIG_FOO").text();
          var orig_source = $("#"+param_name).parent().children("#RB_MEAS_ORIG_SIGNAME").text();
          var y = new_params[param_name].value;
          var z = y;
          var factor = '';

          $("#"+param_name).parent().children("#RB_MEAS_UNITS").text(factor + orig_units);
      }
      */

      /*
        // Find the field having ID equal to current parameter name
        // TODO: Use classes instead of ids, to be able to use a param name in multiple fields and to loop through all fields to set new values
        var field = $('#' + param_name);

        // Do not change fields from dialogs when user is editing something or new parameter value is the same as the old one
        if (field.closest('.menu-content').length == 0
            || (!RB.state.editing && (old_params[param_name] === undefined || old_params[param_name].value !== new_params[param_name].value))) {

          if (field.is('select') || (field.is('input') && !field.is('input:radio')) || field.is('input:text')) {
                if (param_name == "RB_CH1_OFFSET")
                {
                    var units;
                    if (new_params["RB_CH1_SCALE"] != undefined)
                    {
                        if (Math.abs(new_params["RB_CH1_SCALE"].value) >= 1) {
                            units = 'V';
                        }
                        else if (Math.abs(new_params["RB_CH1_SCALE"].value) >= 0.001) {
                            units = 'mV';
                        }
                    }
                    else
                        units = $('#RB_CH1_OFFSET_UNIT').html();
                    var multiplier = units == "mV" ? 1000 : 1;
                    field.val(RB.formatValue(new_params[param_name].value * multiplier));
                } else if (param_name == "RB_CH2_OFFSET")
        */

    }

    if (send_all_params) {
      RB.sendParams();
    }
  };

  // Processes newly received data for signals
  RB.processSignalsFrmsCnt = 0;
  RB.processSignals = function(new_signals) {
    var visible_btns = [];
    var visible_plots = [];
    var visible_info = '';
    var start = +new Date();

    // Do nothing if no parameters received yet
    if ($.isEmptyObject(RB.params.orig)) {
      return;
    }

    // (Re)draw every signal
    for (sig_name in new_signals) {

      // Ignore empty signals
      if (new_signals[sig_name].size == 0) {
        continue;
      }

      /* ... */
    }

    // Reset resize flag
    RB.state.resized = false;

    var fps = 1000/(+new Date() - start);

    if (RB.processSignalsFrmsCnt++ >= 20 && RB.params.orig['DEBUG_SIGNAL_PERIOD']) {
      var new_period = 1100/fps < 25 ? 25 : 1100/fps;
      var period = {};

      period['DEBUG_SIGNAL_PERIOD'] = { value: new_period };
      RB.ac.send(JSON.stringify({ parameters: period }));
      //RB.ws.send(JSON.stringify({ parameters: period }));
      RB.processSignalsFrmsCnt = 0;
    }
  };


  /* Front-end to server communication */

  // Sends to server modified parameters
  RB.sendParams = function() {
    if ($.isEmptyObject(RB.params.local)) {
      return false;
    }

    /*
    if (!RB.state.socket_opened) {
      console.log('ERROR: Cannot save changes, socket not opened');
      return false;
    }
    */

    RB.state.sending = true;

    //RB.ws.send(JSON.stringify({ parameters: RB.params.local }));
    $.ajax({
      type: 'POST',
      url: post_url,
      data: JSON.stringify({ datasets: { params: RB.params.local } }),
      timeout: RB.config.request_timeout,
      cache: false
    })
    .done(function(dresult) {
      // OK: Load the params received as POST result
      if (dresult.datasets !== undefined) {
        RB.parsePacket(dresult);
        RB.params.local = {};
      }
      else if(dresult.status == 'ERROR') {
   	    RB.state.socket_opened = false;
        showModalError((dresult.reason ? dresult.reason : 'Failure returned when connecting the web-server - can not start the application (ERR1).'), false, true, true);
        RB.state.send_que = false;
      }
      else {
        RB.state.socket_opened = false;
        showModalError('Unknown connection state - can not start the application (ERR2).', false, true, true);
      }
    })
    .fail(function() {
      RB.state.socket_opened = false;
      showModalError('Can not connect the web-server (ERR3).', false, true, true);
    })
    .always(function() {
      RB.state.sending = false;
      RB.state.editing = false;

      if (RB.state.send_que) {
        RB.state.send_que = false;
        setTimeout(function(refresh_data) {
          sendParams(refresh_data);
        }, 100);
      }
    });

    return true;
  };


  /* Controller handling */

  // Exits from editing mode - create local parameters of changed values and send them away
  RB.exitEditing = function(noclose) {
   for (var key in RB.params.orig) {
      var field = $('#' + key);
      var value = undefined;

      if (key == 'RB_RUN'){
        value = (field.is(':visible') ? 0 : 1);
      }
      else if (field.is('select') || (field.is('input') && !field.is('input:radio')) || field.is('input:text')) {
        value = field.val();
      }
      else if (field.is('button')) {
        value = (field.hasClass('active') ? 1 : 0);
      }
      else if (field.is('input:radio')) {
        value = $('input[name="' + key + '"]:checked').val();
      }
      /*
      else {
        value = field.html();
      }
      */

      if (value !== undefined && value != RB.params.orig[key].value) {
        var new_value = ($.type(RB.params.orig[key].value) == 'boolean' ?  !!value : value);

        console.log(key + ' changed from ' + RB.params.orig[key].value + ' to ' + new_value);
        RB.params.local[key] = { value: new_value };
      }
    }

    // Send params then reset editing state and hide dialog
    RB.sendParams();
    RB.state.editing = false;
    if (noclose) {
      return;
    }

    $('.dialog:visible').hide();
    $('#right_menu').show(); 
  };
}(window.RB = window.RB || {}, jQuery));


// Page onload event handler
$(function() {
  $('#modal-warning').hide();

  $('button').bind('activeChanged', function() {
    RB.exitEditing(true);
  });

  $('select, input').on('change', function() {
    RB.exitEditing(true);
  });

  // Initialize FastClick to remove the 300ms delay between a physical tap and the firing of a click event on mobile browsers
  //new FastClick(document.body);

  // Process clicks on top menu buttons
  //$('#RB_RUN').on('click touchstart', function(ev) {
  $('#RB_RUN').on('click', function(ev) {
    ev.preventDefault();
    $('#RB_RUN').hide();
    $('#RB_STOP').css('display','block');
    RB.params.local['RB_RUN'] = { value: true };
    RB.sendParams();
  });

  //$('#RB_STOP').on('click touchstart', function(ev) {
  $('#RB_STOP').on('click', function(ev) {
    ev.preventDefault();
    $('#RB_STOP').hide();
    $('#RB_RUN').show();
    RB.params.local['RB_RUN'] = { value: false };
    RB.sendParams();
  });

  /*
  // Selecting active signal
  //$('.menu-btn').on('click touchstart', function() {
  $('.menu-btn').on('click', function() {
    $('#right_menu .menu-btn').not(this).removeClass('active');
    if (!$(this).hasClass('active'))
        RB.state.sel_sig_name = $(this).data('signal');
    else
        RB.state.sel_sig_name = null;
    $('.y-offset-arrow').css('z-index', 10);
    $('#' + RB.state.sel_sig_name + '_offset_arrow').css('z-index', 11);
  });
  */

  $('.btn').on('click', function() {
    var btn = $(this);
    setTimeout(function() { 
      btn.blur();
    }, 10);
  });

  $('.btn').mouseup(function() {
    setTimeout(function() {
  	  //updateLimits();
  	  //formatVals();
      RB.exitEditing(true);
	}, 20);
  });

  // Close parameters dialog after Enter key is pressed
  $('input').keyup(function(event) {
    if (event.keyCode == 13) {
      RB.exitEditing(true);
    }
  });

  // Close parameters dialog on close button click
  //$('.close-dialog').on('click touchstart', function() {
  $('.close-dialog').on('click', function() {
    RB.exitEditing();
  });

  /*
  // Touch events
  $(document).on('touchstart', '.plot', function(ev) {
    ev.preventDefault();

    // Multi-touch is used for zooming
    if (!RB.touch.start && ev.originalEvent.touches.length > 1) {
      RB.touch.zoom_axis = null;
      RB.touch.start = [
        { clientX: ev.originalEvent.touches[0].clientX, clientY: ev.originalEvent.touches[0].clientY },
        { clientX: ev.originalEvent.touches[1].clientX, clientY: ev.originalEvent.touches[1].clientY }
      ];
    }
    // Single touch is used for changing offset
    else if (! RB.state.simulated_drag) {
      RB.state.simulated_drag = true;
      RB.touch.offset_axis = null;
      RB.touch.start = [
        { clientX: ev.originalEvent.touches[0].clientX, clientY: ev.originalEvent.touches[0].clientY }
      ];
    }
  });
  */

  // Preload images which are not visible at the beginning
  $.preloadImages = function() {
    for (var i = 0; i < arguments.length; i++) {
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

  // Bind to the window resize event to redraw the graph; trigger that event to do the first drawing
  $(window).resize(function() {
    if (RB.ac !== undefined) {
      RB.params.local['in_command'] = { value: 'send_all_params' };
      RB.ac();
    }
    /*
    if (RB.ws) {
      RB.params.local['in_command'] = { value: 'send_all_params' };
      RB.ws.send(JSON.stringify({ parameters: RB.params.local }));
      RB.params.local = {};
    }
    */
    RB.state.resized = true;
  }).resize();

  // Stop the application when page is unloaded
  window.onbeforeunload = function() {
    RB.state.app_started = false;
    RB.state.socket_opened = false;
    $.ajax({
      url: RB.config.stop_app_url,
      async: false
    });
  };

  // Everything prepared, start application
  RB.startApp();
});


$(".limits").change(function() {
  if (['SOUR1_PHAS', 'SOUR1_DCYC', 'SOUR2_PHAS', 'SOUR2_DCYC'].indexOf($(this).attr('id')) != -1) {
    var min = 0;
    var max = $(this).attr('id').indexOf('DCYC') > 0 ? 100 : 180;

    if (isNaN($(this).val()) || $(this).val() < min)
      $(this).val(min);
    else if ($(this).val() > max)
      $(this).val(max);
  } else {
    var min = $(this).attr('id').indexOf('OFFS') > 0 ? -1 : 0;
    var max = 1;
    if (isNaN($(this).val()) || $(this).val() < min)
      $(this).val(min == -1 ? 0 : 1);
    else if (isNaN($(this).val()) || $(this).val() > max)
      $(this).val(min == -1 ? 0 : 1);
  }
}).change();


function updateLimits() {
    /*
    { // RB_CH1_OFFSET limits
        var probeAttenuation = parseInt($("#RB_CH1_PROBE option:selected").text());
        var jumperSettings = $("#RB_CH1_IN_GAIN").parent().hasClass("active") ? 1 : 20;
        var units = $('#RB_CH1_OFFSET_UNIT').html();
        var multiplier = units == "mV" ? 1000 : 1;
        var newMin = -1 * 10 * jumperSettings * probeAttenuation * multiplier;
        var newMax =  1 * 10 * jumperSettings * probeAttenuation * multiplier;
        $("#RB_CH1_OFFSET").attr("min", newMin);
        $("#RB_CH1_OFFSET").attr("max", newMax);
    }
    */
}


/*
$('#RB_CH1_OFFSET_UNIT').bind("DOMSubtreeModified",function() {
    updateLimits();
    formatVals();
});
*/

/*
-    $( document ).ready(function() {
    updateLimits();
    formatVals();
});
*/


(function ($) {
  $.fn.iLightInputNumber = function (options) {
    var inBox = '.input-number-box',
        newInput = '.input-number',
        moreVal = '.input-number-more',
        lessVal = '.input-number-less';

    this.each(function () {
      var el = $(this);
      $('<div class="' + inBox.substr(1) + '"></div>').insertAfter(el);
      var parent = el.find('+ ' + inBox);
      parent.append(el);
      var classes = el.attr('class');

      el.addClass(classes);
      var attrValue;

      parent.append('<div class=' + moreVal.substr(1) + '></div>');
      parent.append('<div class=' + lessVal.substr(1) + '></div>');
    }); //end each

    var value,
        step;

    var interval = null,
        timeout = null;

    function ToggleValue(input) {
      input.val(parseInt(input.val(), 10) + d);
      console.log(input);
    }

    $('body').on('mousedown', moreVal, function () {
      var el = $(this);
      var input = el.siblings(newInput);
      moreValFn(input);
      timeout = setTimeout(function() {
        interval = setInterval(function() { moreValFn(input); }, 50);
      }, 200);
    });

    $('body').on('mousedown', lessVal, function () {
      var el = $(this);
      var input = el.siblings(newInput);
      lessValFn(input);
      timeout = setTimeout(function() {
        interval = setInterval(function() { lessValFn(input); }, 50);
      }, 200);
    });

    $(moreVal +', '+ lessVal).on("mouseup mouseout", function() {
      clearTimeout(timeout);
      clearInterval(interval);
    });


    function getLimits(input) {
      var min = parseFloat(input.attr('min'));
      var max = parseFloat(input.attr('max'));
      return {'min': min, 'max': max};
    }

    function moreValFn(input) {
      var max;
      var limits = getLimits(input);
      max = limits.max;
      checkInputAttr(input);

      var newValue = value + step;
      var parts = step.toString().split('.');
      var signs = parts.length < 2 ? 0 : parts[1].length;
      newValue = parseFloat(newValue.toFixed(signs));

      if (newValue > max) {
        newValue = max;
      }
      changeInputsVal(input, newValue);
    }

    function lessValFn(input) {
      var min;
      var limits = getLimits(input);
      min = limits.min;

      checkInputAttr(input);

      var newValue = value - step;
      var parts = step.toString().split('.');
      var signs = parts.length < 2 ? 0 : parts[1].length;
      newValue = parseFloat(newValue.toFixed(signs));
      if (newValue < min) {
        newValue = min;
      }
      changeInputsVal(input, newValue);
    }

    function changeInputsVal(input, newValue) {
      input.val(newValue);
      RB.exitEditing(true);
    }


    function checkInputAttr(input) {
      value = parseFloat(input.val());

      if (!($.isNumeric(value))) {
        value = 0;
      } else {
        step = 1;
      }
    }

    $(newInput).change(function () {
      var input = $(this);

      checkInputAttr(input);
      var limits = getLimits(input);
      var min = limits.min;
      var max = limits.max;

      var parts = step.toString().split('.');
      var signs = parts.length < 2 ? 0 : parts[1].length;
      value = parseFloat(value.toFixed(signs));

      if (value < min) {
        value = min;
      } else if (value > max) {
        value = max;
      }

      if (!($.isNumeric(value))) {
        value = 0;
      }
      input.val(value);
    });

    $(newInput).keydown(function(e) {
      var input = $(this);
      var k = e.keyCode;
      if (k == 38) {
        moreValFn(input);
      } else if (k == 40) {
        lessValFn(input);
      }
    });
  };
})(jQuery);

$('input[type=text]').iLightInputNumber({
    mobile: false
});
