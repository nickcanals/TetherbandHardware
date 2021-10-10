//Battery Gauge Library
//V1.0
//© 2016 Texas Instruments Inc.
#include <string.h>
#include <stdlib.h>

#include "gauge.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_delay.h"

#define SET_CFGUPDATE 0x0013
#define CMD_DATA_CLASS 0x3E
#define CMD_BLOCK_DATA 0x40
#define CMD_CHECK_SUM 0x60
#define CMD_FLAGS 0x06
#define CFGUPD 0x0010

#define TWI_SCL 3
#define TWI_SDA 4
#define TWI_INSTANCE_ID  0
#define GAUGE_ADDRESS 0x55

#define STATE_SUBCLASS_0_DEFAULT 0x40 // has to be reset the first time update_config_parameter() is called
                                        // because read_data_block() returns the register shifted one to left.

const char control_status_keys[16][11] = {{"RSVD"},{"VOK"},{"RUP_DIS"},{"LDMD"},{"SLEEP"},{"RSVD"},{"HIBERNATE"},{"INITCOMP"}, // low byte, LSB first
                                          {"RES_UP"},{"QMAX_UP"},{"BCA"},{"CCA"},{"CALMODE"},{"SS"},{"WDRESET"},{"SHUTDOWNEN"}}; // high byte LSB first

const char flags_keys[16][11] = {{"DSG"},{"SOCF"},{"SOC1"},{"BAT_DET"},{"CFGUPMODE"},{"ITPOR"},{"RSVD"},{"OCVTAKEN"},
                                 {"CHG"},{"FC"},{"RSVD"},{"RSVD"},{"RSVD"},{"RSVD"},{"UT"},{"OT"}};

const char op_config_keys[16][11] = {{"TEMPS"},{"RSVD0"},{"BATLOWEN"},{"RSVD1"},{"RMFCC"},{"SLEEP"},{"RSVD1"},{"RSVD1"},
                                     {"RSVD1"},{"RSVD0"},{"RSVD1"},{"GPIOPOL"},{"BI_PU_EN"},{"BIE"},{"RSVD0"},{"RSVD0"}};


static volatile bool xfer_done = false;
const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

void twi_handler(nrf_drv_twi_evt_t const * p_event, void * p_context)
{
    switch (p_event->type)
    {
        case NRF_DRV_TWI_EVT_DONE:
            xfer_done = true;
            break;
        default:
            break;
    }
}

void twi_init(void)
{
    ret_code_t err_code;
    const nrf_drv_twi_config_t twi_config = {
        .scl = TWI_SCL,
        .sda = TWI_SDA,
        .frequency = NRF_DRV_TWI_FREQ_100K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH, //will need to be changed when soft device is implemented.
        .clear_bus_init = false
    };

    err_code = nrf_drv_twi_init(&m_twi, &twi_config, twi_handler, NULL);
    APP_ERROR_CHECK(err_code);

    nrf_drv_twi_enable(&m_twi);
}



//gauge_read: read bytes from gauge (must be implemented for a specific system)
//pHandle: handle to communications adapater
//nRegister: first register (=standard command) to read from
//pData: pointer to a data buffer
//nLength: number of bytes
//return value: number of bytes read (0 if error)
unsigned int gauge_read(void * pHandle, unsigned char nRegister, unsigned char * pData, unsigned char nLength)
{
    ret_code_t err_code;
    xfer_done = false;

    err_code = nrf_drv_twi_tx(&m_twi, GAUGE_ADDRESS, (uint8_t *)&nRegister, 1, true);
    while(xfer_done == false){}

    if(NRF_SUCCESS != err_code)
    {
        return 0;
    }

    xfer_done = false;
    err_code = nrf_drv_twi_rx(&m_twi, GAUGE_ADDRESS, pData, nLength);
    while(xfer_done == false){}

    return sizeof(pData);
};

