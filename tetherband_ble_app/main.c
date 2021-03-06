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
#include "custom_services1.h"

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
#include "bsp_nfc.h"
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
#include "ble_tps.h"
#include "ble_ias.h"
#include "ble_lls.h"
/////////////////////////////

//Local Alerts Includes//////
#include "nrf_drv_pwm.h"
#include "bsp.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
/////////////////////////////

//Touch Sensor Includes /////
#include "nrf_csense.h"
#include "nrf_drv_timer.h"
/////////////////////////////

#include "nrf_drv_gpiote.h" // for power button


//PWM and Touch Sensor Parameters ////////////////////////////////////////////////////
#define m_top 1000
#define m_step 100
#define test_1 10
#define SPEAKER_OUT 12


/* Time between RTC interrupts. */
#define APP_TIMER_TICKS_TIMEOUT APP_TIMER_TICKS(50)

/* Scale range. */
#define RANGE                   50
#define THRESHOLD_BUTTON        800

/*lint -e19 -save */
NRF_CSENSE_BUTTON_DEF(m_button, (TOUCH_IN, THRESHOLD_BUTTON));

/*lint -restore*/
static nrfx_pwm_t m_pwm0 =NRFX_PWM_INSTANCE(0);
uint16_t step = m_top / m_step;
static nrf_pwm_values_common_t sequence_values[m_step * 4];//array to play a sequence
uint16_t value=0;
void bracelet_removed_flash();
void repeated_timer_handler();
uint8_t bracelet_removed = 0;
uint8_t bracelet_initial = 2;
static bool csense_started = false;

nrf_pwm_sequence_t const seq0=
{
    .values.p_common = sequence_values,
    .length          = NRF_PWM_VALUES_LENGTH(sequence_values),
    .repeats         = 0,
    .end_delay       = 0
};

#define LOCAL_ALERT_QUEUE_SIZE 7
static uint8_t local_alert_queue[LOCAL_ALERT_QUEUE_SIZE]; // To decide which alert should be running 
static uint8_t alerts_started = 0; // flag to know if alerts are started

// END TOUCH SENSOR AND PWM ITEMS ///////////////////////////////////////////

// DK MAC address is DD:C8:1A:7E:E8:F5
// BLE Defines //////////////////////////////////////////////////////////////

#define APP_BLE_CONN_CFG_TAG                1 // used in advertising init
#define APP_BLE_OBSERVER_PRIORITY           3

// GAP Profile Parameters
#define DEVICE_NAME                         "Teth1"
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
#define BATTERY_LEVEL_MEAS_INTERVAL     APP_TIMER_TICKS(30000)
//#define CSENSE_MEAS_INTERVAL            APP_TIMER_TICKS(500)
#define LOCAL_ALERT_INTERVAL            APP_TIMER_TICKS(500)

#define INITIAL_LLS_ALERT_LEVEL         BLE_CHAR_ALERT_LEVEL_NO_ALERT // For link loss service     
#define TX_POWER_LEVEL                  (-4) // Sets the radio transmit power and is the value sent in TX power service used in distance determination.                                    
// End BLE Defines //////////////////////////////////////////////////////////


// App Scheduler Defines and NFC Defines ////////////////////////////////////
#define BLE_BAS_SCHED_EVENT_DATA_SIZE sizeof(ble_bas_evt_t)
#define EVENT_GROUP_1 MAX(APP_TIMER_SCHED_EVENT_DATA_SIZE, BLE_BAS_SCHED_EVENT_DATA_SIZE) // MAX macro only takes two arguments, so had to be done this way
#define EVENT_GROUP_2 MAX(sizeof(nrf_csense_evt_t), sizeof(nrf_pwr_mgmt_evt_t))
#define APP_SCHED_MAX_EVENT_SIZE MAX(EVENT_GROUP_1, EVENT_GROUP_2) /**< Maximum size of scheduler events. */
#define APP_SCHED_QUEUE_SIZE     30                  /**< Maximum number of events in the scheduler queue. */
//#define APP_DEFAULT_BTN          BSP_BOARD_BUTTON_0 /**< Button used to set default NDEF message. */
// End NFC Defines //////////////////////////////////////////////////////////


