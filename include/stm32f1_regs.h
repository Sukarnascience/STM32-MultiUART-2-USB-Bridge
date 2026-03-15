#ifndef STM32F1_REGS_H
#define STM32F1_REGS_H

#include <stdint.h>

/* =========================================================================
 * Non-USB peripherals: 32-bit access (correct)
 * ====================================================================== */
#define IO  volatile
#define RO  volatile const

#define PERIPH_BASE   0x40000000UL
#define APB1_BASE     (PERIPH_BASE)
#define APB2_BASE     (PERIPH_BASE + 0x00010000UL)
#define AHB_BASE      (PERIPH_BASE + 0x00018000UL)

#define USART2_BASE   (APB1_BASE + 0x4400UL)
#define USART3_BASE   (APB1_BASE + 0x4800UL)
#define GPIOA_BASE    (APB2_BASE + 0x0800UL)
#define GPIOB_BASE    (APB2_BASE + 0x0C00UL)
#define GPIOC_BASE    (APB2_BASE + 0x1000UL)
#define USART1_BASE   (APB2_BASE + 0x3800UL)
#define RCC_BASE      (AHB_BASE  + 0x09000UL)
#define NVIC_BASE     0xE000E100UL
#define SYSTICK_BASE  0xE000E010UL

/* RCC */
typedef struct {
    IO uint32_t CR, CFGR, CIR, APB2RSTR, APB1RSTR,
                AHBENR, APB2ENR, APB1ENR, BDCR, CSR;
} RCC_t;
#define RCC ((RCC_t *)RCC_BASE)

#define RCC_CR_HSEON         (1UL<<16)
#define RCC_CR_HSERDY        (1UL<<17)
#define RCC_CR_PLLON         (1UL<<24)
#define RCC_CR_PLLRDY        (1UL<<25)
#define RCC_CFGR_SW_PLL      0x00000002UL
#define RCC_CFGR_SWS_PLL     0x00000008UL
#define RCC_CFGR_PPRE1_2     (0x4UL<<8)
#define RCC_CFGR_PLLSRC_HSE  (1UL<<16)
#define RCC_CFGR_PLLMUL_9    (0x7UL<<18)
#define RCC_CFGR_USBPRE_1_5  (0UL<<22)
#define RCC_APB2ENR_AFIOEN   (1UL<<0)
#define RCC_APB2ENR_IOPAEN   (1UL<<2)
#define RCC_APB2ENR_IOPBEN   (1UL<<3)
#define RCC_APB2ENR_IOPCEN   (1UL<<4)
#define RCC_APB2ENR_USART1EN (1UL<<14)
#define RCC_APB1ENR_USART2EN (1UL<<17)
#define RCC_APB1ENR_USART3EN (1UL<<18)
#define RCC_APB1ENR_USBEN    (1UL<<23)

/* FLASH */
#define FLASH_ACR            (*(IO uint32_t *)0x40022000UL)
#define FLASH_ACR_LATENCY_1  0x1UL
#define FLASH_ACR_PRFTBE     (1UL<<4)

/* GPIO */
typedef struct {
    IO uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR;
} GPIO_t;
#define GPIOA ((GPIO_t *)GPIOA_BASE)
#define GPIOB ((GPIO_t *)GPIOB_BASE)
#define GPIOC ((GPIO_t *)GPIOC_BASE)

#define GPIO_OUT_PP_2MHZ   0x2UL
#define GPIO_OUT_PP_50MHZ  0x3UL
#define GPIO_AF_PP_50MHZ   0xBUL
#define GPIO_IN_FLOAT      0x4UL

#define GPIO_CRL_SET(port,pin,val) \
    do{(port)->CRL=((port)->CRL&~(0xFUL<<((pin)*4)))|((uint32_t)(val)<<((pin)*4));}while(0)
#define GPIO_CRH_SET(port,pin,val) \
    do{(port)->CRH=((port)->CRH&~(0xFUL<<(((pin)-8)*4)))|((uint32_t)(val)<<(((pin)-8)*4));}while(0)

/* USART */
typedef struct {
    IO uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
} USART_t;
#define USART1 ((USART_t *)USART1_BASE)
#define USART2 ((USART_t *)USART2_BASE)
#define USART3 ((USART_t *)USART3_BASE)