//gauge_read: write bytes to gauge (must be implemented for a specific system)
//pHandle: handle to communications adapater
//nRegister: first register (=standard command) to write to
//pData: pointer to a data buffer
//nLength: number of bytes
//return value: number of bytes written (0 if error)
unsigned int gauge_write(void * pHandle, unsigned char nRegister, unsigned char * pData, unsigned char nLength)
{
    ret_code_t err_code;
    xfer_done = false;
    
    uint8_t tx_buffer[1 + nLength];
    // all data must be sent in one transfer, so combine standard command and 2 byte subcommand into same buffer address space
    memcpy(tx_buffer, &nRegister, 1);
    memcpy(tx_buffer + 1, pData, nLength);
    
/*    for(int i = 0; i < 1+nLength; i++)
    {
        if(i==0){
            NRF_LOG_INFO("tx_buffer[%d] is: 0x%x", i, tx_buffer[i]);
        }
        else
        {
            NRF_LOG_INFO("tx_buffer[%d] is: 0x%x, pData[%d] is: 0x%x", i, tx_buffer[i], i-1, pData[i-1]);
        }
        NRF_LOG_PROCESS();
    }
    NRF_LOG_FLUSH();*/
    err_code = nrf_drv_twi_tx(&m_twi, GAUGE_ADDRESS, tx_buffer, nLength + 1, false);
    while(xfer_done == false){}

    if(NRF_SUCCESS != err_code)
    {
        NRF_LOG_DEBUG("Error in gauge write function.");
        return 0;
    }
    return nLength;
};

//gauge_address: set device address for gauge (must be implemented for a specific system; notrequiredfor HDQ)
//pHandle: handle to communications adapater
//nAddress: device address (e.g. 0xAA)
extern void gauge_address(void * pHandle, unsigned char nAddress);

//gauge_control: issues a sub command
//pHandle: handle to communications adapter
//nSubCmd: sub command number
//return value: result from sub command

unsigned int gauge_control(void * pHandle, unsigned int nSubCmd, uint8_t control_label) {
    unsigned int nResult = 0;
    char pData[2];
    pData[0] = nSubCmd & 0xFF;
    pData[1] = (nSubCmd >> 8) & 0xFF;
    gauge_write(pHandle, control_label, pData, 2); // issue control and sub command
    gauge_read(pHandle, control_label, pData, 2); // read data
    nResult = ((pData[1] << 8) | pData[0]);
    return nResult;
}

//gauge_cmd_read: read data from standard command
//pHandle: handle to communications adapter
//nCmd: standard command
//return value: result from standard command
unsigned int gauge_cmd_read(void * pHandle, unsigned char nCmd) {
    unsigned char pData[2];
    gauge_read(pHandle, nCmd, pData, 2);
    return ((pData[1] << 8) | pData[0]);
}

//gauge_cmd_write: write data to standard command
//pHandle: handle to communications adapter
//nCmd: standard command
//return value: number of bytes written to gauge
unsigned int gauge_cmd_write(void * pHandle, unsigned char nCmd, unsigned int nData) {
    unsigned char pData[2];
    pData[0] = nData & 0xFF;
    pData[1] = (nData >> 8) & 0xFF;
    return gauge_write(pHandle, nCmd, pData, 2);
}

//gauge_cfg_update: enter configuration update mode for rom gauges
//pHandle: handle to communications adapter
//return value: true = success, false = failure
#define MAX_ATTEMPTS 5
bool gauge_cfg_update(void * pHandle) {
    unsigned int nFlags;
    int nAttempts = 0;
    gauge_control(pHandle, SET_CFGUPDATE, 0);
    do {
        nFlags = gauge_cmd_read(pHandle, CMD_FLAGS);
        if (!(nFlags & CFGUPD)) nrf_delay_us(500000); //usleep is linux function, not sure if this affects anything on the MCU side.
    } while (!(nFlags & CFGUPD) && (nAttempts++ < MAX_ATTEMPTS));
    return (nAttempts < MAX_ATTEMPTS);
}

