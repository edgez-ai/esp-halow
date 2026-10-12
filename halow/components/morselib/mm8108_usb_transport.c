/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"

#include "mm8108_usb.h"

#include "common/morse_error.h"
#include "driver/morse_driver/morse.h"
#include "mmhal_wlan.h"
#include "mmosal.h"
#include "mmdrv.h"

/* This esp-halow transport translates the local ESP USB service into the
 * SDK-private driver ABI. It intentionally stays beside libmorse rather than
 * in the platform-independent EdgeZ RF adapter. */
extern int morse_hw_irq_handle(struct driver_data *driverd);
extern void morse_hw_toggle_aon_latch(struct driver_data *driverd);
extern void mmdrv_host_update_tx_paused(
    uint16_t sources_mask, mmdrv_host_update_tx_paused_cb_t cb);

#define MM8108_IRQ_POLL_MS 10
#define MM8108_IRQ_TASK_STACK 2048
#define MM8108_USB_READY_WAIT_MS 10000
#define MM8108_RC_CLK_POWER_OFF_ADDRESS 0x00405020U
#define MM8108_RC_CLK_POWER_OFF_MASK 0x00000040U
#define MM8108_AON_ADDRESS 0x00002114U
#define MM8108_AON_USB_RESET_VALUE 0x00000008U

static const char *TAG = "mm8108_morse";
static struct mmosal_mutex *s_bus_lock;
static struct mmosal_task *s_irq_task;
static volatile bool s_irq_task_run;
static volatile bool s_irq_task_finished;
static volatile bool s_irq_enabled;

static morse_error_t usb_result(esp_err_t err)
{
    return err == ESP_OK ? MORSE_SUCCESS : MORSE_FAILED;
}

static bool pktmem_flow_control_is_paused(void)
{
    return mmhal_wlan_pktmem_tx_flow_control_state() == MMWLAN_TX_PAUSED;
}

static void pktmem_flow_control_handler(void)
{
    mmdrv_host_update_tx_paused(MMDRV_PAUSE_SOURCE_MASK_PKTMEM,
                                pktmem_flow_control_is_paused);
}

static void mm8108_irq_task(void *arg)
{
    struct driver_data *driverd = arg;
    while (s_irq_task_run) {
        mmosal_task_sleep(MM8108_IRQ_POLL_MS);
        if (!s_irq_enabled || !mm8108_usb_is_ready()) {
            continue;
        }
        MMOSAL_MUTEX_GET_INF(s_bus_lock);
        if (s_irq_enabled) {
            (void)morse_hw_irq_handle(driverd);
        }
        MMOSAL_MUTEX_RELEASE(s_bus_lock);
    }
    s_irq_task_finished = true;
}

void morse_trns_init(void)
{
    struct mmhal_wlan_pktmem_init_args args = {
        .tx_flow_control_cb = pktmem_flow_control_handler,
    };
    mmhal_wlan_pktmem_init(&args);
}

void morse_trns_deinit(void)
{
    mmhal_wlan_pktmem_deinit();
}

morse_error_t morse_trns_start(struct driver_data *driverd)
{
    s_bus_lock = mmosal_mutex_create("MM8108 USB bus");
    if (s_bus_lock == NULL) {
        return MORSE_ENOMEM;
    }

    unsigned waits = MM8108_USB_READY_WAIT_MS / MM8108_IRQ_POLL_MS;
    while (!mm8108_usb_is_ready() && waits-- > 0) {
        mmosal_task_sleep(MM8108_IRQ_POLL_MS);
    }
    if (!mm8108_usb_is_ready()) {
        ESP_LOGE(TAG, "MM8108 USB transport did not become ready");
        mmosal_mutex_delete(s_bus_lock);
        s_bus_lock = NULL;
        return MORSE_FAILED;
    }

    MMOSAL_MUTEX_GET_INF(s_bus_lock);
    esp_err_t err = mm8108_usb_reg32_read(driverd->cfg->regs->chip_id_address,
                                          &driverd->chip_id);
    MMOSAL_MUTEX_RELEASE(s_bus_lock);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MM8108 probe failed: %s", esp_err_to_name(err));
        mmosal_mutex_delete(s_bus_lock);
        s_bus_lock = NULL;
        return MORSE_FAILED;
    }

    ESP_LOGI(TAG, "official morselib using MM8108 USB, chip ID 0x%08lx",
             (unsigned long)driverd->chip_id);
    s_irq_task_run = true;
    s_irq_task_finished = false;
    s_irq_task = mmosal_task_create(mm8108_irq_task, driverd,
                                    MMOSAL_TASK_PRI_HIGH,
                                    MM8108_IRQ_TASK_STACK,
                                    "mm8108_usb_irq");
    if (s_irq_task == NULL) {
        s_irq_task_run = false;
        return MORSE_ENOMEM;
    }
    return MORSE_SUCCESS;
}