#define USART_SR_RXNE    (1UL<<5)
#define USART_SR_TXE     (1UL<<7)
#define USART_CR1_UE     (1UL<<13)
#define USART_CR1_TXEIE  (1UL<<7)
#define USART_CR1_RXNEIE (1UL<<5)
#define USART_CR1_TE     (1UL<<3)
#define USART_CR1_RE     (1UL<<2)

/* =========================================================================
 * USB peripheral: ALL registers are 16-bit on STM32F1.
 * MUST use uint16_t access. Using uint32_t corrupts the peripheral.
 * ====================================================================== */
#define USB_BASE      0x40005C00UL
#define USB_SRAM_BASE 0x40006000UL

/* USB control/status registers - 16-bit */
#define USB_CNTR    (*(volatile uint16_t*)(USB_BASE + 0x40U))
#define USB_ISTR    (*(volatile uint16_t*)(USB_BASE + 0x44U))
#define USB_FNR     (*(volatile uint16_t*)(USB_BASE + 0x48U))
#define USB_DADDR   (*(volatile uint16_t*)(USB_BASE + 0x4CU))
#define USB_BTABLE  (*(volatile uint16_t*)(USB_BASE + 0x50U))

/* Endpoint registers - 16-bit, at BASE + n*4 */
#define USB_EPR(n)  (*(volatile uint16_t*)(USB_BASE + (uint32_t)(n)*4U))

/* USB_CNTR bits */
#define CNTR_FRES    (1U<<0)
#define CNTR_PDWN    (1U<<1)
#define CNTR_RESETM  (1U<<10)
#define CNTR_SUSPM   (1U<<11)
#define CNTR_CTRM    (1U<<15)

/* USB_ISTR bits */
#define ISTR_CTR     (1U<<15)
#define ISTR_RESET   (1U<<10)
#define ISTR_SUSP    (1U<<11)
#define ISTR_WKUP    (1U<<12)
#define ISTR_SOF     (1U<<9)
#define ISTR_DIR     (1U<<4)
#define ISTR_EP_ID   (0xFU)

/* USB_DADDR */
#define DADDR_EF     (1U<<7)

/* EPR bit masks */
#define EP_CTR_RX    (1U<<15)
#define EP_DTOG_RX   (1U<<14)
#define EP_STAT_RX   (3U<<12)
#define EP_SETUP     (1U<<11)
#define EP_TYPE_MSK  (3U<<9)
#define EP_KIND      (1U<<8)
#define EP_CTR_TX    (1U<<7)
#define EP_DTOG_TX   (1U<<6)
#define EP_STAT_TX   (3U<<4)
#define EP_EA        (0xFU)

/* EP_TYPE values (shifted into position) */
#define EP_TYPE_BULK (0U<<9)
#define EP_TYPE_CTRL (1U<<9)
#define EP_TYPE_ISO  (2U<<9)
#define EP_TYPE_INTR (3U<<9)

/* STAT values (NOT shifted - use in helpers) */
#define STAT_DIS  0U
#define STAT_STL  1U
#define STAT_NAK  2U
#define STAT_VLD  3U

/*
 * EPR_RW: bits that are plain read/write (no toggle behaviour).
 * Include CTR bits set to 1 so they are never accidentally cleared.
 * Exactly matches the working bluepill-uvc project.
 */
#define EPR_RW  (EP_CTR_RX | EP_SETUP | EP_TYPE_MSK | EP_KIND | EP_CTR_TX | EP_EA)

/* Toggle-safe STAT setters - copied from working bluepill-uvc */
static inline void ep_stat_tx(uint8_t ep, uint16_t s)
{
    uint16_t v = USB_EPR(ep);
    uint16_t w = (v & EPR_RW) | EP_CTR_RX | EP_CTR_TX;
    w ^= (uint16_t)((v ^ (uint16_t)(s << 4U)) & EP_STAT_TX);
    USB_EPR(ep) = w;
}
static inline void ep_stat_rx(uint8_t ep, uint16_t s)
{
    uint16_t v = USB_EPR(ep);
    uint16_t w = (v & EPR_RW) | EP_CTR_RX | EP_CTR_TX;
    w ^= (uint16_t)((v ^ (uint16_t)(s << 12U)) & EP_STAT_RX);
    USB_EPR(ep) = w;
}
static inline void ep_clr_tx(uint8_t ep)
{
    uint16_t v = USB_EPR(ep);
    USB_EPR(ep) = (uint16_t)((v & EPR_RW & (uint16_t)~EP_CTR_TX) | EP_CTR_RX);
}
static inline void ep_clr_rx(uint8_t ep)
{
    uint16_t v = USB_EPR(ep);
    USB_EPR(ep) = (uint16_t)((v & EPR_RW & (uint16_t)~EP_CTR_RX) | EP_CTR_TX);
}

