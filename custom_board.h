

#ifndef BOARD_CUSTOM_H
#define BOARD_CUSTOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

// LEDs definitions for Test PCB
#define LEDS_NUMBER    7

// Pin numbers for the LEDs using the P0 notation. i.e. P013, P014, etc.
#define LED_START      13
#define LED_RED        13
#define LED_PURPLE     14
#define LED_BLUE       15
#define LED_GREEN      16
#define LED_ORANGE     17
#define LED_YELLOW     20 
#define LED_CHARGE     19
#define LED_STOP       20

// Indices of the LEDs in the LEDS_LIST array. 
// Use these for functions that require the index instead of the pin number.
// i.e. bsp_board_led_on() or bsp_board_led_off() both take the index as the 
// argument, not the actual pin number.
#define LED_PURPLE_IDX     0
#define LED_YELLOW_IDX     1
#define LED_ORANGE_IDX     2
#define LED_RED_IDX        3
#define LED_BLUE_IDX       4
#define LED_GREEN_IDX      5
#define LED_CHARGE_IDX     6

// The LEDs on the development kit board use active low operation, or 0, LEDs on a
// breadboard use active high, or 1.
#define LEDS_ACTIVE_STATE  1

#define LEDS_INV_MASK  LEDS_MASK

// Used in some Nordic provided functions like bsp_board_leds_init()
#define LEDS_LIST { LED_PURPLE, LED_YELLOW, LED_ORANGE, LED_RED, LED_BLUE, LED_GREEN, LED_CHARGE }

// Button definitions, only using a power button.
#define BUTTONS_NUMBER 1

#define BUTTON_START   20
#define BUTTON_POWER   23
#define BUTTON_STOP    20
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_POWER }

#define AIN0            0 // Does not refer to P0#, this is actually P02, but the function to
                          // start up ADC for touch sensor needs the index of available analog inputs
#define TOUCH_IN    AIN0
#define SPEAKER_OUT 12
#define MOTOR_OUT   11

#define BATTERY_SCL 3
#define BATTERY_SDA 4

// edits end here

#define RX_PIN_NUMBER  8
#define TX_PIN_NUMBER  6
#define CTS_PIN_NUMBER 7
#define RTS_PIN_NUMBER 5
#define HWFC           true
/*
#define SPIS_MISO_PIN   28  // SPI MISO signal.
#define SPIS_CSN_PIN    12  // SPI CSN signal.
#define SPIS_MOSI_PIN   25  // SPI MOSI signal.
#define SPIS_SCK_PIN    29  // SPI SCK signal.

#define SPIM0_SCK_PIN   29  // SPI clock GPIO pin number.
#define SPIM0_MOSI_PIN  25  // SPI Master Out Slave In GPIO pin number.
#define SPIM0_MISO_PIN  28  // SPI Master In Slave Out GPIO pin number.
#define SPIM0_SS_PIN    12  // SPI Slave Select GPIO pin number.

#define SPIM1_SCK_PIN   2   // SPI clock GPIO pin number.
#define SPIM1_MOSI_PIN  3   // SPI Master Out Slave In GPIO pin number.
#define SPIM1_MISO_PIN  4   // SPI Master In Slave Out GPIO pin number.
#define SPIM1_SS_PIN    5   // SPI Slave Select GPIO pin number.

#define SPIM2_SCK_PIN   12  // SPI clock GPIO pin number.
#define SPIM2_MOSI_PIN  13  // SPI Master Out Slave In GPIO pin number.
#define SPIM2_MISO_PIN  14  // SPI Master In Slave Out GPIO pin number.
#define SPIM2_SS_PIN    15  // SPI Slave Select GPIO pin number.

// serialization APPLICATION board - temp. setup for running serialized MEMU tests
#define SER_APP_RX_PIN              23    // UART RX pin number.
#define SER_APP_TX_PIN              24    // UART TX pin number.
#define SER_APP_CTS_PIN             2     // UART Clear To Send pin number.
#define SER_APP_RTS_PIN             25    // UART Request To Send pin number.

#define SER_APP_SPIM0_SCK_PIN       27     // SPI clock GPIO pin number.
#define SER_APP_SPIM0_MOSI_PIN      2      // SPI Master Out Slave In GPIO pin number
#define SER_APP_SPIM0_MISO_PIN      26     // SPI Master In Slave Out GPIO pin number
#define SER_APP_SPIM0_SS_PIN        23     // SPI Slave Select GPIO pin number
#define SER_APP_SPIM0_RDY_PIN       25     // SPI READY GPIO pin number
#define SER_APP_SPIM0_REQ_PIN       24     // SPI REQUEST GPIO pin number

// serialization CONNECTIVITY board
#define SER_CON_RX_PIN              24    // UART RX pin number.
#define SER_CON_TX_PIN              23    // UART TX pin number.
#define SER_CON_CTS_PIN             25    // UART Clear To Send pin number. Not used if HWFC is set to false.
#define SER_CON_RTS_PIN             2     // UART Request To Send pin number. Not used if HWFC is set to false.


#define SER_CON_SPIS_SCK_PIN        27    // SPI SCK signal.
#define SER_CON_SPIS_MOSI_PIN       2     // SPI MOSI signal.
#define SER_CON_SPIS_MISO_PIN       26    // SPI MISO signal.
#define SER_CON_SPIS_CSN_PIN        23    // SPI CSN signal.
#define SER_CON_SPIS_RDY_PIN        25    // SPI READY GPIO pin number.
#define SER_CON_SPIS_REQ_PIN        24    // SPI REQUEST GPIO pin number.
*/
#define SER_CONN_CHIP_RESET_PIN     11    // Pin used to reset connectivity chip

#ifdef __cplusplus
}
#endif

#endif // BOARD_CUSTOM_H