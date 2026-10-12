/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "usb/usb_helpers.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"

#include "mm8108_usb.h"

// Values and wire protocol are defined by Morse Micro's OpenWrt/Linux driver.
#define MORSE_USB_VENDOR_ID          0x325b
#define MORSE_USB_MM810X_PRODUCT_ID  0x8100
#define MM8108_CHIP_ID_ADDRESS       0x00002d20U
#define MM8108_USB_MAX_TRANSFER_SIZE (16 * 1024)
#define MM8108_USB_COMMAND_SIZE      12
#define MM8108_USB_TIMEOUT_MS        250
#define MM8108_USB_DEVICE_LIST_SIZE  8
#define MM8108_USB_RESCAN_INTERVAL_MS 1000
#define MM8108_USB_WAIT_LOG_MS        5000

enum mm8108_usb_direction {
    MM8108_USB_WRITE = 0x00,
    MM8108_USB_READ = 0x80,
    MM8108_USB_RESET = 0x02,
};

typedef struct {
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t memory_in;
    uint8_t memory_out;
    uint8_t register_in;
    uint8_t register_out;
    uint8_t interrupt_in;
    uint16_t memory_in_max_packet;
    uint16_t memory_out_max_packet;
    uint16_t interrupt_max_packet;
} mm8108_endpoints_t;

typedef struct {
    usb_host_client_handle_t client;
    usb_device_handle_t device;
    usb_transfer_t *transfer;
    SemaphoreHandle_t bus_mutex;
    SemaphoreHandle_t transfer_done_sem;
    TaskHandle_t client_task;
    mm8108_endpoints_t endpoints;
    uint8_t pending_address;
    volatile bool transfer_done;
    volatile bool transfer_in_flight;
    volatile bool device_pending;
    volatile bool device_gone;
    bool interface_claimed;
    bool started;
    volatile bool ready;
    bool transfer_stalled;
    usb_transfer_status_t transfer_status;
    int transfer_actual_bytes;
    int64_t perf_started_us;
    uint64_t perf_read_bytes;
    uint64_t perf_write_bytes;
    uint64_t perf_read_us;
    uint64_t perf_write_us;
    uint64_t perf_wait_us;
    uint32_t perf_read_calls;
    uint32_t perf_write_calls;
    uint32_t perf_wait_calls;
    uint32_t perf_read_max_us;
    uint32_t perf_write_max_us;
    uint32_t perf_wait_max_us;
    uint32_t perf_errors;
} mm8108_usb_state_t;

static const char *TAG = "mm8108_usb";
static mm8108_usb_state_t s_mm8108;

typedef struct {
    uint32_t elapsed_ms;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t read_us;
    uint64_t write_us;
    uint64_t wait_us;
    uint32_t read_calls;
    uint32_t write_calls;
    uint32_t wait_calls;
    uint32_t read_max_us;
    uint32_t write_max_us;
    uint32_t wait_max_us;
    uint32_t errors;
} mm8108_usb_perf_report_t;

static void update_max_u32(uint32_t *maximum, uint32_t value)
{
    if (value > *maximum) *maximum = value;
}

static bool take_usb_performance(mm8108_usb_state_t *state, int64_t now_us,
                                 mm8108_usb_perf_report_t *report)
{
    if (state->perf_started_us == 0) {
        state->perf_started_us = now_us;
        return false;
    }
    const uint32_t elapsed_ms = (uint32_t)((now_us - state->perf_started_us) / 1000);
    if (elapsed_ms < 15000U) return false;
    *report = (mm8108_usb_perf_report_t) {
        .elapsed_ms = elapsed_ms,
        .read_bytes = state->perf_read_bytes,
        .write_bytes = state->perf_write_bytes,
        .read_us = state->perf_read_us,
        .write_us = state->perf_write_us,
        .wait_us = state->perf_wait_us,
        .read_calls = state->perf_read_calls,
        .write_calls = state->perf_write_calls,
        .wait_calls = state->perf_wait_calls,
        .read_max_us = state->perf_read_max_us,
        .write_max_us = state->perf_write_max_us,
        .wait_max_us = state->perf_wait_max_us,
        .errors = state->perf_errors,
    };
    state->perf_started_us = now_us;
    state->perf_read_bytes = state->perf_write_bytes = 0;
    state->perf_read_us = state->perf_write_us = state->perf_wait_us = 0;
    state->perf_read_calls = state->perf_write_calls = state->perf_wait_calls = 0;
    state->perf_read_max_us = state->perf_write_max_us = state->perf_wait_max_us = 0;
    state->perf_errors = 0;
    return true;
}