/* =========================================================================
 * PMA (Packet Memory Area) access
 *
 * STM32F1 PMA is 16-bit wide at 32-bit physical stride.
 * Logical byte offset N -> physical address = USB_SRAM_BASE + N*2
 * BTABLE stores logical byte offsets.
 *
 * PMA16(logi) gives a pointer to the 16-bit word at logical offset logi.
 * Advancing by 2 in pointer arithmetic = advancing by 4 in physical bytes.
 * ====================================================================== */
#define PMA16(logi)  ((volatile uint16_t*)(USB_SRAM_BASE + (uint32_t)(logi)*2U))

/* BTABLE field accessors (logical byte offsets into PMA) */
#define BT_ATXT(n)  (*PMA16((n)*8U + 0U))   /* TX buffer address */
#define BT_CTXT(n)  (*PMA16((n)*8U + 2U))   /* TX byte count     */
#define BT_ARXR(n)  (*PMA16((n)*8U + 4U))   /* RX buffer address */
#define BT_CRXR(n)  (*PMA16((n)*8U + 6U))   /* RX block size cfg */

/* RX block size: BL_SIZE=1, NUM_BLOCK=2 -> 64 bytes */
#define RXBLK_64  ((1U<<15)|(2U<<10))
/* RX block size: BL_SIZE=0, NUM_BLOCK=8 -> 16 bytes */
#define RXBLK_16  (8U<<10)

static inline void pma_write(uint16_t logi, const uint8_t *s, uint16_t len)
{
    volatile uint16_t *d = PMA16(logi);
    for (uint16_t i = 0; i < len; i += 2U) {
        uint16_t w = s[i];
        if ((uint16_t)(i + 1U) < len) w |= (uint16_t)((uint16_t)s[i+1U] << 8U);
        *d = w;
        d += 2U;   /* advance by 2 uint16_t = 4 bytes physical */
    }
}
static inline void pma_read(uint16_t logi, uint8_t *d, uint16_t len)
{
    volatile uint16_t *s = PMA16(logi);
    for (uint16_t i = 0; i < len; i += 2U) {
        uint16_t w = *s;
        s += 2U;
        d[i] = (uint8_t)(w & 0xFFU);
        if ((uint16_t)(i + 1U) < len) d[i+1U] = (uint8_t)(w >> 8U);
    }
}

/* NVIC */
typedef struct {
    IO uint32_t ISER[8]; uint32_t _r0[24];
    IO uint32_t ICER[8]; uint32_t _r1[24];
    IO uint32_t ISPR[8]; uint32_t _r2[24];
    IO uint32_t ICPR[8]; uint32_t _r3[24];
    IO uint32_t IABR[8]; uint32_t _r4[56];
    IO uint8_t  IPR[240];
} NVIC_t;
#define NVIC ((NVIC_t *)NVIC_BASE)
static inline void nvic_enable(uint8_t n)
{ NVIC->ISER[n>>5] = (1UL<<(n&31U)); }
static inline void nvic_prio(uint8_t n, uint8_t p)
{ NVIC->IPR[n] = p; }

#define IRQ_USART1  37U
#define IRQ_USART2  38U
#define IRQ_USART3  39U

/* SysTick */
typedef struct { IO uint32_t CTRL, LOAD, VAL; RO uint32_t CALIB; } SysTick_t;
#define SYSTICK ((SysTick_t *)SYSTICK_BASE)
#define SYSTICK_CTRL_ENABLE    (1UL<<0)
#define SYSTICK_CTRL_TICKINT   (1UL<<1)
#define SYSTICK_CTRL_CLKSOURCE (1UL<<2)

#endif /* STM32F1_REGS_H */