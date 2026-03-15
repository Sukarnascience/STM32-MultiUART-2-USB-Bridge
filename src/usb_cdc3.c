#include "usb_cdc3.h"
#include "uart_driver.h"
#include "stm32f1_regs.h"
#include "ring_buffer.h"
#include "clock.h"

#include <stdint.h>
#include <string.h>

/* =========================================================================
 * PMA layout  (logical byte offsets stored in BTABLE)
 *
 *   BTABLE   0x000  (8 EPs x 8 logical bytes = 64 logical bytes)
 *   EP0 TX   0x040  64 bytes
 *   EP0 RX   0x080  64 bytes
 *   EP1 TX   0x0C0  16 bytes  port0 notif
 *   EP2 TX   0x0D0  16 bytes  port0 data IN
 *   EP2 RX   0x0E0  16 bytes  port0 data OUT
 *   EP3 TX   0x0F0  16 bytes  port1 notif
 *   EP4 TX   0x100  16 bytes  port1 data IN
 *   EP4 RX   0x110  16 bytes  port1 data OUT
 *   EP5 TX   0x120  16 bytes  port2 notif
 *   EP6 TX   0x130  16 bytes  port2 data IN
 *   EP6 RX   0x140  16 bytes  port2 data OUT
 *   Total:   0x150 = 336 bytes  (PMA = 512 bytes on F103C8)
 * ====================================================================== */
#define PMA_EP0_TX  0x040U
#define PMA_EP0_RX  0x080U
#define PMA_EP1_TX  0x0C0U
#define PMA_EP2_TX  0x0D0U
#define PMA_EP2_RX  0x0E0U
#define PMA_EP3_TX  0x0F0U
#define PMA_EP4_TX  0x100U
#define PMA_EP4_RX  0x110U
#define PMA_EP5_TX  0x120U
#define PMA_EP6_TX  0x130U
#define PMA_EP6_RX  0x140U

static const uint8_t  DATA_EP[3]     = { 2U, 4U, 6U };
static const uint8_t  NOTIF_EP[3]    = { 1U, 3U, 5U };
static const uint16_t DATA_TX_PMA[3] = { PMA_EP2_TX, PMA_EP4_TX, PMA_EP6_TX };
static const uint16_t DATA_RX_PMA[3] = { PMA_EP2_RX, PMA_EP4_RX, PMA_EP6_RX };
static const uint16_t NOTIF_TX_PMA[3]= { PMA_EP1_TX, PMA_EP3_TX, PMA_EP5_TX };

/* =========================================================================
 * Descriptors
 * ====================================================================== */
static const uint8_t g_dev_desc[] = {
    18, 0x01,
    0x00, 0x02,       /* bcdUSB 2.0 */
    0xEF, 0x02, 0x01, /* Misc class, IAD */
    64,               /* EP0 MPS */
    (uint8_t)(USB_VID), (uint8_t)(USB_VID>>8),
    (uint8_t)(USB_PID), (uint8_t)(USB_PID>>8),
    0x00, 0x02,
    0x01, 0x02, 0x03, 0x01
};

#define CFG_LEN 207U
#define CS 0x24U  /* CS_INTERFACE */

