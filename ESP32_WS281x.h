/***************************************************************************************************/
/*
   This is an Arduino library that uses the Espressif ESP32 RMT peripheral to control Adafruit
   NeoPixels, FLORA RGB Smart Pixels, WS2811, WS2812, WS2812B, SK6812, etc.

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

#ifndef ESP32_WS281x_H
#define ESP32_WS281x_H


#include <Arduino.h>

#include "ESP32_RMT.h"


typedef uint8_t ledPixelType; //< 3-rd arg for "ESP32_WS281x" constructor


/*
   The order of primary colors in the "ESP32_WS281x" data stream can vary
   among device types, manufacturers and even different revisions of the same
   item. For that matter the third parameter to the 'ESP32_WS281x' constructor
   encodes the per-pixel byte offsets of the red, green and blue primaries (plus
   white, if present) in the data stream. The following #defines provide an
   easier-to-use named version for each permutation.
   e.g. "LED_GRB"
   - indicates a LED driver expecting 3-bytes per pixel, with the
     1-st byte transmitted containing the green(G) value, 2-nd containing red(R)
     and 3-rd containing blue(B)
   e.g. binary representation:
   - 0bRRRRGGBB for RGB LED drivers
   - 0bWWRRGGBB for RGBW LED drivers
*/
//RGB LED permutations for "ledPixelType"
//offset:         W          R          G          B
#define LED_RGB ((0 << 6) | (0 << 4) | (1 << 2) | (2)) //transmit as R,G,B
#define LED_RBG ((0 << 6) | (0 << 4) | (2 << 2) | (1)) //transmit as R,B,G
#define LED_GRB ((1 << 6) | (1 << 4) | (0 << 2) | (2)) //transmit as G,R,B
#define LED_GBR ((2 << 6) | (2 << 4) | (0 << 2) | (1)) //transmit as G,B,R
#define LED_BRG ((1 << 6) | (1 << 4) | (2 << 2) | (0)) //transmit as B,R,G
#define LED_BGR ((2 << 6) | (2 << 4) | (1 << 2) | (0)) //transmit as B,G,R

//RGBW LED permutations for "ledPixelType"
//offset:          W          R          G          B
#define LED_WRGB ((0 << 6) | (1 << 4) | (2 << 2) | (3)) //transmit as W,R,G,B
#define LED_WRBG ((0 << 6) | (1 << 4) | (3 << 2) | (2)) //transmit as W,R,B,G
#define LED_WGRB ((0 << 6) | (2 << 4) | (1 << 2) | (3)) //transmit as W,G,R,B
#define LED_WGBR ((0 << 6) | (3 << 4) | (1 << 2) | (2)) //transmit as W,G,B,R
#define LED_WBRG ((0 << 6) | (2 << 4) | (3 << 2) | (1)) //transmit as W,B,R,G
#define LED_WBGR ((0 << 6) | (3 << 4) | (2 << 2) | (1)) //transmit as W,B,G,R

#define LED_RWGB ((1 << 6) | (0 << 4) | (2 << 2) | (3)) //transmit as R,W,G,B
#define LED_RWBG ((1 << 6) | (0 << 4) | (3 << 2) | (2)) //transmit as R,W,B,G
#define LED_RGWB ((2 << 6) | (0 << 4) | (1 << 2) | (3)) //transmit as R,G,W,B
#define LED_RGBW ((3 << 6) | (0 << 4) | (1 << 2) | (2)) //transmit as R,G,B,W
#define LED_RBWG ((2 << 6) | (0 << 4) | (3 << 2) | (1)) //transmit as R,B,W,G
#define LED_RBGW ((3 << 6) | (0 << 4) | (2 << 2) | (1)) //transmit as R,B,G,W

#define LED_GWRB ((1 << 6) | (2 << 4) | (0 << 2) | (3)) //transmit as G,W,R,B
#define LED_GWBR ((1 << 6) | (3 << 4) | (0 << 2) | (2)) //transmit as G,W,B,R
#define LED_GRWB ((2 << 6) | (1 << 4) | (0 << 2) | (3)) //transmit as G,R,W,B
#define LED_GRBW ((3 << 6) | (1 << 4) | (0 << 2) | (2)) //transmit as G,R,B,W
#define LED_GBWR ((2 << 6) | (3 << 4) | (0 << 2) | (1)) //transmit as G,B,W,R
#define LED_GBRW ((3 << 6) | (2 << 4) | (0 << 2) | (1)) //transmit as G,B,R,W