// Index of the power button, used with bsp button functions
#define BTN_ID_WAKEUP 0
#define BTN_ID_SLEEP  0 

// Module Instances /////////////////////////////////////////////////////////

NRF_BLE_QWR_DEF(m_qwr); // create queue writer object
NRF_BLE_GATT_DEF(m_gatt); // create gatt object
BLE_ADVERTISING_DEF(m_advert); // create advertising object
BLE_IDENTIFY_DEF(m_identify); // create custom identifying service instance
APP_TIMER_DEF(m_battery_timer_id); // setup app timer for battery level reads
APP_TIMER_DEF(m_local_alert_timer_id);
BLE_BAS_DEF(m_bas); // instance of Battery Level Wervice
BLE_TPS_DEF(m_tps); // TX Power Service instance                                 
BLE_IAS_DEF(m_ias, NRF_SDH_BLE_TOTAL_LINK_COUNT); // Immediate Alert Service instance       
BLE_LLS_DEF(m_lls); // Link Loss Service instance
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
static uint8_t color_config[19];
static bool nfc_stopped;
static char team_color;
static uint32_t team_color_index;
// End NFC globals ////////////////////////////////////////////////////////////////////


static uint8_t battery_percent; // updated by SoC IC, sent through battery service when notifications enabled.
static uint8_t last_batt_percent;
static uint16_t simulate_battery = 100;

// NFC Functions ///////////////////////////////////////////////////////////////////////////////////////////////

bool shutdown_handler(nrf_pwr_mgmt_evt_t event)
{
    uint32_t err_code;

    switch (event)
    {
        case NRF_PWR_MGMT_EVT_PREPARE_SYSOFF: // TO BE TRIGGERED WHEN BATTERY SUPER LOW
            NRF_LOG_INFO("NRF_PWR_MGMT_EVT_PREPARE_SYSOFF");
            err_code = bsp_buttons_disable();
            APP_ERROR_CHECK(err_code);
            break;

        case NRF_PWR_MGMT_EVT_PREPARE_WAKEUP:
           // NRF_LOG_INFO("NRF_PWR_MGMT_EVT_PREPARE_WAKEUP");
            err_code = bsp_buttons_disable();
            // Suppress NRF_ERROR_NOT_SUPPORTED return code.
            UNUSED_VARIABLE(err_code);

            err_code = bsp_nfc_sleep_mode_prepare();
            // Suppress NRF_ERROR_NOT_SUPPORTED return code.
            UNUSED_VARIABLE(err_code);
            break;
    }
    return true;
}

NRF_PWR_MGMT_HANDLER_REGISTER(shutdown_handler, 0);

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
    team_color = (char*)m_ndef_msg_buf[11]; 

    switch(team_color){
        case 'o':
            team_color_index = LED_ORANGE_IDX;
            NRF_LOG_INFO("Color Sent Over NFC Is: Orange");
            break;
        
        case 'p':
            team_color_index = LED_PURPLE_IDX;
            NRF_LOG_INFO("Color Sent Over NFC Is: Purple");
            break;

        case 'r':
            team_color_index = LED_RED_IDX;
            NRF_LOG_INFO("Color Sent Over NFC Is: Red");
            break;

        case 'y':
            team_color_index = LED_YELLOW_IDX;
            NRF_LOG_INFO("Color Sent Over NFC Is: Yellow");
            break;
        
        case 'b':
            team_color_index = LED_BLUE_IDX;
            NRF_LOG_INFO("Color Sent Over NFC Is: Blue");
            break;

        case 'g':
            team_color_index = LED_GREEN_IDX;
            NRF_LOG_INFO("Color Sent Over NFC Is: Green");
            break;
    }
    bsp_board_led_on(team_color_index);
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
            NRF_LOG_INFO("EVENT: NFC T4T EVENT FIELD ON TRIGGERED");
            break;

        case NFC_T4T_EVENT_FIELD_OFF:
            NRF_LOG_INFO("EVENT: NFC_T4T_EVENT_FIELD_OFF TRIGGERED");
            break;

        case NFC_T4T_EVENT_NDEF_READ:;
            NRF_LOG_INFO("EVENT: NFC_T4T_EVENT_NDEF_READ TRIGGERED");
            break;

        case NFC_T4T_EVENT_NDEF_UPDATED:
            NRF_LOG_INFO("EVENT: NFC_T4T_EVENT_NDEF_UPDATED TRIGGERED");
            if (dataLength > 0)
            {
                ret_code_t err_code;

                // Schedule update of NDEF message in the flash file.
                m_ndef_msg_len = dataLength;
                err_code       = app_sched_event_put(NULL, 0, scheduler_ndef_file_update);
                APP_ERROR_CHECK(err_code);
            }
            break;

        default:
            break;
    }
}

