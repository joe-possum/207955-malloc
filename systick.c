#include "systick.h"

volatile uint32_t systicks;
volatile uint32_t systick_latency;

void init_systick(void) {
  systicks = 1;
  SysTick->LOAD = 0x00ffffff;
  SysTick->CTRL = 7;
}

void SysTick_Handler (void) {
  systick_latency = SysTick->VAL;
  (void)SysTick->CTRL;
  systicks++;
}