static void log_usb_performance(const mm8108_usb_perf_report_t *report)
{
    ESP_LOGW(TAG,
             "PERF %lums read=%lukbps/%luops avg=%luus max=%luus; "
             "write=%lukbps/%luops avg=%luus max=%luus; bus_wait avg=%luus max=%luus errors=%lu",
             (unsigned long)report->elapsed_ms,
             (unsigned long)((report->read_bytes * 8U) / report->elapsed_ms),
             (unsigned long)report->read_calls,
             (unsigned long)(report->read_calls ? report->read_us / report->read_calls : 0),
             (unsigned long)report->read_max_us,
             (unsigned long)((report->write_bytes * 8U) / report->elapsed_ms),
             (unsigned long)report->write_calls,
             (unsigned long)(report->write_calls ? report->write_us / report->write_calls : 0),
             (unsigned long)report->write_max_us,
             (unsigned long)(report->wait_calls ? report->wait_us / report->wait_calls : 0),
             (unsigned long)report->wait_max_us,
             (unsigned long)report->errors);
}

static void put_le32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

static uint32_t get_le32(const uint8_t *source)
{
    return (uint32_t)source[0] |
           ((uint32_t)source[1] << 8) |
           ((uint32_t)source[2] << 16) |
           ((uint32_t)source[3] << 24);
}

static bool is_mm8108(uint16_t vendor_id, uint16_t product_id)
{
    return vendor_id == MORSE_USB_VENDOR_ID && product_id == MORSE_USB_MM810X_PRODUCT_ID;
}

static void client_event_callback(const usb_host_client_event_msg_t *event, void *arg)
{
    mm8108_usb_state_t *state = arg;
    if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV && !state->device) {
        state->pending_address = event->new_dev.address;
        state->device_pending = true;
    } else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE &&
               event->dev_gone.dev_hdl == state->device) {
        state->device_gone = true;
        state->ready = false;
    }
}

static void transfer_callback(usb_transfer_t *transfer)
{
    mm8108_usb_state_t *state = transfer->context;
    state->transfer_status = transfer->status;
    state->transfer_actual_bytes = transfer->actual_num_bytes;
    state->transfer_in_flight = false;
    state->transfer_done = true;
    xSemaphoreGive(state->transfer_done_sem);
}

