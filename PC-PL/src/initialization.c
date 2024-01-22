#include "initialization.h"
#include "PL_to_PC.h"
#include "PC_to_PL.h"

#include "xaxidma.h"
#include "config.h"
#include "xparameters.h"
#include "xscugic.h"

#include "lwip/tcp.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/priv/tcp_priv.h"

#include "xil_printf.h"
#include "netif/xadapter.h"
#include "xscutimer.h"
#include "xuartps.h"

static XScuTimer TimerInstance;
static int ResetRxCntr = 0;
volatile int TcpFastTmrFlag = 0;
volatile int TcpSlowTmrFlag = 0;
volatile int dhcp_timoutcntr = DHCP_CLIENT_TIMEOUT;
ip_addr_t ipaddr, netmask, gw;
static struct netif server_netif;
struct netif *tcp_interface_netif;
struct tcp_pcb *pcb;

XUartPs_Config	*Uart_0_Config;
XUartPs         Uart_0;

XAxiDma_Config *Config;
XAxiDma AxiDma;

u8 *RXBufferPtr;

extern u32 PL2PC_TxTransferLength;

/*###### UART ######*/

int UART_Init_9600(void) {
    int Status;
    Uart_0_Config = XUartPs_LookupConfig(XPAR_XUARTPS_0_DEVICE_ID);
    if (NULL == Uart_0_Config) {
        return XST_FAILURE;
    }
    Status = XUartPs_CfgInitialize(&Uart_0, Uart_0_Config, Uart_0_Config->BaseAddress);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }
    XUartPs_SetBaudRate(&Uart_0,9600 ); // 115200
    return XST_SUCCESS;
}


/*###### BUFFER ######*/

int DataTransfer_Init() {
	xil_printf("\r\n--- Entering DataTransferInit() --- \r\n");

	RXBufferPtr	= (u8 *)RX_BUFFER_BASE;

	if (DmaTransfer_Init()) {return XST_FAILURE;}

	xil_printf("\r\n--- Exiting DataTransferInit() --- \r\n");

	return XST_SUCCESS;
}

/*###### DMA ######*/

int DmaTransfer_Init(void) {
	int Status;

	Config = XAxiDma_LookupConfig(DMA_DEV_ID);
	if (!Config) {
		xil_printf("No config found for %d\r\n", DMA_DEV_ID);

		return XST_FAILURE;
	}

	Status = XAxiDma_CfgInitialize(&AxiDma, Config);

	if (Status != XST_SUCCESS) {
		xil_printf("AXI DMA: Initialization failed %d\r\n", Status);
		return XST_FAILURE;
	}

	if(XAxiDma_HasSg(&AxiDma)){
		xil_printf("AXI DMA: Device configured as SG mode \r\n");
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

void DmaTransfer_EnableInterrupt(void) {
	//XAxiDma_IntrEnable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrEnable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DEVICE_TO_DMA);
}

void DmaTransfer_DisableInterrupt(void) {
	//XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DMA_TO_DEVICE);

	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DEVICE_TO_DMA);
}

/*###### TCP/IP ######*/

void timer_callback(XScuTimer * TimerInstance) {
	static int DetectEthLinkStatus = 0;
	static int odd = 1;
    static int dhcp_timer = 0;
	DetectEthLinkStatus++;
	TcpFastTmrFlag = 1;

	odd = !odd;
	ResetRxCntr++;

	if (odd) {
		dhcp_timer++;
		dhcp_timoutcntr--;
		TcpSlowTmrFlag = 1;
		dhcp_fine_tmr();
		if (dhcp_timer >= 120) {
			dhcp_coarse_tmr();
			dhcp_timer = 0;
		}
	}

	if (ResetRxCntr >= RESET_RX_CNTR_LIMIT) {
		xemacpsif_resetrx_on_no_rxdata(tcp_interface_netif);
		ResetRxCntr = 0;
	}
	/* For detecting Ethernet phy link status periodically */
	if (DetectEthLinkStatus == ETH_LINK_DETECT_INTERVAL) {
		eth_link_detect(tcp_interface_netif);
		DetectEthLinkStatus = 0;
	}

	XScuTimer_ClearInterruptStatus(TimerInstance);
}

void print_ip(char *msg, ip_addr_t *ip) {
	print(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),
			ip4_addr3(ip), ip4_addr4(ip));
}

void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw) {
	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}

err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
	/* do not read the packet if we are not in ESTABLISHED state */
	if (!p) {
		tcp_close(tpcb);
		tcp_recv(tpcb, NULL);
		return ERR_OK;
	}

	pcb = tpcb;

	/* indicate that the packet has been received */
	tcp_recved(tpcb, p->len);

	/* pass payload to data_transfer module */
	Receiver_DataManagement(p->payload, p->len);

	/* free the received pbuf */
	pbuf_free(p);

	return ERR_OK;
}



