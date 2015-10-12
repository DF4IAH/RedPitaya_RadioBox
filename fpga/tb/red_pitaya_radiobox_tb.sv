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
  realtime  TP125 =  8.0ns,                     // 125 MHz
  realtime  TP20  = 50.0ns                      //  20 MHz

  // DUT configuration
/*
  int unsigned ADC_DW = 14,                     // ADC data width
  int unsigned RSZ = 14                         // RAM size is 2**RSZ
*/
);


////////////////////////////////////////////////////////////////////////////////
//
// Settings

localparam TASK01_FREE1 = 32'd0;
localparam TASK01_FREE2 = 32'd0;


////////////////////////////////////////////////////////////////////////////////
//
// Connections

// System signals
int unsigned               clk_cntr = 999999 ;
reg                        clk_adc_125mhz    ;
reg                        clk_adc_20mhz     ;
reg                        adc_rstn_i        ;

// Output signals
wire                    osc1_saxi_m_vld      ;  // OSC1 output valid
wire           [ 15: 0] osc1_saxi_m_dat      ;  // OSC1 output
wire           [ 15: 0] osc1_mixed           ;  // OSC1 amplitude mixer output
wire                    osc2_saxi_m_vld      ;  // OSC2 output valid
wire           [ 15: 0] osc2_saxi_m_dat      ;  // OSC2 output
wire           [ 15: 0] osc2_mixed           ;  // OSC2 amplitude mixer output

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
  .clk            ( clk_adc_125mhz          ),
  .rstn           ( adc_rstn_i              ),

  // bus protocol signals
  .sys_addr       ( sys_addr                ),
  .sys_wdata      ( sys_wdata               ),
  .sys_sel        ( sys_sel                 ),
  .sys_wen        ( sys_wen                 ),
  .sys_ren        ( sys_ren                 ),
  .sys_rdata      ( sys_rdata               ),
  .sys_err        ( sys_err                 ),
  .sys_ack        ( sys_ack                 )
);

red_pitaya_radiobox #(
//.RSZ            ( RSZ                     )
) radiobox        (
  // ADC
  .clk_adc_125mhz ( clk_adc_125mhz          ),  // ADC based clock, 125 MHz
  .clk_adc_20mhz  ( clk_adc_20mhz           ),  // ADC based clock,  20 MHz, for OSC2
  .adc_rstn_i     ( adc_rstn_i              ),  // ADC reset - active low

  /*
  .adc_a_i        (                         ),  // ADC data CHA
  .adc_b_i        (                         ),  // ADC data CHB
  */

  .osc1_saxi_m_vld ( osc1_saxi_m_vld        ),  //  OSC1 output valid
  .osc1_saxi_m_dat ( osc1_saxi_m_dat        ),  //  OSC1 output
  .osc1_mixed      ( osc1_mixed             ),  //  OSC1 amplitude mixer output
  .osc2_saxi_m_vld ( osc2_saxi_m_vld        ),  //  OSC2 output valid
  .osc2_saxi_m_dat ( osc2_saxi_m_dat        ),  //  OSC2 output
  .osc2_mixed      ( osc2_mixed             ),  //  OSC2 amplitude mixer output

  // System bus
  .sys_addr       ( sys_addr                ),
  .sys_wdata      ( sys_wdata               ),
  .sys_sel        ( sys_sel                 ),
  .sys_wen        ( sys_wen                 ),
  .sys_ren        ( sys_ren                 ),
  .sys_rdata      ( sys_rdata               ),
  .sys_err        ( sys_err                 ),
  .sys_ack        ( sys_ack                 )
);


////////////////////////////////////////////////////////////////////////////////
//
// Helpers

/*
// Task: read_blk
logic signed   [ 32-1: 0]  rdata_blk [];

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
   clk_adc_125mhz   = 1'b0;
   clk_adc_20mhz    = 1'b0;
   adc_rstn_i       = 1'b0;

   repeat(10) @(negedge clk_adc_125mhz);
   adc_rstn_i = 1'b1;
end

always begin
   #(TP125 / 2)
   clk_adc_125mhz = 1'b1;

   if (adc_rstn_i)
      clk_cntr = clk_cntr + 1;
   else
      clk_cntr = 32'd0;


   #(TP125 / 2)
   clk_adc_125mhz = 1'b0;
end

always begin
   #(TP20 / 2)
   clk_adc_20mhz = 1'b1;

   #(TP20 / 2)
   clk_adc_20mhz = 1'b0;
end


// main FSM
initial begin
   // presets
/*
   blk_size = 20 ;
   trig_ext = 1'b0 ;                            // external trigger
*/

  // get to initial state
  wait (adc_rstn_i)
  repeat(2) @(posedge clk_adc_125mhz);

  // TASK 01: addition - set two registers and request the result
  bus.write(20'h00000, 32'h00000000);          // control
  bus.write(20'h00008, 32'h00000000);          // clear ICR
  bus.write(20'h00010, 32'h00000000);          // clear DMA

  bus.write(20'h00020, 32'h00010000);          // OSC1 INC LO - lowest value possible
  bus.write(20'h00024, 32'h10000000);          // OSC1 INC HI
  bus.write(20'h00028, 32'h00000000);          // OSC1 PHASE

  bus.write(20'h00030, 32'h00000001);          // OSC2 INC - lowest value possible
  bus.write(20'h00038, 32'h00000000);          // OSC2 PHASE


  bus.read (20'h00000, task_check);            // read result register
  if (task_check != 32'h00000000)
     $display("FAIL - Task:01.01 read REG_RW_RB_CTRL, read=%d, (should be: %d)", task_check, 32'h00000000);
  else
     $display("PASS - Task:01.01 read REG_RW_RB_CTRL");

  bus.read (20'h00008, task_check);            // read result register
  if (task_check != 32'h00000000)
     $display("FAIL - Task:01.02 read REG_RW_RB_ICR, read=%d, (should be: %d)", task_check, 32'h00000000);
  else
     $display("PASS - Task:01.02 read REG_RW_RB_ICR");

  bus.read (20'h00010, task_check);            // read result register
  if (task_check != 32'h00000000)
     $display("FAIL - Task:01.03 read REG_RW_RB_DMA_CTRL, read=%d, (should be: %d)", task_check, 32'h00000000);
  else
     $display("PASS - Task:01.03 read REG_RW_RB_DMA_CTRL");


  bus.write(20'h00000, 32'h00000001);          // control: enable RadioBox

  repeat(100) @(posedge clk_adc_125mhz);
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
