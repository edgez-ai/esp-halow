/*
 * Copyright 2026 EdgeZeron AI AB. All rights reserved.
 * Licensed under the EdgeZ Commercial Software License.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "edgez_batman_adv_lite.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*edgez_mesh_payload_cb_t)(const uint8_t originator[6],
                                        const uint8_t *ethernet_frame,
                                        size_t frame_len,
                                        void *arg);

#define EDGEZ_MESH_TRANSPORT_MAX_PEERS 4U

typedef struct edgez_mesh_rf_metrics edgez_mesh_rf_metrics_t;

/** Vendor-independent link operations used by the BATMAN data plane. */
typedef struct {
    bool (*get_local_mac)(void *ctx, uint8_t mac[6]);
    size_t (*snapshot_authorized_peers)(void *ctx, uint8_t (*peers)[6],
                                        size_t capacity);
    esp_err_t (*transmit_ethernet)(void *ctx, const uint8_t *frame,
                                  size_t frame_len, const uint8_t receiver[6],
                                  uint8_t tid, uint32_t timeout_ms);
    void (*note_traffic)(void *ctx);
    bool (*snapshot_rf_metrics)(void *ctx, edgez_mesh_rf_metrics_t *metrics);
} edgez_mesh_transport_ops_t;

/** Live HaLow RF and BATMAN counters used by system/status reporting. */
struct edgez_mesh_rf_metrics {
    uint8_t peer_count;
    bool radio_enabled;
    bool available;
    bool signal_valid;
    bool noise_valid;
    int16_t average_signal_dbm;
    int16_t minimum_signal_dbm;
    int16_t noise_dbm;
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t tx_retries;
    uint32_t tx_failed;
};

/** Select a transport before edgez_mesh_init(). The operations are borrowed. */
esp_err_t edgez_mesh_set_transport(const edgez_mesh_transport_ops_t *ops,
                                   void *ctx);

/** Inject a decoded Ethernet frame received by the selected private UMAC. */
bool edgez_mesh_receive_ethernet(const uint8_t *frame, size_t frame_len,
                                 const uint8_t transmitter[6]);

/** Start the ESP-IDF BATMAN-IV worker on the already initialized HaLow radio. */
esp_err_t edgez_mesh_init(uint32_t originator_interval_ms);

/** Flush pending packets and reset all learned BATMAN routes while pausing the
 * data plane. This provides a clean boundary while the radio peer table is
 * replaced during a mesh-profile change. */
esp_err_t edgez_mesh_reset_and_pause(void);

/** Resume a BATMAN data plane paused by edgez_mesh_reset_and_pause(). */
esp_err_t edgez_mesh_resume(void);

/** Advertise this node as an Internet gateway in its BATMAN OGMs. */
void edgez_mesh_set_gateway(bool available);

/** Receive locally delivered inner Ethernet frames in worker-task context. */
void edgez_mesh_register_payload_cb(edgez_mesh_payload_cb_t callback, void *arg);

/** Independent IPv4/ARP/Alfred consumer; does not replace the MQTT consumer.
 * Register once at startup; callbacks run on the mesh worker and must not block. */
void edgez_mesh_register_network_cb(edgez_mesh_payload_cb_t callback, void *arg);

esp_err_t edgez_mesh_send_unicast(const uint8_t destination[6],
                                  const uint8_t *ethernet_frame,
                                  size_t frame_len);
esp_err_t edgez_mesh_send_broadcast(const uint8_t *ethernet_frame,
                                    size_t frame_len);

/** Preserve the caller's radio priority and admission deadline. TID is 0..7.
 * Existing send APIs retain TID 0 and a 50 ms admission timeout. */
esp_err_t edgez_mesh_send_unicast_qos(const uint8_t destination[6],
                                      const uint8_t *ethernet_frame, size_t frame_len,
                                      uint8_t tid, uint32_t timeout_ms);
esp_err_t edgez_mesh_send_broadcast_qos(const uint8_t *ethernet_frame, size_t frame_len,
                                        uint8_t tid, uint32_t timeout_ms);

bool edgez_mesh_lookup_route(const uint8_t destination[6],
                             uint8_t next_hop[6]);
/** Resolve an Alfred bridge/client MAC through the learned BATMAN TT. */
bool edgez_mesh_resolve_originator(const uint8_t client[6], uint8_t originator[6]);
bool edgez_mesh_mark_mqtt_gateway(const uint8_t originator[6],
                                  const uint8_t ethernet_mac[6]);
bool edgez_mesh_selected_mqtt_gateway(uint8_t originator[6],
                                      uint8_t ethernet_mac[6]);
size_t edgez_mesh_snapshot_routes(edgez_batadv_route_snapshot_t *routes,
                                  size_t capacity);

/** Snapshot live RF link quality and cumulative BATMAN packet counters. */
bool edgez_mesh_snapshot_rf_metrics(edgez_mesh_rf_metrics_t *metrics);

#ifdef __cplusplus
}
#endif
