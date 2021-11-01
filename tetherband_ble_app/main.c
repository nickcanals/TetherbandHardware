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
#include <stdio.h>
#include "custom_services.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

// NFC Includes //////////////
#include "app_error.h"
#include "app_scheduler.h"
#include "boards.h"
#include "nfc_t4t_lib.h"
#include "ndef_file_m.h"
#include "nfc_ndef_msg.h"
//////////////////////////////

// State of Charge IC Includes
#include "app_util_platform.h"
#include "nrf_drv_twi.h"
#include "gauge.h"
#include "nrf_delay.h"
//////////////////////////////

// BLE Includes///////////////
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
/////////////////////////////

//Local Alerts Includes///////////////
#include "nrf_drv_pwm.h"
#include "bsp.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"

//Touch Sensor //////////////////////
#include "nrf_csense.h"
#include "nrf_drv_timer.h"


//Touch Sensor Parameters ///////////
/* Time between RTC interrupts. */
#define APP_TIMER_TICKS_TIMEOUT APP_TIMER_TICKS(50)

/* Scale range. */
#define RANGE                   50

/* Analog inputs. */
#define AIN1                    1
#define AIN2                    2
#define AIN3                    3
#define AIN4                    4
#define AIN7                    7

/* Definition which pads use which analog inputs. */
#define BUTTON                  AIN7
#define PAD1                    AIN4
#define PAD4                    AIN4
#ifdef NRF51
#define PAD2                    AIN2
#define PAD3                    AIN3
#else
#define PAD2                    AIN1
#define PAD3                    AIN2
#endif
/*#define SPEAKER_OUT  6
#define MOTOR_OUT    7
#define LED_PURPLE   13
#define LED_YELLOW   14
#define LED_ORANGE   15
#define LED_RED      16
#define LED_BLUE     17
#define LED_GREEN    18*/

//* Threshold values for pads and button
#define THRESHOLD_PAD_1         800
#define THRESHOLD_PAD_2         800
#define THRESHOLD_PAD_3         800
#define THRESHOLD_PAD_4         800
#define THRESHOLD_BUTTON        800

/*lint -e19 -save */
NRF_CSENSE_BUTTON_DEF(m_button, (BUTTON, THRESHOLD_BUTTON));
NRF_CSENSE_SLIDER_4_DEF(m_slider,
                        RANGE,
                        (PAD1, THRESHOLD_PAD_1),
                        (PAD2, THRESHOLD_PAD_2),
                        (PAD3, THRESHOLD_PAD_3),
                        (PAD4, THRESHOLD_PAD_4));


void bracelet_removed_flash();
void repeated_timer_handler();
uint8_t bracelet_removed = 0;
uint8_t bracelet_initial = 2;


// DK MAC address is DD:C8:1A:7E:E8:F5
// BLE Defines //////////////////////////////////////////////////////////////

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

// Time in between battery level reads (ms)
#define BATTERY_LEVEL_MEAS_INTERVAL     APP_TIMER_TICKS(1200)
// End BLE Defines //////////////////////////////////////////////////////////


// App Scheduler Defines and NFC Defines ////////////////////////////////////
#define BLE_BAS_SCHED_EVENT_DATA_SIZE sizeof(ble_bas_evt_t)
#define APP_SCHED_MAX_EVENT_SIZE MAX(APP_TIMER_SCHED_EVENT_DATA_SIZE, BLE_BAS_SCHED_EVENT_DATA_SIZE) /**< Maximum size of scheduler events. */
#define APP_SCHED_QUEUE_SIZE     20                  /**< Maximum number of events in the scheduler queue. */
#define APP_DEFAULT_BTN          BSP_BOARD_BUTTON_0 /**< Button used to set default NDEF message. */
// End NFC Defines //////////////////////////////////////////////////////////


// Module Instances /////////////////////////////////////////////////////////

NRF_BLE_QWR_DEF(m_qwr); // create queue writer object
NRF_BLE_GATT_DEF(m_gatt); // create gatt object
BLE_ADVERTISING_DEF(m_advert); // create advertising object
BLE_IDENTIFY_DEF(m_identify); // create custom identifying service
APP_TIMER_DEF(m_battery_timer_id); // setup app timer for battery level reads
BLE_BAS_DEF(m_bas); // instance of battery level service
// End Module Instances ////////////////////////////////////////////////////