//gauge_exit: exit configuration update mode for rom gauges
//pHandle: handle to communications adapter
//nSubCmd: sub command to exit configuration update mode
//return value: true = success, false = failure
bool gauge_exit(void * pHandle, unsigned int nSubCmd) {
    unsigned int nFlags;
    int nAttempts = 0;
    gauge_control(pHandle, nSubCmd, 0);
    do {
        nFlags = gauge_cmd_read(pHandle, CMD_FLAGS);
        if (nFlags & CFGUPD) nrf_delay_us(500000);
    } while ((nFlags & CFGUPD) && (nAttempts++ < MAX_ATTEMPTS));
    return (nAttempts < MAX_ATTEMPTS);
}

//gauge_read_data_class: read a data class
//pHandle: handle to communications adapter
//nDataClass: data class number
//pData: buffer holding the whole data class (all blocks)
//nLength: length of data class (all blocks)
//return value: 0 = success
int gauge_read_data_class(void * pHandle, unsigned char nDataClass, unsigned char * pData, unsigned char nLength) 
{
    unsigned char nRemainder = nLength;
    unsigned int nOffset = 0;
    unsigned char nDataBlock = 0x00;
    unsigned int nData;
    if (nLength < 1) return 0;
    do {
        nLength = nRemainder;
        if (nLength > 32) {
            nRemainder = nLength - 32;
            nLength = 32;
        } else nRemainder = 0;
        nData = (nDataBlock << 8) | nDataClass;
        gauge_cmd_write(pHandle, CMD_DATA_CLASS, nData);
        if (gauge_read(pHandle, CMD_BLOCK_DATA, pData, nLength) != nLength) return -1;
        pData += nLength;
        nDataBlock++;
    } while (nRemainder > 0);
    return 0;
}

//check_sum: calculate check sum for block transfer
//pData: pointer to data block
//nLength: length of data block
unsigned char check_sum(unsigned char * pData, unsigned char nLength) {
    unsigned char nSum = 0x00;
    unsigned char n;
    for (n = 0; n < nLength; n++)
        nSum += pData[n];
    nSum = 0xFF - nSum;
    return nSum;
}

//gauge_write_data_class: write a data class
//pHandle: handle to communications adapter
//nDataClass: data class number
//pData: buffer holding the whole data class (all blocks)
//nLength: length of data class (all blocks)
//return value: 0 = success
int gauge_write_data_class(void * pHandle, unsigned char nDataClass, unsigned char * pData, unsigned char nLength, uint8_t data_block) 
{
    unsigned char nRemainder = nLength;
    unsigned int nOffset = 0;
    unsigned char pCheckSum[2] = {
        0x00,
        0x00
    };
    unsigned int nData;
    unsigned char nDataBlock = 0x00;
    if (nLength < 1) return 0;
    do {
        nLength = nRemainder;
        if (nLength < 32) {
            nRemainder = nLength - 32;
            nLength = 32;
        } else nRemainder = 0;
        nData = (nDataBlock << 8) | nDataClass;
        gauge_cmd_write(pHandle, CMD_DATA_CLASS, nData);
        gauge_cmd_write(pHandle, SET_WRITE_DATA_BLOCK, data_block);
        if (gauge_write(pHandle, CMD_BLOCK_DATA, pData, nLength) != nLength) return -1;
        pCheckSum[0] = check_sum(pData, nLength);
        gauge_write(pHandle, CMD_CHECK_SUM, pCheckSum, 1);
        nrf_delay_us(10000);
        gauge_cmd_write(pHandle, CMD_DATA_CLASS, nData);
        gauge_read(pHandle, CMD_CHECK_SUM, pCheckSum + 1, 1);
        if (pCheckSum[0] != pCheckSum[1]) return -2;
        pData += nLength;
        nDataBlock++;
    } while (nRemainder > 0);
    return 0;
}

