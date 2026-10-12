/*
 * Copyright 2026 EdgeZeron AI AB. All rights reserved.
 * Licensed under the EdgeZ Commercial Software License.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Prepare the board-owned radio transport before the protected mesh runtime
 * initializes the radio. The consumer platform provides this function. */
esp_err_t edgez_halow_platform_prepare(void);

#ifdef __cplusplus
}
#endif
