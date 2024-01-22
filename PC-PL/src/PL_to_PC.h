#ifndef __PL_TO_PC_H_
#define __PL_TO_PC_H_

#include "xil_types.h"
#include "xscugic.h"

#include "lwip/tcp.h"

#define PACKET_SIZE		 (0x1FFFFFF) // PS:0xFFFFF, PL:0xFFFFE ;// 0x1FFFFFF 32MB // (0x3FFFFF) == 4MB

typedef struct CircullarBuffer {
	u8 *CurrentTop;
	u8 *Head;
	u8 *Tail;
} CircullarBuffer;

int 	PL2PC_DMA_Setup(XScuGic * IntcInstancePtr);
void 	PL2PC_start();
void 	OnDmaRx(void *Callback);

void    Tcp_SendDataChunk();
err_t   OnTcpTx(void *arg, struct tcp_pcb *tpcb, u16_t tcp_acknowledged_length);

void 	PL2PC_Tick();

void 	Transmitter_Buffer_Write(u8 * pointer, u32 length);

#endif
