#include "custom_services.h"
#include "sdk_common.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

extern uint8_t received_val;

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
    
    err_code = bracelet_config_char_add(p_identify, p_identify_init);
    return err_code;
}

static ret_code_t bracelet_config_char_add(ble_identify_t * p_identify, const ble_identify_init_t * p_identify_init){
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
    char_md.char_props.notify        = 0;
    char_md.p_char_user_desc         = NULL;
    char_md.p_char_pf                = NULL;
    char_md.p_user_desc_md           = NULL;
    //char_md.p_cccd_md                = NULL;
    char_md.p_cccd_md                = &cccd_md;
    char_md.p_sccd_md                = NULL;
    

    ble_uuid128_t base_uuid = {CONFIG_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_identify->uuid_type);
    APP_ERROR_CHECK(err_code);

    ble_uuid.type = p_identify->uuid_type;
    ble_uuid.uuid = CONFIG_UUID_CHAR;
    
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

    return sd_ble_gatts_characteristic_add(p_identify->service_handle, &char_md, &attr_char_value, &p_identify->value_handles);
}

static void on_write(ble_identify_t * p_identify, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t * p_evt_write = (ble_gatts_evt_write_t *) &p_ble_evt->evt.gatts_evt.params.write;
    uint8_t check[3] = {0,0,0};

    if((p_evt_write->handle == p_identify->value_handles.value_handle)){
        check[0] = 1;
    }
    if(p_evt_write->len == 1){
        check[1] = 1;
    }
    if(p_identify->evt_handler != NULL){
        check[2] = 1;
    }

    /*if ((p_evt_write->handle == p_identify->value_handles.value_handle) &&
        (p_evt_write->len == 1) &&
        (p_identify->evt_handler != NULL))
    {*/
    if ((p_evt_write->handle == p_identify->value_handles.value_handle) &&
        (p_evt_write->len == 1))
    {
      // Handle what happens on a write event to the characteristic value
        received_val = p_evt_write->data[0];
        NRF_LOG_INFO("Received value from custom service characteristic. Val is: %d", received_val);
        NRF_LOG_FLUSH();
    }
    else{
        NRF_LOG_INFO("Didn't get into if statement in on_write. check values are: %d, %d, %d", check[0], check[1], check[2]);
    }
}


void ble_identify_on_evt(ble_identify_t * p_identify, ble_evt_t const * p_ble_evt){
    
    ble_identify_t * p_identify_local = p_identify;

    NRF_LOG_INFO("BLE event received. Event type = %d\r\n", p_ble_evt->header.evt_id);

    if (p_identify_local == NULL || p_ble_evt == NULL)
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTS_EVT_WRITE:
            NRF_LOG_INFO("IN CASE STATEMENT IN BLE IDENTIFY FUNCTION");
            on_write(p_identify_local, p_ble_evt);
            break;
            
        default:
            // No implementation needed.
            break;
    }
}