static esp_err_t wait_for_transfer(mm8108_usb_state_t *state)
{
    if (xTaskGetCurrentTaskHandle() == state->client_task) {
        while (!state->transfer_done && !state->device_gone) {
            esp_err_t err = usb_host_client_handle_events(state->client, pdMS_TO_TICKS(50));
            if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
                return err;
            }
        }
        return state->transfer_done ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(state->transfer_done_sem, pdMS_TO_TICKS(1000)) != pdTRUE) {
        // Never reuse a transfer whose completion state is unknown.
        state->transfer_stalled = true;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static esp_err_t submit_transfer(mm8108_usb_state_t *state, uint8_t endpoint,
                                 int num_bytes, int expected_bytes)
{
    if (state->transfer_stalled) {
        return ESP_ERR_INVALID_STATE;
    }
    while (xSemaphoreTake(state->transfer_done_sem, 0) == pdTRUE) {
    }

    usb_transfer_t *transfer = state->transfer;
    transfer->device_handle = state->device;
    transfer->bEndpointAddress = endpoint;
    transfer->num_bytes = num_bytes;
    transfer->callback = transfer_callback;
    transfer->context = state;
    transfer->timeout_ms = MM8108_USB_TIMEOUT_MS;
    state->transfer_done = false;
    state->transfer_in_flight = true;
    state->transfer_status = USB_TRANSFER_STATUS_ERROR;
    state->transfer_actual_bytes = 0;

    const esp_err_t submit_err = usb_host_transfer_submit(transfer);
    if (submit_err != ESP_OK) {
        // A rejected submission never owns the transfer, so cleanup must not
        // wait for a callback that cannot occur.
        state->transfer_in_flight = false;
        state->transfer_done = true;
        ESP_LOGE(TAG, "endpoint 0x%02x submit failed: %s",
                 endpoint, esp_err_to_name(submit_err));
        return submit_err;
    }
    ESP_RETURN_ON_ERROR(wait_for_transfer(state), TAG,
                        "endpoint 0x%02x completion timed out", endpoint);
    ESP_RETURN_ON_FALSE(state->transfer_status == USB_TRANSFER_STATUS_COMPLETED,
                        ESP_FAIL, TAG, "endpoint 0x%02x transfer status %d",
                        endpoint, state->transfer_status);
    ESP_RETURN_ON_FALSE(state->transfer_actual_bytes == expected_bytes,
                        ESP_ERR_INVALID_SIZE, TAG,
                        "endpoint 0x%02x transferred %d bytes, expected %d",
                        endpoint, state->transfer_actual_bytes, expected_bytes);
    return ESP_OK;
}

static esp_err_t send_command(mm8108_usb_state_t *state, uint32_t direction,
                              uint32_t address, uint32_t length)
{
    uint8_t *command = state->transfer->data_buffer;
    put_le32(command, direction);
    put_le32(command + 4, address);
    put_le32(command + 8, length);
    return submit_transfer(state, state->endpoints.memory_out,
                           MM8108_USB_COMMAND_SIZE, MM8108_USB_COMMAND_SIZE);
}

static esp_err_t mem_read_locked(mm8108_usb_state_t *state, uint32_t address,
                                 void *data, size_t length)
{
    uint8_t *destination = data;
    while (length > 0) {
        const size_t chunk = length > MM8108_USB_MAX_TRANSFER_SIZE
                                 ? MM8108_USB_MAX_TRANSFER_SIZE : length;
        ESP_RETURN_ON_ERROR(send_command(state, MM8108_USB_READ, address, chunk),
                            TAG, "read command failed at 0x%08" PRIx32, address);
        const int request_size = usb_round_up_to_mps((int)chunk,
                                                     state->endpoints.memory_in_max_packet);
        ESP_RETURN_ON_ERROR(submit_transfer(state, state->endpoints.memory_in,
                                            request_size, (int)chunk),
                            TAG, "read failed at 0x%08" PRIx32, address);
        memcpy(destination, state->transfer->data_buffer, chunk);
        destination += chunk;
        address += chunk;
        length -= chunk;
    }
    return ESP_OK;
}

static esp_err_t mem_write_locked(mm8108_usb_state_t *state, uint32_t address,
                                  const void *data, size_t length)
{
    const uint8_t *source = data;
    while (length > 0) {
        const size_t chunk = length > MM8108_USB_MAX_TRANSFER_SIZE
                                 ? MM8108_USB_MAX_TRANSFER_SIZE : length;
        ESP_RETURN_ON_ERROR(send_command(state, MM8108_USB_WRITE, address, chunk),
                            TAG, "write command failed at 0x%08" PRIx32, address);
        memcpy(state->transfer->data_buffer, source, chunk);
        ESP_RETURN_ON_ERROR(submit_transfer(state, state->endpoints.memory_out,
                                            (int)chunk, (int)chunk),
                            TAG, "write failed at 0x%08" PRIx32, address);
        source += chunk;
        address += chunk;
        length -= chunk;
    }
    return ESP_OK;
}

static bool endpoints_are_complete(const mm8108_endpoints_t *endpoints)
{
    return endpoints->memory_in && endpoints->memory_out && endpoints->interrupt_in &&
           endpoints->interrupt_max_packet >= 8;
}

static bool find_endpoints(const usb_config_desc_t *config, mm8108_endpoints_t *result)
{
    const uint8_t *cursor = (const uint8_t *)config;
    const uint8_t *end = cursor + config->wTotalLength;
    mm8108_endpoints_t candidate = {0};
    bool have_interface = false;

    while (cursor + 2 <= end && cursor[0] >= 2 && cursor + cursor[0] <= end) {
        if (cursor[1] == USB_B_DESCRIPTOR_TYPE_INTERFACE &&
            cursor[0] >= sizeof(usb_intf_desc_t)) {
            if (have_interface && endpoints_are_complete(&candidate)) {
                *result = candidate;
                return true;
            }
            const usb_intf_desc_t *interface = (const usb_intf_desc_t *)cursor;
            memset(&candidate, 0, sizeof(candidate));
            candidate.interface_number = interface->bInterfaceNumber;
            candidate.alternate_setting = interface->bAlternateSetting;
            have_interface = true;
        } else if (have_interface && cursor[1] == USB_B_DESCRIPTOR_TYPE_ENDPOINT &&
                   cursor[0] >= sizeof(usb_ep_desc_t)) {
            const usb_ep_desc_t *endpoint = (const usb_ep_desc_t *)cursor;
            const uint8_t transfer_type = endpoint->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK;
            const bool input = endpoint->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK;
            if (transfer_type == USB_BM_ATTRIBUTES_XFER_BULK && input) {
                if (!candidate.memory_in) {
                    candidate.memory_in = endpoint->bEndpointAddress;
                    candidate.memory_in_max_packet = endpoint->wMaxPacketSize;
                } else if (!candidate.register_in) {
                    candidate.register_in = endpoint->bEndpointAddress;
                }
            } else if (transfer_type == USB_BM_ATTRIBUTES_XFER_BULK && !input) {
                if (!candidate.memory_out) {
                    candidate.memory_out = endpoint->bEndpointAddress;
                    candidate.memory_out_max_packet = endpoint->wMaxPacketSize;
                } else if (!candidate.register_out) {
                    candidate.register_out = endpoint->bEndpointAddress;
                }
            } else if (transfer_type == USB_BM_ATTRIBUTES_XFER_INT && input &&
                       !candidate.interrupt_in) {
                candidate.interrupt_in = endpoint->bEndpointAddress;
                candidate.interrupt_max_packet = endpoint->wMaxPacketSize;
            }
        }
        cursor += cursor[0];
    }

    if (have_interface && endpoints_are_complete(&candidate)) {
        *result = candidate;
        return true;
    }
    return false;
}

static void close_device(mm8108_usb_state_t *state)
{
    if (!state->device) {
        return;
    }
    xSemaphoreTake(state->bus_mutex, portMAX_DELAY);
    state->ready = false;
    if (state->interface_claimed) {
        usb_host_endpoint_halt(state->device, state->endpoints.memory_in);
        usb_host_endpoint_halt(state->device, state->endpoints.memory_out);
        usb_host_endpoint_flush(state->device, state->endpoints.memory_in);
        usb_host_endpoint_flush(state->device, state->endpoints.memory_out);

        // A flushed in-flight transfer is returned through the normal callback.
        // Pump client events before freeing the shared transfer object.
        unsigned wait_attempts = 50;
        while (state->transfer_in_flight && wait_attempts-- > 0) {
            const esp_err_t err = usb_host_client_handle_events(state->client,
                                                                 pdMS_TO_TICKS(10));
            if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "waiting for canceled transfer failed: %s",
                         esp_err_to_name(err));
                break;
            }
        }
        if (state->transfer_in_flight) {
            ESP_LOGW(TAG, "canceled transfer is still pending; deferring USB cleanup");
            state->device_gone = true;
            xSemaphoreGive(state->bus_mutex);
            return;
        }
    }
    if (state->transfer) {
        usb_host_transfer_free(state->transfer);
        state->transfer = NULL;
    }
    if (state->interface_claimed) {
        usb_host_interface_release(state->client, state->device,
                                   state->endpoints.interface_number);
    }
    usb_host_device_close(state->client, state->device);
    state->device = NULL;
    state->interface_claimed = false;
    state->transfer_stalled = false;
    memset(&state->endpoints, 0, sizeof(state->endpoints));
    xSemaphoreGive(state->bus_mutex);
    ESP_LOGW(TAG, "MM8108 disconnected");
}

