#include "uart_driver.h"
#include "stm32f1_regs.h"
#include "clock.h"

uart_ch_t uart_ch[UART_CH_COUNT];

static USART_t * const g_usart[UART_CH_COUNT] = { USART1, USART2, USART3 };
static const uint32_t  g_apb[UART_CH_COUNT]   = { APB2_FREQ_HZ, APB1_FREQ_HZ, APB1_FREQ_HZ };

static void gpio_clock_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN
                  | RCC_APB2ENR_IOPAEN
                  | RCC_APB2ENR_IOPBEN
                  | RCC_APB2ENR_USART1EN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN
                  | RCC_APB1ENR_USART3EN;

    /* USART1: PA9=TX, PA10=RX */
    GPIO_CRH_SET(GPIOA,  9, GPIO_AF_PP_50MHZ);
    GPIO_CRH_SET(GPIOA, 10, GPIO_IN_FLOAT);
    /* USART2: PA2=TX, PA3=RX */
    GPIO_CRL_SET(GPIOA, 2, GPIO_AF_PP_50MHZ);
    GPIO_CRL_SET(GPIOA, 3, GPIO_IN_FLOAT);
    /* USART3: PB10=TX, PB11=RX */
    GPIO_CRH_SET(GPIOB, 10, GPIO_AF_PP_50MHZ);
    GPIO_CRH_SET(GPIOB, 11, GPIO_IN_FLOAT);
}

static void usart_setup(uint8_t ch, uint32_t baud)
{
    USART_t *u = g_usart[ch];
    u->CR1 = 0;
    u->BRR = (uint32_t)(g_apb[ch] / baud);
    u->CR2 = 0;
    u->CR3 = 0;
    u->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
}

void uart_driver_init(uint32_t baud)
{
    for (uint8_t i = 0; i < UART_CH_COUNT; i++) {
        ring_init(&uart_ch[i].rx);
        ring_init(&uart_ch[i].tx);
    }
    gpio_clock_init();
    for (uint8_t i = 0; i < UART_CH_COUNT; i++)
        usart_setup(i, baud);

    nvic_prio(IRQ_USART1, 0x40);
    nvic_prio(IRQ_USART2, 0x40);
    nvic_prio(IRQ_USART3, 0x40);
    nvic_enable(IRQ_USART1);
    nvic_enable(IRQ_USART2);
    nvic_enable(IRQ_USART3);
}

void uart_set_baud(uint8_t ch, uint32_t baud)
{
    if (ch >= UART_CH_COUNT || baud == 0) return;
    g_usart[ch]->CR1 &= ~USART_CR1_UE;
    g_usart[ch]->BRR  = (uint32_t)(g_apb[ch] / baud);
    g_usart[ch]->CR1 |= USART_CR1_UE;
}

void uart_kick_tx(uint8_t ch)
{
    if (ch >= UART_CH_COUNT) return;
    if (!ring_empty(&uart_ch[ch].tx))
        g_usart[ch]->CR1 |= USART_CR1_TXEIE;
}

static void usart_isr_common(uint8_t ch)
{
    USART_t *u  = g_usart[ch];
    uint32_t sr = u->SR;

    if (sr & USART_SR_RXNE) {
        uint8_t b = (uint8_t)(u->DR & 0xFFU);
        ring_push(&uart_ch[ch].rx, b);
    }
    if ((sr & USART_SR_TXE) && (u->CR1 & USART_CR1_TXEIE)) {
        uint8_t b;
        if (ring_pop(&uart_ch[ch].tx, &b))
            u->DR = b;
        else
            u->CR1 &= ~USART_CR1_TXEIE;
    }
}

void USART1_IRQHandler(void) { usart_isr_common(UART_CH0); }
void USART2_IRQHandler(void) { usart_isr_common(UART_CH1); }
void USART3_IRQHandler(void) { usart_isr_common(UART_CH2); }