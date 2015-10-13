/**
 * $Id: red_pitaya_radiobox.v 001 2015-09-11 18:10:00Z DF4IAH $
 *
 * @brief Red Pitaya RadioBox application, used to expand RedPitaya for
 * radio ham operators. Transmitter as well as receiver components are
 * included like modulators/demodulators, filters, (R)FFT transformations
 * and that like.
 *
 * @Author Ulrich Habel, DF4IAH
 *
 * (c) Ulrich Habel / GitHub.com open source  http://df4iah.github.io/RedPitaya_RadioBox/
 *
 * This part of code is written in Verilog hardware description language (HDL).
 * Please visit http://en.wikipedia.org/wiki/Verilog
 * for more details on the language used herein.
 */

/**
 * GENERAL DESCRIPTION:
 *
 * TODO: explanations.
 *
 * TODO: graphics - exmaple by red_pitaya_scope.v
 *
 * TODO: detailed information
 * 
 */


`timescale 1ns / 1ps

module red_pitaya_radiobox #(
  // parameter RSZ = 14  // RAM size 2^RSZ
)(
   // ADC
   input                 clk_adc_125mhz  ,      // ADC based clock, 125 MHz
   input                 clk_adc_20mhz   ,      // ADC based clock,  20 MHz, for OSC2
   input                 adc_rstn_i      ,      // ADC reset - active low

   /*
   input        [ 13: 0] adc_a_i         ,      // ADC data CHA
   input        [ 13: 0] adc_b_i         ,      // ADC data CHB
   */

   output                osc1_axis_m_vld ,      // OSC1 output valid
   output       [ 15: 0] osc1_axis_m_data,      // OSC1 output
   output       [ 15: 0] osc1_mixed      ,      // OSC1 amplitude mixer output
   output                osc2_axis_m_vld ,      // OSC2 output valid
   output       [ 15: 0] osc2_axis_m_data,      // OSC2 output
   output       [ 15: 0] osc2_mixed      ,      // OSC2 amplitude mixer output

   // System bus - slave
   input        [ 31: 0] sys_addr        ,      // bus saddress
   input        [ 31: 0] sys_wdata       ,      // bus write data
   input        [  3: 0] sys_sel         ,      // bus write byte select
   input                 sys_wen         ,      // bus write enable
   input                 sys_ren         ,      // bus read enable
   output reg   [ 31: 0] sys_rdata       ,      // bus read data
   output reg            sys_err         ,      // bus error indicator
   output reg            sys_ack                // bus acknowledge signal
);



//---------------------------------------------------------------------------------
//  Registers accessed by the system bus

enum {
    REG_RW_RB_CTRL                      = 0,    // RB control register
    REG_RD_RB_STATUS,                           // EB status register
    REG_RW_RB_ICR,                              // RB interrupt control register
    REG_RD_RB_ISR,                              // RB interrupt status register
    REG_RW_RB_DMA_CTRL,                         // RB DMA control register

    REG_RW_RB_OSC1_INC_LO,                      // RB OSC1 increment register LSB (Bit 15.. 0), 16'b0
    REG_RW_RB_OSC1_INC_HI,                      // RB OSC1 increment register MSB (Bit 47..16)
    REG_RW_RB_OSC1_OFS,                         // RB OSC1 offset register

    REG_RW_RB_OSC2_INC,                         // RB OSC2 increment register
    REG_RW_RB_OSC2_OFS,                         // RB OSC2 offset register

    REG_RB_COUNT
} REG_RB_ENUMS;

reg  [31: 0]    regs    [REG_RB_COUNT];         // registers to be accessed by the system bus


enum {
    RB_CTRL_ENABLE                      = 0,    // enabling the RadioBox sub-module
    RB_CTRL_RSVD01,
    RB_CTRL_RSVD02,
    RB_CTRL_RSVD03,
    RB_CTRL_RSVD04,
    RB_CTRL_OSC1_INC_SRC_STREAM,                // OSC1 incrementing: use stream instead of register setting
    RB_CTRL_OSC1_OFS_SRC_STREAM,                // OSC1 offset: use stream instead of register setting
    RB_CTRL_RSVD07,
    RB_CTRL_OSC2_RESYNC,                    
    RB_CTRL_OSC2_INC_SRC_STREAM,                // OSC2 incrementing: use stream instead of register setting
    RB_CTRL_OSC2_OFS_SRC_STREAM,                // OSC2 offset: use stream instead of register setting
    RB_CTRL_RSVD11,
    RB_CTRL_RSVD12,
    RB_CTRL_RSVD13,
    RB_CTRL_RSVD14,
    RB_CTRL_RSVD15,
    RB_CTRL_RSVD16,
    RB_CTRL_RSVD17,
    RB_CTRL_RSVD18,
    RB_CTRL_RSVD19,
    RB_CTRL_RSVD20,
    RB_CTRL_RSVD21,
    RB_CTRL_RSVD22,
    RB_CTRL_RSVD23,
    RB_CTRL_RSVD24,
    RB_CTRL_RSVD25,
    RB_CTRL_RSVD26,
    RB_CTRL_RSVD27,
    RB_CTRL_RSVD28,
    RB_CTRL_RSVD29,
    RB_CTRL_RSVD30,
    RB_CTRL_RSVD31
} RB_CTRL_BITS_ENUM;


wire rb_enable = regs[REG_RW_RB_CTRL][RB_CTRL_ENABLE];

reg          rb_enable_last = 1'b0;
reg          rb_clk_en      = 1'b0;
reg          rb_reset_n     = 1'b0;
reg  [ 1: 0] rb_enable_ctr  = 2'b0;

//wire         mixed_vld;


//---------------------------------------------------------------------------------
//  RadioBox sub-module activation

always @(posedge clk_adc_20mhz)
begin
   if (!adc_rstn_i) begin
      rb_clk_en     <= 1'b0;
      rb_reset_n    <= 1'b0;
      rb_enable_ctr <= 2'b0;
   end else begin
      if (rb_enable != rb_enable_last) begin
         rb_enable_ctr <= 2'b11;                // load timer on enable bit change
         if (rb_enable)                         // just enabled
            rb_clk_en <= 1'b1;                  // firing up
         else                                   // just disabled
            rb_reset_n <= 1'b0;                 // resetting before sleep
      end else if (rb_enable_ctr)               // counter runs
         rb_enable_ctr <= rb_enable_ctr - 1;

      if (rb_enable == rb_enable_last && 
          !rb_enable_ctr)                       // after the counter has stopped
         if (rb_enable)                         // when enabling counter elapsed
            rb_reset_n <= 1'b1;                 // release reset
         else                                   // when disabling counter elapsed
            rb_clk_en <= 1'b0;                  // going to sleep
   end

   rb_enable_last <= rb_enable;
end

//---------------------------------------------------------------------------------
//  Signal generation OSC1

wire        osc1_inc_mux_stream = regs[REG_RW_RB_CTRL][RB_CTRL_OSC1_INC_SRC_STREAM];
wire [47:0] osc1_inc_stream = 48'b0;
wire [47:0] osc1_inc = ( osc1_inc_mux_stream ?  osc1_inc_stream : {regs[REG_RW_RB_OSC1_INC_HI], regs[REG_RW_RB_OSC1_INC_LO][31:16]} );

wire        osc1_ofs_mux_stream = regs[REG_RW_RB_CTRL][RB_CTRL_OSC1_OFS_SRC_STREAM];
wire [31:0] osc1_ofs_stream = 32'b0;
wire [47:0] osc1_ofs = ( osc1_ofs_mux_stream ?  {osc1_ofs_stream, {16'b0}} : {regs[REG_RW_RB_OSC1_OFS], {16'b0}} );  // min: 29 mHz

wire        osc1_axis_s_vld  = rb_reset_n;  // TODO
wire [95:0] osc1_axis_s_phase = {osc1_ofs, osc1_inc};

//wire        osc1_axis_m_vld;
//wire [15:0] osc1_axis_m_data;

rb_osc1_dds i_rb_osc1_dds (
  // global signals
  .aclk                 ( clk_adc_125mhz    ),  // global 125 MHz clock
  .aclken               ( rb_clk_en         ),  // enable of RadioBox sub-module
  .aresetn              ( rb_reset_n        ),  // enable of RadioBox sub-module

  // simple-AXI slave in port: streaming data for OSC1 modulation
  .s_axis_phase_tvalid  ( osc1_axis_s_vld   ),  // AXI slave data valid
  .s_axis_phase_tdata   ( osc1_axis_s_phase ),  // AXI slave data

  // simple-AXI master out port: OSC1 signal
  .m_axis_data_tvalid   ( osc1_axis_m_vld   ),  // AXI master data valid
  .m_axis_data_tdata    ( osc1_axis_m_data  )   // AXI master data
);


//---------------------------------------------------------------------------------
//  Signal generation OSC2

//wire        osc2_resync = 1'b0;
wire        osc2_inc_mux_stream = regs[REG_RW_RB_CTRL][RB_CTRL_OSC2_INC_SRC_STREAM];
wire [31:0] osc2_inc_stream = 32'b0;
wire [31:0] osc2_inc = ( osc2_inc_mux_stream ?  osc2_inc_stream : regs[REG_RW_RB_OSC2_INC] );  // max: 20 MHz / 2^4 = 1.25 MHz  -  min: 20 MHz / 2^36 = 291 µHz

wire        osc2_ofs_mux_stream = regs[REG_RW_RB_CTRL][RB_CTRL_OSC2_OFS_SRC_STREAM];
wire [31:0] osc2_ofs_stream = 32'b0;
wire [31:0] osc2_ofs = ( osc2_ofs_mux_stream ?  osc2_ofs_stream : regs[REG_RW_RB_OSC2_OFS] );

wire        osc2_resync = regs[REG_RW_RB_CTRL][RB_CTRL_OSC2_RESYNC];

wire        osc2_axis_s_vld  = rb_reset_n;  // TODO
wire [71:0] osc2_axis_s_phase = {{7'b0}, osc2_resync, osc2_ofs, osc2_inc};

//wire        osc2_axis_m_vld;
//wire [15:0] osc2_axis_m_data;

rb_osc2_dds i_rb_osc2_dds (
  // global signals
  .aclk                 ( clk_adc_20mhz     ),  // global 20 MHz clock
  .aclken               ( rb_clk_en         ),  // enable of RadioBox sub-module
  .aresetn              ( rb_reset_n        ),  // enable of RadioBox sub-module

  // AXI-Stream slave in port: streaming data for OSC2 modulation
  .s_axis_phase_tvalid  ( osc2_axis_s_vld   ),  // AXI slave data valid
  .s_axis_phase_tdata   ( osc2_axis_s_phase ),  // AXI slave data

  // AXI-Stream master out port: OSC2 signal
  .m_axis_data_tvalid   ( osc2_axis_m_vld   ),  // AXI master data valid
  .m_axis_data_tdata    ( osc2_axis_m_data  )   // AXI master data
);


//---------------------------------------------------------------------------------
//  OSC1 signal amplitude multiplications

wire [15:0] osc1_gain = 16'h7fff;

//wire [15:0] osc1_mixed;

rb_osc1_mlt i_rb_osc1_mlt (
  // global signals
  .clk                  ( clk_adc_125mhz    ),  // global 125 MHz clock
  .ce                   ( rb_clk_en         ),  // enable part 1 of RadioBox sub-module
  .sclr                 ( !rb_reset_n       ),  // enable part 2 of RadioBox sub-module

  // multiplier input
  .a                    ( osc1_axis_m_data  ),  // OSC1 signal
  .b                    ( osc1_gain         ),  // OSC1 gain setting

  // multiplier output
  .p                    ( osc1_mixed        )
);


//---------------------------------------------------------------------------------
//  OSC2 signal amplitude multiplications

wire [15:0] osc2_gain = 16'h7fff;

//wire [15:0] osc2_mixed;

rb_osc2_mlt i_rb_osc2_mlt (
  // global signals
  .clk                  ( clk_adc_20mhz     ),  // global 20 MHz clock
  .ce                   ( rb_clk_en         ),  // enable part 1 of RadioBox sub-module
  .sclr                 ( !rb_reset_n       ),  // enable part 2 of RadioBox sub-module

  // multiplier input
  .a                    ( osc2_axis_m_data  ),  // OSC2 signal
  .b                    ( osc2_gain         ),  // OSC2 gain setting

  // multiplier output
  .p                    ( osc2_mixed        )
);


//---------------------------------------------------------------------------------
//  Signal connection matrix


//---------------------------------------------------------------------------------
//  System bus connection

// write access to the registers
always @(posedge clk_adc_125mhz)
if (!adc_rstn_i) begin
   regs[REG_RW_RB_CTRL]         <= 32'h00000000;
   regs[REG_RD_RB_STATUS]       <= 32'h00000000;
   regs[REG_RW_RB_ICR]          <= 32'h00000000;
   regs[REG_RD_RB_ISR]          <= 32'h00000000;
   regs[REG_RW_RB_DMA_CTRL]     <= 32'h00000000;
   regs[REG_RW_RB_OSC1_INC_LO]  <= 32'h00000000;
   regs[REG_RW_RB_OSC1_INC_HI]  <= 32'h00000000;
   regs[REG_RW_RB_OSC1_OFS]     <= 32'h00000000;
   regs[REG_RW_RB_OSC2_INC]     <= 32'h00000000;
   regs[REG_RW_RB_OSC2_OFS]     <= 32'h00000000;
   end

else begin
   if (sys_wen) begin
      casez (sys_addr[19:0])

      /* control */
      20'h00000: begin
         regs[REG_RW_RB_CTRL]           <= sys_wdata[31:0];
         end
      20'h00004: begin
         /* RD  REG_RD_RB_STATUS */
         end
      20'h00008: begin
         regs[REG_RW_RB_ICR]            <= sys_wdata[31:0];
         end
      20'h0000C: begin
         /* RD  REG_RD_RB_ISR */
         end
      20'h00010: begin
         regs[REG_RW_RB_DMA_CTRL]       <= sys_wdata[31:0];
         end
      20'h00014: begin
         /*  n/a  */
         end
      20'h00018: begin
         /*  n/a  */
         end
      20'h0001C: begin
         /*  n/a  */
         end

      /* OSC1 */
      20'h00020: begin
         regs[REG_RW_RB_OSC1_INC_LO]    <= sys_wdata[31:0];
         end
      20'h00024: begin
         regs[REG_RW_RB_OSC1_INC_HI]    <= sys_wdata[31:0];
         end
      20'h00028: begin
         regs[REG_RW_RB_OSC1_OFS]       <= sys_wdata[31:0];
         end
      20'h0002C: begin
         /*  n/a  */
         end

      /* OSC2 */
      20'h00030: begin
         regs[REG_RW_RB_OSC2_INC]       <= sys_wdata[31:0];
         end
      20'h00034: begin
         /*  n/a  */
         end
      20'h00038: begin
         regs[REG_RW_RB_OSC2_OFS]       <= sys_wdata[31:0];
         end
      20'h0003C: begin
         end

      default:   begin
         end

      endcase
   end
end

wire sys_en;
assign sys_en = sys_wen | sys_ren;

// read access to the registers
always @(posedge clk_adc_125mhz)
if (!adc_rstn_i) begin
   sys_err      <= 1'b0;
   sys_ack      <= 1'b0;
   sys_rdata    <= 32'h00000000;
   end

else begin
   sys_err <= 1'b0;
   if (sys_ren) begin
      casez (sys_addr[19:0])

      /* control */
      20'h00000: begin
         sys_ack   <= sys_en;
         sys_rdata <= regs[REG_RW_RB_CTRL];
         end
      20'h00004: begin
         sys_ack   <= sys_en;
         sys_rdata <= regs[REG_RD_RB_STATUS];
         end
      20'h00008: begin
         sys_ack   <= sys_en;
         sys_rdata <= regs[REG_RW_RB_ICR];
         end
      20'h0000C: begin
         sys_ack   <= sys_en;
         sys_rdata <= regs[REG_RD_RB_ISR];
         end
      20'h00010: begin
         sys_ack   <= sys_en;
         sys_rdata <= regs[REG_RW_RB_DMA_CTRL];
         end

      /* OSC1 */
      20'h00020: begin
         sys_ack   <= sys_en;
         sys_rdata <= regs[REG_RW_RB_OSC1_INC_LO];
         end
      20'h00024: begin
         sys_ack   <= sys_en;
         sys_rdata <= regs[REG_RW_RB_OSC1_INC_HI];
         end
      20'h00028: begin
         sys_ack   <= sys_en;
         sys_rdata <= regs[REG_RW_RB_OSC1_OFS];
         end

      /* OSC2 */
      20'h00030: begin
         sys_ack   <= sys_en;
         sys_rdata <= regs[REG_RW_RB_OSC2_INC];
         end
      20'h00038: begin
         sys_ack   <= sys_en;
         sys_rdata <= regs[REG_RW_RB_OSC2_OFS];
         end

      default:   begin
         sys_ack   <= sys_en;
         sys_rdata <= 32'h00000000;
         end

      endcase
   end

   else if (sys_wen) begin                      // keep sys_ack assignment in this process
      sys_ack <= sys_en;
   end

   else begin
      sys_ack <= 1'b0;
   end
end

endmodule
