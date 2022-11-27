/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "bleprph.h"
#include "services/ans/ble_svc_ans.h"
#include "led_task.h"
#include "esp_log.h"

/**
 * The vendor specific security test service consists of two characteristics:
 *     o random-number-generator: generates a random 32-bit number each time
 *       it is read.  This characteristic can only be read over an encrypted
 *       connection.
 *     o static-value: a single-byte characteristic that can always be read,
 *       but can only be written over an encrypted connection.
 */

/* 59462f12-9543-9999-12c8-58b459a2712d */
static const ble_uuid128_t gatt_svr_svc_sec_test_uuid =
    BLE_UUID128_INIT(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
                     0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

/* 5c3a659e-897e-45e1-b016-007107c96df6 */
static const ble_uuid128_t gatt_svr_chr_sec_test_rand_uuid =
    BLE_UUID128_INIT(0xf6, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
                     0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

/* 5c3a659e-897e-45e1-b016-007107c96df7 */
static const ble_uuid128_t gatt_svr_chr_sec_test_static_uuid =
    BLE_UUID128_INIT(0xf7, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
                     0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

static uint8_t gatt_svr_sec_test_static_val;

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

// UUIDs for LED service
// TODO: currently backwards
/* 41c6b692-0ba0-4b73-b586-35a268a320ef */
static const ble_uuid128_t gatt_svr_svc_led_uuid =
    BLE_UUID128_INIT(0x41, 0xc6, 0xb6, 0x92, 0x0b, 0xa0, 0x4b, 0x73,
                     0xb5, 0x86, 0x35, 0xa2, 0x68, 0xa3, 0x20, 0xef);

/* d7419b26-1437-4f29-a6c8-259cf01bc815 */
static const ble_uuid128_t gatt_svr_chr_led_static_red_uuid =
    BLE_UUID128_INIT(0xd7, 0x41, 0x9b, 0x26, 0x14, 0x37, 0x4f, 0x29,
                     0xa6, 0xc8, 0x25, 0x9c, 0xf0, 0x1b, 0xc8, 0x15);

/* 3fa4eea9-5368-4f1b-9687-10574f0adcae */
static const ble_uuid128_t gatt_svr_chr_led_static_green_uuid =
    BLE_UUID128_INIT(0x3f, 0xa4, 0xee, 0xa9, 0x53, 0x68, 0x4f, 0x1b,
                     0x96, 0x87, 0x10, 0x57, 0x4f, 0x0a, 0xdc, 0xae);

/* 8f61467a-c4ff-4ebb-943d-49596f9fd4e7 */
static const ble_uuid128_t gatt_svr_chr_led_static_blue_uuid =
    BLE_UUID128_INIT(0x8f, 0x61, 0x46, 0x7a, 0xc4, 0xff, 0x4e, 0xbb,
                     0x94, 0x3d, 0x49, 0x59, 0x6f, 0x9f, 0xd4, 0xe7);

/* dfae6ade-d0fe-453e-ba47-07b8a3c6bbb5 */
static const ble_uuid128_t gatt_svr_chr_led_delay_uuid =
    BLE_UUID128_INIT(0xdf, 0xae, 0x6a, 0xde, 0xd0, 0xfe, 0x45, 0x3e,
                     0xba, 0x47, 0x07, 0xb8, 0xa3, 0xc6, 0xbb, 0xb5);

static const ble_uuid16_t user_description_uuid = BLE_UUID16_INIT(0x2901);
static const char* red_user_desc = "RedLedBrightness";
static const char* green_user_desc = "GreenLedBrightness";
static const char* blue_user_desc = "BlueLedBrightness";
static const char* delay_user_desc = "LedDelayBrightness";

// Log prefix
static const char *tag = "GATT";

// Callback for LED service

static int
gatt_svr_chr_access_led(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt,
                        void *arg);

static int
gatt_svr_usr_chr_red(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt,
                        void *arg) {
    int rc;
    ESP_LOGI(tag, "Red\n");
    rc = os_mbuf_append(ctxt->om, &red_user_desc,
                        sizeof(red_user_desc));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int
gatt_svr_usr_chr_green(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt,
                        void *arg) {
    int rc;
    ESP_LOGI(tag, "Green\n");
    rc = os_mbuf_append(ctxt->om, &green_user_desc,
                        sizeof(green_user_desc));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int
gatt_svr_usr_chr_blue(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt,
                        void *arg) {
    int rc;
    ESP_LOGI(tag, "Blue\n");
    rc = os_mbuf_append(ctxt->om, &blue_user_desc,
                        sizeof(blue_user_desc));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int
gatt_svr_usr_chr_delay(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt,
                        void *arg) {
    int rc;
    ESP_LOGI(tag, "Delay\n");
    rc = os_mbuf_append(ctxt->om, &delay_user_desc,
                        sizeof(delay_user_desc));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Security test. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_sec_test_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                /*** Characteristic: Random number generator. */
                .uuid = &gatt_svr_chr_sec_test_rand_uuid.u,
                .access_cb = gatt_svr_chr_access_sec_test,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            }, {
                /*** Characteristic: Static value. */
                .uuid = &gatt_svr_chr_sec_test_static_uuid.u,
                .access_cb = gatt_svr_chr_access_sec_test,
                .flags = BLE_GATT_CHR_F_READ |
                BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
            }, {
                0, /* No more characteristics in this service. */
            }
        },
    },
    {
        /*** Service: LED control. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_led_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                /*** Characteristic: Amount of red when LED is static. */
                .uuid = &gatt_svr_chr_led_static_red_uuid.u,
                .access_cb = gatt_svr_chr_access_led,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                    .uuid = &user_description_uuid.u,
                    .access_cb = gatt_svr_usr_chr_red,
                    .att_flags = BLE_ATT_F_READ,
                    }, {
                    0,
                    }
                }
            }, {
                /*** Characteristic: Amount of green when LED is static. */
                .uuid = &gatt_svr_chr_led_static_green_uuid.u,
                .access_cb = gatt_svr_chr_access_led,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                    .uuid = &user_description_uuid.u,
                    .access_cb = gatt_svr_usr_chr_green,
                    .att_flags = BLE_ATT_F_READ,
                    }, {
                    0,
                    }
                }
            }, {
                /*** Characteristic: Amount of blue when LED is static. */
                .uuid = &gatt_svr_chr_led_static_blue_uuid.u,
                .access_cb = gatt_svr_chr_access_led,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                    .uuid = &user_description_uuid.u,
                    .access_cb = gatt_svr_usr_chr_blue,
                    .att_flags = BLE_ATT_F_READ,
                    }, {
                    0,
                    }
                }
            }, {
                /*** Characteristic: Delay in ms between changing rainbow LED colors.
                 *   If 0, uses the RGB value set by other chars instead. */
                .uuid = &gatt_svr_chr_led_delay_uuid.u,
                .access_cb = gatt_svr_chr_access_led,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                    .uuid = &user_description_uuid.u,
                    .access_cb = gatt_svr_usr_chr_delay,
                    .att_flags = BLE_ATT_F_READ,
                    }, {
                    0,
                    }
                }
            }, {
                0, /* No more characteristics in this service. */
            }
        },
    },
    {
        0, /* No more services. */
    },
};

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}


