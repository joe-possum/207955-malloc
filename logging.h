#include <stdint.h>

uint16_t log_fill(void);
void logging_process(void);
void init_logging(void);
size_t sizeof_event(void);
uint32 get_push_count(void);
uint32 get_pop_count(void);
uint32 get_uart_count(void);
uint32 get_equal_count(void);
uint32 get_active_count(void);
uint32 get_complete_count(void);
size_t sizeof_event(void);
void log_event(uint8 event, uint32 line);
void log_event_id(uint8 event, uint32 line, uint32 id);
uint8 rx_fill(void);
uint8 rx_get(uint8 len, uint8*buf);
