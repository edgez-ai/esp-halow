/*
 * Copyright 2026 EdgeZeron AI AB. All rights reserved.
 * Licensed under the EdgeZ Commercial Software License.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * High-level mesh service configuration.
 *
 * The service owns radio initialization, the station profile, fixed-channel
 * discovery and the optional BATMAN framework data plane. String storage only
 * needs to remain valid until edgez_halow_mesh_service_start() returns.
 */
typedef enum {
    EDGEZ_HALOW_LINK_DISABLED = 0,
    EDGEZ_HALOW_LINK_CONNECTING = 1,
    EDGEZ_HALOW_LINK_CONNECTED = 2,
} edgez_halow_link_state_t;

typedef void (*edgez_halow_link_status_cb_t)(edgez_halow_link_state_t state);

typedef struct {
    const char *country_code;
    const char *mesh_id;
    const char *passphrase;
    uint32_t frequency_khz;
    uint8_t bandwidth_mhz;
    uint16_t beacon_interval_tus;
    uint16_t scan_dwell_ms;
    uint32_t scan_initial_delay_ms;
    uint32_t scan_initial_interval_ms;
    uint32_t scan_connected_interval_ms;
    edgez_halow_link_status_cb_t status_cb;

    bool enable_batman;
    uint32_t batman_originator_interval_ms;
} edgez_halow_mesh_service_config_t;

#define EDGEZ_HALOW_MESH_SERVICE_CONFIG_INIT { \
    .country_code = NULL, \
    .mesh_id = NULL, \
    .passphrase = "", \
    .frequency_khz = 0, \
    .bandwidth_mhz = 0, \
    .beacon_interval_tus = 100, \
    .scan_dwell_ms = 120, \
    .scan_initial_delay_ms = 1000, \
    .scan_initial_interval_ms = 1000, \
    .scan_connected_interval_ms = 8000, \
    .status_cb = NULL, \
    .enable_batman = false, \
    .batman_originator_interval_ms = 3000, \
}

typedef struct {
    uint8_t mac[6];
} edgez_halow_mesh_service_info_t;

/**
 * Initialize and start a fixed-channel 802.11s HaLow service.
 *
 * This is synchronous and may block while the radio transport boots. Call it
 * from an application worker task. On success the radio, peer discovery and
 * requested optional data plane is running.
 */
esp_err_t edgez_halow_mesh_service_start(
    const edgez_halow_mesh_service_config_t *config,
    edgez_halow_mesh_service_info_t *info);

#ifdef __cplusplus
}
#endif
