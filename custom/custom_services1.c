#include "custom_services1.h"
#include "sdk_common.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "app_timer.h"
#include "ble_conn_state.h"

extern uint8_t received_val;

APP_TIMER_DEF(m_background_distance_timer_id);

uint32_t ble_identify_init(ble_identify_t * p_identify, const ble_identify_init_t * p_identify_init){
    if(p_identify == NULL || p_identify_init == NULL){
        return NRF_ERROR_NULL;
    }

    uint32_t err_code;
    ble_uuid_t ble_uuid;

    p_identify->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_identify->evt_handler = p_identify_init->evt_handler;

    ble_uuid128_t base_uuid = {IDENTIFY_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_identify->uuid_type);
    VERIFY_SUCCESS(err_code);
    ble_uuid.type = p_identify->uuid_type;
    ble_uuid.uuid = IDENTIFY_UUID_SERVICE;
    
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_identify->service_handle);
    if (err_code != NRF_SUCCESS){
        return err_code;
    }
    
    err_code = bracelet_char_add(p_identify, p_identify_init, (ble_uuid128_t){CONFIG_UUID_BASE}, CONFIG_UUID_CHAR, 0);
    APP_ERROR_CHECK(err_code);
    
    err_code = bracelet_char_add(p_identify, p_identify_init, (ble_uuid128_t){CAPSENSE_UUID_BASE}, CAPSENSE_UUID_CHAR, 1);
    APP_ERROR_CHECK(err_code);

    return err_code;
}

