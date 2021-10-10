//Battery Gauge Library
//V1.0
//© 2016 Texas Instruments Inc.

#ifndef __GAUGE_H
#define __GAUGE_H
#include <stdbool.h>
#include "nrf_drv_twi.h"
#define SOFT_RESET 0x0042

// battery configuration parameters
#define DESIGN_CAPACITY 0x01F4 // 500 mAh
#define DESIGN_ENERGY 0x073A // 1850 - equal to Design Capacity*3.7 per TI reference docs
#define TERMINATE_VOLTAGE 0x0BEA // 3050mV - battery has automatic circuit to shutdown at 3V, this makes sure some power is left to do shutdown of the micro.

//#define TAPER_RATE 5/2 - Taper current is defined by the charger we are using (mAh value at which the charger considers the battery to be full)

// Gauge extended commands - used with gauge_cmd_read() or gauge_cmd_write()
#define GET_DESIGN_CAPACITY 0X3C // return currently configured design capacity - use for debug
#define SET_WRITE_DATA_BLOCK 0x3F // Sets the data block to be read/written by the next instruction
#define BLOCK_DATA_CONTROL_EN 0x61 // in unsealed mode, writing 0x00 to this enables RAM access.
#define SET_SUBCLASS 0x3E // set the state subclass to write to in memory
#define GET_FLAGS 0x06 // return the current contents of the flags register
#define GET_OP_CONFIG 0x3A // return the current contents of the opconfig register
#define GET_BATT_PCT 0x1C

// Control subcommands - used with gauge_control()
#define UNSEAL 0x8000 // unseals the IC so memory can be modified (i.e. for setting config values)
#define SEAL 0x0020 // seals the IC so certain commands cannot be run
#define GAUGE_STATUS 0x0000 // return the current control status register
#define EXIT_CFG_UPDATE 0x0043
#define RESET_TO_DEFAULT 0x0041

// Data block offsets - used to access specific parameters in their data blocks
#define DESIGN_CAP_OFFSET 10
#define DESIGN_EGY_OFFSET 12
#define TERM_VOLT_OFFSET 16
#define TAPER_RATE_OFFSET 27
#define OFFSET_0 0x00 // used with SET_WRITE_DATA_BLOCK, sets offsets 0-31 to be accessed
#define OFFSET_1 0x01 // used with SET_WRITE_DATA_BLOCK, sets offsets 32-41 to be accessed

// Data subclasses
#define STATE_SUB 0x52 // only one we need to use for our config updates
#define STATE_SUB_LEN 64

extern const char control_status_keys[16][11];
extern const char flags_keys[16][11];
extern const char op_config_keys[16][11];


void twi_init(void);

void twi_handler(nrf_drv_twi_evt_t const * p_event, void * p_context);

unsigned int gauge_read(void * pHandle, unsigned char nRegister, unsigned char * pData, unsigned char nLength);

unsigned int gauge_write(void * pHandle, unsigned char nRegister, unsigned char * pData, unsigned char nLength);

bool update_config_parameter(void * pHandle, uint16_t update_param, uint8_t offset, uint8_t data_block, uint8_t subclass, uint8_t subclass_len);

void print_register_status(uint16_t current_status, uint16_t register_command);

void print_register_keys(const char keys[16][11], char register_name[20], uint16_t current_status);

bool check_status(uint16_t current_status, uint8_t bit_index);

void shift_register(char * pData, uint8_t length);

void unseal_gauge();

void set_starting_config();

//gauge_control: issues a sub command
//pHandle: handle to communications adapter
//nSubCmd: sub command number
//return value: result from sub command
unsigned int gauge_control(void *pHandle, unsigned int nSubCmd, uint8_t control_label);

//gauge_cmd_read: read data from standard command
//pHandle: handle to communications adapter
//nCmd: standard command
//return value: result from standard command
unsigned int gauge_cmd_read(void *pHandle, unsigned char nCmd);

//gauge_cmd_write: write data to standard command
//pHandle: handle to communications adapter
//nCmd: standard command
//return value: number of bytes written to gauge
unsigned int gauge_cmd_write(void *pHandle, unsigned char nCmd, unsigned int nData);

//gauge_cfg_update: enter configuration update mode for rom gauges
//pHandle: handle to communications adapter
//return value: true = success, false = failure
bool gauge_cfg_update(void *pHandle);

//gauge_exit: exit configuration update mode for rom gauges
//pHandle: handle to communications adapter
//nSubCmd: sub command to exit configuration update mode
//return value: true = success, false = failure
bool gauge_exit(void *pHandle, unsigned int nSubCmd);

//gauge_read_data_class: read a data class
//pHandle: handle to communications adapter
//nDataClass: data class number
//pData: buffer holding the whole data class (all blocks)
//nLength: length of data class (all blocks)
//return value: 0 = success
int gauge_read_data_class(void *pHandle, unsigned char nDataClass, unsigned char *pData, unsigned char nLength);

//gauge_write_data_class: write a data class
//pHandle: handle to communications adapter
//nDataClass: data class number
//pData: buffer holding the whole data class (all blocks)
//nLength: length of data class (all blocks)
//return value: 0 = success
int gauge_write_data_class(void *pHandle, unsigned char nDataClass, unsigned char *pData, unsigned char nLength, uint8_t data_block);

//gauge_execute_fs: execute a flash stream file
//pHandle: handle to communications adapter
//pFS: zero-terminated buffer with flash stream file
//return value: success: pointer to end of flash stream file
//error: point of error in flashstream file
char *gauge_execute_fs(void *pHandle, char *pFS);
#endif