static void inspect_device(mm8108_usb_state_t *state)
{
    ESP_RETURN_VOID_ON_ERROR(
        usb_host_device_open(state->client, state->pending_address, &state->device),
        TAG, "device open failed");

    const usb_device_desc_t *device_desc = NULL;
    esp_err_t err = usb_host_get_device_descriptor(state->device, &device_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "device descriptor failed: %s", esp_err_to_name(err));
        close_device(state);
        return;
    }
    ESP_LOGI(TAG, "USB device %04x:%04x", device_desc->idVendor, device_desc->idProduct);
    if (!is_mm8108(device_desc->idVendor, device_desc->idProduct)) {
        ESP_LOGI(TAG, "device is not a Morse Micro MM8108; ignoring it");
        usb_host_device_close(state->client, state->device);
        state->device = NULL;
        return;
    }

    const usb_config_desc_t *config = NULL;
    err = usb_host_get_active_config_descriptor(state->device, &config);
    if (err != ESP_OK || !find_endpoints(config, &state->endpoints)) {
        ESP_LOGE(TAG, "MM8108 vendor interface or required endpoints unavailable");
        close_device(state);
        return;
    }

    err = usb_host_interface_claim(state->client, state->device,
                                   state->endpoints.interface_number,
                                   state->endpoints.alternate_setting);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "interface claim failed: %s", esp_err_to_name(err));
        close_device(state);
        return;
    }
    state->interface_claimed = true;

    err = usb_host_transfer_alloc(MM8108_USB_MAX_TRANSFER_SIZE, 0, &state->transfer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "transfer allocation failed: %s", esp_err_to_name(err));
        close_device(state);
        return;
    }
    state->ready = true;

    ESP_LOGI(TAG,
             "MM8108 transport ready: interface %u alt %u, memory IN 0x%02x/%u, "
             "OUT 0x%02x/%u, IRQ 0x%02x/%u",
             state->endpoints.interface_number, state->endpoints.alternate_setting,
             state->endpoints.memory_in, state->endpoints.memory_in_max_packet,
             state->endpoints.memory_out, state->endpoints.memory_out_max_packet,
             state->endpoints.interrupt_in, state->endpoints.interrupt_max_packet);

    uint32_t chip_id = 0;
    err = mm8108_usb_reg32_read(MM8108_CHIP_ID_ADDRESS, &chip_id);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "MM8108 chip ID register: 0x%08" PRIx32, chip_id);
        ESP_LOGI(TAG, "USB bus probe complete");
    } else {
        ESP_LOGE(TAG, "MM8108 chip ID read failed: %s", esp_err_to_name(err));
        close_device(state);
    }
}

