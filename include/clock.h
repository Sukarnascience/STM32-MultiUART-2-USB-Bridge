#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/*
 * Clock: HSE 8MHz x PLL6 = 48MHz SYSCLK
 * USB clock = 48MHz / 1 = 48MHz  (USBPRE=1, no division)
 * APB1 = 48MHz (no prescaler)
 * APB2 = 48MHz (no prescaler)
 *
 * Matches bluepill-uvc exactly.
 */
void     clock_init_48mhz(void);
void     systick_init(void);
uint32_t millis(void);
void     delay_ms(uint32_t ms);

/* Both APB buses run at 48MHz with this clock config */
#define APB1_FREQ_HZ  48000000UL
#define APB2_FREQ_HZ  48000000UL

#endif