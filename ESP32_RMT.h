/***************************************************************************************************/
/*
   This is a low-level Arduino driver that uses the Espressif SoC's RMT peripheral to control
   Adafruit NeoPixels, FLORA RGB Smart Pixels, WS2811, WS2812, WS2812B, SK6812, etc.

   written by : enjoyneering
   sourse code: https://github.com/enjoyneering/
   based on:    Adafruit_NeoPixel library v1.12.5

   RMT (Remote Control Peripheral) channels:
   - ESP32 has 8 RMT channels for sending & receiving infrared remote control
     signals. Can be assign to any GPIO pins
   - ESP32-S3 has 4 TX channels (to send) & 4 RX channels (to receive) channels for
     infrared remote control signals. DMA access for TX mode on channel 3 & for RX
     mode on channel 7

   ESP32 strapping pins:
   - GPIO0, internal pull-up
   - GPIO2, internal pull-down
   - GPIO4, internal pull-down
   - GPIO5, internal pull-up
   - GPIO12/MTDI, internal pull-down
   - GPIO15/MTDO, internal pull-up
   - GPIO0, 2, 4 & 15 cannot be used on ESP-WROVER due to external connections
     for PSRAM chip
   - GPIO34..39 input only pins. These pins don’t have internal pull-up or
     pull-down resistors

   Supported frameworks:
   ESP32 Core - https://github.com/espressif/arduino-esp32


   GNU GPL license, all text above must be included in any redistribution,
   see link for details - https://www.gnu.org/licenses/licenses.html
*/
/***************************************************************************************************/

#ifndef ESP32_RMT_H
#define ESP32_RMT_H


#include <Arduino.h>

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#error "The 'ESP32_WS281x' library requires arduino-esp32 version greater than 3.0.0"
#endif


void espInit();
void espShow(uint8_t pin, uint8_t *pixels, uint32_t numBytes);

#endif