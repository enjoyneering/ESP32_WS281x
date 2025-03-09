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

#include "ESP32_RMT.h"


/************************************************************************************/
/*
   Local defines & variables
*/
/************************************************************************************/
#define SEMAPHORE_TIMEOUT_MS 50

static SemaphoreHandle_t _showMutex = NULL;


/************************************************************************************/
/*
   espInit()

   Initializing the mutex

   NOTE:
    - to avoid race condition initializing the mutex, all instances of
      "ESP32_WS281x" must be constructed before launching and child threads
*/
/************************************************************************************/
void espInit()
{
  if (_showMutex == NULL) {_showMutex = xSemaphoreCreateMutex();}
}


/************************************************************************************/
/*
   espShow()

   Send pixel color buffer (data) to LED drivers via ESP32 RMT peripheral

   NOTE:
   - because RTM pin is shared between all instances, we will end up
     releasing/initializing the RMT channels each time we invoke on different pins.
     This is OK, but not efficient. "ledData" is shared between all instances
     but will be allocated with enough space for the largest instance, data is not
     used beyond the mutex lock so this should be fine

   - to release RMT resources (RMT channels and "ledData"):
     - call "updateLength(0)" to set number of pixels/bytes to zero
     - then call "show()" to invoke this code and free resources
*/
/************************************************************************************/
void espShow(uint8_t pin, uint8_t *pixels, uint32_t numBytes)
{
  static rmt_data_t* ledData     = NULL;
  static uint32_t    ledDataSize = 0;
  static int         rmtPin      = -1;

  if (_showMutex && xSemaphoreTake(_showMutex, SEMAPHORE_TIMEOUT_MS / portTICK_PERIOD_MS) == pdTRUE)
  {
    uint32_t requiredSize = numBytes * 8;

    if (requiredSize > ledDataSize)
    {
      free(ledData);

      if ((ledData = (rmt_data_t *)malloc(requiredSize * sizeof(rmt_data_t)))!= NULL)
      {
        ledDataSize = requiredSize;
      }
      else
      {
        ledDataSize = 0;
      }
    }
    else if (requiredSize == 0) //see NOTE
    {
      free(ledData);

      ledData = NULL;

      if (rmtPin >= 0)
      {
        rmtDeinit(rmtPin);

        rmtPin = -1;
      }

      ledDataSize = 0;
    }

    if ((ledDataSize > 0) && (requiredSize <= ledDataSize))
    {
      if (pin != rmtPin)
      {
        if (rmtPin >= 0)
        {
          rmtDeinit(rmtPin);

          rmtPin = -1;
        }

        if (rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000) != true) //rmtInit(int pin, rmt_ch_dir_t channel_direction, rmt_reserve_memsize_t memsize, uint32_t frequency_Hz)
        {
          log_e("Failed to init RMT TX mode on pin %d", pin);

          return;
        }

        rmtPin = pin;
      }

      if (rmtPin >= 0)
      {
        int i=0;

        for (int b=0; b < numBytes; b++)
        {
          for (int bit=0; bit<8; bit++)
          {
            if (pixels[b] & (1<<(7-bit)))
            {
              ledData[i].level0    = 1;
              ledData[i].duration0 = 8;
              ledData[i].level1    = 0;
              ledData[i].duration1 = 4;
            }
            else
            {
              ledData[i].level0    = 1;
              ledData[i].duration0 = 4;
              ledData[i].level1    = 0;
              ledData[i].duration1 = 8;
            }

            i++;
          }
        }

        rmtWrite(pin, ledData, numBytes * 8, RMT_WAIT_FOR_EVER);
      }
    }

    xSemaphoreGive(_showMutex);
  }
}