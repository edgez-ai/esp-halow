/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#include "edgez_halow_platform.h"

#include <inttypes.h>
#include <stdint.h>

#include "esp_log.h"
#include "sdkconfig.h"

#if CONFIG_HALOW_TRANSPORT_USB
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mm8108_usb.h"
#include "usb_host_service.h"
#endif

static const char *TAG = "halow_platform";

esp_err_t edgez_halow_platform_prepare(void)
{
#if CONFIG_HALOW_TRANSPORT_USB
    ESP_LOGI(TAG, "[mesh-stage 0/4] Starting ESP USB host");
    esp_err_t err = usb_host_service_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "USB host startup failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "[mesh-stage 0/4] Starting MM8108 USB client");
    err = mm8108_usb_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MM8108 USB client startup failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Waiting for MM8108 USB adapter");
    uint32_t waited_ms = 0;
    while (!mm8108_usb_is_ready()) {
        vTaskDelay(pdMS_TO_TICKS(10));
        waited_ms += 10;
#if CONFIG_HALOW_USB_DEVICE_TIMEOUT_MS > 0
        if (waited_ms >= CONFIG_HALOW_USB_DEVICE_TIMEOUT_MS) {
            ESP_LOGE(TAG, "MM8108 USB adapter did not enumerate within %d ms",
                     CONFIG_HALOW_USB_DEVICE_TIMEOUT_MS);
            return ESP_ERR_TIMEOUT;
        }
#endif
    }
    ESP_LOGI(TAG, "[mesh-stage 0/4] MM8108 enumerated after %" PRIu32 " ms",
             waited_ms);
#endif
    return ESP_OK;
}
