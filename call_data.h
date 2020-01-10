#ifndef CALL_DATA_H_
#define CALL_DATA_H_

struct event {
  uint32_t size, nmemb, function, ptr, timestamp;
  uint8_t name;
} __attribute__((__packed__));

#define N_CALL_DATA 1024
extern struct event call_data[];
extern volatile uint16_t head, tail;

#endif /* CALL_DATA_H_ */