static const uint8_t g_cfg_desc[CFG_LEN] = {
    0x09,0x02, (uint8_t)CFG_LEN,0x00, 0x06,0x01,0x00,0x80,0xFA,
    /* Port 0 */
    0x08,0x0B,0x00,0x02,0x02,0x02,0x01,0x00,
    0x09,0x04,0x00,0x00,0x01,0x02,0x02,0x01,0x00,
    0x05,CS,0x00,0x10,0x01,
    0x05,CS,0x01,0x00,0x01,
    0x04,CS,0x02,0x02,
    0x05,CS,0x06,0x00,0x01,
    0x07,0x05,0x81,0x03,CDC_NOTIF_MPS,0x00,0xFF,
    0x09,0x04,0x01,0x00,0x02,0x0A,0x00,0x00,0x00,
    0x07,0x05,0x02,0x02,CDC_DATA_MPS,0x00,0x00,
    0x07,0x05,0x82,0x02,CDC_DATA_MPS,0x00,0x00,
    /* Port 1 */
    0x08,0x0B,0x02,0x02,0x02,0x02,0x01,0x00,
    0x09,0x04,0x02,0x00,0x01,0x02,0x02,0x01,0x00,
    0x05,CS,0x00,0x10,0x01,
    0x05,CS,0x01,0x00,0x03,
    0x04,CS,0x02,0x02,
    0x05,CS,0x06,0x02,0x03,
    0x07,0x05,0x83,0x03,CDC_NOTIF_MPS,0x00,0xFF,
    0x09,0x04,0x03,0x00,0x02,0x0A,0x00,0x00,0x00,
    0x07,0x05,0x04,0x02,CDC_DATA_MPS,0x00,0x00,
    0x07,0x05,0x84,0x02,CDC_DATA_MPS,0x00,0x00,
    /* Port 2 */
    0x08,0x0B,0x04,0x02,0x02,0x02,0x01,0x00,
    0x09,0x04,0x04,0x00,0x01,0x02,0x02,0x01,0x00,
    0x05,CS,0x00,0x10,0x01,
    0x05,CS,0x01,0x00,0x05,
    0x04,CS,0x02,0x02,
    0x05,CS,0x06,0x04,0x05,
    0x07,0x05,0x85,0x03,CDC_NOTIF_MPS,0x00,0xFF,
    0x09,0x04,0x05,0x00,0x02,0x0A,0x00,0x00,0x00,
    0x07,0x05,0x06,0x02,CDC_DATA_MPS,0x00,0x00,
    0x07,0x05,0x86,0x02,CDC_DATA_MPS,0x00,0x00,
};
#undef CS

#define S(c) (c),0x00
static const uint8_t g_str0[] = {4,0x03,0x09,0x04};
static const uint8_t g_str1[] = {18,0x03,S('B'),S('e'),S('e'),S('B'),S('o'),S('t'),S('i'),S('x')};
static const uint8_t g_str2[] = {24,0x03,S('B'),S('e'),S('e'),S('S'),S('e'),S('r'),S('i'),S('a'),S('l'),S('3'),S('x')};
static const uint8_t g_str3[] = {22,0x03,S('B'),S('E'),S('E'),S('S'),S('R'),S('L'),S('0'),S('0'),S('1')};
#undef S
static const uint8_t * const g_str[] = {g_str0,g_str1,g_str2,g_str3};

/* =========================================================================
 * State
 * ====================================================================== */
static uint8_t  g_addr;
static uint8_t  g_configured;
static volatile uint8_t g_dtr[3];
static volatile uint8_t g_tx_busy[7];

typedef struct __attribute__((packed)) {
    uint32_t dwDTERate; uint8_t bCharFormat, bParityType, bDataBits;
} lc_t;
static lc_t g_lc[3] = {{115200,0,0,8},{115200,0,0,8},{115200,0,0,8}};

/* EP0 TX state */
static const uint8_t *g_ep0_ptr;
static uint16_t       g_ep0_len;
static uint16_t       g_ep0_sent;

/* EP0 RX state (SetLineCoding data phase) */
static uint8_t  g_rx_buf[16];
static uint16_t g_rx_got;
static uint16_t g_rx_need;
static uint8_t  g_rx_iface;
static uint8_t  g_rx_pending;   /* 1 = waiting for data OUT on EP0 */

/* =========================================================================
 * EP0 helpers - modelled on working bluepill-uvc
 * ====================================================================== */
