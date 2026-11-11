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

typedef int (*edgez_mqtt_publish_fn_t)(const char *topic, const void *payload,
                                       size_t length, int qos, bool retain);
typedef void (*edgez_mqtt_command_fn_t)(const char *topic,
                                        const uint8_t *payload,
                                        size_t length);
typedef void (*edgez_mqtt_network_packet_fn_t)(const uint8_t *payload,
                                                size_t length,
                                                void *arg);

/** Stable discriminator carried as the first byte of an EdgeZ relay payload. */
typedef enum {
  EDGEZ_MQTT_PAYLOAD_SYSTEM_STATUS = 0,
  EDGEZ_MQTT_PAYLOAD_TELEMETRY = 1,
  /** A complete ai.edgez.halow.NetworkPacket protobuf. */
  EDGEZ_MQTT_PAYLOAD_NETWORK_PACKET = 2,
} edgez_mqtt_payload_kind_t;

/**
 * Initialize the MQTT namespace and transparent BATMAN relay transport.
 * The SDK owns the system/status and telemetry/sensors topic selection and
 * the leading relay discriminator; payload bytes are never inspected.
 */
esp_err_t edgez_mqtt_init(bool gateway,
                          const char *device_serial,
                          edgez_mqtt_publish_fn_t publish_fn,
                          edgez_mqtt_command_fn_t command_fn);
void edgez_mqtt_set_gateway_online(bool online);
void edgez_mqtt_set_local_mac(const uint8_t mac[6]);
bool edgez_mqtt_gateway_available();

/** Register the local consumer for peer NetworkPacket broadcasts/unicasts. */
void edgez_mqtt_register_network_packet_cb(
    edgez_mqtt_network_packet_fn_t callback, void *arg);

/**
 * Send one opaque NetworkPacket using the common BATMAN L2 relay envelope.
 * A null destination (or ff:ff:ff:ff:ff:ff) broadcasts. Any other destination
 * is routed as a BATMAN unicast. This outer destination is routing metadata
 * only. Source MAC, device/user identity, logical `to`, message ID, MIME, and
 * beacon fields remain inside the unmodified protobuf. The relay payload is
 * discriminator 2 followed by those protobuf bytes.
 */
esp_err_t edgez_mqtt_send_network_packet(const uint8_t destination[6],
                                         const void *payload, size_t length);

/** Publish opaque protobuf system status bytes to system/status. */
int edgez_mqtt_publish_system_status(const void *payload, size_t length,
                                     int qos, bool retain);

/** Publish opaque application bytes to telemetry/sensors. */
int edgez_mqtt_publish_telemetry(const void *payload, size_t length,
                                 int qos, bool retain);

bool edgez_mqtt_forward_command(const char *topic, const void *payload,
                                size_t length, int qos, bool retain);
void edgez_mqtt_receive(const uint8_t originator[6], const uint8_t *data,
                        size_t length);

#ifdef __cplusplus
}
#endif
