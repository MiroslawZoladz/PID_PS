#ifndef __PC_TO_PL_H_
#define __PC_TO_PL_H_

#include "xil_types.h"
#include "xscugic.h"

int 	Receiver_DataManagement(u8 * tcp_packet_ptr, u32 tcp_packet_length);
int 	SetAxiDmaTransferToPL(u8 * pointer, u32 length);

#endif