#define LED_BWRG ((1 << 6) | (2 << 4) | (3 << 2) | (0)) //transmit as B,W,R,G
#define LED_BWGR ((1 << 6) | (3 << 4) | (2 << 2) | (0)) //transmit as B,W,G,R
#define LED_BRWG ((2 << 6) | (1 << 4) | (3 << 2) | (0)) //transmit as B,R,W,G
#define LED_BRGW ((3 << 6) | (1 << 4) | (2 << 2) | (0)) //transmit as B,R,G,W
#define LED_BGWR ((2 << 6) | (3 << 4) | (1 << 2) | (0)) //transmit as B,G,W,R
#define LED_BGRW ((3 << 6) | (2 << 4) | (1 << 2) | (0)) //transmit as B,G,R,W


/*
   table is declared outside of the "ESP32_WS281x" class for oldschool
   compilers that don't handle the C++11 constexpr keyword
*/
/* PROGMEM (flash mem) table containing 8-bit gamma-correction table */
static const uint8_t PROGMEM _ledPixelGammaTable[256] =
{
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   2,   3,
  3,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   5,   6,
  6,   6,   6,   7,   7,   7,   8,   8,   8,   9,   9,   9,   10,  10,  10,
  11,  11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,
  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,  24,  25,
  25,  26,  27,  27,  28,  29,  29,  30,  31,  31,  32,  33,  34,  34,  35,
  36,  37,  38,  38,  39,  40,  41,  42,  42,  43,  44,  45,  46,  47,  48,
  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
  64,  65,  66,  68,  69,  70,  71,  72,  73,  75,  76,  77,  78,  80,  81,
  82,  84,  85,  86,  88,  89,  90,  92,  93,  94,  96,  97,  99,  100, 102,
  103, 105, 106, 108, 109, 111, 112, 114, 115, 117, 119, 120, 122, 124, 125,
  127, 129, 130, 132, 134, 136, 137, 139, 141, 143, 145, 146, 148, 150, 152,
  154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182,
  184, 186, 188, 191, 193, 195, 197, 199, 202, 204, 206, 209, 211, 213, 215,
  218, 220, 223, 225, 227, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252,
  255
};


class ESP32_WS281x
{

  public:
  ESP32_WS281x(uint16_t ledQnt, int8_t dataPin = 6, ledPixelType ledType = LED_GRB);
  ESP32_WS281x();
 ~ESP32_WS281x();

  void                begin();
  bool                canShow();
  void                show();

  void                setPin(int8_t dataPin);
  const  int8_t       getPin();
  void                setBrightness(uint8_t brightness);
  const  uint8_t      getBrightness();
  void                setLength(uint16_t ledQnt);
  const  uint16_t     getLength();
  void                setPixelType(ledPixelType ledType);
  static ledPixelType strToPixelType(const char *strValue);

  void                setPixelColor(uint16_t ledIndex, uint8_t r, uint8_t g, uint8_t b);
  void                setPixelColor(uint16_t ledIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
  void                setPixelColor(uint16_t ledIndex, uint32_t color);
  const  uint32_t     getPixelColor(uint16_t ledIndex);
  const  uint8_t*     getRibbonColor();
  void                fill(uint32_t color = 0, uint16_t ledIndex = 0, uint16_t numOfLEDs = 0);
  void                rainbow(uint16_t firstHue = 0, int8_t reps = 1, uint8_t saturation = 255, uint8_t brightness = 255, bool gammify = true);
  void                clear();

  static uint32_t     color(uint8_t r, uint8_t g, uint8_t b);
  static uint32_t     color(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
  static uint32_t     colorHSV(uint16_t hue, uint8_t sat = 255, uint8_t brightness = 255);
  static uint8_t      gamma8(uint8_t colorValue);
  static uint32_t     gamma32(uint32_t colorValue);


private:
  //empty

protected:
  bool     _isStarted;  //true if "begin()" previously called
  int8_t   _pin;        //output pin number, -1 if not yet set
  uint8_t  _brightness; //strip brightness 0..255 (stored as +1, e.g. 1..256)
  uint8_t  _rOffset;    //red index within each 3--byte or 4-byte pixel
  uint8_t  _gOffset;    //index of green byte
  uint8_t  _bOffset;    //index of blue byte
  uint8_t  _wOffset;    //index of white (==rOffset if no white)
  uint16_t _numLEDs;    //number of RGB LEDs in strip
  uint16_t _numBytes;   //size of '_pixels' buffer below (3-bytes or 4-bytes per pixel)
  uint8_t* _pixels;     //buffer to hold LED color values (3-bytes or 4-bytes each color)
  uint32_t _endTime;    //latch timing reference

};

#endif