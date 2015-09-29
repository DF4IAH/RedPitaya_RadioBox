DF4IAH: the following Xilinx system_bd.tcl script has been checked item by item and hand-built to the graphical system.db tool within of Vivado2015.2
Nearly every part from the V0.93 could be modified by this script entries. There were some flaws, which I corrected with the graphical tool or by
edditing the system.bd file by hand due to a bug of that Xilinx tool. The number of MAX_BURST_LENGTH resets to 256, that is wrong - use 16 instead.

==========================


  # Create interface ports
 DONE set DDR [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:ddrx_rtl:1.0 DDR ]
 DONE set FIXED_IO [ create_bd_intf_port -mode Master -vlnv xilinx.com:display_processing_system7:fixedio_rtl:1.0 FIXED_IO ]
 DONE set M_AXI_GP0 [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:aximm_rtl:1.0 M_AXI_GP0 ]
 DONE set_property -dict [ list CONFIG.ADDR_WIDTH {32} CONFIG.DATA_WIDTH {32} CONFIG.FREQ_HZ {125000000} CONFIG.PROTOCOL {AXI3}  ] $M_AXI_GP0
 DONE set S_AXI_HP0 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 S_AXI_HP0 ]
 DONE set_property -dict [ list CONFIG.ADDR_WIDTH {32} CONFIG.ARUSER_WIDTH {0} CONFIG.AWUSER_WIDTH {0} CONFIG.BUSER_WIDTH {0} CONFIG.DATA_WIDTH {64} CONFIG.FREQ_HZ {125000000} CONFIG.ID_WIDTH {0} CONFIG.MAX_BURST_LENGTH {16} CONFIG.NUM_READ_OUTSTANDING {1} CONFIG.NUM_WRITE_OUTSTANDING {1} CONFIG.PHASE {0.000} CONFIG.PROTOCOL {AXI3} CONFIG.READ_WRITE_MODE {READ_WRITE} CONFIG.RUSER_WIDTH {0} CONFIG.SUPPORTS_NARROW_BURST {1} CONFIG.WUSER_WIDTH {0}  ] $S_AXI_HP0
 DONE set S_AXI_HP1 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 S_AXI_HP1 ]
 DONE set_property -dict [ list CONFIG.ADDR_WIDTH {32} CONFIG.ARUSER_WIDTH {0} CONFIG.AWUSER_WIDTH {0} CONFIG.BUSER_WIDTH {0} CONFIG.DATA_WIDTH {64} CONFIG.FREQ_HZ {125000000} CONFIG.ID_WIDTH {0} CONFIG.MAX_BURST_LENGTH {16} CONFIG.NUM_READ_OUTSTANDING {1} CONFIG.NUM_WRITE_OUTSTANDING {1} CONFIG.PHASE {0.000} CONFIG.PROTOCOL {AXI3} CONFIG.READ_WRITE_MODE {READ_WRITE} CONFIG.RUSER_WIDTH {0} CONFIG.SUPPORTS_NARROW_BURST {1} CONFIG.WUSER_WIDTH {0}  ] $S_AXI_HP1
 DONE set Vaux0 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vaux0 ]
 DONE set Vaux1 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vaux1 ]
 DONE set Vaux8 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vaux8 ]
 DONE set Vaux9 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vaux9 ]
 DONE set Vp_Vn [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vp_Vn ]

  # Create ports
 DONE set FCLK_CLK0 [ create_bd_port -dir O -type clk FCLK_CLK0 ]
 DONE set FCLK_CLK1 [ create_bd_port -dir O -type clk FCLK_CLK1 ]
 DONE set FCLK_CLK2 [ create_bd_port -dir O -type clk FCLK_CLK2 ]
 DONE set FCLK_CLK3 [ create_bd_port -dir O -type clk FCLK_CLK3 ]
 DONE set FCLK_RESET0_N [ create_bd_port -dir O -type rst FCLK_RESET0_N ]
 DONE set FCLK_RESET1_N [ create_bd_port -dir O -type rst FCLK_RESET1_N ]
 DONE set FCLK_RESET2_N [ create_bd_port -dir O -type rst FCLK_RESET2_N ]
 DONE set FCLK_RESET3_N [ create_bd_port -dir O -type rst FCLK_RESET3_N ]
 DONE set M_AXI_GP0_ACLK [ create_bd_port -dir I -type clk M_AXI_GP0_ACLK ]
 DONE set_property -dict [ list CONFIG.ASSOCIATED_BUSIF {M_AXI_GP0} CONFIG.FREQ_HZ {125000000}  ] $M_AXI_GP0_ACLK
 DONE set S_AXI_HP0_aclk [ create_bd_port -dir I -type clk S_AXI_HP0_aclk ]
 DONE set_property -dict [ list CONFIG.FREQ_HZ {125000000}  ] $S_AXI_HP0_aclk
 DONE set S_AXI_HP1_aclk [ create_bd_port -dir I -type clk S_AXI_HP1_aclk ]
 DONE set_property -dict [ list CONFIG.FREQ_HZ {125000000}  ] $S_AXI_HP1_aclk

  # Create instance: axi_protocol_converter_0, and set properties
 DONE set axi_protocol_converter_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_protocol_converter:2.1 axi_protocol_converter_0 ]

  # Create instance: proc_sys_reset, and set properties
 DONE set proc_sys_reset [ create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset ]
 DONE set_property -dict [ list CONFIG.C_EXT_RST_WIDTH {1}  ] $proc_sys_reset

  # Create instance: processing_system7, and set properties
 DONE set processing_system7 [ create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 processing_system7 ]
 DONE set_property -dict [ list CONFIG.PCW_ENET0_ENET0_IO {MIO 16 .. 27} \
 DONE CONFIG.PCW_ENET0_GRP_MDIO_ENABLE {1} 
 DONE CONFIG.PCW_ENET0_PERIPHERAL_CLKSRC {ARM PLL} \
 DONE CONFIG.PCW_ENET0_PERIPHERAL_ENABLE {1} 
 DONE CONFIG.PCW_EN_CLK1_PORT {1} \
 DONE CONFIG.PCW_EN_CLK2_PORT {1} 
 DONE CONFIG.PCW_EN_CLK3_PORT {1} \
 DONE CONFIG.PCW_EN_RST1_PORT {1} 
 DONE CONFIG.PCW_EN_RST2_PORT {1} \
 DONE CONFIG.PCW_EN_RST3_PORT {1} 
 DONE CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {125} \
 DONE CONFIG.PCW_FPGA1_PERIPHERAL_FREQMHZ {250} 
 DONE CONFIG.PCW_FPGA3_PERIPHERAL_FREQMHZ {200} \
 DONE CONFIG.PCW_GPIO_MIO_GPIO_ENABLE {1} 
 DONE CONFIG.PCW_I2C0_I2C0_IO {MIO 50 .. 51} \
 DONE CONFIG.PCW_I2C0_PERIPHERAL_ENABLE {1} 
 DONE CONFIG.PCW_IRQ_F2P_INTR {1} \
 DONE CONFIG.PCW_MIO_16_PULLUP {disabled} CONFIG.PCW_MIO_16_SLEW {fast} \
 DONE CONFIG.PCW_MIO_17_PULLUP {disabled} CONFIG.PCW_MIO_17_SLEW {fast} \
 DONE CONFIG.PCW_MIO_18_PULLUP {disabled} CONFIG.PCW_MIO_18_SLEW {fast} \
 DONE CONFIG.PCW_MIO_19_PULLUP {disabled} CONFIG.PCW_MIO_19_SLEW {fast} \
 DONE CONFIG.PCW_MIO_20_PULLUP {disabled} CONFIG.PCW_MIO_20_SLEW {fast} \
 DONE CONFIG.PCW_MIO_21_PULLUP {disabled} CONFIG.PCW_MIO_21_SLEW {fast} \
 DONE CONFIG.PCW_MIO_22_PULLUP {disabled} CONFIG.PCW_MIO_22_SLEW {fast} \
 DONE CONFIG.PCW_MIO_23_PULLUP {disabled} CONFIG.PCW_MIO_23_SLEW {fast} \
 DONE CONFIG.PCW_MIO_24_PULLUP {disabled} CONFIG.PCW_MIO_24_SLEW {fast} \
 DONE CONFIG.PCW_MIO_25_PULLUP {disabled} CONFIG.PCW_MIO_25_SLEW {fast} \
 DONE CONFIG.PCW_MIO_26_PULLUP {disabled} CONFIG.PCW_MIO_26_SLEW {fast} \
 DONE CONFIG.PCW_MIO_27_PULLUP {disabled} CONFIG.PCW_MIO_27_SLEW {fast} \
 DONE CONFIG.PCW_MIO_28_PULLUP {disabled} CONFIG.PCW_MIO_28_SLEW {fast} \
 DONE CONFIG.PCW_MIO_29_PULLUP {disabled} CONFIG.PCW_MIO_29_SLEW {fast} \
 DONE CONFIG.PCW_MIO_30_PULLUP {disabled} CONFIG.PCW_MIO_30_SLEW {fast} \
 DONE CONFIG.PCW_MIO_31_PULLUP {disabled} CONFIG.PCW_MIO_31_SLEW {fast} \
 DONE CONFIG.PCW_MIO_32_PULLUP {disabled} CONFIG.PCW_MIO_32_SLEW {fast} \
 DONE CONFIG.PCW_MIO_33_PULLUP {disabled} CONFIG.PCW_MIO_33_SLEW {fast} \
 DONE CONFIG.PCW_MIO_34_PULLUP {disabled} CONFIG.PCW_MIO_34_SLEW {fast} \
 DONE CONFIG.PCW_MIO_35_PULLUP {disabled} CONFIG.PCW_MIO_35_SLEW {fast} \
 DONE CONFIG.PCW_MIO_36_PULLUP {disabled} CONFIG.PCW_MIO_36_SLEW {fast} \
 DONE CONFIG.PCW_MIO_37_PULLUP {disabled} CONFIG.PCW_MIO_37_SLEW {fast} \
 DONE CONFIG.PCW_MIO_38_PULLUP {disabled} CONFIG.PCW_MIO_38_SLEW {fast} \
 DONE CONFIG.PCW_MIO_39_PULLUP {disabled} CONFIG.PCW_MIO_39_SLEW {fast} \
 DONE CONFIG.PCW_PRESET_BANK1_VOLTAGE {LVCMOS 2.5V} 
 DONE CONFIG.PCW_QSPI_PERIPHERAL_CLKSRC {ARM PLL} \
 DONE CONFIG.PCW_QSPI_PERIPHERAL_ENABLE {1} 
 DONE CONFIG.PCW_QSPI_PERIPHERAL_FREQMHZ {125} \
 DONE CONFIG.PCW_SD0_GRP_CD_ENABLE {1} 
 DONE CONFIG.PCW_SD0_GRP_CD_IO {MIO 46} \
 DONE CONFIG.PCW_SD0_GRP_WP_ENABLE {1} 
 DONE CONFIG.PCW_SD0_GRP_WP_IO {MIO 47} \
 DONE CONFIG.PCW_SD0_PERIPHERAL_ENABLE {1} 
 DONE CONFIG.PCW_SPI0_PERIPHERAL_ENABLE {0} \
 DONE CONFIG.PCW_SPI1_PERIPHERAL_ENABLE {1} 
 DONE CONFIG.PCW_SPI1_SPI1_IO {MIO 10 .. 15} \
 DONE CONFIG.PCW_TTC0_PERIPHERAL_ENABLE {1} 
 DONE CONFIG.PCW_UART0_PERIPHERAL_ENABLE {1} \
 DONE CONFIG.PCW_UART0_UART0_IO {MIO 14 .. 15} 
 DONE CONFIG.PCW_UART1_PERIPHERAL_ENABLE {1} \
 DONE CONFIG.PCW_UART1_UART1_IO {MIO 8 .. 9} 
 DONE CONFIG.PCW_UIPARAM_DDR_BUS_WIDTH {16 Bit} \
 DONE CONFIG.PCW_UIPARAM_DDR_PARTNO {MT41J256M16 RE-125} 
 DONE CONFIG.PCW_USB0_PERIPHERAL_ENABLE {1} \
 DONE CONFIG.PCW_USB0_RESET_ENABLE {1} 
 DONE CONFIG.PCW_USB0_RESET_IO {MIO 48} \
 DONE CONFIG.PCW_USE_FABRIC_INTERRUPT {1} 
 DONE CONFIG.PCW_USE_M_AXI_GP1 {1} \
 DONE CONFIG.PCW_USE_S_AXI_HP0 {1} 
 DONE CONFIG.PCW_USE_S_AXI_HP1 {1} \
 ] $processing_system7

  # Create instance: xadc, and set properties
 DONE set xadc [ create_bd_cell -type ip -vlnv xilinx.com:ip:xadc_wiz:3.1 xadc ]
 DONE set_property -dict [ list CONFIG.CHANNEL_ENABLE_VAUXP0_VAUXN0 {true} CONFIG.CHANNEL_ENABLE_VAUXP1_VAUXN1 {true} CONFIG.CHANNEL_ENABLE_VAUXP8_VAUXN8 {true} CONFIG.CHANNEL_ENABLE_VAUXP9_VAUXN9 {true} CONFIG.CHANNEL_ENABLE_VP_VN {true} CONFIG.EXTERNAL_MUX_CHANNEL {VP_VN} CONFIG.SEQUENCER_MODE {Off} CONFIG.SINGLE_CHANNEL_SELECTION {TEMPERATURE} CONFIG.XADC_STARUP_SELECTION {independent_adc}  ] $xadc

  # Create instance: xlconstant, and set properties
 DONE set xlconstant [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant ]

  # Create interface connections
 DONE connect_bd_intf_net -intf_net Vaux0_1 [get_bd_intf_ports Vaux0] [get_bd_intf_pins xadc/Vaux0]
 DONE connect_bd_intf_net -intf_net Vaux1_1 [get_bd_intf_ports Vaux1] [get_bd_intf_pins xadc/Vaux1]
 DONE connect_bd_intf_net -intf_net Vaux8_1 [get_bd_intf_ports Vaux8] [get_bd_intf_pins xadc/Vaux8]
 DONE connect_bd_intf_net -intf_net Vaux9_1 [get_bd_intf_ports Vaux9] [get_bd_intf_pins xadc/Vaux9]
 DONE connect_bd_intf_net -intf_net Vp_Vn_1 [get_bd_intf_ports Vp_Vn] [get_bd_intf_pins xadc/Vp_Vn]
 DONE connect_bd_intf_net -intf_net axi_protocol_converter_0_M_AXI [get_bd_intf_pins axi_protocol_converter_0/M_AXI] [get_bd_intf_pins xadc/s_axi_lite]
 DONE connect_bd_intf_net -intf_net processing_system7_0_M_AXI_GP0 [get_bd_intf_ports M_AXI_GP0] [get_bd_intf_pins processing_system7/M_AXI_GP0]
 DONE connect_bd_intf_net -intf_net processing_system7_0_M_AXI_GP1 [get_bd_intf_pins axi_protocol_converter_0/S_AXI] [get_bd_intf_pins processing_system7/M_AXI_GP1]
 DONE connect_bd_intf_net -intf_net processing_system7_0_ddr [get_bd_intf_ports DDR] [get_bd_intf_pins processing_system7/DDR]
 DONE connect_bd_intf_net -intf_net processing_system7_0_fixed_io [get_bd_intf_ports FIXED_IO] [get_bd_intf_pins processing_system7/FIXED_IO]
 DONE connect_bd_intf_net -intf_net s_axi_hp0_1 [get_bd_intf_ports S_AXI_HP0] [get_bd_intf_pins processing_system7/S_AXI_HP0]
 DONE connect_bd_intf_net -intf_net s_axi_hp1_1 [get_bd_intf_ports S_AXI_HP1] [get_bd_intf_pins processing_system7/S_AXI_HP1]

  # Create port connections
 DONE connect_bd_net -net m_axi_gp0_aclk_1 [get_bd_ports M_AXI_GP0_ACLK] [get_bd_pins processing_system7/M_AXI_GP0_ACLK]
 DONE connect_bd_net -net proc_sys_reset_0_interconnect_aresetn [get_bd_pins axi_protocol_converter_0/aresetn] [get_bd_pins proc_sys_reset/interconnect_aresetn]
 DONE connect_bd_net -net proc_sys_reset_0_peripheral_aresetn [get_bd_pins proc_sys_reset/peripheral_aresetn] [get_bd_pins xadc/s_axi_aresetn]
 DONE connect_bd_net -net processing_system7_0_fclk_clk0 [get_bd_ports FCLK_CLK0] [get_bd_pins processing_system7/FCLK_CLK0]
 DONE connect_bd_net -net processing_system7_0_fclk_clk1 [get_bd_ports FCLK_CLK1] [get_bd_pins processing_system7/FCLK_CLK1]
 DONE connect_bd_net -net processing_system7_0_fclk_clk2 [get_bd_ports FCLK_CLK2] [get_bd_pins processing_system7/FCLK_CLK2]
 DONE connect_bd_net -net processing_system7_0_fclk_clk3 [get_bd_ports FCLK_CLK3] [get_bd_pins axi_protocol_converter_0/aclk] [get_bd_pins proc_sys_reset/slowest_sync_clk] [get_bd_pins processing_system7/FCLK_CLK3] [get_bd_pins processing_system7/M_AXI_GP1_ACLK] [get_bd_pins xadc/s_axi_aclk]
 DONE connect_bd_net -net processing_system7_0_fclk_reset0_n [get_bd_ports FCLK_RESET0_N] [get_bd_pins processing_system7/FCLK_RESET0_N]
 DONE connect_bd_net -net processing_system7_0_fclk_reset1_n [get_bd_ports FCLK_RESET1_N] [get_bd_pins processing_system7/FCLK_RESET1_N]
 DONE connect_bd_net -net processing_system7_0_fclk_reset2_n [get_bd_ports FCLK_RESET2_N] [get_bd_pins processing_system7/FCLK_RESET2_N]
 DONE connect_bd_net -net processing_system7_0_fclk_reset3_n [get_bd_ports FCLK_RESET3_N] [get_bd_pins proc_sys_reset/ext_reset_in] [get_bd_pins processing_system7/FCLK_RESET3_N]
 DONE connect_bd_net -net s_axi_hp0_aclk [get_bd_ports S_AXI_HP0_aclk] [get_bd_pins processing_system7/S_AXI_HP0_ACLK]
 DONE connect_bd_net -net s_axi_hp1_aclk [get_bd_ports S_AXI_HP1_aclk] [get_bd_pins processing_system7/S_AXI_HP1_ACLK]
 DONE connect_bd_net -net xadc_wiz_0_ip2intc_irpt [get_bd_pins processing_system7/IRQ_F2P] [get_bd_pins xadc/ip2intc_irpt]
 DONE connect_bd_net -net xlconstant_dout [get_bd_pins proc_sys_reset/aux_reset_in] [get_bd_pins xlconstant/dout]

  # Create address segments
 DONE create_bd_addr_seg -range 0x40000000 -offset 0x40000000 [get_bd_addr_spaces processing_system7/Data] [get_bd_addr_segs M_AXI_GP0/Reg] SEG_system_Reg
 DONE create_bd_addr_seg -range 0x10000 -offset 0x83C00000 [get_bd_addr_spaces processing_system7/Data] [get_bd_addr_segs xadc/s_axi_lite/Reg] SEG_xadc_wiz_0_Reg
 DONE create_bd_addr_seg -range 0x20000000 -offset 0x0 [get_bd_addr_spaces S_AXI_HP0] [get_bd_addr_segs processing_system7/S_AXI_HP0/HP0_DDR_LOWOCM] SEG_processing_system7_0_HP0_DDR_LOWOCM
 DONE create_bd_addr_seg -range 0x20000000 -offset 0x0 [get_bd_addr_spaces S_AXI_HP1] [get_bd_addr_segs processing_system7/S_AXI_HP1/HP1_DDR_LOWOCM] SEG_processing_system7_0_HP1_DDR_LOWOCM

=================================

All DONE items were made by hand.



<EOF>  