static void ep0_queue(const uint8_t *buf, uint16_t len, uint16_t wlen)
{
    if (len > wlen) len = wlen;
    g_ep0_ptr  = buf;
    g_ep0_len  = len;
    g_ep0_sent = 0;
    uint16_t c = (len > 64U) ? 64U : len;
    pma_write(PMA_EP0_TX, buf, c);
    BT_CTXT(0) = c;
    ep_stat_tx(0, STAT_VLD);
    g_ep0_sent = c;
}

static void ep0_next(void)
{
    if (g_ep0_sent < g_ep0_len) {
        uint16_t r = g_ep0_len - g_ep0_sent;
        uint16_t c = (r > 64U) ? 64U : r;
        pma_write(PMA_EP0_TX, g_ep0_ptr + g_ep0_sent, c);
        BT_CTXT(0) = c;
        ep_stat_tx(0, STAT_VLD);
        g_ep0_sent += c;
    }
}

static void ep0_zlp(void)
{
    g_ep0_len = 0; g_ep0_sent = 0;
    BT_CTXT(0) = 0;
    ep_stat_tx(0, STAT_VLD);
}

/* =========================================================================
 * USB reset - open EP0 only, exactly like bluepill-uvc handle_reset()
 * ====================================================================== */
static void usb_handle_reset(void)
{
    g_addr       = 0;
    g_configured = 0;
    g_rx_pending = 0;
    for (uint8_t i = 0; i < 3U; i++) {
        g_dtr[i] = 0;
        g_tx_busy[DATA_EP[i]] = 0;
    }

    USB_BTABLE = 0;
    BT_ATXT(0) = PMA_EP0_TX; BT_CTXT(0) = 0;
    BT_ARXR(0) = PMA_EP0_RX; BT_CRXR(0) = RXBLK_64;
    USB_EPR(0) = (uint16_t)(EP_TYPE_CTRL | 0x00U);
    ep_stat_tx(0, STAT_NAK);
    ep_stat_rx(0, STAT_VLD);
    USB_DADDR = (uint16_t)DADDR_EF;
}

/* =========================================================================
 * SETUP handler
 * ====================================================================== */
