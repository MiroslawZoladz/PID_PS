#ifndef __CONFIG_H_
#define __CONFIG_H_

#define PLATFORM_ZYNQ

#define MB16   (0x1000000)
#define MB67  (0x03FFFFFF)

#define MB100 (0x06400000)
#define MB268 (0x10000000)

/* data transfer */
#define RESET_RX_CNTR_LIMIT			(400)

#define DMA_DEV_ID					(XPAR_AXIDMA_0_DEVICE_ID)

#define RX_INTR_ID					(XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR)

#define MEM_BASE_ADDR				(XPAR_PS7_DDR_0_S_AXI_BASEADDR)
#define MEM_HIGH_ADDR				(XPAR_PS7_DDR_0_S_AXI_HIGHADDR) // 1GB

#define APP_BASE					(MEM_BASE_ADDR)
#define APP_TOP						(MEM_BASE_ADDR+MB16)

#define RX_BUFFER_BASE				(APP_TOP)
#define RX_BUFFER_LENGTH	  		(MB16)
#define RX_BUFFER_TOP				(RX_BUFFER_BASE + RX_BUFFER_LENGTH)

#define CIRC_BUFFER_BASE			(RX_BUFFER_TOP)
#define CIRC_BUFFER_LENGTH  		(MB268)
#define CIRC_BUFFER_TOP   			(CIRC_BUFFER_BASE + CIRC_BUFFER_LENGTH)//(MEM_HIGH_ADDR)

//#define MAX_PKT_LEN					(MB67)	/* 67_108_863 */

/* TCP interface */
#define PLATFORM_EMAC_BASEADDR 		(XPAR_XEMACPS_0_BASEADDR)

#define ETH_LINK_DETECT_INTERVAL 	(4)
#define DHCP_CLIENT_TIMEOUT			(24)

#define INTC_DEVICE_ID				(XPAR_SCUGIC_SINGLE_DEVICE_ID)

#define TIMER_DEVICE_ID				(XPAR_SCUTIMER_DEVICE_ID)
#define INTC_BASE_ADDR				(XPAR_SCUGIC_0_CPU_BASEADDR)
#define INTC_DIST_BASE_ADDR			(XPAR_SCUGIC_0_DIST_BASEADDR)
#define TIMER_IRPT_INTR				(XPAR_SCUTIMER_INTR)


#endif