bool update_config_parameter(void * pHandle, uint16_t update_param, uint8_t offset, uint8_t data_block, uint8_t subclass, uint8_t subclass_len)
{
    uint16_t result;
    char pData[64]; // holds all the data in the whole subclass
    
    result = gauge_read_data_class(pHandle, subclass, pData, subclass_len); // get the pointer to the current info in subclass element
    
    if(result == -1){
        NRF_LOG_INFO("Reading from subclass: %d returned error -1. Inspect with debugger.", subclass);
        return false;
    }
    // Had to add in this function because gauge_read_data_class was consistently returning the contents
    // of the register shifted by one which when calling this funciton multiple times caused value errors.
    shift_register(pData, 64);

    pData[offset] = (update_param & 0xFF00) >>8; // write top byte of new data
    pData[offset + 1] = update_param & 0xFF; // write bottom byte of new data
    result = gauge_write_data_class(pHandle, subclass, pData, 32, data_block); // write the desired value to the subclass element
    
    if(result == -2){
        NRF_LOG_INFO("Writing to subclass: %d returned error -2. Expected checksum and new checksum do not match. Inspect with debugger.", subclass);
        return false;
    }
    
    return true; // successful update
}


void set_starting_config(){
    bool update_status;
    //uint16_t current_status;
    unseal_gauge();
    
    if(!gauge_cfg_update(NULL)){ // set to cfg update mode
        NRF_LOG_INFO("Failed to enter config update mode in set_starting_config()");
        NRF_LOG_FLUSH();
        return;
    }

    update_status = update_config_parameter(NULL, DESIGN_CAPACITY, DESIGN_CAP_OFFSET, OFFSET_0, STATE_SUB, STATE_SUB_LEN);
    if(!update_status){
        NRF_LOG_INFO("Failed to update Design Capacity.");
        NRF_LOG_FLUSH();
    }
    else{
        NRF_LOG_INFO("Design Capacity successfully updated.");
        NRF_LOG_FLUSH();
    }

    update_status = update_config_parameter(NULL, DESIGN_ENERGY, DESIGN_EGY_OFFSET, OFFSET_0, STATE_SUB, STATE_SUB_LEN);
    if(!update_status){
        NRF_LOG_INFO("Failed to update Design Energy.");
        NRF_LOG_PROCESS();
    }
    else{
        NRF_LOG_INFO("Design Energy successfully updated.");
        NRF_LOG_FLUSH();
    }

    update_status = update_config_parameter(NULL, TERMINATE_VOLTAGE, TERM_VOLT_OFFSET, OFFSET_0, STATE_SUB, STATE_SUB_LEN);
    if(!update_status){
        NRF_LOG_INFO("Failed to update Terminate Voltage.");
        NRF_LOG_FLUSH();
    }
    else{
        NRF_LOG_INFO("Terminate Voltage successfully updated.");
        NRF_LOG_FLUSH();
    }

    if(!gauge_exit(NULL, EXIT_CFG_UPDATE)){// exit update config mode. This automatically reseals data memory.
        NRF_LOG_INFO("Failed to exit config update mode in set_starting_config()");
        return;
    }
}

void shift_register(char * pData, uint8_t length){
    for(int i = length-1; i > 0; --i){
        pData[i] = pData[i-1];
    }
    pData[0] = STATE_SUBCLASS_0_DEFAULT;
}

/*  
    Function to print an entire status register in readable format. 
    Can pass OpConfig, Flags, or Control_Status register values.
    params: current_status - return value from subcommand used
            register_command - hex constant used to get the current_status value
*/
void print_register_status(uint16_t current_status, uint16_t register_command)
{
    char register_name[20];

    switch(register_command){
        case GAUGE_STATUS:
            strcpy(register_name, "Control Status");
            print_register_keys(control_status_keys, register_name, current_status);
            break;
        case GET_FLAGS:
            strcpy(register_name, "Flags");
            print_register_keys(flags_keys, register_name, current_status);
            break;
        case GET_OP_CONFIG:
            strcpy(register_name, "OpConfig");
            print_register_keys(op_config_keys, register_name, current_status);
            break;
        default:
            NRF_LOG_INFO("No valid register commmand hexcode provided to print_register_status().");
            NRF_LOG_FLUSH();
            return;
    }
}