static int
gatt_svr_chr_access_led(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt,
                        void *arg) {
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    uint8_t color_val;
    uint32_t delay_val;
    int rc;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_led_static_red_uuid.u) == 0) {
        ESP_LOGI(tag, "Log %i, rw %i\n", 0, ctxt->op);
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            color_val = getColor(RED);
            rc = os_mbuf_append(ctxt->om, &color_val,
                                sizeof(color_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            rc = gatt_svr_chr_write(ctxt->om,
                                    sizeof(color_val),
                                    sizeof(color_val),
                                    &color_val, NULL);
            if (rc == 0) {
                setColor(RED, color_val);
            }
            return rc;

        default:
            return BLE_ATT_ERR_UNLIKELY;
        }
    } else if (ble_uuid_cmp(uuid, &gatt_svr_chr_led_static_green_uuid.u) == 0) {
        ESP_LOGI(tag, "Log %i, rw %i\n", 0, ctxt->op);
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            color_val = getColor(GREEN);
            rc = os_mbuf_append(ctxt->om, &color_val,
                                sizeof(color_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            rc = gatt_svr_chr_write(ctxt->om,
                                    sizeof(color_val),
                                    sizeof(color_val),
                                    &color_val, NULL);
            if (rc == 0) {
                setColor(GREEN, color_val);
            }
            return rc;

        default:
            return BLE_ATT_ERR_UNLIKELY;
        }
    } else if (ble_uuid_cmp(uuid, &gatt_svr_chr_led_static_blue_uuid.u) == 0) {
        ESP_LOGI(tag, "Log %i, rw %i\n", 0, ctxt->op);
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            color_val = getColor(BLUE);
            rc = os_mbuf_append(ctxt->om, &color_val,
                                sizeof(color_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            rc = gatt_svr_chr_write(ctxt->om,
                                    sizeof(color_val),
                                    sizeof(color_val),
                                    &color_val, NULL);
            if (rc == 0) {
                setColor(BLUE, color_val);
            }
            return rc;

        default:
            return BLE_ATT_ERR_UNLIKELY;
        }
    } else if (ble_uuid_cmp(uuid, &gatt_svr_chr_led_delay_uuid.u) == 0) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            delay_val = getDelay();
            rc = os_mbuf_append(ctxt->om, &delay_val,
                                sizeof(delay_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            rc = gatt_svr_chr_write(ctxt->om,
                                    sizeof(delay_val),
                                    sizeof(delay_val),
                                    &delay_val, NULL);
            if (rc == 0) {
                setDelay(delay_val);
            }
            return rc;

        default:
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    const ble_uuid_t *uuid;
    int rand_num;
    int rc;

    uuid = ctxt->chr->uuid;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_test_rand_uuid.u) == 0) {
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);

        /* Respond with a 32-bit random number. */
        rand_num = rand();
        rc = os_mbuf_append(ctxt->om, &rand_num, sizeof rand_num);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_test_static_uuid.u) == 0) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            rc = os_mbuf_append(ctxt->om, &gatt_svr_sec_test_static_val,
                                sizeof gatt_svr_sec_test_static_val);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            rc = gatt_svr_chr_write(ctxt->om,
                                    sizeof gatt_svr_sec_test_static_val,
                                    sizeof gatt_svr_sec_test_static_val,
                                    &gatt_svr_sec_test_static_val, NULL);
            return rc;

        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    /* Unknown characteristic; the nimble stack should not have called this
     * function.
     */
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                    "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int
gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_ans_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
