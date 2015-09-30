/**
 *
 * @brief Red Pitaya oscilloscope testbench.
 *
 * @Author Matej Oblak, Ulrich Habel (DF4IAH)
 *
 * (c) Red Pitaya  http://www.redpitaya.com
 *
 * This part of code is written in Verilog hardware description language (HDL).
 * Please visit http://en.wikipedia.org/wiki/SystemVerilog
 * for more details on the language used herein.
 */

/**
 * GENERAL DESCRIPTION:
 *
 * Testbench for the Red Pitaya radiobox module.
 *
 * This testbench populates all registers and function blocks of the
 * radiobox module for correct implementation. Look up the following detailed
 * test plan.
 *
 */

// test plan
// 1. DDS1
// 1.1. signal generation
// n. ...


`timescale 1ns / 1ps

module red_pitaya_radiobox_tb #(
  // time periods
  realtime  TP = 8.0ns                          // 125MHz

  // DUT configuration
/*
  int unsigned ADC_DW = 14,                     // ADC data width
  int unsigned RSZ = 14                         // RAM size is 2**RSZ
*/
);


////////////////////////////////////////////////////////////////////////////////
//
// Settings

localparam TASK01_OP_A = 32'd15;
localparam TASK01_OP_B = 32'd17;


////////////////////////////////////////////////////////////////////////////////
//
// Connections

// System signals
int unsigned               clk_cntr = 999999 ;
reg                        clk               ;
reg                        rstn              ;

// System bus
wire           [ 32-1: 0]  sys_addr          ;
wire           [ 32-1: 0]  sys_wdata         ;
wire           [  4-1: 0]  sys_sel           ;
wire                       sys_wen           ;
wire                       sys_ren           ;
wire           [ 32-1: 0]  sys_rdata         ;
wire                       sys_err           ;
wire                       sys_ack           ;

/*
// Ext. signals and triggering
logic                      trig_ext          ;
*/

// Local
reg            [ 32-1: 0]  task_check        ;

/*
logic          [ 32-1: 0]  rdata;
bit   signed   [ 32-1: 0]  rdata_ref []      ;
int unsigned               rdata_trg [$]     ;
int unsigned               blk_size          ;
*/


////////////////////////////////////////////////////////////////////////////////
//
// Module instances

sys_bus_model bus (
  // system signals
  .clk            ( clk                    ) ,
  .rstn           ( rstn                   ) ,

  // bus protocol signals
  .sys_addr       ( sys_addr               ) ,
  .sys_wdata      ( sys_wdata              ) ,
  .sys_sel        ( sys_sel                ) ,
  .sys_wen        ( sys_wen                ) ,
  .sys_ren        ( sys_ren                ) ,
  .sys_rdata      ( sys_rdata              ) ,
  .sys_err        ( sys_err                ) ,
  .sys_ack        ( sys_ack                )
);

red_pitaya_radiobox #(
//.RSZ            ( RSZ                    )
) radiobox        (

  // ADC
  .adc_clk_i      ( clk                    ) ,
  .adc_rstn_i     ( rstn                   ) ,
/*
  .adc_a_i        ( adc_a                  ) ,  // CH 1
  .adc_b_i        ( adc_b                  ) ,  // CH 2
*/

/*
  // trigger sources
  .trig_ext_i     ( trig_ext               ) ,  // external trigger
  .trig_asg_i     ( trig_ext               ) ,  // ASG trigger
*/

/*
  // AXI0 master                                // AXI1 master
  .axi0_clk_o     (                        ) ,  .axi1_clk_o     (                        ) ,
  .axi0_rstn_o    (                        ) ,  .axi1_rstn_o    (                        ) ,
  .axi0_waddr_o   (                        ) ,  .axi1_waddr_o   (                        ) ,
  .axi0_wdata_o   (                        ) ,  .axi1_wdata_o   (                        ) ,
  .axi0_wsel_o    (                        ) ,  .axi1_wsel_o    (                        ) ,
  .axi0_wvalid_o  (                        ) ,  .axi1_wvalid_o  (                        ) ,
  .axi0_wlen_o    (                        ) ,  .axi1_wlen_o    (                        ) ,
  .axi0_wfixed_o  (                        ) ,  .axi1_wfixed_o  (                        ) ,
  .axi0_werr_i    ( 1'b0                   ) ,  .axi1_werr_i    ( 1'b0                   ) ,
  .axi0_wrdy_i    ( 1'b1                   ) ,  .axi1_wrdy_i    ( 1'b1                   ) ,
*/

  // System bus
  .sys_addr       ( sys_addr               ) ,
  .sys_wdata      ( sys_wdata              ) ,
  .sys_sel        ( sys_sel                ) ,
  .sys_wen        ( sys_wen                ) ,
  .sys_ren        ( sys_ren                ) ,
  .sys_rdata      ( sys_rdata              ) ,
  .sys_err        ( sys_err                ) ,
  .sys_ack        ( sys_ack                )
);


////////////////////////////////////////////////////////////////////////////////
//
// Helpers

/*
// Task: read_blk
logic signed   [ 32-1: 0]  rdata_blk []      ;

task read_blk (
  input int          adr,
  input int unsigned len
);
  rdata_blk = new [len];
  for (int unsigned i=0; i<len; i++) begin
    bus.read(adr + 4*i, rdata_blk[i]);
  end
endtask: read_blk
*/


////////////////////////////////////////////////////////////////////////////////
//
// Stimuli

// Clock and Reset generation
initial begin
   clk  = 1'b0 ;
   rstn = 1'b0 ;

   repeat(10) @(negedge clk) ;
   rstn = 1'b1 ;
end

always begin
   #(TP/2)
   clk = 1'b1 ;

   if (rstn)
      clk_cntr = clk_cntr + 1 ;
   else
      clk_cntr = 32'd0 ;


   #(TP/2)
   clk = 1'b0 ;
end

/*
// ADC signal generation
function [ADC_DW-1:0] saw_a (input int unsigned cyc);
  saw_a = ADC_DW'(cyc*23);
endfunction: saw_a

function [ADC_DW-1:0] saw_b (input int unsigned cyc);
  cyc = cyc % (2**ADC_DW/5);
  saw_b = -2**(ADC_DW-1) + ADC_DW'(cyc*5);
endfunction: saw_b

logic [ADC_DW-1:0] adc_a;
logic [ADC_DW-1:0] adc_b;

assign adc_a = saw_a(adc_cyc);
assign adc_b = saw_b(adc_cyc);
*/

// main FSM
initial begin
   // presets
/*
   blk_size = 20 ;
   trig_ext = 1'b0 ;                            // external trigger
*/

   // get to initial state
   wait (rstn)
   repeat(2) @(posedge clk) ;

   // TASK 01: addition - set two registers and request the result
   bus.write(20'h00000, TASK01_OP_A) ;          // set A operand
   bus.write(20'h00004, TASK01_OP_B) ;          // set A operand

   bus.read (20'h00000, task_check) ;           // read result register
   if (task_check != TASK01_OP_A)
      $display("FAIL - Task:01.01 read operand A, A=%d, (should be: %d)", task_check, TASK01_OP_A) ;
   else
      $display("PASS - Task:01.01 read operand A") ;

   bus.read (20'h00004, task_check) ;           // read result register
   if (task_check != TASK01_OP_B)
      $display("FAIL - Task:01.02 read operand B, B=%d, (should be: %d)", task_check, TASK01_OP_B) ;
   else
      $display("PASS - Task:01.02 read operand B") ;

   bus.read (20'h00008, task_check) ;           // read result register
   if (task_check != (TASK01_OP_A + TASK01_OP_B))
      $display("FAIL - Task:01.03 addition A+B=?, A=%d, B=%d, ?=%d (should be: %d)", TASK01_OP_A, TASK01_OP_B, task_check, TASK01_OP_A + TASK01_OP_B) ;
   else
      $display("PASS - Task:01.03 addition A+B=?") ;

/*
  // configure filter and decimator
  bus.write(32'h14, 32'd1    );  // data decimation     (data is decimated by a factor of 8)

  // configure internal trigger level
  bus.write(32'h08,-32'd0000 );  // A trigger treshold  (trigger at treshold     0 where signal range is -8192:+8191)
  bus.write(32'h0C,-32'd7000 );  // B trigger treshold  (trigger at treshold -7000 where signal range is -8192:+8191)
  bus.write(32'h20, 32'd20   );  // A hysteresis
  bus.write(32'h24, 32'd200  );  // B hysteresis

  // configure AXI memory limits
  bus.write(32'h50, 32'd0100);  // A start address
  bus.write(32'h54, 32'd0200);  // A stop  address
  bus.write(32'h70, 32'd0100);  // B start address
  bus.write(32'h74, 32'd0200);  // B stop  address

  bus.write(32'h10, blk_size );  // after trigger delay (the buffer contains 2**14=16384 locations, 16384-10 before and 32 after trigger)

//  // software trigger
//  bus.write(32'h00, 32'h1    );  // start aquisition (ARM, start writing data into memory
//  repeat(200) @(posedge clk);
//  bus.write(32'h04, 32'h1    );  // do SW trigger
//  repeat(200) @(posedge clk);
//  bus.write(32'h00, 32'h2    );  // reset before aquisition ends
//  repeat(200) @(posedge clk);
//
//  // A ch rising edge trigger
//  bus.write(32'h04, 32'h2    );  // configure trigger mode
//  repeat(200) @(posedge clk);
//  bus.write(32'h00, 32'h2    );  // reset before aquisition ends
//  repeat(200) @(posedge clk);
//
//  // external rising edge trigger
//  bus.write(32'h90, 32'h0    );  // set debouncer length to zero
//  bus.write(32'h04, 32'h6    );  // configure trigger mode
//  bus.write(32'h00, 32'h1    );  // start aquisition (ARM, start writing data into memory
//  repeat(200) @(posedge clk);
//  trig_ext = 1'b1;
//  repeat(200) @(posedge clk);
//  trig_ext = 1'b0;

  fork
    // provide external trigger
    begin: scope_trg
      // short trigger pulse
      repeat(20) @(posedge clk);       trig_ext = 1'b1;
      repeat( 1) @(posedge clk);       trig_ext = 1'b0;
    end
    // pool accumulation run status
    begin: scope_run
      // pooling loop
      do begin
        bus.read(32'h94, rdata);  // read value from memory
        repeat(20) @(posedge clk);
      end while (rdata & 2);
      repeat(20) @(posedge clk);
      // readout
      read_blk (32'h30000, blk_size);
      // check
      $display ("trigger positions: %p", rdata_trg);
      $display ("data reference: %p", rdata_ref);
      $display ("data read     : %p", rdata_blk);
      if (rdata_ref == rdata_blk) $display ("SUCCESS");
      else                        $display ("FAILURE");
    end
  join
*/

  repeat(100) @(posedge clk) ;
  $finish () ;
end


////////////////////////////////////////////////////////////////////////////////
// Waveforms output
////////////////////////////////////////////////////////////////////////////////

initial begin
  $dumpfile("red_pitaya_radiobox_tb.vcd");
  $dumpvars(0, red_pitaya_radiobox_tb);
end

endmodule: red_pitaya_radiobox_tb