void morse_trns_stop(struct driver_data *driverd)
{
    (void)driverd;
    s_irq_enabled = false;
    s_irq_task_run = false;
    while (s_irq_task != NULL && !s_irq_task_finished) {
        mmosal_task_sleep(1);
    }
    s_irq_task = NULL;
    if (s_bus_lock != NULL) {
        mmosal_mutex_delete(s_bus_lock);
        s_bus_lock = NULL;
    }
}

void morse_trns_set_irq_enabled(struct driver_data *driverd, bool enabled)
{
    const bool bus_held = s_bus_lock != NULL &&
        mmosal_mutex_is_held_by_active_task(s_bus_lock);
    if (!bus_held) {
        MMOSAL_MUTEX_GET_INF(s_bus_lock);
    }
    s_irq_enabled = enabled;
    if (enabled && mm8108_usb_is_ready()) {
        (void)morse_hw_irq_handle(driverd);
    }
    if (!bus_held) {
        MMOSAL_MUTEX_RELEASE(s_bus_lock);
    }
}

void morse_trns_claim(struct driver_data *driverd)
{
    (void)driverd;
    MMOSAL_MUTEX_GET_INF(s_bus_lock);
}

void morse_trns_release(struct driver_data *driverd)
{
    (void)driverd;
    MMOSAL_MUTEX_RELEASE(s_bus_lock);
}

morse_error_t morse_trns_read_multi_byte(struct driver_data *driverd,
                                         uint32_t address, uint8_t *data,
                                         uint32_t len)
{
    (void)driverd;
    if (s_bus_lock == NULL ||
        !mmosal_mutex_is_held_by_active_task(s_bus_lock) || data == NULL ||
        len == 0 || (len & 3U) != 0U) {
        return MORSE_INVALID_ARGUMENT;
    }
    return usb_result(mm8108_usb_mem_read(address, data, len));
}

morse_error_t morse_trns_write_multi_byte(struct driver_data *driverd,
                                          uint32_t address,
                                          const uint8_t *data, uint32_t len)
{
    (void)driverd;
    if (s_bus_lock == NULL ||
        !mmosal_mutex_is_held_by_active_task(s_bus_lock) || data == NULL ||
        len == 0 || (len & 3U) != 0U) {
        return MORSE_INVALID_ARGUMENT;
    }
    return usb_result(mm8108_usb_mem_write(address, data, len));
}

morse_error_t morse_trns_read_le32(struct driver_data *driverd,
                                   uint32_t address, uint32_t *data)
{
    (void)driverd;
    if (s_bus_lock == NULL ||
        !mmosal_mutex_is_held_by_active_task(s_bus_lock) || data == NULL) {
        return MORSE_INVALID_ARGUMENT;
    }
    return usb_result(mm8108_usb_reg32_read(address, data));
}