static void nfc_default_config(){
    ret_code_t err_code;
    
    err_code = ndef_file_load(m_ndef_msg_buf, sizeof(m_ndef_msg_buf));
    APP_ERROR_CHECK(err_code);

    /* Set up NFC */
    err_code = nfc_t4t_setup(nfc_callback, NULL);
    APP_ERROR_CHECK(err_code);

    /* Run Read-Write mode for Type 4 Tag platform */
    err_code = nfc_t4t_ndef_rwpayload_set(m_ndef_msg_buf, sizeof(m_ndef_msg_buf));
    APP_ERROR_CHECK(err_code);
    NRF_LOG_INFO("DEFAULT NFC VALUES SET");

    /* Start sensing NFC field */
    err_code = nfc_t4t_emulation_start();
    APP_ERROR_CHECK(err_code);

    nfc_stopped = false; // flag used to disable/enable nfc when phone disconnected or tracking started
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

// error handler used when services need to pass an error to the application
static void service_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

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

// used to perform actions given one of three alert levels from IAS or LLS.
static void alert_signal(uint8_t alert_level)
{
    ret_code_t err_code;

    switch (alert_level)
    {
        case BLE_CHAR_ALERT_LEVEL_NO_ALERT:
            NRF_LOG_INFO("No Alert.");
            err_code = bsp_indication_set(BSP_INDICATE_ALERT_OFF);
            APP_ERROR_CHECK(err_code);
            break; // BLE_CHAR_ALERT_LEVEL_NO_ALERT

        case BLE_CHAR_ALERT_LEVEL_MILD_ALERT:
            NRF_LOG_INFO("Mild Alert.");
            err_code = bsp_indication_set(BSP_INDICATE_ALERT_0);
            APP_ERROR_CHECK(err_code);
            break; // BLE_CHAR_ALERT_LEVEL_MILD_ALERT

        case BLE_CHAR_ALERT_LEVEL_HIGH_ALERT:
            NRF_LOG_INFO("HIGH Alert.");
            err_code = bsp_indication_set(BSP_INDICATE_ALERT_3);
            APP_ERROR_CHECK(err_code);
            break; // BLE_CHAR_ALERT_LEVEL_HIGH_ALERT

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for initializing the TX Power Service.
 */
static void tps_init(void)
{
    ret_code_t     err_code;
    ble_tps_init_t tps_init_obj;

    memset(&tps_init_obj, 0, sizeof(tps_init_obj));
    tps_init_obj.initial_tx_power_level = TX_POWER_LEVEL;

    tps_init_obj.tpl_rd_sec = SEC_OPEN;

    err_code = ble_tps_init(&m_tps, &tps_init_obj);
    APP_ERROR_CHECK(err_code);
}

static void on_lls_evt(ble_lls_t * p_lls, ble_lls_evt_t * p_evt)
{
    switch (p_evt->evt_type)
    {
        case BLE_LLS_EVT_LINK_LOSS_ALERT:
            alert_signal(p_evt->params.alert_level);
            break; // BLE_LLS_EVT_LINK_LOSS_ALERT

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for initializing the Link Loss Service.
 */
static void lls_init(void)
{
    ret_code_t     err_code;
    ble_lls_init_t lls_init_obj;

    // Initialize Link Loss Service
    memset(&lls_init_obj, 0, sizeof(lls_init_obj));

    lls_init_obj.evt_handler         = on_lls_evt;
    lls_init_obj.error_handler       = service_error_handler;
    lls_init_obj.initial_alert_level = INITIAL_LLS_ALERT_LEVEL;

    lls_init_obj.alert_level_rd_sec = SEC_OPEN;
    lls_init_obj.alert_level_wr_sec = SEC_OPEN;

    err_code = ble_lls_init(&m_lls, &lls_init_obj);
    APP_ERROR_CHECK(err_code);
}

static void on_ias_evt(ble_ias_t * p_ias, ble_ias_evt_t * p_evt)
{
    switch (p_evt->evt_type)
    {
        case BLE_IAS_EVT_ALERT_LEVEL_UPDATED:
            if (p_evt->p_link_ctx != NULL)
            {
                alert_signal(p_evt->p_link_ctx->alert_level);
            }
            break; // BLE_IAS_EVT_ALERT_LEVEL_UPDATED

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for initializing the Immediate Alert Service.
 */
static void ias_init(void)
{
    ret_code_t     err_code;
    ble_ias_init_t ias_init_obj;

    memset(&ias_init_obj, 0, sizeof(ias_init_obj));
    ias_init_obj.evt_handler  = on_ias_evt;

    ias_init_obj.alert_wr_sec = SEC_OPEN;

    err_code = ble_ias_init(&m_ias, &ias_init_obj);
    APP_ERROR_CHECK(err_code);
}

// runs when the battery measurement timer expires every BATTERY_LEVEL_MEAS_INTERVAL (ms).
// if client has notifications enabled for this service it will get update of battery level.
static void battery_level_meas_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    // REMOVE THIS AFTER TESTING WITH SOC IC AND BATTERY CONNECTED 
    /*if(simulate_battery != 0){
        simulate_battery -= 1;
    }*/
    //NRF_LOG_INFO("SIMULATE BATTERY LEVEL = %d", simulate_battery);
    
    //uint8_t percentage_batt_lvl = simulate_battery;
    
    battery_percent = get_battery_pct();
    NRF_LOG_INFO("Battery percent successfully read at %d%", battery_percent);
    NRF_LOG_FLUSH();

    ret_code_t err_code;
    if(battery_percent != last_batt_percent){
        NRF_LOG_INFO("BATTERY LEVEL CHANGED, UPDATING VALUE.");
        err_code = ble_bas_battery_level_update(&m_bas, battery_percent, BLE_CONN_HANDLE_ALL);
        APP_ERROR_CHECK(err_code);
    }
    last_batt_percent = battery_percent; // save value for next battery update
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
            battery_level_meas_timeout_handler(NULL);
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


//COMMENT OUT OR DELETE
// Handles distance notifications when iPhone app is put in background mode (locked or app switched)
static void background_distance_timeout_handler( ){
    ret_code_t err_code;
    
    err_code = ble_identify_background_distance_update(&m_identify, BLE_CONN_HANDLE_ALL);
    APP_ERROR_CHECK(err_code);
}

// Plays the speaker
static void pwm_play(void){
    (void)nrfx_pwm_simple_playback(&m_pwm0, &seq0, 1, NRFX_PWM_FLAG_LOOP);
}

// Stop the speaker
static void pwm_stop(void){
    (void)nrfx_pwm_simple_playback(&m_pwm0, &seq0, 1, NRFX_PWM_FLAG_STOP);
}

static void shift_queue_left(){
    for(int i = 0; i < LOCAL_ALERT_QUEUE_SIZE - 1; i++){
        local_alert_queue[i] = local_alert_queue[i+1];
    }
    local_alert_queue[LOCAL_ALERT_QUEUE_SIZE - 1] = 0;
}

// priority queue where the only values that are held are odd values. Higher number means higher priority
// Even priority values serve to clear the odd value that is just below it. Basically acts like a toggle switch
// for even/odd numbers adjacent to each other.
static void add_to_queue(uint8_t val){
    uint8_t prev_max_prio_event = local_alert_queue[0];
    if(val > prev_max_prio_event){
        if((val - prev_max_prio_event == 1) && (val % 2 == 0)){ // if current event is negating highest priority event (like turning off emergency alert)
            shift_queue_left();
            if(local_alert_queue[0] % 2 == 0 && local_alert_queue[0] != 0){ // Next event in queue needs to be cleared
                NRF_LOG_INFO("MOD BLOCK HIT")
                shift_queue_left();
                shift_queue_left();
            }
        }
        else{
            for(int i = LOCAL_ALERT_QUEUE_SIZE - 1; i > 0; i--){
                local_alert_queue[i] = local_alert_queue[i-1]; // shift everything down one
            }
            local_alert_queue[0] = val; // put highest value on top
        }
    }
    else{
        for(int i = 1; i < LOCAL_ALERT_QUEUE_SIZE - 1; i--){
            if(val > local_alert_queue[i]){ // if new value greater than current value
                for(int j = LOCAL_ALERT_QUEUE_SIZE - 1; j > i; j--){
                    local_alert_queue[j] = local_alert_queue[j-1]; // shift all down one
                }
                local_alert_queue[i] = val; // put new value in current value position
                break;
            }
        }
    }
    /*for(int i = 0; i < LOCAL_ALERT_QUEUE_SIZE; i++){
                    NRF_LOG_INFO("LOCAL ALERT QUEUE INDEX %d is %d", i, local_alert_queue[i]);
                    //NRF_LOG_FLUSH();
                }*/
}

// Adds to the local alert priority queue and then responds to the first thing on the queue.
static void local_alert(uint8_t priority){
    add_to_queue(priority);
    uint8_t current_max_priority = local_alert_queue[0];
    if(current_max_priority == 0){
        app_timer_stop(m_local_alert_timer_id);
        if(!nrfx_pwm_is_stopped(&m_pwm0)){
            pwm_stop();
        }
        alerts_started = 0;
        bsp_board_led_on(team_color_index); // turn team color back on
    }
    else{
        if(alerts_started == 0){
            alerts_started = 1;
            bsp_board_led_off(team_color_index); // turn off team color before starting alerts.
            NRF_LOG_INFO("in alerts started block");
            app_timer_start(m_local_alert_timer_id, LOCAL_ALERT_INTERVAL, NULL);
        }
    }
}


static void execute_local_alert(){
    uint8_t current_max_priority = local_alert_queue[0];
    switch(current_max_priority){
        case POWER_OFF:
            NRF_LOG_INFO("GOING INTO SYSTEM OFF MODE");
            NRF_LOG_FLUSH();
            app_timer_stop_all();
            nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
            break;
        
        case START_EMERGENCY_ALERT:           
            if(nrfx_pwm_is_stopped(&m_pwm0)){
                pwm_play();
            }
            nrf_gpio_pin_set(MOTOR_OUT);
            nrf_gpio_pin_set(LED_BLUE);
            nrf_delay_ms(70);
            nrf_gpio_pin_clear(LED_BLUE);
            nrf_gpio_pin_set(LED_YELLOW);
            nrf_delay_ms(70);
            nrf_gpio_pin_clear(LED_YELLOW);
            nrf_gpio_pin_set(LED_RED);
            nrf_delay_ms(70);
            nrf_gpio_pin_clear(LED_RED);
            nrf_gpio_pin_set(LED_GREEN);
            nrf_delay_ms(70);
            nrf_gpio_pin_clear(LED_GREEN);
            nrf_gpio_pin_set(LED_ORANGE);
            nrf_delay_ms(70);
            nrf_gpio_pin_clear(LED_ORANGE);
            nrf_gpio_pin_set(LED_PURPLE);
            nrf_delay_ms(70);
            nrf_gpio_pin_clear(LED_PURPLE);
            nrf_gpio_pin_clear(MOTOR_OUT);
            nrf_delay_ms(70);
            break;

        case OUT_OF_RANGE:
            if(nrfx_pwm_is_stopped(&m_pwm0)){
                pwm_play();
            }
            nrf_gpio_pin_set(MOTOR_OUT);
            nrf_gpio_pin_set(LED_RED);
            nrf_delay_ms(200);
            nrf_gpio_pin_clear(LED_RED);
            nrf_gpio_pin_clear(MOTOR_OUT);
            break;

        case BRACELET_REMOVED:
            if(nrfx_pwm_is_stopped(&m_pwm0)){
                pwm_play();
            }
            nrf_gpio_pin_set(LED_BLUE);
            nrf_delay_ms(200);
            nrf_gpio_pin_clear(LED_BLUE);
            nrf_delay_ms(200);
            break;
    }
}

/**
 * @brief Event handler for Capacitive Sensor High module.
 *
 * @param [in] p_evt_type                    Pointer to event data structure.
 */
void nrf_csense_handler(nrf_csense_evt_t * p_evt)
{
       ret_code_t err_code;

       switch (p_evt->nrf_csense_evt_type)
       {
        case NRF_CSENSE_BTN_EVT_PRESSED:
            NRF_LOG_INFO("Bracelet is on");
            
            if(alerts_started == 1){ // have to have removed it first before triggering this.
                local_alert(BRACELET_ON);
            }

            ble_identify_update(&m_identify, BLE_CONN_HANDLE_ALL, CAPSENSE_BRACELET_ON_NOTIFICATION); // send notification to app
            break;
          
        case NRF_CSENSE_BTN_EVT_RELEASED:

            NRF_LOG_INFO("BRACELET REMOVED");
            local_alert(BRACELET_REMOVED);

            ble_identify_update(&m_identify, BLE_CONN_HANDLE_ALL, CAPSENSE_BRACELET_REMOVED_NOTIFICATION); // send notification to app
            break;
        
        default:
            NRF_LOG_WARNING("Unknown event.");
            break;
    }
}

/**
 * @brief Function for starting Capacitive Sensor High module.
 *
 * Function enables one slider and one button.
 */
static void csense_start(void)
{
    ret_code_t err_code;
    
    err_code = nrf_csense_init(nrf_csense_handler, APP_TIMER_TICKS_TIMEOUT);
    APP_ERROR_CHECK(err_code);
    
    err_code = nrf_csense_add(&m_button);
    APP_ERROR_CHECK(err_code);

    csense_started = true;
}

// Handles written values to the custom identify uuid
static void on_identify_evt(uint8_t event_flag){
    switch(event_flag){
        case TRACKING_STARTED:
            if(!nfc_stopped){
                nfc_t4t_emulation_stop(); // disable nfc to minimize interference with ble
                nfc_stopped = true;
            }
            NRF_LOG_INFO("DISTANCE TRACKING STARTED, BEGINNING TOUCH SENSOR MONITORING");
            if(!csense_started){
                csense_start();
            }
            break;
        default:
            local_alert(event_flag);
            break;
    }
}

// Step 9: Init services with queue writer
static void services_init(void)
{
    ret_code_t err_code;

    nrf_ble_qwr_init_t qwr_init = {0};
    ble_identify_init_t identify_init;

    qwr_init.error_handler = nrf_qwr_error_handler;
    memset(&identify_init, 0 , sizeof(identify_init));
    identify_init.evt_handler = on_identify_evt;
    
    err_code = ble_identify_init(&m_identify, &identify_init);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    bas_init(); // setting up all bas init objects done in this function
    tps_init(); // Transmit power service
    ias_init(); // Immediate alert service
    lls_init(); // Link loss service
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
            nrf_gpio_pin_set(LED_BLUE);
            if(nfc_stopped){
                nfc_default_config(); // set up nfc field again if it was turned off during distance tracking
            }
            NRF_LOG_INFO("DEVICE DISCONNECTED!!!!!!!!!!!!!!!!!");
            break;

        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Device has been connected!");
            nrf_gpio_pin_clear(LED_BLUE);
      
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
           // NRF_LOG_INFO("IN EVENT WRITE CASE IN MAIN");
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

// handler for idle power state
static void idle_state_handle(void)
{
    app_sched_execute();
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

    err_code = app_timer_create(&m_local_alert_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                execute_local_alert);
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

// Change the transmit power to value defined in TX_POWER_LEVEL
static void tx_power_set(void)
{
    ret_code_t err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, m_advert.adv_handle, TX_POWER_LEVEL);
    APP_ERROR_CHECK(err_code);
}

///// Local Alert Handler ////////////////////////////////////////////////////////
static void pwm_common_init(void)//assigning array values
{

   for(int i = 0; i<m_step; i++)
   {
     value += step; //1st step:0 +100=100...2nd:100+100=200...so on
     sequence_values[i]=value;
     sequence_values[m_step+i] = m_top - value; //sequence_values[100+1] = 1,000 -100=990...so on
   }

   nrfx_pwm_config_t const config0=
   {
    .output_pins=
    {
       SPEAKER_OUT,
       NRFX_PWM_PIN_NOT_USED,
       NRFX_PWM_PIN_NOT_USED,
     },
     .irq_priority= APP_IRQ_PRIORITY_LOWEST,
     .base_clock = NRF_PWM_CLK_2MHz,
     .count_mode = NRF_PWM_MODE_UP,
     .top_value = m_top,
     .load_mode = NRF_PWM_LOAD_COMMON,
     .step_mode = NRF_PWM_STEP_AUTO
      };

      APP_ERROR_CHECK(nrfx_pwm_init(&m_pwm0, &config0,NULL));
}

/**@brief Function for application main entry.
 */
int main(void)
{
    // SoC IC variables ////////////
    ret_code_t err_code;
    uint8_t address;
    uint8_t sample_data = 0x00;
    uint16_t config_capacity;
    uint16_t config_energy; 
    uint16_t config_term_voltage;
    uint16_t config_taper_rate;
    ////////////////////////////////

    // System Init ////////////////
    log_init();
    APP_SCHED_INIT(APP_SCHED_MAX_EVENT_SIZE, APP_SCHED_QUEUE_SIZE);
    timers_init();
    leds_init();
    power_management_init();
    pwm_common_init();
    bsp_button_config();
    nrf_gpio_cfg_output(MOTOR_OUT);
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
    twi_init(); 

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
    memcpy(&config_energy, copy_array, sizeof(config_energy));
    
    copy_array[0] = state_buffer[TERM_VOLT_OFFSET + 1];
    copy_array[1] = state_buffer[TERM_VOLT_OFFSET];
    memcpy(&config_term_voltage, copy_array, sizeof(config_term_voltage));

    copy_array[0] = state_buffer[TAPER_RATE_OFFSET + 1];
    copy_array[1] = state_buffer[TAPER_RATE_OFFSET];
    memcpy(&config_taper_rate, copy_array, sizeof(config_taper_rate));
    
    gauge_control(NULL, SEAL, 0);
    // Done reading out the new values

    NRF_LOG_INFO("New config parameters:\nDesign Capacity: %d mAh\nDesign Energy: %d\nTerminate Voltage: %d mV\nTaper Rate: %d", 
                  config_capacity, config_energy, config_term_voltage, config_taper_rate);
    NRF_LOG_FLUSH();
    ///// End SoC Init and Config setup ////////////////////////////////////////////////

    // NFC Init and Setup /////////////////////////////////////////////////////////////
    err_code = ndef_file_setup();
    APP_ERROR_CHECK(err_code);

    nfc_default_config();
    ///// End NFC Init and Config setup ////////////////////////////////////////////////

    // zero out the local alert queue
    memset(local_alert_queue, 0, sizeof(local_alert_queue));

    advertising_start();
    tx_power_set();
    NRF_LOG_INFO("BLE Base Application started!");

    nrf_gpio_pin_set(LED_BLUE);

    // Enter main loop.
    for(;;)
    {
        idle_state_handle();
    }
}
