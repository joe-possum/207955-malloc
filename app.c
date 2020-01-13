/***************************************************************************//**
 * @file app.c
 * @brief Silicon Labs Empty Example Project
 *
 * This example demonstrates the bare minimum needed for a Blue Gecko C application
 * that allows Over-the-Air Device Firmware Upgrading (OTA DFU). The application
 * starts advertising after boot and restarts advertising after a connection is closed.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

/* Bluetooth stack headers */
#include "bg_types.h"
#include "native_gecko.h"
#include "gatt_db.h"
#include "uartdrv.h"
#include "app.h"
//#include "dump.h"
#include "bg_version.h"

#include "logging.h"
#include "systick.h"
#include "rac.h"
#include "em_gpio.h"

extern uint16 head, tail;
extern uint16 headu[], tailu[], next_tailu[];

#if(0)
uint16 heads[1000],tails[1000],actives[1000];
uint16 head_count = 0;
#endif

uint32 start,stop;
uint16 histo[256];
uint32 packets[10];

void timer_start() {
  start = SysTick->VAL;      
}
void timer_stop(void) {
  stop = SysTick->VAL;
  if(stop > start) start += 0xffffff;
  printf("Delta: %lu\n",start-stop);
}

int scanning = 0;
uint32 total;

void show_stats(void) {
	  printf("%ld bytes received, push_count: %ld, pop_count: %ld, uart: %ld, active: %ld, equal: %ld, complete: %ld\n",
			  total,get_push_count(),get_pop_count(),get_uart_count(),get_active_count(),get_equal_count(),get_complete_count());
	  printf("Histo:\n");
	  total = 0;
	  for(int i = 0; i < 256; i++) {
		  if(histo[i]) printf("%d: %d\n",i,histo[i]);
		  total += histo[i];
	  }
	  //for(int i = 0; i < get_uart_count(); i++) printf(" head:%d,tail:%d,next_tail:%d\n",headu[i],tailu[i],next_tailu[i]);
	  uint16 pc = 0;
	  for(int i = 0; pc < total; i++) {
		  for(int j = 0; j < 32; j++) {
			  if(packets[i] & 1 << j) {
				  printf("1");
				  pc++;
			  } else {
				  printf(".");
			  }
			  RETARGET_SerialFlush();
		  }
	  }
#if(0)
	  for(int i = 0; i < head_count; i++) printf(" %3d,%3d,%3d%s",heads[i],tails[i],actives[i],(7==(i&7))?"\n":"");
	  printf("\n");
	  printf("DMADRV_MAX_XFER_COUNT: %d\n",DMADRV_MAX_XFER_COUNT);
	  printf("close event: %08x\n",gecko_evt_le_connection_closed_id);
#endif

}

