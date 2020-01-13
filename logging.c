#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "call_data.h"
#include "systick.h"
#include "uartdrv.h"
#include "native_gecko.h"
#include "logging.h"

struct event call_data[N_CALL_DATA];
volatile uint16_t head = 0, tail = 0;
uint32 push_count = 0, pop_count = 0, uart_count = 0, active_count = 0, equal_count = 0, complete_count = 0;
volatile uint8_t uart_active, full = 0, overrun = 0;
uint16 headu[500], tailu[500], next_tailu[500];

#define DATA call_data[head]
int flag = 0;
#define A register uint32_t lr; __asm volatile ("MOV %0, LR\n" : "=r" (lr) )
#define B register uint32_t sp; __asm volatile ("MOV %0, SP\n" : "=r" (sp) )
#define COMMON(LR,NAME,SIZE,NMEMB,PTR) do { \
	if(overrun) break; \
	if(full) { \
		overrun = 1; \
		gecko_external_signal(4); \
		break; \
	} \
	DATA.function = LR; \
	DATA.name = NAME; \
	DATA.size = SIZE; \
	DATA.nmemb = NMEMB; \
	DATA.ptr = (uint32_t)PTR; \
	DATA.timestamp = timestamp(); \
	head++; \
	head &= (N_CALL_DATA-1); \
	push_count++; \
	if (head == tail) full = 1; \
	/*if(!uart_active && (((N_CALL_DATA + head - tail) & (N_CALL_DATA-1)) > 10)) gecko_external_signal(1);*/ \
	} while(0)
#define M(P,N) \
	void *__real ## P ## malloc(size_t); \
	void *__wrap ## P ## malloc(size_t size) { \
		A; \
		B; \
		flag++; \
		void *rc = __real ## P ## malloc(size); \
		flag--; \
		if(flag) return rc; \
		COMMON(lr,N,size,((int32_t*)sp)[11],rc); \
		return rc; \
	}
#define C(P,N) \
	void *__real ## P ## calloc(size_t, size_t); \
	void *__wrap ## P ## calloc(size_t nmemb, size_t size) { \
		A; \
		flag++; \
		void *rc = __real ## P ## calloc(nmemb, size); \
		flag--; \
		if(flag) return rc; \
		COMMON(lr,N,size,nmemb,rc); \
		return rc; \
	}
#define F(P,N) \
	void __real ## P ## free(void*); \
	void __wrap ## P ## free(void *ptr) { \
		A; \
		flag++; \
		__real ## P ## free(ptr); \
		flag--; \
		if(flag) return; \
		COMMON(lr,N,0,0,ptr); \
	}

M(_bg_,8)
M(_sl_,4)
M(_,0)
C(_bg_,9)
C(_sl_,5)
C(_,1)
F(_bg_,10)
F(_sl_,6)
F(_,2)

uint16_t log_fill(void) {
  return (head + N_CALL_DATA - tail) & (N_CALL_DATA-1);
}

uint32 get_push_count(void) {
	return push_count;
}

uint32 get_pop_count(void) {
	return pop_count;
}

uint32 get_uart_count(void)  {
	return uart_count;
}

uint32 get_equal_count(void) { return equal_count; }

uint32 get_active_count(void) { return active_count; }

uint32 get_complete_count(void) { return complete_count; }

size_t sizeof_event(void) {
  return sizeof(struct event);
}

void log_event(uint8 event, uint32 line) {
	A;
	COMMON(lr,event,line,push_count,pop_count);
}

void log_event_id(uint8 event, uint32 line, uint32 id, uint32 eptr) {
	A;
	COMMON(lr,event,eptr,push_count,id);
}

static UARTDRV_HandleData_t handle;
DEFINE_BUF_QUEUE(3,rxBufferQueue);
DEFINE_BUF_QUEUE(3,txBufferQueue);

#define CTS_PIN                            (10U)
#define CTS_PORT                           (gpioPortC)
#define CTS_LOC                            (11U)

#define RTS_PIN                            (11U)
#define RTS_PORT                           (gpioPortC)
#define RTS_LOC                            (11U)

#define RX_PIN                          (12U)
#define RX_PORT                         (gpioPortD)
#define RX_LOC                          (19U)

