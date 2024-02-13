#include "PL_to_PC.h"
#include "initialization.h"
#include <string.h>
#include "xtime_l.h"
#include "config.h"
#include "xaxidma.h"
#include "lwip/tcp.h"

extern struct tcp_pcb *pcb;
extern XAxiDma AxiDma;

u32 debug_uint = 0;
u32 debug_flag_uint = 0;
u32 debug_flag_bool = 0;

u32 hard_beet_prescaler = 0;

u32 prescaller = 0;

CircullarBuffer *CircBuff;

/*###### DMA ######*/

void Buffer_Init(CircullarBuffer *cb) {
	cb->CurrentTop = (u8 *)CIRC_BUFFER_TOP;
	cb->Head = (u8 *)CIRC_BUFFER_BASE;
	cb->Tail = (u8 *)CIRC_BUFFER_BASE;
}

u32 GetCbSize(CircullarBuffer *cb) {
	u32 size = cb->CurrentTop - (u8 *)CIRC_BUFFER_BASE; //max

	if(cb->Head >= cb->Tail) {
		size = (cb->Head - cb->Tail);
	} else {
		size = (size + cb->Head - cb->Tail);
	}

	return size;
}

void SetAxiDmaTransferFromPL(CircullarBuffer *cb) {
	if((cb->CurrentTop - cb->Head) < PACKET_SIZE) {
		cb->CurrentTop = cb->Head;
		cb->Head = (u8 *)CIRC_BUFFER_BASE;
	}

	int Status = XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR) cb->Head, PACKET_SIZE, XAXIDMA_DEVICE_TO_DMA);

	if (Status != XST_SUCCESS) debug_flag_bool = 1;
}

int PL2PC_DMA_Setup(XScuGic * IntcInstancePtr) {
	int Status;
	XScuGic_SetPriorityTriggerType(IntcInstancePtr, RX_INTR_ID, 0xA0, 0x3);
	Status = XScuGic_Connect(IntcInstancePtr, RX_INTR_ID, (Xil_InterruptHandler)OnDmaRx, &AxiDma);
	if (Status != XST_SUCCESS) return Status;
	XScuGic_Enable(IntcInstancePtr, RX_INTR_ID);
	Buffer_Init(CircBuff);
	return XST_SUCCESS;
}

void PL2PC_start() {
	//xil_printf("CBuffer:268MB\n\r");
	debug_flag_bool=1;
	Xil_DCacheFlushRange((UINTPTR) CIRC_BUFFER_BASE, CIRC_BUFFER_LENGTH);
	SetAxiDmaTransferFromPL(CircBuff);
}

void OnDmaRx(void *Callback) {

	debug_uint = XAxiDma_ReadReg(XPAR_AXIDMA_0_BASEADDR, 0x58);
	CircBuff->Head += debug_uint;
	SetAxiDmaTransferFromPL(CircBuff);

	XAxiDma *AxiDmaInst = (XAxiDma *)Callback;
	u32 IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DEVICE_TO_DMA);
	XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DEVICE_TO_DMA);

	debug_flag_uint = 1;
}

void PL2PC_Tick() {

	//XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DEVICE_TO_DMA);

	/*if (debug_flag_uint){
		debug_flag_uint=0;
		prescaller++;
		if (prescaller==200){
			prescaller=0;
			xil_printf("%d\n\r", GetCbSize(CircBuff)/1000000);
		}
	}*/

		if (debug_flag_bool){
			debug_flag_bool=0;
			//xil_printf("start\n\r");
		}

		if (debug_flag_uint){
			debug_flag_uint=0;
			//xil_printf("%d\n\r",debug_uint);
		}

		/*hard_beet_prescaler++;
		if (hard_beet_prescaler==1000000) {
			hard_beet_prescaler=0;
			xil_printf("Tick\n\r",debug_uint);
		}*/

		u32 TcpBuffSize = tcp_sndbuf(pcb);
		if (TcpBuffSize > 0) {
			u32 NrOfBytesToTransmit = GetCbSize(CircBuff);
			if (NrOfBytesToTransmit>0){
				if (NrOfBytesToTransmit > TcpBuffSize) NrOfBytesToTransmit = TcpBuffSize;

				if((CircBuff->CurrentTop - CircBuff->Tail) < NrOfBytesToTransmit) {
					NrOfBytesToTransmit = CircBuff->CurrentTop - CircBuff->Tail;
					tcp_write(pcb, CircBuff->Tail, NrOfBytesToTransmit, TCP_WRITE_FLAG_COPY|TCP_WRITE_FLAG_MORE);
					CircBuff->CurrentTop = (u8 *)CIRC_BUFFER_TOP; //reset to full buffer size
					CircBuff->Tail = (u8 *)CIRC_BUFFER_BASE;
				} else {
					tcp_write(pcb, CircBuff->Tail, NrOfBytesToTransmit,TCP_WRITE_FLAG_COPY|TCP_WRITE_FLAG_MORE);
					CircBuff->Tail += NrOfBytesToTransmit;
				}
				//tcp_output(pcb);
			}
		}
		tcp_output(pcb);
		//XAxiDma_IntrEnable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DEVICE_TO_DMA);
}



