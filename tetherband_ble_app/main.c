/**
 * Copyright (c) 2014 - 2020, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup ble_sdk_app_template_main main.c
 * @{
 * @ingroup ble_sdk_app_template
 * @brief Template project main file.
 *
 * This file contains a template for creating a new application. It has the code necessary to wakeup
 * from button, advertise, get a connection restart advertising on disconnect and if no new
 * connection created go back to system-off mode.
 * It can easily be used as a starting point for creating a new application, the comments identified
 * with 'YOUR_JOB' indicates where and how you can customize.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "custom_services.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "app_timer.h"
#include "bsp_btn_ble.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "nrf_ble_qwr.h"
#include "nrf_ble_gatt.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_bas.h"


#define APP_BLE_CONN_CFG_TAG                1 // used in advertising init
#define APP_BLE_OBSERVER_PRIORITY           3

// GAP Profile Parameters
#define DEVICE_NAME                         "Tether01"
#define MIN_CONN_INTERVAL                   MSEC_TO_UNITS(100, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL                   MSEC_TO_UNITS(200, UNIT_1_25_MS)
#define SLAVE_LATENCY                       0
#define CONN_SUP_TIMEOUT                    MSEC_TO_UNITS(3000, UNIT_10_MS)

// BLE Advertising parameters
#define APP_ADV_INTERVAL                    300
#define APP_ADV_DURATION                    0

#define FIRST_CONN_PARAMS_UPDATE_DELAY      APP_TIMER_TICKS(5000)
#define NEXT_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(30000)
#define MAX_CONN_PARAMS_UPDATE_COUNT        3

// Time in between battery level reads
#define BATTERY_LEVEL_MEAS_INTERVAL     APP_TIMER_TICKS(120000)


NRF_BLE_QWR_DEF(m_qwr); // create queue writer object
NRF_BLE_GATT_DEF(m_gatt); // create gatt object
BLE_ADVERTISING_DEF(m_advert); // create advertising object
BLE_IDENTIFY_DEF(m_identify); // create custom identifying service
APP_TIMER_DEF(m_battery_timer_id); // setup app timer for battery level reads
BLE_BAS_DEF(m_bas); // instance of battery level service

// connection handle used in ble_evt_handler
static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

static ble_uuid_t m_adv_uuids[] = {
                                    {IDENTIFY_UUID_BASE, BLE_UUID_TYPE_VENDOR_BEGIN},
                                    {BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE}
                                  };

// To have UUID be advertised correctly, the object passed in advertising init has to be the 12th and 13th octet. 
// IDENTIFY_UUID_SERVICE matches the 12th and 13th octet so it can be passed properly.
static ble_uuid_t custom_uuids[] = {{IDENTIFY_UUID_SERVICE, BLE_UUID_TYPE_VENDOR_BEGIN}};

static uint8_t simulate_battery = 100; // for simulating battery service


// error handler for queue writer used in services
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

// event handler for connection parameters update
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    ret_code_t err_code;
    if(p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
    else if(p_evt->evt_type == BLE_CONN_PARAMS_EVT_SUCCEEDED)
    {
        NRF_LOG_INFO("conn params event succeeded");/////////////// fill in later
    }
}

// error handler for connection parameters update
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

// Step 10: Set up connection parameters
static void conn_params_init(void)
{
    ret_code_t err_code;

    ble_conn_params_init_t conn_params_init;
    memset(&conn_params_init, 0 , sizeof(conn_params_init));

    conn_params_init.p_conn_params = NULL;
    conn_params_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    conn_params_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
    conn_params_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
    conn_params_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
    conn_params_init.disconnect_on_fail = false;
    conn_params_init.error_handler = conn_params_error_handler;
    conn_params_init.evt_handler = on_conn_params_evt;

    err_code = ble_conn_params_init(&conn_params_init);
    APP_ERROR_CHECK(err_code);
} 

// runs when the battery measurement timer expires every 3 seconds.
// if client has notifications enabled for this service it will get update of battery level.
static void battery_level_meas_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    if(simulate_battery != 0){
        simulate_battery -= 1;
    }
    
    uint8_t percentage_batt_lvl = simulate_battery;
    ret_code_t err_code;
    err_code = ble_bas_battery_level_update(&m_bas, percentage_batt_lvl, BLE_CONN_HANDLE_ALL);
    APP_ERROR_CHECK(err_code);
}

// battery service handler
static void on_bas_evt(ble_bas_t * p_bas, ble_bas_evt_t * p_evt)
{
    ret_code_t err_code;

    switch (p_evt->evt_type)
    {
        case BLE_BAS_EVT_NOTIFICATION_ENABLED:
            // Start battery timer
            NRF_LOG_INFO("Notification Enabled event received");
            NRF_LOG_FLUSH();
            err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
            APP_ERROR_CHECK(err_code);
            break; // BLE_BAS_EVT_NOTIFICATION_ENABLED

        case BLE_BAS_EVT_NOTIFICATION_DISABLED:
            NRF_LOG_INFO("Notification Disabled event received");
            NRF_LOG_FLUSH();
            err_code = app_timer_stop(m_battery_timer_id);
            APP_ERROR_CHECK(err_code);
            break; // BLE_BAS_EVT_NOTIFICATION_DISABLED

        default:
            NRF_LOG_INFO("BLE BAS event received but does not match cases");
            NRF_LOG_FLUSH();
            // No implementation needed.
            break;
    }
}

// Initialize battery service
static void bas_init(void)
{
    ret_code_t     err_code;
    ble_bas_init_t bas_init_obj;

    memset(&bas_init_obj, 0, sizeof(bas_init_obj));

    bas_init_obj.evt_handler          = on_bas_evt;
    bas_init_obj.support_notification = true;
    bas_init_obj.p_report_ref         = NULL;
    bas_init_obj.initial_batt_level   = 100;

    bas_init_obj.bl_rd_sec        = SEC_OPEN;
    bas_init_obj.bl_cccd_wr_sec   = SEC_OPEN;
    bas_init_obj.bl_report_rd_sec = SEC_OPEN;

    err_code = ble_bas_init(&m_bas, &bas_init_obj);
    APP_ERROR_CHECK(err_code);
}

// Step 9: Init services with queue writer
static void services_init(void)
{
    ret_code_t err_code;

    nrf_ble_qwr_init_t qwr_init = {0};
    ble_identify_init_t identify_init;
    identify_init.evt_handler = ble_identify_on_evt;

    qwr_init.error_handler = nrf_qwr_error_handler;
    memset(&identify_init, 0 , sizeof(identify_init));
    
    err_code = ble_identify_init(&m_identify, &identify_init);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    

    bas_init(); // setting up all bas init objects done in this function
}

// Advertising handler
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    ret_code_t err_code;

    switch(ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            NRF_LOG_INFO("fast advertising...");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_ADV_EVT_IDLE: 
            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);
            break;
     
        default:
            break;
  }
}


// Step 8: Advertising init
static void advertising_init(void)
{
    ret_code_t err_code;
    ble_advertising_init_t init;
    memset(&init, 0 , sizeof(init));

    /*ble_advdata_manuf_data_t manuf_data;
    uint8_t identifier[] = {0x01};
    manuf_data.company_identifier = 0x0059; // specific to nordic
    manuf_data.data.p_data = identifier;
    manuf_data.data.size = sizeof(identifier);
    NRF_LOG_INFO("Manufacturer data is: %s", manuf_data.data.p_data);
    init.advdata.p_manuf_specific_data = &manuf_data;*/


    init.advdata.name_type = BLE_ADVDATA_SHORT_NAME;
    init.advdata.short_name_len = 4;
    init.advdata.include_appearance = true;
    init.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    init.advdata.uuids_complete.uuid_cnt = 1;
    init.advdata.uuids_complete.p_uuids = custom_uuids;

    init.config.ble_adv_fast_enabled = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL; // if using fast advertising mode, you have to set this and fast timeout
    init.config.ble_adv_fast_timeout = APP_ADV_DURATION;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advert, &init); // init ble object with set values
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advert, APP_BLE_CONN_CFG_TAG); // attach config tag
}