static void usb_handle_setup(void)
{
    uint8_t s[8];
    pma_read(PMA_EP0_RX, s, 8);

    uint8_t  bmRT = s[0], bReq = s[1];
    uint16_t wVal = (uint16_t)s[2] | ((uint16_t)s[3] << 8U);
    uint16_t wIdx = (uint16_t)s[4] | ((uint16_t)s[5] << 8U);
    uint16_t wLen = (uint16_t)s[6] | ((uint16_t)s[7] << 8U);

    /* GET_DESCRIPTOR */
    if (bmRT == 0x80U && bReq == 0x06U) {
        uint8_t dtype = (uint8_t)(wVal >> 8U);
        uint8_t didx  = (uint8_t)(wVal & 0xFFU);
        if (dtype == 0x01U) { ep0_queue(g_dev_desc, sizeof(g_dev_desc), wLen); return; }
        if (dtype == 0x02U && didx == 0U) { ep0_queue(g_cfg_desc, CFG_LEN, wLen); return; }
        if (dtype == 0x03U && didx < 4U)  { ep0_queue(g_str[didx], g_str[didx][0], wLen); return; }
        ep_stat_tx(0, STAT_STL); return;
    }

    /* SET_ADDRESS */
    if (bmRT == 0x00U && bReq == 0x05U) {
        g_addr = (uint8_t)(wVal & 0x7FU);
        ep0_zlp(); return;
    }

    /* SET_CONFIGURATION */
    if (bmRT == 0x00U && bReq == 0x09U) {
        g_configured = (uint8_t)(wVal & 0xFFU);
        if (g_configured) {
            /* Open notification EPs (interrupt IN, NAK) */
            for (uint8_t p = 0; p < 3U; p++) {
                uint8_t ep = NOTIF_EP[p];
                BT_ATXT(ep) = NOTIF_TX_PMA[p]; BT_CTXT(ep) = 0;
                USB_EPR(ep) = (uint16_t)(EP_TYPE_INTR | ep);
                ep_stat_tx(ep, STAT_NAK);
            }
            /* Open data EPs (bulk IN+OUT) */
            for (uint8_t p = 0; p < 3U; p++) {
                uint8_t ep = DATA_EP[p];
                BT_ATXT(ep) = DATA_TX_PMA[p]; BT_CTXT(ep) = 0;
                BT_ARXR(ep) = DATA_RX_PMA[p]; BT_CRXR(ep) = RXBLK_16;
                USB_EPR(ep) = (uint16_t)(EP_TYPE_BULK | ep);
                ep_stat_tx(ep, STAT_NAK);
                ep_stat_rx(ep, STAT_VLD);
            }
        }
        ep0_zlp(); return;
    }

    /* CDC GET_LINE_CODING (bmRT=0xA1) */
    if (bmRT == 0xA1U && bReq == 0x21U) {
        uint8_t iface = (uint8_t)(wIdx & 0xFFU);
        for (uint8_t p = 0; p < 3U; p++) {
            if (iface == (p * 2U)) {
                ep0_queue((const uint8_t *)&g_lc[p], sizeof(lc_t), wLen);
                return;
            }
        }
        ep_stat_tx(0, STAT_STL); return;
    }

    /* CDC SET_LINE_CODING (bmRT=0x21, bReq=0x20) */
    if (bmRT == 0x21U && bReq == 0x20U) {
        g_rx_got    = 0;
        g_rx_need   = sizeof(lc_t);
        g_rx_iface  = (uint8_t)(wIdx & 0xFFU);
        g_rx_pending = 1;
        BT_CRXR(0)  = RXBLK_64;
        ep_stat_rx(0, STAT_VLD);
        /* No ZLP yet - wait for data OUT phase */
        return;
    }

    /* CDC SET_CONTROL_LINE_STATE (bmRT=0x21, bReq=0x22) */
    if (bmRT == 0x21U && bReq == 0x22U) {
        uint8_t iface = (uint8_t)(wIdx & 0xFFU);
        for (uint8_t p = 0; p < 3U; p++) {
            if (iface == (p * 2U)) {
                g_dtr[p] = (wVal & 0x01U) ? 1U : 0U;
                break;
            }
        }
        ep0_zlp(); return;
    }

    /* Unhandled */
    ep_stat_tx(0, STAT_STL);
    ep_stat_rx(0, STAT_STL);
}

/* =========================================================================
 * Poll - called from main loop, exactly like bluepill-uvc usb_core_poll()
 * ====================================================================== */
