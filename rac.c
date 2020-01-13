#include "em_prs.h"
#include "em_cmu.h"

void route_rac(void) {
#define Pt gpioPortC
#define pt 10
#define Pr gpioPortC
#define pr 11
        CMU_ClockEnable(cmuClock_GPIO,1);
        GPIO_PinModeSet(Pt,pt,gpioModePushPull,0);
        GPIO_PinModeSet(Pr,pr,gpioModePushPull,0);
        PRS->CH[0].CTRL = PRS_RAC_TX;
        PRS->CH[9].CTRL = PRS_RAC_RX;
        PRS->ROUTELOC0 = 12 << 0;
        PRS->ROUTELOC2 = 16 << 8;
        PRS->ROUTEPEN = (1 << 0) | (1 << 9);
#undef Pt
#undef pt
#undef Pr
#undef pr
}