err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
	static int connection = 1;

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);
	//tcp_sent(newpcb, OnTcpTx); !!!

	/* just use an integer number indicating the connection id as the
	   callback argument */
	tcp_arg(newpcb, (void*)(UINTPTR)connection);

	/* increment for subsequent accepted connections */
	connection++;

	return ERR_OK;
}

int start_application() {
	struct tcp_pcb *pcb;
	err_t err;
	unsigned port = 7;

	/* create new TCP PCB structure */
	pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		xil_printf("Error creating PCB. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	err = tcp_bind(pcb, IP_ANY_TYPE, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb, NULL);

	/* listen for connections */
	pcb = tcp_listen(pcb);
	if (!pcb) {
		xil_printf("Out of memory while tcp_listen\n\r");
		return -3;
	}

	/* specify callback to use for incoming connections */
	tcp_accept(pcb, accept_callback);

	xil_printf("TCP echo server started @ port %d\n\r", port);


	return 0;
}

void TcpInterface_SetupTimer(void) {
	int Status = XST_SUCCESS;
	XScuTimer_Config *ConfigPtr;
	int TimerLoadValue = 0;

	ConfigPtr = XScuTimer_LookupConfig(TIMER_DEVICE_ID);
	Status = XScuTimer_CfgInitialize(&TimerInstance, ConfigPtr,ConfigPtr->BaseAddr);
	if (Status != XST_SUCCESS) {
		xil_printf("In %s: Scutimer Cfg initialization failed...\r\n",__func__);
		return;
	}

	Status = XScuTimer_SelfTest(&TimerInstance);
	if (Status != XST_SUCCESS) {
		xil_printf("In %s: Scutimer Self test failed...\r\n",__func__);
		return;
	}

	XScuTimer_EnableAutoReload(&TimerInstance);
	/*
	 * Set for 250 milli seconds timeout.
	 */
	TimerLoadValue = XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ / 8;

	XScuTimer_LoadTimer(&TimerInstance, TimerLoadValue);
}

void TcpInterface_SetupInterrupt(void) {
	XScuGic_RegisterHandler(INTC_BASE_ADDR, TIMER_IRPT_INTR,(Xil_ExceptionHandler)timer_callback,(void *)&TimerInstance);
	XScuGic_EnableIntr(INTC_DIST_BASE_ADDR, TIMER_IRPT_INTR);
}

void TcpInterface_EnableInterrupt(void) {
	XScuTimer_EnableInterrupt(&TimerInstance);
	XScuTimer_Start(&TimerInstance);
}

int TcpInterface_Init() {
	/* the mac address of the board. this should be unique per board */
	unsigned char mac_ethernet_address[] = {0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	tcp_interface_netif = &server_netif;

    ipaddr.addr = 0;
	gw.addr = 0;
	netmask.addr = 0;

	lwip_init();

	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(tcp_interface_netif, &ipaddr, &netmask,
						&gw, mac_ethernet_address,
						PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}

	netif_set_default(tcp_interface_netif);

	return XST_SUCCESS;
}

int TcpInterface_Start() {
	/* specify that the network if is up */
	netif_set_up(tcp_interface_netif);

	
	/* Create a new DHCP client for this interface */

	/*
	dhcp_start(tcp_interface_netif);
	dhcp_timoutcntr = 24;

	while(((tcp_interface_netif->ip_addr.addr) == 0) && (dhcp_timoutcntr > 0))
		xemacif_input(tcp_interface_netif);

	if (dhcp_timoutcntr <= 0) {
		if ((tcp_interface_netif->ip_addr.addr) == 0) {
			xil_printf("! DHCP Timeout\r\n");
			xil_printf("Configuring default IP of 192.168.0.5\r\n");
			IP4_ADDR(&(tcp_interface_netif->ip_addr),  192, 168,   0, 5);
			IP4_ADDR(&(tcp_interface_netif->netmask), 255, 255, 255,  0);
			IP4_ADDR(&(tcp_interface_netif->gw),      192, 168,   0,  1);
		}
	}*/

	xil_printf("Configuring default IP of 192.168.0.5\r\n");
	IP4_ADDR(&(tcp_interface_netif->ip_addr),  192, 168,   0, 5);
	IP4_ADDR(&(tcp_interface_netif->netmask), 255, 255, 255,  0);
	IP4_ADDR(&(tcp_interface_netif->gw),      192, 168,   0,  1);


	ipaddr.addr = tcp_interface_netif->ip_addr.addr;
	gw.addr = tcp_interface_netif->gw.addr;
	netmask.addr = tcp_interface_netif->netmask.addr;

	print_ip_settings(&ipaddr, &netmask, &gw);

	start_application();

	return XST_SUCCESS;
}

void TcpInterface_Tick() {
	if (TcpFastTmrFlag) {
		tcp_fasttmr();
		TcpFastTmrFlag = 0;
	}
	if (TcpSlowTmrFlag) {
		tcp_slowtmr();
		TcpSlowTmrFlag = 0;
	}
	xemacif_input(tcp_interface_netif);
}
