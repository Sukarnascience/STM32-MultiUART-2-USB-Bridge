#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include "ring_buffer.h"

#define UART_CH_COUNT 3U
#define UART_CH0      0U
#define UART_CH1      1U
#define UART_CH2      2U

typedef struct {
    ring_buf_t rx;
    ring_buf_t tx;
} uart_ch_t;

extern uart_ch_t uart_ch[UART_CH_COUNT];

void uart_driver_init(uint32_t baud);
void uart_set_baud(uint8_t ch, uint32_t baud);
void uart_kick_tx(uint8_t ch);

#endif