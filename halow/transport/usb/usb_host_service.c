/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb_host_service.h"

static const char *TAG = "usb_host";
static TaskHandle_t s_daemon_task;

static void usb_daemon_task(void *arg)
{
#if CONFIG_HALOW_USB_DEBUG_LOG
    esp_log_level_set("USB HOST", ESP_LOG_DEBUG);
    esp_log_level_set("USBH", ESP_LOG_DEBUG);
    esp_log_level_set("HUB", ESP_LOG_DEBUG);
    esp_log_level_set("ENUM", ESP_LOG_DEBUG);
    esp_log_level_set("HCD DWC", ESP_LOG_DEBUG);
#endif

    const usb_host_config_t config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
        .peripheral_map = 1U << CONFIG_HALOW_USB_HOST_PERIPHERAL,
    };
    ESP_ERROR_CHECK(usb_host_install(&config));
    ESP_LOGI(TAG, "USB 2.0 HS host installed on peripheral %d%s",
             CONFIG_HALOW_USB_HOST_PERIPHERAL,
#if CONFIG_HALOW_USB_DEBUG_LOG
             " with enumeration diagnostics"
#else
             ""
#endif
    );
    xTaskNotifyGive((TaskHandle_t)arg);

    while (true) {
        uint32_t flags = 0;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &flags));
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGW(TAG, "no registered USB clients");
        }
        if (flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "all USB devices freed");
        }
    }
}

esp_err_t usb_host_service_start(void)
{
    const BaseType_t created = xTaskCreatePinnedToCore(
        usb_daemon_task, "usb_daemon", CONFIG_HALOW_USB_HOST_TASK_STACK_SIZE,
        xTaskGetCurrentTaskHandle(), CONFIG_HALOW_USB_HOST_TASK_PRIORITY,
        &s_daemon_task, 0);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "failed to create USB daemon");
    ESP_RETURN_ON_FALSE(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000)) == 1,
                        ESP_ERR_TIMEOUT, TAG, "USB host installation timed out");
    return ESP_OK;
}
