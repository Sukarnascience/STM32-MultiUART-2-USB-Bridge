#include "stm32f1_regs.h"
#include "clock.h"
#include "uart_driver.h"
#include "usb_cdc3.h"

static inline void led_on(void)  { GPIOC->BRR  = (1U << 13); }
static inline void led_off(void) { GPIOC->BSRR = (1U << 13); }

static void blink(int n, uint32_t ms)
{
    for (int i = 0; i < n; i++) {
        led_on();  delay_ms(ms);
        led_off(); delay_ms(ms);
    }
    delay_ms(300);
}

int main(void)
{
    /* Step 0: clock + systick */
    clock_init_48mhz();
    systick_init();

    /* LED init */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIO_CRH_SET(GPIOC, 13, GPIO_OUT_PP_2MHZ);
    GPIOC->BSRR = (1U << 13);

    /* 1 blink = entered main, clock OK */
    blink(1, 200);

    /* Step 1: UART init */
    uart_driver_init(115200);

    /* 2 blinks = uart_driver_init returned OK */
    blink(2, 200);

    /* Step 2: USB init */
    usb_cdc3_init();

    /* 3 blinks = usb_cdc3_init returned OK */
    blink(3, 200);

    /* Main loop */
    uint32_t last  = 0;
    int      state = 0;
    while (1) {
        usb_cdc3_poll();
        uint32_t now = millis();
        if ((now - last) >= 500U) {
            last  = now;
            state = !state;
            if (state) led_on(); else led_off();
        }
    }
}