// BLE globals ////////////////////////////////////////////////////////////////////////

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID; // connection handle used in ble_evt_handler
static ble_uuid_t m_adv_uuids[] = {
                                    {IDENTIFY_UUID_BASE, BLE_UUID_TYPE_VENDOR_BEGIN},
                                    {BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE}
                                  };
// To have UUID be advertised correctly, the object passed in advertising init has to be the 12th and 13th octet. 
// IDENTIFY_UUID_SERVICE matches the 12th and 13th octet so it can be passed properly.
static ble_uuid_t custom_uuids[] = {{IDENTIFY_UUID_SERVICE, BLE_UUID_TYPE_VENDOR_BEGIN}};
// End BLE globals /////////////////////////////////////////////////////////////////////


// NFC globals ////////////////////////////////////////////////////////////////////////

static uint8_t m_ndef_msg_buf[NDEF_FILE_SIZE];      /**< Buffer for NDEF file. */
static uint8_t m_ndef_msg_len;                      /**< Length of the NDEF message. */
// End NFC globals ////////////////////////////////////////////////////////////////////


static uint16_t battery_percent; // updated by SoC IC, sent through battery service when notifications enabled.
static uint16_t simulate_battery = 100;

////////////////////////////////// NFC Functions ///////////////////////////////////////////////////////////////

//  brief Function for updating NDEF message in the flash file.

static void scheduler_ndef_file_update(void * p_event_data, uint16_t event_size)
{
    ret_code_t err_code;

    UNUSED_PARAMETER(p_event_data);
    UNUSED_PARAMETER(event_size);

    // Update flash file with new NDEF message.
    err_code = ndef_file_update(m_ndef_msg_buf, m_ndef_msg_len + NLEN_FIELD_SIZE);
    APP_ERROR_CHECK(err_code);
    
    for(int i = 11; i < 17; i++){
        NRF_LOG_INFO("HERE: %c", m_ndef_msg_buf[i]);
    }
    

    if((char*)m_ndef_msg_buf[11] == 'o'){
        bsp_board_led_on(LED_ORANGE_IDX);
        NRF_LOG_INFO("Color Sent Over NFC Is: Orange");
    }
    else if((char*)m_ndef_msg_buf[11] == 'p'){
        bsp_board_led_on(LED_PURPLE_IDX);
        NRF_LOG_INFO("Color Sent Over NFC Is: Purple");
    }
    else if((char*)m_ndef_msg_buf[11] == 'y'){
        bsp_board_led_on(LED_YELLOW_IDX);
        NRF_LOG_INFO("Color Sent Over NFC Is: Yellow");
    }
    else if((char*)m_ndef_msg_buf[11] == 'r'){
        bsp_board_led_on(LED_RED_IDX);
        NRF_LOG_INFO("Color Sent Over NFC Is: Red");
    }
    else if((char*)m_ndef_msg_buf[11] == 'b'){
        bsp_board_led_on(LED_BLUE_IDX);
        NRF_LOG_INFO("Color Sent Over NFC Is: Blue");
    }
    else{
        bsp_board_led_on(LED_GREEN_IDX);
        NRF_LOG_INFO("Color Sent Over NFC Is: Green");
    }


    NRF_LOG_INFO("NDEF message updated!");
}


/**
 * @brief Callback function for handling NFC events.
 */