uint8 cmd[256];
/* Main application */
void appMain(gecko_configuration_t *pconfig)
{
#if DISABLE_SLEEP > 0
  pconfig->sleep.flags = 0;
#endif

  /* Initialize debug prints. Note: debug prints are off by default. See DEBUG_LEVEL in app.h */
initLog();
printf("Hello, World!\n");
printf("sizeof(struct event): %d\n",sizeof_event());

init_systick();
init_logging();
route_rac();
#define SYNC_PORT gpioPortC
#define SYNC_PIN (9u)
#define SYNC(X) (X)?GPIO_PinOutSet(SYNC_PORT,SYNC_PIN):GPIO_PinOutClear(SYNC_PORT,SYNC_PIN)
GPIO_PinModeSet(SYNC_PORT,SYNC_PIN,gpioModePushPull,0);

#if(1)
while(rx_fill() < 1);
while(1) {
	uint8 rc = rx_get(1,&cmd[0]);
	if(rc) {
		printf("rx_get(1,*) returned %d\n",rc);
		while(1);
	}
	if('%' == cmd[0]) {
		while(rx_fill() < 6);
		rc = rx_get(6,&cmd[1]);
		if(rc) {
			printf("rx_get(6,*) returned %d\n",rc);
			while(1);
		}
		break;
	}
}
printf("cmd: %s\n",cmd);
#endif

#ifdef NO
GPIO_PinModeSet(BSP_BUTTON0_PORT,BSP_BUTTON0_PIN,gpioModeInputPullFilter,1);
printf("waiting for connection ... ");
RETARGET_SerialFlush();
while(GPIO_PinInGet(BSP_BUTTON0_PORT,BSP_BUTTON0_PIN));
#endif
printf("connected\n");

	log_event_id(0xfe,__LINE__,BG_VERSION_MAJOR << 24 | BG_VERSION_MINOR << 16 | BG_VERSION_PATCH << 8);
	SYNC(1);
/* Initialize stack */
  gecko_init(pconfig);

  log_event(0xff,__LINE__);
  while (1) {
    /* Event pointer for handling events */
    struct gecko_cmd_packet* evt;

    /* if there are no events pending then the next call to gecko_wait_event() may cause
     * device go to deep sleep. Make sure that debug prints are flushed before going to sleep */
    //log_event(0x10,__LINE__);
    int rc = gecko_event_pending();
    //log_event(0x20,__LINE__);
    if (!rc) {
      flushLog();
    }

    /* Check for stack event. This is a blocking event listener. If you want non-blocking please see UG136. */
    log_event(0x30,__LINE__);
    SYNC(0);
    evt = gecko_wait_event();
    SYNC(1);
    log_event_id(0x40,__LINE__,BGLIB_MSG_ID(evt->header));
#ifdef DUMP
    switch(BGLIB_MSG_ID(evt->header)) {
    //case gecko_evt_hardware_soft_timer_id:
    //case gecko_evt_gatt_server_user_write_request_id:
    	break;
    default:
    	printf("timestamp:%08x:%08x",BGLIB_MSG_ID(evt->header),timestamp());
    	dump_event(evt);
    }
#endif

	//printf("Calling logging_process()\n");
	logging_process();
	//printf("...done\n");
	//printf("%d events in queue\n",log_fill());

	uint32 current = SysTick->VAL;
	uint32 target = (current - 1*38400) & 0xffffff;

	/* Handle events */
    switch (BGLIB_MSG_ID(evt->header)) {
      /* This boot event is generated when the system boots up after reset.
       * Do not call any stack commands before receiving the boot event.
       * Here the system is set to start advertising immediately after boot procedure. */
      case gecko_evt_system_boot_id:
    	  total = 0;
    	  memset(histo,0,sizeof(histo));
    	  memset(packets,0,sizeof(packets));
    	  gecko_cmd_system_get_bt_address();
    	  printf("wwr: %04x\n",gattdb_wwr);
    	  printf("%d calls so far\n",log_fill());
    	  //gecko_cmd_hardware_set_soft_timer(5<<15,0,0);
    	  scanning = 0;
    	  log_event(0xf1,__LINE__);
    	  //gecko_cmd_le_gap_set_discovery_timing(le_gap_phy_1m,1600,1600);
    	  gecko_cmd_le_gap_set_advertise_timing(0, 160, 160, 0, 0);
          log_event(0xf2,__LINE__);
    	  //gecko_cmd_le_gap_start_discovery(le_gap_phy_1m,le_gap_discover_observation);
    	  gecko_cmd_le_gap_start_advertising(0, le_gap_general_discoverable, le_gap_connectable_scannable);
    	  log_event(0xf3,__LINE__);
        break;

    case gecko_evt_hardware_soft_timer_id: /***************************************************************** hardware_soft_timer **/
#define ED evt->data.evt_hardware_soft_timer
    	switch(scanning) {
    	case 0:
    		gecko_cmd_le_gap_end_procedure();
    		scanning++;
    		break;
    	case 1:
    		log_event(0xf4,__LINE__);
    		scanning++;
    		break;
    	case 2:
    		while(log_fill()) logging_process();
    		NVIC_SystemReset();
            gecko_cmd_le_gap_set_advertise_timing(0, 160, 160, 0, 0);
    		//gecko_cmd_le_gap_start_discovery(le_gap_phy_1m,le_gap_discover_observation);
    		scanning = 2;
    		break;
    	case 3:
            gecko_cmd_le_gap_start_advertising(0, le_gap_general_discoverable, le_gap_connectable_scannable);
    		//gecko_cmd_le_gap_end_procedure();
    		scanning = 0;
    		break;
    	case 4:
    		gecko_cmd_sm_set_bondable_mode(1);
    		scanning++;
    		break;
    	case 5:
    		gecko_cmd_sm_configure(3,sm_io_capability_displayonly);
    		scanning = 1;
    	}
      break;
#undef ED

      case gecko_evt_sm_passkey_display_id:
    	  printf("Passkey: %06ld\n",evt->data.evt_sm_passkey_display.passkey);
    	  break;

    case gecko_evt_system_external_signal_id: /*********************************************************** system_external_signal **/
#define ED evt->data.evt_system_external_signal
      //if(ED.extsignals & 1) printf("Transfer completed\n");
      if(ED.extsignals & 2) printf("... unexpected discrepancy\n");
      if(ED.extsignals & 4) {
    	  printf("Overrun\n");
    	  gecko_cmd_system_halt(1);
    	  show_stats();
      }
      break;
#undef ED

    case gecko_evt_le_connection_opened_id:
        break;

      case gecko_evt_le_connection_closed_id:
    	  show_stats();
        break;

      /* Add additional event handlers as your application requires */

      case gecko_evt_gatt_server_user_write_request_id:
#define ED evt->data.evt_gatt_server_user_write_request
    	  break;
    	  total += ED.value.len;
    	  histo[ED.value.len]++;
    	  {
    		  uint32 index = *(uint32*)&ED.value.data[0];
    		  packets[index >> 5] |= 1 << (index & 0x1f);
    		  printf("index: %ld\n",index);
    	  }
#if(0)
    	  actives[head_count] = get_pop_count();
    	  tails[head_count] = tail;
    	  heads[head_count++] = head;
#endif
    	  break;

      default:
        break;
    }
#if(0)
	if(target > current) while(SysTick->VAL < current);
	while(SysTick->VAL > target);
#endif
  }
}