void usb_cdc3_poll(void)
{
    uint16_t istr = USB_ISTR;

    if (istr & ISTR_RESET) {
        USB_ISTR = (uint16_t)~ISTR_RESET;
        usb_handle_reset();
        return;
    }

    if (!(istr & ISTR_CTR)) {
        /* No transfer complete - drain UART RX into USB if ready */
        goto drain_uart;
    }

    {
        uint8_t ep  = (uint8_t)(istr & ISTR_EP_ID);
        uint8_t dir = (istr & ISTR_DIR) ? 1U : 0U;

        if (ep == 0U) {
            uint16_t epr = USB_EPR(0);

            if (epr & EP_SETUP) {
                ep_clr_rx(0);
                usb_handle_setup();
                ep_stat_rx(0, STAT_VLD);
            } else if (dir) {
                /* OUT on EP0 - SetLineCoding data phase */
                ep_clr_rx(0);
                if (g_rx_pending) {
                    uint16_t cnt = (uint16_t)(BT_CRXR(0) & 0x3FFU);
                    uint16_t space = (uint16_t)(sizeof(g_rx_buf) - g_rx_got);
                    if (cnt > space) cnt = space;
                    pma_read(PMA_EP0_RX, g_rx_buf + g_rx_got, cnt);
                    g_rx_got += cnt;
                    if (g_rx_got >= g_rx_need) {
                        for (uint8_t p = 0; p < 3U; p++) {
                            if (g_rx_iface == (p * 2U)) {
                                memcpy(&g_lc[p], g_rx_buf, sizeof(lc_t));
                                uart_set_baud(p, g_lc[p].dwDTERate);
                                break;
                            }
                        }
                        g_rx_pending = 0;
                        ep0_zlp();
                    } else {
                        ep_stat_rx(0, STAT_VLD);
                    }
                } else {
                    ep_stat_rx(0, STAT_VLD);
                }
            } else {
                /* IN complete on EP0 */
                ep_clr_tx(0);
                /* Apply SET_ADDRESS after ZLP is ACKed */
                if (g_addr) {
                    USB_DADDR = (uint16_t)(DADDR_EF | g_addr);
                    g_addr = 0;
                }
                ep0_next();
            }
        } else {
            uint16_t epr = USB_EPR(ep);

            if (epr & EP_CTR_RX) {
                for (uint8_t p = 0; p < 3U; p++) {
                    if (ep == DATA_EP[p]) {
                        uint16_t cnt = (uint16_t)(BT_CRXR(ep) & 0x3FFU);
                        if (cnt > 0U && cnt <= CDC_DATA_MPS) {
                            uint8_t tmp[CDC_DATA_MPS];
                            pma_read(DATA_RX_PMA[p], tmp, cnt);
                            ring_push_bulk(&uart_ch[p].tx, tmp, cnt);
                            uart_kick_tx(p);
                        }
                        ep_clr_rx(ep);
                        ep_stat_rx(ep, STAT_VLD);
                        break;
                    }
                }
            }

            if (epr & EP_CTR_TX) {
                for (uint8_t p = 0; p < 3U; p++) {
                    if (ep == DATA_EP[p])  { ep_clr_tx(ep); g_tx_busy[ep] = 0; break; }
                    if (ep == NOTIF_EP[p]) { ep_clr_tx(ep); break; }
                }
            }
        }

        USB_ISTR = 0;
    }

drain_uart:
    if (!g_configured) return;
    {
        uint8_t buf[CDC_DATA_MPS];
        for (uint8_t p = 0; p < 3U; p++) {
            if (!g_dtr[p]) continue;
            uint8_t ep = DATA_EP[p];
            if (g_tx_busy[ep]) continue;
            uint32_t n = ring_pop_bulk(&uart_ch[p].rx, buf, CDC_DATA_MPS);
            if (n > 0U) {
                g_tx_busy[ep] = 1;
                pma_write(DATA_TX_PMA[p], buf, (uint16_t)n);
                BT_CTXT(ep) = (uint16_t)n;
                ep_stat_tx(ep, STAT_VLD);
            }
        }
    }
}

/* =========================================================================
 * Init - modelled exactly on bluepill-uvc usb_core_init()
 * ====================================================================== */
void usb_cdc3_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    /* Pull D+ low to force host to see disconnect */
    GPIO_CRH_SET(GPIOA, 12, GPIO_OUT_PP_2MHZ);
    GPIOA->BRR = (1U << 12);
    delay_ms(500);

    /* USB power-up sequence from bluepill-uvc */
    USB_CNTR = (uint16_t)(CNTR_FRES | CNTR_PDWN);
    delay_ms(5);
    USB_CNTR = (uint16_t)CNTR_FRES;
    delay_ms(5);
    USB_CNTR = 0;
    USB_ISTR = 0;
    usb_handle_reset();

    /* Enable CTR and RESET - NO NVIC (polling mode like bluepill-uvc) */
    USB_CNTR = (uint16_t)(CNTR_RESETM | CNTR_CTRM);

    /* Release D+ - USB peripheral drives it high through board resistor */
    GPIO_CRH_SET(GPIOA, 12, GPIO_IN_FLOAT);
}

int usb_cdc3_port_ready(uint8_t port)
{
    if (port >= 3U) return 0;
    return g_dtr[port] ? 1 : 0;
}