/**
 * @file
 * @brief Implements BTM 112 Bluetooth driver.
 *
 * @date 15.07.11
 * @author Anton Kozlov
 */

#include <types.h>
#include <embox/unit.h>
#include <hal/reg.h>

#include <drivers/at91sam7s256.h>
#include <drivers/pins.h>
#include <drivers/bluetooth.h>
#include <kernel/timer.h>
#include <string.h>
#include <unistd.h>

extern void bt_handle(uint8_t *buff);

#define BTM_BT_RX_PIN  ((uint32_t) CONFIG_BTM_BT_RX_PIN)
#define BTM_BT_TX_PIN  ((uint32_t) CONFIG_BTM_BT_TX_PIN)
#define BTM_BT_SCK_PIN ((uint32_t) CONFIG_BTM_BT_SCK_PIN)
#define BTM_BT_RTS_PIN ((uint32_t) CONFIG_BTM_BT_RTS_PIN)
#define BTM_BT_CTS_PIN ((uint32_t) CONFIG_BTM_BT_CTS_PIN)

static volatile AT91PS_USART us_dev_regs = ((AT91PS_USART) CONFIG_BTM_BT_SERIAL_PORT_OFFSET);

#define BTM_BT_BAUD_RATE 19200

EMBOX_UNIT_INIT(btm_bluetooth_init);

uint8_t *btm_bt_read_buff;
int btm_bt_read_len;

static int nop(void) {
	return 0;
}

CALLBACK_INIT_DEF(nxt_bt_rx_handle_t, __bt_rx, nop);
CALLBACK_INIT_DEF(nxt_bt_state_handle_t, bt_state, nop);

static irq_return_t btm_bt_us_handler(int irq_num, void *dev_id) {

	CALLBACK(__bt_rx)();

	return IRQ_HANDLED;
}

static void init_usart(void) {
	/* Configure the usart */
	REG_STORE(AT91C_PMC_PCER, (1 << CONFIG_BTM_BT_US_DEV_ID));

	REG_STORE(AT91C_PIOA_PDR, BTM_BT_RX_PIN | BTM_BT_TX_PIN |
			BTM_BT_SCK_PIN | BTM_BT_RTS_PIN | BTM_BT_CTS_PIN);
	REG_STORE(AT91C_PIOA_ASR, BTM_BT_RX_PIN | BTM_BT_TX_PIN |
			BTM_BT_SCK_PIN | BTM_BT_RTS_PIN | BTM_BT_CTS_PIN);

	REG_STORE(&(us_dev_regs->US_PTCR), (AT91C_PDC_RXTDIS | AT91C_PDC_TXTDIS));

	REG_STORE(&(us_dev_regs->US_CR),  AT91C_US_RXDIS | AT91C_US_TXDIS);
	REG_STORE(&(us_dev_regs->US_CR),  AT91C_US_RSTSTA | AT91C_US_RSTRX | AT91C_US_RSTTX);
	REG_STORE(&(us_dev_regs->US_CR), AT91C_US_STTTO);

	REG_STORE(&(us_dev_regs->US_MR), AT91C_US_USMODE_HWHSH
			| AT91C_US_CLKS_CLOCK | AT91C_US_CHRL_8_BITS | AT91C_US_PAR_NONE
			| AT91C_US_NBSTOP_1_BIT);
	REG_STORE(&(us_dev_regs->US_BRGR), CONFIG_SYS_CLOCK / (16 * BTM_BT_BAUD_RATE));

	REG_STORE(&(us_dev_regs->US_IDR), ~0);

	REG_STORE(&(us_dev_regs->US_RCR), 0);
	REG_STORE(&(us_dev_regs->US_TCR), 0);
	REG_STORE(&(us_dev_regs->US_RNPR), 0);
	REG_STORE(&(us_dev_regs->US_TNPR), 0);
	REG_LOAD(&(us_dev_regs->US_RHR));
	REG_LOAD(&(us_dev_regs->US_CSR));

	REG_STORE(&(us_dev_regs->US_CR), AT91C_US_RXEN | AT91C_US_TXEN);
	REG_STORE(&(us_dev_regs->US_PTCR), AT91C_PDC_RXTEN | AT91C_PDC_TXTEN);
	REG_STORE(&(us_dev_regs->US_IER), AT91C_US_ENDRX);
}
#if 0
static void link_pin_handler(pin_mask_t ch_mask, pin_mask_t mon_mask) {
	if (!ch_mask &&  conn_state == CONN_CONNECTED) {
		CALLBACK_DO(bluetooth_uart, BT_DRV_MSG_DISCONNECTED, NULL);
	}
}
#endif

void bluetooth_hw_hard_reset(void) {
	pin_config_output(CONFIG_BTM_BT_RST_PIN);
	pin_set_output(CONFIG_BTM_BT_RST_PIN);
	usleep(1000);

	pin_clear_output(CONFIG_BTM_BT_RST_PIN);
	usleep(5000);

	pin_config_input(CONFIG_BTM_BT_LINK_PIN);
	REG_STORE(AT91C_PIOA_PPUER, CONFIG_BTM_BT_LINK_PIN);
	REG_STORE(AT91C_PIOA_MDDR, CONFIG_BTM_BT_LINK_PIN);
}

static int btm_bluetooth_init(void) {
	irq_attach((irq_nr_t) CONFIG_BTM_BT_US_IRQ,
		(irq_handler_t) &btm_bt_us_handler, 0, NULL, "bt reader");

	init_usart();
//	pin_set_input_monitor(CONFIG_BTM_BT_LINK_PIN, &link_pin_handler);

	return 0;
}

size_t bluetooth_write(uint8_t *buff, size_t len) {
	while (!(REG_LOAD(&(us_dev_regs->US_CSR)) & AT91C_US_ENDTX)) {
	}
	REG_STORE(&(us_dev_regs->US_TPR), (uint32_t) buff);
	REG_STORE(&(us_dev_regs->US_TCR), len);
	return len;
}

size_t bluetooth_read(uint8_t *buff, size_t len) {
	btm_bt_read_buff = buff;
	btm_bt_read_len = len;
	REG_STORE(&(us_dev_regs->US_RPR), (uint32_t) buff);
	REG_STORE(&(us_dev_regs->US_RCR), len);

	return 0;
}