morse_error_t morse_trns_write_le32(struct driver_data *driverd,
                                    uint32_t address, uint32_t data)
{
    (void)driverd;
    if (s_bus_lock == NULL ||
        !mmosal_mutex_is_held_by_active_task(s_bus_lock)) {
        return MORSE_INVALID_ARGUMENT;
    }
    if (address == MM8108_AON_ADDRESS && data == 0) {
        ESP_LOGI(TAG, "preserving MM8108 USB power domain for firmware boot");
        data = MM8108_AON_USB_RESET_VALUE;
    }
    return usb_result(mm8108_usb_reg32_write(address, data));
}

void mmhal_wlan_init(void) {}
void mmhal_wlan_deinit(void) {}
void mmhal_wlan_hard_reset(void) {}
void mmhal_wlan_spi_cs_assert(void) {}
void mmhal_wlan_spi_cs_deassert(void) {}
uint8_t mmhal_wlan_spi_rw(uint8_t data) { return data; }
void mmhal_wlan_spi_read_buf(uint8_t *buf, unsigned len)
{
    memset(buf, 0xff, len);
}
void mmhal_wlan_spi_write_buf(const uint8_t *buf, unsigned len)
{
    (void)buf;
    (void)len;
}
void mmhal_wlan_send_training_seq(void) {}
void mmhal_wlan_register_spi_irq_handler(mmhal_irq_handler_t handler)
{
    (void)handler;
}
bool mmhal_wlan_spi_irq_is_asserted(void) { return false; }
void mmhal_wlan_set_spi_irq_enabled(bool enabled) { (void)enabled; }
void mmhal_wlan_wake_assert(void) {}
void mmhal_wlan_wake_deassert(void) {}
bool mmhal_wlan_busy_is_asserted(void) { return false; }
void mmhal_wlan_register_busy_irq_handler(mmhal_irq_handler_t handler)
{
    (void)handler;
}
void mmhal_wlan_set_busy_irq_enabled(bool enabled) { (void)enabled; }

static int mm8108_usb_digital_reset(struct driver_data *driverd)
{
    uint32_t rc_clock;
    int result = MORSE_SUCCESS;

    MMOSAL_MUTEX_GET_INF(s_bus_lock);
    result = morse_trns_read_le32(driverd,
                                  MM8108_RC_CLK_POWER_OFF_ADDRESS,
                                  &rc_clock);
    if (result == MORSE_SUCCESS) {
        rc_clock &= ~MM8108_RC_CLK_POWER_OFF_MASK;
        result = morse_trns_write_le32(driverd,
                                       MM8108_RC_CLK_POWER_OFF_ADDRESS,
                                       rc_clock);
    }
    if (result == MORSE_SUCCESS) {
        morse_hw_toggle_aon_latch(driverd);
        result = usb_result(mm8108_usb_reset());
    }
    if (result == MORSE_SUCCESS) {
        mmosal_task_sleep(driverd->cfg->warm_boot_delay_ms);
    }
    MMOSAL_MUTEX_RELEASE(s_bus_lock);
    if (result != MORSE_SUCCESS) {
        ESP_LOGE(TAG, "MM8108 non-destructive USB reset failed");
    }
    return result;
}

const struct mmhal_chip *mmhal_get_chip(void)
{
    static struct mmhal_chip usb_chip;
    static bool initialized;
    if (!initialized) {
        usb_chip = mmhal_mm8108;
        usb_chip.enable_sdio_burst_mode = NULL;
        usb_chip.digital_reset = mm8108_usb_digital_reset;
        initialized = true;
    }
    return &usb_chip;
}

void mmhal_read_mac_addr(uint8_t *mac_addr)
{
    static const uint8_t halow_namespace[6] = {
        0x02, 0x48, 0x61, 0x4c, 0x6f, 0x57,
    };
    bool all_zero = true;
    for (size_t index = 0; index < 6; ++index) {
        all_zero &= mac_addr[index] == 0;
    }
    if (!all_zero) {
        return;
    }
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac_addr));
    for (size_t index = 0; index < sizeof(halow_namespace); ++index) {
        mac_addr[index] ^= halow_namespace[index];
    }
    mac_addr[0] = (mac_addr[0] | 0x02U) & 0xfeU;
}