// Step 7: GATT init
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}

// Step 6: GAP Init
static void gap_params_init(void)
{
    ret_code_t err_code;
    ble_gap_conn_params_t     gap_conn_params;
    ble_gap_conn_sec_mode_t   sec_mode;
    // set security mode
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    // set device name
    err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)DEVICE_NAME, strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);
    memset(&gap_conn_params, 0, sizeof(gap_conn_params)); // clear memory of connection parameters
    // set the parameters
    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;
    // pass the gap ppcp params to the soft device
    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

// BLE Event Handler
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    ret_code_t err_code = NRF_SUCCESS;

    switch(p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Device has been disconnected!");
            break;

        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Device has been connected!");
      
            // light corresponding leds on board
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
  
            // get handle of connected device in case disconnected in future
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
            NRF_LOG_DEBUG("PHY update request");
            // allows the speed of PHY to be changed if central device requests it.
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };

            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_WRITE:
            NRF_LOG_INFO("IN EVENT WRITE CASE IN MAIN");
            ble_identify_on_evt(&m_identify, p_ble_evt);
            break;

        default:
            break;
  }
}

// Step 5 init ble stack
static void ble_stack_init()
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIORITY, ble_evt_handler, NULL);
}

// Step 4 init power management
static void power_management_init(void)
{
    ret_code_t err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

// handler for idle power state
static void idle_state_handle(void)
{
    if(NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}

// Step 3 init BSP
static void leds_init(void)
{
    ret_code_t err_code = bsp_init(BSP_INIT_LEDS, NULL);
    APP_ERROR_CHECK(err_code);
}

// Step 2, initialize app timer
static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_battery_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                battery_level_meas_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

// First step, initialize logger
static void log_init()
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

// Step 11: Function to start advertising
static void advertising_start(void)
{
    ret_code_t err_code = ble_advertising_start(&m_advert, BLE_ADV_MODE_FAST); // have to use the same type of advertising that was given in the init function.
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for application main entry.
 */
int main(void)
{
    // System init
    log_init();
    timers_init();
    leds_init();
    power_management_init();

    // Bluetooth init
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();

    NRF_LOG_INFO("BLE Base Application started!");

    advertising_start();

    // Not sure if you should put the NFC initialization code before the Bluetooth init stuff or after,
    // I would try after it first and see if everything still works.

    // Enter main loop.
    for(;;)
    {
        idle_state_handle();
    }
}


/**
 * @}
 */