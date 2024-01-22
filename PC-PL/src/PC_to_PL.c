#include "PC_to_PL.h"
#include "PL_to_PC.h"
#include "initialization.h"
#include <string.h>
#include "lwip/tcp.h"
#include "xtime_l.h"
#include "xaxidma.h"
#include "xscugic.h"
#include "config.h"

enum ReceiverStatus {EMPTY, GET_PAYLOAD, DONE};
static enum ReceiverStatus Status;

extern XAxiDma AxiDma;

extern u8 *RXBufferPtr;

u32 PLTransmission = 0;

u32 RxPayloadLength = 0;
u32 PL2PC_TxTransferLength = 0; 

/*###### TCP/IP ######*/

static u32 BytesTo32uint(u8 * pointer) {
	return (pointer[0] << 24) | (pointer[1] << 16) | (pointer[2] << 8) | pointer[3];
}

static enum ReceiverStatus Buffer_Append(u8 * pointer, u32 length) {
	static u32 ReceivedBytes = 0;
	memcpy(RXBufferPtr + ReceivedBytes, pointer, length);
	ReceivedBytes += length;
	if (ReceivedBytes >= RxPayloadLength) {
		ReceivedBytes = 0;
		return DONE;
	}
	return GET_PAYLOAD;
}

// funkcja jest wywo�ywana w momencie odebrania przez LWIP pakietu danych
// jako argumenty wywo�annia funckja dostaje wska�nik na dane pakietu oraz dlugo�� pakietu wyra�on� w bajtach
// posczeg�lne wywo�ania tej funkcji maj� doprawdzi� do odebrania ci�gu bajt�w opisuj�cego pojedy�cz� akwizycj� danych z uk�adu scalongo
// opis ten sk��da si� z nag�owka po kt�rym nast�pujwe sekwencja bajt� definuj�caprzebiegi sygna�ow steruj�cych uk�adem scalonym.
// nag�wek sk��da si� zd�ch cz�ci, tj. d�ugo�ci sekwencji opisuj�cej sterowanie oraz liczbe danych pomiarowych do odes�ania
// d�ugo�ci s� wyra�one w bnajtach i zapisane w postyaci liczby 32-bitowej (4 bajty)
// po odebraniu ostatniego bajtu opisu steroawnia funkcja uruchamian wysa�nie opisu sterowania do Sekwencera

int Receiver_DataManagement(u8 * tcp_packet_ptr, u32 tcp_packet_length) {

	u8 * tmp_pointer = tcp_packet_ptr;
	u32 tmp_length = tcp_packet_length;

	switch(Status){
	case EMPTY:
		xil_printf("EMPTY %d\r\n",tcp_packet_length);
		if (tcp_packet_length < 12) {
			xil_printf("RX tcp_packet length invalid \r\n");
			return XST_FAILURE;
		}
		RxPayloadLength  		= BytesTo32uint(tcp_packet_ptr);
		PL2PC_TxTransferLength 	= BytesTo32uint(tcp_packet_ptr + 4);
		PLTransmission 	 		= BytesTo32uint(tcp_packet_ptr + 8);

		if ((RxPayloadLength == 0) | (RxPayloadLength > RX_BUFFER_LENGTH)) {// | (PL2PC_TxTransferLength > MAX_TR_SIZE)){
			xil_printf("RX or TX header length invalid \r\n");
			return XST_FAILURE;
		}

		tmp_pointer = tcp_packet_ptr + 12;
		tmp_length = tcp_packet_length - 12;

	case GET_PAYLOAD:
		xil_printf("GET_PAYLOAD %d %d %d\r\n",RxPayloadLength, PL2PC_TxTransferLength, PLTransmission);
		Status = Buffer_Append(tmp_pointer, tmp_length);
		if(Status == GET_PAYLOAD) break;

	case DONE:
		PL2PC_start(); // Arm PL->PS
		SetAxiDmaTransferToPL(RXBufferPtr, RxPayloadLength);// Trigger PS->PL
		xil_printf("DONE\r\n");
		Status = EMPTY;
		break;
	}
	return XST_SUCCESS;
}

/*###### DMA ######*/

int SetAxiDmaTransferToPL(u8 * pointer, u32 length) {
	Xil_DCacheFlushRange((UINTPTR)pointer, length);

	int Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)pointer,
											length, XAXIDMA_DMA_TO_DEVICE);
	// xil_printf("AXI_DMA_arm_TX ");
	if (Status != XST_SUCCESS)
	{
		// xil_printf("Error\r\n");
	}
	else
	{
		// xil_printf("Success\r\n");
	}
	return Status;
}

