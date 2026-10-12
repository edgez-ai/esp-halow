/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mm8108_usb_start(void);
bool mm8108_usb_is_ready(void);

esp_err_t mm8108_usb_mem_read(uint32_t address, void *data, size_t length);
esp_err_t mm8108_usb_mem_write(uint32_t address, const void *data, size_t length);
esp_err_t mm8108_usb_reg32_read(uint32_t address, uint32_t *value);
esp_err_t mm8108_usb_reg32_write(uint32_t address, uint32_t value);

/** Perform the MM8108 non-destructive reset without USB re-enumeration. */
esp_err_t mm8108_usb_reset(void);

#ifdef __cplusplus
}
#endif