static int scan_existing_devices(mm8108_usb_state_t *state)
{
    uint8_t device_addresses[MM8108_USB_DEVICE_LIST_SIZE];
    int device_count = 0;
    const esp_err_t err = usb_host_device_addr_list_fill(
        MM8108_USB_DEVICE_LIST_SIZE, device_addresses, &device_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "USB device rescan failed: %s", esp_err_to_name(err));
        return -1;
    }

    for (int i = 0; i < device_count && !state->device; ++i) {
        state->pending_address = device_addresses[i];
        inspect_device(state);
    }
    return device_count;
}

static void mm8108_client_task(void *arg)
{
    mm8108_usb_state_t *state = arg;
    state->client_task = xTaskGetCurrentTaskHandle();
    const usb_host_client_config_t config = {
        .is_synchronous = false,
        .max_num_event_msg = 8,
        .async = {
            .client_event_callback = client_event_callback,
            .callback_arg = state,
        },
    };
    ESP_ERROR_CHECK(usb_host_client_register(&config, &state->client));
    ESP_LOGI(TAG, "MM8108 USB client registered");

    /* NEW_DEV is edge-triggered. Discover devices that completed enumeration
     * before this asynchronous client was registered as well. */
    (void)scan_existing_devices(state);

    TickType_t last_rescan = xTaskGetTickCount();
    TickType_t last_wait_log = last_rescan;

    while (true) {
        const esp_err_t err = usb_host_client_handle_events(state->client, pdMS_TO_TICKS(100));
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "USB client event error: %s", esp_err_to_name(err));
        }
        if (state->device_gone) {
            state->device_gone = false;
            close_device(state);
        }
        if (state->device_pending) {
            state->device_pending = false;
            inspect_device(state);
        }

        const TickType_t now = xTaskGetTickCount();
        if (!state->device &&
            now - last_rescan >= pdMS_TO_TICKS(MM8108_USB_RESCAN_INTERVAL_MS)) {
            last_rescan = now;
            const int device_count = scan_existing_devices(state);
            if (!state->device &&
                now - last_wait_log >= pdMS_TO_TICKS(MM8108_USB_WAIT_LOG_MS)) {
                last_wait_log = now;
                usb_host_lib_info_t host_info = {0};
                const esp_err_t info_err = usb_host_lib_info(&host_info);
                if (info_err == ESP_OK) {
                    ESP_LOGW(TAG,
                             "Still waiting for MM8108; enumerated addresses=%d, "
                             "host device objects=%d, clients=%d",
                             device_count < 0 ? 0 : device_count,
                             host_info.num_devices, host_info.num_clients);
                } else {
                    ESP_LOGW(TAG,
                             "Still waiting for MM8108; enumerated addresses=%d, "
                             "host status unavailable: %s",
                             device_count < 0 ? 0 : device_count,
                             esp_err_to_name(info_err));
                }
            }
        }
    }
}

