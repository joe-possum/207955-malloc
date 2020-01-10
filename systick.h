#ifndef H_SYSTICK
#define H_SYSTICK
#include <stdint.h>
#include "em_device.h"
#include "em_core.h"

extern volatile uint32_t systicks;
extern volatile uint32_t systick_latency;
void init_systick(void);
inline uint32_t timestamp(void) {
  uint32_t rc;
  CORE_CRITICAL_SECTION(rc = (systicks << 24) + ((SysTick->CTRL & (1<<16)) << 8) - SysTick->VAL;)
  return rc;
}
#endif