#define TX_PIN                          (11U)
#define TX_PORT                         (gpioPortD)
#define TX_LOC                          (19U)


static uint8 rx_buffer[256];
volatile static uint8 rx_head = 0, rx_tail = 0;

uint8 rx_fill(void) {
	return rx_head - rx_tail;
}

uint8 rx_get(uint8 len, uint8*buf) {
	if(!len) return 1;
	if(len > rx_fill()) return 2;
	if(rx_tail > rx_head) {
		uint8 tlen = -rx_tail;
		if(tlen > len) tlen = len;
		memcpy(buf,&rx_buffer[rx_tail],tlen);
		buf += tlen;
		len -= tlen;
		rx_tail += tlen;
	}
	if(!len) return 0;
	memcpy(buf,&rx_buffer[rx_tail],len);
	rx_tail += len;
	return 0;
}

void callback_rx(UARTDRV_Handle_t handle, Ecode_t transferStatus, uint8_t *data, UARTDRV_Count_t transferCount) {
	rx_head++;
	if (rx_head == rx_tail) rx_head--;
	UARTDRV_Receive(handle,(uint8_t*)&rx_buffer[rx_head],1,callback_rx);
}

void init_logging(void) {
	UARTDRV_Init_t init;
	init.baudRate = 115200;
	init.fcType = uartdrvFlowControlNone;
	init.mvdis = false;
	init.oversampling = usartOVS16;
	init.parity = usartNoParity;
	init.port = USART1;
	init.portLocationCts = CTS_LOC;
	init.portLocationRts = RTS_LOC;
	init.portLocationRx = RX_LOC;
	init.portLocationTx = TX_LOC;
	init.ctsPin = CTS_PIN;
	init.ctsPort = CTS_PORT;
	init.rtsPin = RTS_PIN;
	init.rtsPort = RTS_PORT;
	init.stopBits = usartStopbits1;
	init.rxQueue = (UARTDRV_Buffer_FifoQueue_t *) &rxBufferQueue;
	init.txQueue = (UARTDRV_Buffer_FifoQueue_t *) &txBufferQueue;
	GPIO_PinModeSet(TX_PORT,TX_PIN,gpioModePushPull,1);
	GPIO_PinModeSet(RX_PORT,RX_PIN,gpioModeInputPull,1);
	USART1->ROUTELOC0 = (TX_LOC << 8) | RX_LOC;
	USART1->ROUTEPEN = 3;
	UARTDRV_Init(&handle,&init);
	uart_active = 0;
	UARTDRV_Receive(&handle,&rx_buffer[rx_head],1,callback_rx);
}

UARTDRV_Count_t expect;
uint16_t next_tail;

void callback(UARTDRV_Handle_t handle, Ecode_t transferStatus, uint8_t *data, UARTDRV_Count_t transferCount) {
  //	gecko_external_signal(1);
(void)handle;
(void)transferStatus;
(void)data;
(void)transferCount;
uart_active = 0;
tail = next_tail;
if(transferCount != expect) gecko_external_signal(2);
complete_count++;
if(overrun) return;
if (head != tail) logging_process();
}

#define MAX_EVENTS (DMADRV_MAX_XFER_COUNT/sizeof(struct event))
void logging_process(void) {
	int rc = 0;
	CORE_CRITICAL_SECTION(
	if(uart_active) { active_count++; rc = 1; }
	else if(head == tail) { equal_count++; rc = 1; }
	)
	if(rc) return;
	CORE_CRITICAL_SECTION(
		headu[uart_count] = head;
		tailu[uart_count] = tail;
		next_tail = tail;
		if(head < tail) {
			expect = (N_CALL_DATA - tail);
		} else {
			expect = head - tail;
		}
	)
	if(expect > MAX_EVENTS) {
		expect = MAX_EVENTS;
	}
	next_tail += expect;
	next_tail &= (N_CALL_DATA-1);
	uart_active = 1;
	pop_count += expect;
	expect *= sizeof(struct event);
	next_tailu[uart_count] = next_tail;
	if(UARTDRV_Transmit(&handle,(uint8_t*)&call_data[tailu[uart_count]],expect,callback)) {
		gecko_external_signal(8);
	}
	uart_count++;
}