static ret_code_t bracelet_char_add(ble_identify_t * p_identify, const ble_identify_init_t * p_identify_init, ble_uuid128_t char_base_uuid, uint16_t char_uuid, uint8_t char_index){
    ret_code_t err_code;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    // CCCD metadata giving read and write permissions
    memset(&cccd_md, 0, sizeof(cccd_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK; // put memory on the soft device stack, not application stack.
    
    // characteristic metadata
    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props.read          = 1;
    char_md.char_props.write_wo_resp = 1;
    char_md.char_props.notify        = 1;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    //char_md.p_cccd_md                = NULL;
    char_md.p_cccd_md                = &cccd_md;
    char_md.p_sccd_md                = NULL;
    

    ble_uuid128_t base_uuid = char_base_uuid;
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_identify->uuid_type);
    APP_ERROR_CHECK(err_code);

    ble_uuid.type = p_identify->uuid_type;
    ble_uuid.uuid = char_uuid;
    
    // attributes metadata
    memset(&attr_md, 0, sizeof(attr_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 1;

    // Configure the characteristic value
    memset(&attr_char_value, 0, sizeof(attr_char_value));
    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = sizeof(uint8_t);
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = sizeof(uint8_t);
    attr_char_value.p_value      = NULL;

    return sd_ble_gatts_characteristic_add(p_identify->service_handle, &char_md, &attr_char_value, &p_identify->value_handles[char_index]);
}

static void on_write(ble_identify_t * p_identify, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t * p_evt_write = (ble_gatts_evt_write_t *) &p_ble_evt->evt.gatts_evt.params.write;
    
    for(int i = 0; i < NUM_CHARACTERISTICS; i++){
        if ((p_evt_write->handle == p_identify->value_handles[i].value_handle) &&
            (p_evt_write->len == 1))
        {
          // Handle what happens on a write event to the characteristic value
            received_val = p_evt_write->data[0];

            //if(received_val == TRACKING_STARTED || received_val == START_EMERGENCY_ALERT || received_val == OUT_OF_RANGE\
            //   || received_val == IN_RANGE){
            if(received_val == TRACKING_STARTED || received_val == START_EMERGENCY_ALERT || received_val == STOP_EMERGENCY_ALERT\
               || received_val == OUT_OF_RANGE || received_val == IN_RANGE){
                p_identify->evt_handler(received_val); 
            }
            NRF_LOG_INFO("Received value from custom service characteristic. Val is: %d", received_val);
            NRF_LOG_FLUSH();
        }
        else if ((p_evt_write->handle == p_identify->value_handles[i].cccd_handle) &&
            (p_evt_write->len == 2)){

            ret_code_t err_code;
        
            if (ble_srv_is_notification_enabled(p_evt_write->data))
            {
                NRF_LOG_INFO("iPhone app moved to background");
            
                ble_identify_update(p_identify, BLE_CONN_HANDLE_ALL, BACKGROUND_DISTANCE_NOTIFICATION);
            }
            else
            {
                NRF_LOG_INFO("iPhone app moved to foreground");
            }
        }
    }
}

void ble_identify_on_evt(ble_identify_t * p_identify, ble_evt_t const * p_ble_evt){
    
    ble_identify_t * p_identify_local = p_identify;

    //NRF_LOG_INFO("BLE event received. Event type = %d\r\n", p_ble_evt->header.evt_id);

    if (p_identify_local == NULL || p_ble_evt == NULL)
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTS_EVT_WRITE:
            //NRF_LOG_INFO("IN CASE STATEMENT IN BLE IDENTIFY FUNCTION");
            on_write(p_identify_local, p_ble_evt);
            break;
            
        default:
            // No implementation needed.
            break;
    }
}

ret_code_t ble_identify_update(ble_identify_t * p_identify, uint16_t conn_handle, uint8_t type)
{
    if (p_identify == NULL)
    {
        return NRF_ERROR_NULL;
    }
    
    ret_code_t         err_code = NRF_SUCCESS;
    ble_gatts_value_t  gatts_value;

    uint8_t update_flag = type;
    uint8_t char_index;

    // Initialize value struct.
    memset(&gatts_value, 0, sizeof(gatts_value));

    gatts_value.len     = sizeof(uint8_t);
    gatts_value.offset  = 0;
    gatts_value.p_value = &update_flag;

    // Update database.
    if(type == BACKGROUND_DISTANCE_NOTIFICATION){
        char_index = 0;
    }
    else{
        char_index = 1;
    }
    err_code = sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID,
                                      p_identify->value_handles[char_index].value_handle,
                                      &gatts_value);
    if (err_code == NRF_SUCCESS)
    {
        switch(update_flag){
        case BACKGROUND_DISTANCE_NOTIFICATION:
            NRF_LOG_INFO("Sending Background Distance Notification.");
            break;
        case CAPSENSE_BRACELET_ON_NOTIFICATION:
            NRF_LOG_INFO("Sending Bracelet On Notification.");
            break;
        case CAPSENSE_BRACELET_REMOVED_NOTIFICATION:
            NRF_LOG_INFO("Sending Bracelet Removed Notification");
            break;
        }
    }
    else
    {
        NRF_LOG_DEBUG("Error during Custom Notification: 0x%08X", err_code);
        return err_code;
    }

    ble_gatts_hvx_params_t hvx_params;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_identify->value_handles[char_index].value_handle;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = gatts_value.offset;
    hvx_params.p_len  = &gatts_value.len;
    hvx_params.p_data = gatts_value.p_value;

    if (conn_handle == BLE_CONN_HANDLE_ALL)
    {
        ble_conn_state_conn_handle_list_t conn_handles = ble_conn_state_conn_handles();

        // Try sending notifications to all valid connection handles.
        for (uint32_t i = 0; i < conn_handles.len; i++)
        {
            if (ble_conn_state_status(conn_handles.conn_handles[i]) == BLE_CONN_STATUS_CONNECTED)
            {
                if (err_code == NRF_SUCCESS)
                {
                    err_code = background_distance_notification_send(&hvx_params,
                                                         conn_handles.conn_handles[i]);
                }
                else
                {
                    // Preserve the first non-zero error code
                    UNUSED_RETURN_VALUE(background_distance_notification_send(&hvx_params,
                                                                  conn_handles.conn_handles[i]));
                }
            }
        }
    }
    else
    {
        err_code = background_distance_notification_send(&hvx_params, conn_handle);
    }

    return err_code;
}

static ret_code_t background_distance_notification_send(ble_gatts_hvx_params_t * const p_hvx_params, uint16_t conn_handle)
{
    ret_code_t err_code = sd_ble_gatts_hvx(conn_handle, p_hvx_params);
    if (err_code == NRF_SUCCESS)
    {
        NRF_LOG_INFO("Background Distance Notification has been sent using conn_handle: 0x%04X", conn_handle);
    }
    else
    {
        NRF_LOG_DEBUG("Error: 0x%08X while sending Background Distance notification with conn_handle: 0x%04X",
                      err_code,
                      conn_handle);
    }
    return err_code;
}

ret_code_t add_background_distance_timer(void * timeout_handler){
    
    ret_code_t err_code;

    err_code = app_timer_create(&m_background_distance_timer_id, APP_TIMER_MODE_REPEATED, timeout_handler);
    APP_ERROR_CHECK(err_code);
}