static void nfc_callback(void          * context,
                         nfc_t4t_event_t event,
                         const uint8_t * data,
                         size_t          dataLength,
                         uint32_t        flags)
{
    (void)context;

    switch (event)
    {
        case NFC_T4T_EVENT_FIELD_ON:
            bsp_board_led_on(BSP_BOARD_LED_0);
            NRF_LOG_INFO("EVENT: NFC T4T EVENT FIELD ON TRIGGERED");
            break;

        case NFC_T4T_EVENT_FIELD_OFF:
            //bsp_board_leds_off();
            NRF_LOG_INFO("EVENT: NFC_T4T_EVENT_FIELD_OFF TRIGGERED");
            break;

        case NFC_T4T_EVENT_NDEF_READ:
            bsp_board_led_on(BSP_BOARD_LED_3);
            NRF_LOG_INFO("EVENT: NFC_T4T_EVENT_NDEF_READ TRIGGERED");
            break;

        case NFC_T4T_EVENT_NDEF_UPDATED:
            NRF_LOG_INFO("EVENT: NFC_T4T_EVENT_NDEF_UPDATED TRIGGERED");
            if (dataLength > 0)
            {
                ret_code_t err_code;

                // Schedule update of NDEF message in the flash file.
                m_ndef_msg_len = dataLength;
                NRF_LOG_INFO("IN NFC NDEF UPDATED EVENT");
                err_code       = app_sched_event_put(NULL, 0, scheduler_ndef_file_update);
                APP_ERROR_CHECK(err_code);
            }
            break;

        default:
            break;
    }
}
// End NFC Functions //////////////////////////////////////////////////////////////////////////////////////////


// SoC IC Functions ////////////////////////////////////////////////////////////////////////////////////////////

/*  Repeat of code in main() to print out saved SoC IC config settings. Doesn't return values properly.
void get_battery_config_settings(uint16_t *capacity, uint16_t *energy, uint16_t *term_voltage){
    char state_buffer[STATE_SUB_LEN];
    uint16_t result;
    
    unseal_gauge();
    // Read out all the new values
    *capacity = gauge_cmd_read(NULL, GET_DESIGN_CAPACITY);
    
    result = gauge_read_data_class(NULL, STATE_SUB, state_buffer, STATE_SUB_LEN); // read out the whole state subclass
    shift_register(state_buffer, STATE_SUB_LEN); // have to shift because values are returned shifted one left

    if(result == -1){
        NRF_LOG_INFO("reading from state data class failed");
        NRF_LOG_FLUSH();
    }

    uint8_t copy_array[2];
    copy_array[0] = state_buffer[DESIGN_EGY_OFFSET + 1];
    copy_array[1] = state_buffer[DESIGN_EGY_OFFSET];
    NRF_LOG_INFO("COPY ARRAY CONTENTS BEFORE FIRST MEMCPY: %d, %d", copy_array[0], copy_array[1]);
    NRF_LOG_FLUSH();
    memcpy(&energy, copy_array,  sizeof(energy));
    NRF_LOG_INFO("ENERGY CONTENTS AFTER FIRST MEMCPY: %d", energy);
    NRF_LOG_FLUSH();
    
    copy_array[0] = state_buffer[TERM_VOLT_OFFSET + 1];
    copy_array[1] = state_buffer[TERM_VOLT_OFFSET];
    memcpy(&term_voltage, copy_array,  sizeof(term_voltage));
    
    gauge_control(NULL, SEAL, 0);
    return;
}*/

// End SoC IC Functions //////////////////////////////////////////////////////////////////////////

// BLE Functions ///////////////////////////////////////////////////////////////////////////

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

// runs when the battery measurement timer expires every BATTERY_LEVEL_MEAS_INTERVAL (ms).
// if client has notifications enabled for this service it will get update of battery level.
static void battery_level_meas_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    // REMOVE THIS AFTER TESTING WITH SOC IC AND BATTERY CONNECTED 
    if(simulate_battery != 0){
        simulate_battery -= 1;
    }
    //NRF_LOG_INFO("SIMULATE BATTERY LEVEL = %d", simulate_battery);
    
    uint8_t percentage_batt_lvl = simulate_battery;
    
    /*battery_percent = gauge_cmd_read(NULL, GET_BATT_PCT);
    NRF_LOG_INFO("Battery percent successfully read at %d%", battery_percent);
    NRF_LOG_FLUSH();*/

    ret_code_t err_code;
    //err_code = ble_bas_battery_level_update(&m_bas, (uint8_t)battery_percent, BLE_CONN_HANDLE_ALL);
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
            err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
            APP_ERROR_CHECK(err_code);
            break; // BLE_BAS_EVT_NOTIFICATION_ENABLED

        case BLE_BAS_EVT_NOTIFICATION_DISABLED:
            NRF_LOG_INFO("Notification Disabled event received");
            err_code = app_timer_stop(m_battery_timer_id);
            APP_ERROR_CHECK(err_code);
            break; // BLE_BAS_EVT_NOTIFICATION_DISABLED

        default:
            NRF_LOG_INFO("BLE BAS event received but does not match cases");
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

