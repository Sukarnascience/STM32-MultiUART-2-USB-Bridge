#include "clock.h"
#include "stm32f1_regs.h"

static volatile uint32_t g_tick;

/*
 * HSE 8MHz -> PLL x6 -> 48MHz SYSCLK
 * USBPRE = 1 (not divided) -> USB = 48MHz
 * APB1 prescaler = 1 (no div) -> APB1 = 48MHz
 * APB2 prescaler = 1 (no div) -> APB2 = 48MHz
 * Flash: 1 wait state (required for 24MHz < SYSCLK <= 48MHz)
 */
void clock_init_48mhz(void)
{
    /* Enable HSE */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* Flash: 1 wait state for 48MHz */
    FLASH_ACR = (1UL) | FLASH_ACR_PRFTBE;

    /*
     * PLL: source=HSE, multiplier=x6 -> 48MHz
     * USBPRE bit22=1 -> USB clock = PLLCLK/1 = 48MHz
     * No APB prescalers
     */
    RCC->CFGR = RCC_CFGR_PLLSRC_HSE
              | (4UL << 18)   /* PLLMUL x6: bits[21:18]=0100 */
              | (1UL << 22);  /* USBPRE=1: no division, USB=48MHz */

    /* Enable PLL */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* Switch SYSCLK to PLL */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & (3UL << 2)) != RCC_CFGR_SWS_PLL);
}

void systick_init(void)
{
    /* 48MHz / 48000 = 1kHz = 1ms tick */
    SYSTICK->LOAD = 48000UL - 1UL;
    SYSTICK->VAL  = 0;
    SYSTICK->CTRL = SYSTICK_CTRL_CLKSOURCE
                  | SYSTICK_CTRL_TICKINT
                  | SYSTICK_CTRL_ENABLE;
}

uint32_t millis(void) { return g_tick; }

void delay_ms(uint32_t ms)
{
    uint32_t start = g_tick;
    while ((g_tick - start) < ms);
}

void SysTick_Handler(void) { g_tick++; }

/*
 * stm32cube framework calls HAL_IncTick() from its own SysTick_Handler.
 * Since we define SysTick_Handler ourselves above, the cube one is overridden.
 * But HAL code that calls HAL_GetTick() / HAL_Delay() needs HAL_IncTick.
 * Provide a stub so the linker is happy if any cube object pulls it in.
 */
void HAL_IncTick(void) { g_tick++; }
uint32_t HAL_GetTick(void) { return g_tick; }