esp_err_t mm8108_usb_start(void)
{
    ESP_RETURN_ON_FALSE(!s_mm8108.started, ESP_ERR_INVALID_STATE, TAG,
                        "MM8108 USB client already started");
    s_mm8108.started = true;
    s_mm8108.bus_mutex = xSemaphoreCreateMutex();
    s_mm8108.transfer_done_sem = xSemaphoreCreateBinary();
    if (!s_mm8108.bus_mutex || !s_mm8108.transfer_done_sem) {
        if (s_mm8108.bus_mutex) {
            vSemaphoreDelete(s_mm8108.bus_mutex);
        }
        if (s_mm8108.transfer_done_sem) {
            vSemaphoreDelete(s_mm8108.transfer_done_sem);
        }
        s_mm8108.bus_mutex = NULL;
        s_mm8108.transfer_done_sem = NULL;
        s_mm8108.started = false;
        ESP_LOGE(TAG, "failed to allocate synchronization objects");
        return ESP_ERR_NO_MEM;
    }
    const BaseType_t created = xTaskCreate(mm8108_client_task, "mm8108_usb",
                                           CONFIG_HALOW_USB_CLIENT_TASK_STACK_SIZE,
                                           &s_mm8108, CONFIG_HALOW_USB_CLIENT_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        vSemaphoreDelete(s_mm8108.bus_mutex);
        vSemaphoreDelete(s_mm8108.transfer_done_sem);
        s_mm8108.bus_mutex = NULL;
        s_mm8108.transfer_done_sem = NULL;
        s_mm8108.started = false;
        ESP_LOGE(TAG, "failed to create MM8108 USB task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool mm8108_usb_is_ready(void)
{
    return s_mm8108.ready;
}

esp_err_t mm8108_usb_mem_read(uint32_t address, void *data, size_t length)
{
    ESP_RETURN_ON_FALSE(data || length == 0, ESP_ERR_INVALID_ARG, TAG, "null read buffer");
    ESP_RETURN_ON_FALSE(s_mm8108.bus_mutex, ESP_ERR_INVALID_STATE, TAG,
                        "MM8108 USB client not started");
    mm8108_usb_perf_report_t report;
    const int64_t requested_us = esp_timer_get_time();
    xSemaphoreTake(s_mm8108.bus_mutex, portMAX_DELAY);
    const int64_t started_us = esp_timer_get_time();
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (s_mm8108.ready) {
        err = mem_read_locked(&s_mm8108, address, data, length);
    }
    const int64_t finished_us = esp_timer_get_time();
    const uint32_t wait_us = (uint32_t)(started_us - requested_us);
    const uint32_t operation_us = (uint32_t)(finished_us - started_us);
    ++s_mm8108.perf_wait_calls;
    ++s_mm8108.perf_read_calls;
    s_mm8108.perf_wait_us += wait_us;
    s_mm8108.perf_read_us += operation_us;
    if (err == ESP_OK) s_mm8108.perf_read_bytes += length;
    else ++s_mm8108.perf_errors;
    update_max_u32(&s_mm8108.perf_wait_max_us, wait_us);
    update_max_u32(&s_mm8108.perf_read_max_us, operation_us);
    const bool report_ready = take_usb_performance(&s_mm8108, finished_us, &report);
    xSemaphoreGive(s_mm8108.bus_mutex);
    if (report_ready) log_usb_performance(&report);
    return err;
}

esp_err_t mm8108_usb_mem_write(uint32_t address, const void *data, size_t length)
{
    ESP_RETURN_ON_FALSE(data || length == 0, ESP_ERR_INVALID_ARG, TAG, "null write buffer");
    ESP_RETURN_ON_FALSE(s_mm8108.bus_mutex, ESP_ERR_INVALID_STATE, TAG,
                        "MM8108 USB client not started");
    mm8108_usb_perf_report_t report;
    const int64_t requested_us = esp_timer_get_time();
    xSemaphoreTake(s_mm8108.bus_mutex, portMAX_DELAY);
    const int64_t started_us = esp_timer_get_time();
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (s_mm8108.ready) {
        err = mem_write_locked(&s_mm8108, address, data, length);
    }
    const int64_t finished_us = esp_timer_get_time();
    const uint32_t wait_us = (uint32_t)(started_us - requested_us);
    const uint32_t operation_us = (uint32_t)(finished_us - started_us);
    ++s_mm8108.perf_wait_calls;
    ++s_mm8108.perf_write_calls;
    s_mm8108.perf_wait_us += wait_us;
    s_mm8108.perf_write_us += operation_us;
    if (err == ESP_OK) s_mm8108.perf_write_bytes += length;
    else ++s_mm8108.perf_errors;
    update_max_u32(&s_mm8108.perf_wait_max_us, wait_us);
    update_max_u32(&s_mm8108.perf_write_max_us, operation_us);
    const bool report_ready = take_usb_performance(&s_mm8108, finished_us, &report);
    xSemaphoreGive(s_mm8108.bus_mutex);
    if (report_ready) log_usb_performance(&report);
    return err;
}

esp_err_t mm8108_usb_reg32_read(uint32_t address, uint32_t *value)
{
    ESP_RETURN_ON_FALSE(value, ESP_ERR_INVALID_ARG, TAG, "null register value");
    uint8_t encoded[4];
    const esp_err_t err = mm8108_usb_mem_read(address, encoded, sizeof(encoded));
    if (err != ESP_OK) {
        return err;
    }
    *value = get_le32(encoded);
    return ESP_OK;
}

esp_err_t mm8108_usb_reg32_write(uint32_t address, uint32_t value)
{
    uint8_t encoded[4];
    put_le32(encoded, value);
    return mm8108_usb_mem_write(address, encoded, sizeof(encoded));
}

esp_err_t mm8108_usb_reset(void)
{
    ESP_RETURN_ON_FALSE(s_mm8108.bus_mutex, ESP_ERR_INVALID_STATE, TAG,
                        "MM8108 USB client not started");
    xSemaphoreTake(s_mm8108.bus_mutex, portMAX_DELAY);
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (s_mm8108.ready) {
        err = send_command(&s_mm8108, MM8108_USB_RESET, 0, 0);
    }
    xSemaphoreGive(s_mm8108.bus_mutex);
    return err;
}
/*
 * Copyright 2026 edgez.ai
 * SPDX-License-Identifier: Apache-2.0
 */