// End BLE Functions /////////////////////////////////////////////////////////////////////////////

// Step 4 init power management
static void power_management_init(void)
{
    ret_code_t err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}



void scheduler_touch_sensor_handler(void * p_event_data, uint16_t event_size){
    UNUSED_PARAMETER(p_event_data);
    UNUSED_PARAMETER(event_size);
    
    //NRF_LOG_INFO("CSENSE HANDLER FUNCTION EXECUTING HERE");
    bracelet_removed_flash();
    __WFI();
    NRF_LOG_FLUSH();
}





// handler for idle power state
static void idle_state_handle(void)
{
    app_sched_execute();
    
    //addd in Csense stuff here
    ret_code_t err_code = app_sched_event_put(NULL, 0, scheduler_touch_sensor_handler);
    APP_ERROR_CHECK(err_code);

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


///////////////////////TOUCH SENSOR FUNCTIONS//////////////////////////////////
  /**
 * @brief Function for starting the internal LFCLK XTAL oscillator.
 *
 * Note that when using a SoftDevice, LFCLK is always on.
 *
 * @return Values returned by @ref nrf_drv_clock_init.
 */
static ret_code_t clock_config(void)
{
    ret_code_t err_code;
    err_code = nrf_drv_clock_init();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_clock_lfclk_request(NULL);
    return NRF_SUCCESS;
}

/**
 * @brief Function for handling slider interrupts.
 *
 * @param[in] step                          Detected step.
 */
//static void slider_handler(uint16_t step)
//{
   // static uint16_t slider_val;
   // if (slider_val != step)
    //{
        
       // NRF_LOG_INFO("Bracelt is on");
      //  slider_val = step;
  //  }
//}

/**
 * @brief Event handler for Capacitive Sensor High module.
 *
 * @param [in] p_evt_type                    Pointer to event data structure.
 */
void nrf_csense_handler(nrf_csense_evt_t * p_evt)
{
       
    switch (p_evt->nrf_csense_evt_type)
    {
        case NRF_CSENSE_BTN_EVT_PRESSED:
        bracelet_removed = 0;
        bracelet_initial=0;
        bsp_board_led_on(LED_BLUE_IDX);
        NRF_LOG_INFO("Bracelet is on");
        break;
          
        case NRF_CSENSE_BTN_EVT_RELEASED:
        bracelet_removed = 1;
        //repeated_timer_handler();
        bracelet_removed_flash();
        break;
                      
        /*case NRF_CSENSE_SLIDER_EVT_PRESSED:
        nrf_gpio_pin_set(LED_BLUE);
        break;
        case NRF_CSENSE_SLIDER_EVT_RELEASED:
        break;
        case NRF_CSENSE_SLIDER_EVT_DRAGGED:
        if ((p_evt->p_instance == (&m_slider)) && (p_evt->params.slider.step != UINT16_MAX))
        {
        /*lint -e611 -save */
        // ((void(*)(uint16_t, uint8_t))p_evt->p_instance->p_context)(p_evt->params.slider.step, 2);
        /*lint -restore*/
        // }
        // break;
        default:
        NRF_LOG_WARNING("Unknown event.");
        break;
    }
}


/*
 * Function enables one slider and one button.
 */
static void csense_start(void){
    ret_code_t err_code;
    static uint16_t touched_counter = 0;
    err_code = nrf_csense_init(nrf_csense_handler, APP_TIMER_TICKS_TIMEOUT);
    APP_ERROR_CHECK(err_code);
    nrf_csense_instance_context_set(&m_button, (void*)&touched_counter);
    //nrf_csense_instance_context_set(&m_slider, (void*)slider_handler);
    err_code = nrf_csense_add(&m_button);
    APP_ERROR_CHECK(err_code);
    //err_code = nrf_csense_add(&m_slider);
   // APP_ERROR_CHECK(err_code);
   NRF_LOG_INFO("CSENSE START FUNCTION EXECUTING HERE");
}

void bracelet_removed_flash(){
   if(bracelet_removed==1){
     pwm_play();
     nrf_gpio_pin_set(LED_BLUE);
     nrf_gpio_pin_set(MOTOR_OUT);
     nrf_delay_ms(250);
     nrf_gpio_pin_clear(LED_BLUE);
     nrf_gpio_pin_clear(MOTOR_OUT);
     nrf_delay_ms(250);
     NRF_LOG_INFO("Bracelet is off");
   }
   else{
     if (bracelet_initial!=2){
        nrf_gpio_pin_set(LED_BLUE);
        nrf_gpio_pin_clear(SPEAKER_OUT);
        return;
     }
     else{
      return;
     }
   }
}

/////////////END OF TOUCH SENSOR FUNCTIONS////////////////////////



//////////////////MAIN FUNCTION//////////////////////////////////
int main(void)
{
    // SoC IC variables ////////////
    ret_code_t err_code;
    uint8_t address;
    uint8_t sample_data = 0x00;
    uint16_t config_capacity;
    uint16_t config_energy; 
    uint16_t config_term_voltage;
    ////////////////////////////////

    // System Init ////////////////
    log_init();
    APP_SCHED_INIT(APP_SCHED_MAX_EVENT_SIZE, APP_SCHED_QUEUE_SIZE);
    timers_init();
    leds_init();
    power_management_init();
    ///////////////////////////////

    // BLE Init ///////////////////
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
    ///////////////////////////////
    
    // SoC IC Init and Config setup /////////////////////////////////////////////////////
    /*twi_init(); 

    NRF_LOG_INFO("All Init functions completed. Application Started.");
    NRF_LOG_INFO("Setting battery config parameters.");
    NRF_LOG_FLUSH();
   
    set_starting_config();

    NRF_LOG_INFO("Completed parameter update process.");
    NRF_LOG_INFO("Reading the new saved values.");
    NRF_LOG_FLUSH();
    
    // This function performs the code below until end of SoC init and config setup.
    // It doesn't pass back the values properly right now.
    // get_battery_config_settings(&config_capacity, &config_energy, &config_term_voltage);
    char state_buffer[STATE_SUB_LEN];
    uint16_t result;
    
    unseal_gauge();
    // Read out all the new values
    config_capacity = gauge_cmd_read(NULL, GET_DESIGN_CAPACITY);
    result = gauge_read_data_class(NULL, STATE_SUB, state_buffer, STATE_SUB_LEN);
    shift_register(state_buffer, STATE_SUB_LEN); // have to shift because values are returned shifted one left

    if(result == -1){
        NRF_LOG_INFO("reading from state data class failed");
        NRF_LOG_FLUSH();
    }

    uint8_t copy_array[2];
    copy_array[0] = state_buffer[DESIGN_EGY_OFFSET + 1];
    copy_array[1] = state_buffer[DESIGN_EGY_OFFSET];
    memcpy(&config_energy, copy_array,  sizeof(config_energy));
    
    copy_array[0] = state_buffer[TERM_VOLT_OFFSET + 1];
    copy_array[1] = state_buffer[TERM_VOLT_OFFSET];
    memcpy(&config_term_voltage, copy_array,  sizeof(config_term_voltage));
    
    gauge_control(NULL, SEAL, 0);
    // Done reading out the new values

    NRF_LOG_INFO("New config parameters:\nDesign Capacity: %d mAh\nDesign Energy: %d\nTerminate Voltage: %d mV", 
                  config_capacity, config_energy, config_term_voltage);
    NRF_LOG_FLUSH();

    battery_percent = gauge_cmd_read(NULL, GET_BATT_PCT);
    NRF_LOG_INFO("Battery percent successfully read at %d%", battery_percent);
    NRF_LOG_FLUSH();*/
    ///// End SoC Init and Config setup ////////////////////////////////////////////////

    NRF_LOG_INFO("BLE Base Application started!");

    advertising_start();

    // NFC Init and Setup /////////////////////////////////////////////////////////////

    /* Initialize FDS. */
    err_code = ndef_file_setup();
    APP_ERROR_CHECK(err_code);

    /* Load NDEF message from the flash file. */
    err_code = ndef_file_load(m_ndef_msg_buf, sizeof(m_ndef_msg_buf));
    APP_ERROR_CHECK(err_code);

    // Restore default NDEF message.
    if (bsp_board_button_state_get(APP_DEFAULT_BTN))
    {
        uint32_t size = sizeof(m_ndef_msg_buf);
        err_code = ndef_file_default_message(m_ndef_msg_buf, &size);
        APP_ERROR_CHECK(err_code);
        err_code = ndef_file_update(m_ndef_msg_buf, NDEF_FILE_SIZE);
        APP_ERROR_CHECK(err_code);
        NRF_LOG_DEBUG("Default NDEF message restored!");
    }

    /* Set up NFC */
    err_code = nfc_t4t_setup(nfc_callback, NULL);
    APP_ERROR_CHECK(err_code);

    /* Run Read-Write mode for Type 4 Tag platform */
    err_code = nfc_t4t_ndef_rwpayload_set(m_ndef_msg_buf, sizeof(m_ndef_msg_buf));
    APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("Writable NDEF message example started.");

    /* Start sensing NFC field */
    err_code = nfc_t4t_emulation_start();
    APP_ERROR_CHECK(err_code);

    /////////////////////// TOUCH SENSOR PART /////////////////////////////////////////
    //timer_init();
    //err_code = clock_config();
    //APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("Capacitive sensing library example started.");
    csense_start();

    //while(1){
      //bracelet_removed_flash();
      //__WFI();
     // NRF_LOG_FLUSH();
    //}

    //err_code = app_sched_event_put(NULL, 0, scheduler_touch_sensor_handler);
    //APP_ERROR_CHECK(err_code);

    ////////////////////////////// END TOUCH SENSOR PART //////////////////////////////

    NRF_LOG_INFO("BLAH BLAH BLAH");

    // Once Bluetooth is connected and color properly configured, disable NFC using
    //nfc_t4t_emulation_stop();
    //nfc_t4t_done();

   /* while (1)
    {
        app_sched_execute();

        NRF_LOG_FLUSH();
        __WFE();
    }*/

    bsp_board_led_on(MOTOR_OUT_IDX);

    // Enter main loop.
    for(;;)
    {
        idle_state_handle();
    }


    ////////////////////////// Local Alert Config and Setup ////////////////////////////
    pwm_common_init();
    bsp_board_init(BSP_INIT_LEDS);
    pwm_play();
    NRF_LOG_INFO("PWM application started");
    nrf_gpio_cfg_output(LED_BLUE); //configures pin as output
    nrf_gpio_cfg_output(LED_YELLOW);
    nrf_gpio_cfg_output(LED_RED);
    nrf_gpio_cfg_output(LED_GREEN);
    nrf_gpio_cfg_output(LED_ORANGE);
    nrf_gpio_cfg_output(LED_PURPLE);
    nrf_gpio_cfg_output(MOTOR_OUT);
    nrf_gpio_cfg_output(SPEAKER_OUT);
    /////////////////////// End Local Alert Config and Setup ///////////////////////////


    //////////////////// EMERGENCY ALERT PART /////////////////////////////////////////
    bool emergency_flag = false;
    int flag_flipper = 0;
    while(emergency_flag == true){
      for(int m = 0; m < 1000; m++){
        if(flag_flipper == 0){
          err_code = app_sched_event_put(NULL, 0, scheduler_emergency_alert_ON_handler);
          APP_ERROR_CHECK(err_code);
          flag_flipper = 1;
        }
        else{
          err_code = app_sched_event_put(NULL, 0, scheduler_emergency_alert_OFF_handler);
          APP_ERROR_CHECK(err_code);
          flag_flipper = 0;
        }

        nrf_delay_ms(750);
      }
    }
    ///////////////////// END EMERGENCY ALERT PART /////////////////////////////////////
}