void unseal_gauge(){
    gauge_control(NULL, UNSEAL, 0);
    gauge_control(NULL, UNSEAL, 0);
}

/*  
    Prints the given config register. 
    params: keys - string values for particular register
            register_name - string name of the register
            current_status - values contained in the register
*/
void print_register_keys(const char keys[16][11], char register_name[20], uint16_t current_status){
    NRF_LOG_INFO("Contents of %s Register: 0x%x", register_name, current_status);
    for(int i = 0; i < 16; i++){
        if(check_status(current_status, i)){
            NRF_LOG_INFO("%s: TRUE", keys[i]);
        }
        else{
            NRF_LOG_INFO("%s: FALSE", keys[i]);
        }
    }
    NRF_LOG_FLUSH();
}

/*  
    Function to check a single bit in one of the status registers.
    params: control_status - return value from subcommand used
            bit_index - index between 0-15 of bit to be checked (LSB of Low Byte first)
    return: true if set, false if not.
*/
bool check_status(uint16_t current_status, uint8_t bit_index){
     return ((current_status >> bit_index) & 1);
}

//gauge_execute_fs: execute a flash stream file
//pHandle: handle to communications adapter
//pFS: zero-terminated buffer with flash stream file
//return value: success: pointer to end of flash stream file
//error: point of error in flash stream file
char * gauge_execute_fs(void * pHandle, char * pFS) {
    int nLength = strlen(pFS);
    int nDataLength;
    char pBuf[16];
    char pData[32];
    int n, m;
    char * pEnd = NULL;
    char * pErr;
    bool bWriteCmd = false;
    unsigned char nRegister;
    m = 0;
    for (n = 0; n < nLength; n++)
        if (pFS[n] != ' ') pFS[m++] = pFS[n];
    pEnd = pFS + m;
    pEnd[0] = 0;
    do {
        switch ( * pFS) {
        case ';':
            break;
        case 'W':
        case 'C':
            bWriteCmd = * pFS == 'W';
            pFS++;
            if (( * pFS) != ':') return pFS;
            pFS++;
            n = 0;
            while ((pEnd - pFS > 2) && (n < sizeof(pData) + 2) && ( * pFS != '\n')) {
                pBuf[0] = * (pFS++);
                pBuf[1] = * (pFS++);
                pBuf[2] = 0;
                m = strtoul(pBuf, & pErr, 16);
                if ( * pErr) return (pFS - 2);
                if (n == 0) gauge_address(pHandle, m);
                if (n == 1) nRegister = m;
                if (n > 1) pData[n - 2] = m;
                n++;
            }
            if (n < 3) return pFS;
            nDataLength = n - 2;
            if (bWriteCmd)
                gauge_write(pHandle, nRegister, pData, nDataLength);
            else {
                char pDataFromGauge[nDataLength];
                gauge_read(pHandle, nRegister, pDataFromGauge, nDataLength);
                if (memcmp(pData, pDataFromGauge, nDataLength)) return pFS;
            }
            break;
        case 'X':
            pFS++;
            if (( * pFS) != ':') return pFS;
            pFS++;
            n = 0;
            while ((pFS != pEnd) && ( * pFS != '\n') && (n < sizeof(pBuf) - 1)) {
                pBuf[n++] = * pFS;
                pFS++;
            }
            pBuf[n] = 0;
            n = atoi(pBuf);
            usleep(n * 1000);
            break;
        default:
            return pFS;
        }
        while ((pFS != pEnd) && ( * pFS != '\n')) pFS++; //skip to next line
        if (pFS != pEnd) pFS++;
    } while (pFS != pEnd);
    return pFS;
}