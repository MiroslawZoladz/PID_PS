#ifndef __INITIALIZATION_H_
#define __INITIALIZATION_H_

#include "xscugic.h"
#include "xil_types.h"

#define MAX_TR_SIZE		0x3AFFFFFF

struct {
	u8 * pTxBytes;
	u16 length;
} data_chunk;

int 	UART_Init_9600(void);

int		TcpInterface_Init(void);
int 	TcpInterface_Start(void);
void 	TcpInterface_SetupTimer(void);
void 	TcpInterface_SetupInterrupt(void);
void 	TcpInterface_EnableInterrupt(void);
void 	TcpInterface_Tick(void);

int 	DataTransfer_Init(void);
int 	DmaTransfer_Init(void);

void 	DmaTransfer_EnableInterrupt(void);
void 	DmaTransfer_DisableInterrupt(void);

#endif
