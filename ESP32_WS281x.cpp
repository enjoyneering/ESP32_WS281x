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

#include "ESP32_WS281x.h"


/************************************************************************************/
/*
   Constructor

   NOTE:
   - constructor when "ledQnt", "dataPin" and "ledPixelType" are known at
     compile-time

   - ledQnt, number of LEDs in strand
   - dataPin  Arduino pin number which will drive the LED data in
   - ledType, pixel type

   - to release RMT resources (RMT channels and "led_data"):
     - call "updateLength(0)" to set number of pixels/bytes to zero
     - then call "show()" to invoke this code and free resources
*/
/************************************************************************************/
ESP32_WS281x::ESP32_WS281x(uint16_t ledQnt, int8_t dataPin, ledPixelType ledType) : _isStarted(false), _brightness(0), _pixels(NULL), _endTime(0)
{
  setPixelType(ledType);  //call before 'setLength()'!!!
  setLength(ledQnt);

  _pin = dataPin;
}


/************************************************************************************/
/*
   Constructor

   NOTE:
   - constructor when "ledQnt", "dataPin" and "ledPixelType" are not known at
     compile-time, and must be initialized later with "setPixelType()",
     "setLength()" and "setPin()"

   - his function is deprecated, here only for old projects that may still be
     calling it. New projects should instead use the constructor
     "ESP32_WS281x(length, pin, type)"
*/
/************************************************************************************/
ESP32_WS281x::ESP32_WS281x() :  _isStarted(false), _pin(-1), _brightness(0), _rOffset(1), _gOffset(0), _bOffset(2), _wOffset(1), _numLEDs(0), _numBytes(0), _pixels(NULL), _endTime(0)
{
  //empty
}


/************************************************************************************/
/*
   Destructor

   Deallocate ESP32_WS281x object, set data pin back to INPUT
*/
/************************************************************************************/
ESP32_WS281x::~ESP32_WS281x()
{
  /* release RMT resources (RMT channels and "led_data") by indirectly calling into "espShow()" */
  memset(_pixels, 0, _numBytes);
  _numLEDs  = 0;
  _numBytes = 0;
  show();

  free(_pixels);

  if (_pin >= 0) {pinMode(_pin, INPUT);}
}


/************************************************************************************/
/*
   begin()

   Configure "ESP32_WS281x" data pin for output
*/
/************************************************************************************/
void ESP32_WS281x::begin()
{
  _isStarted = true; //true if "begin()" called

  setPin(_pin);      //set data pin as output, call after "_isStarted = true"
  espInit();         //initialize mutex
}


/************************************************************************************/
/*
   canShow()

   check whether a call to "show()" will start sending data immediately or will
   'block' for a required interval

   NOTE:
   - LED driver require a short quiet time (about 300 microseconds) after the
     last bit is received before the data 'latches' and new data can start being
     received. Usually one's sketch is implicitly using this time to generate a
     new frame of animation...but if it finishes very quickly, this function
     could be used to see if there's some idle time available for some low-priority
     concurrent task.

   - return true if "show()" will start sending immediately, false if "show()"
     would block (meaning some idle time is available for other tasks)


   - it's normal and possible for "_endTime" to exceed "micros()" if the 32-bit
     clock counter has rolled over (about every 70 minutes). The problem arises
     if code invokes "show()" very infrequently...the "micros()" counter may roll
     over MULTIPLE times in that interval, the delta calculation is no longer correct
     and the next update may stall for a very long time. The check below resets the
     latch counter if a rollover has occurred. This can cause an extra delay of up to
     300 microseconds in the rare case where a "show()" call happens precisely around
     the rollover, but that's neither likely nor especially harmful, vs. other code 
     that might stall for 30+ minutes, or having to document and frequently remind
     and/or provide tech support explaining an unintuitive need for "show()" calls at
     least once an hour
*/
/************************************************************************************/ 
bool ESP32_WS281x::canShow()
{
  uint32_t now = micros();

  if (_endTime > now) {_endTime = now;}

  return (now - _endTime) >= 300L; //300 microseconds
}


/**************************************************************************/
/*
   show()

   Transmit pixel data in RAM to LED drivers

   NOTE:
   - ESP32 may not disable interrupts because "espShow()" uses RMT which tries
     to acquire locks

   - data latch = 300+ microsecond pause in the output stream. Rather than
     put a delay at the end of the function, the ending time is noted and
     the function will simply hold off (if needed) on issuing the
     subsequent round of data until the latch time has elapsed. This
     allows the mainline code to start generating the next frame of data
     rather than stalling for the latch.

   - "_endTime" is a private member (rather than global var) so that
     multiple class instances on different pins can be quickly issued in
     succession (each instance doesn't delay the next)
*/
/**************************************************************************/ 
void ESP32_WS281x::show()
{
  if (!_pixels) {return;}

//while (canShow() != true){//empty}  //see NOTE
  while (canShow() != true){yield();} //see NOTE

  espShow(_pin, _pixels, _numBytes);

  _endTime = micros(); // Save EOD time for latch on next call
}


/************************************************************************************/
/*
   setPin()

   Set (change) "ESP32_WS281x" output pin number on the fly

   NOTE:
   - previous pin (if any) is set to INPUT and the new pin is set to OUTPUT
   - Arduino pin number -1=no pin
*/
/************************************************************************************/
void ESP32_WS281x::setPin(int8_t dataPin)
{
  if ((_isStarted == true) && (_pin >= 0)) {pinMode(_pin, INPUT);} //disable existing data output pin

  _pin = dataPin;

  if (_isStarted == true)
  {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
  }
}


/************************************************************************************/
/*
   getPin()

   Retrieve pin number used for "ESP32_WS281x" data output

   NOTE:
   - return pin number -1 if not set
*/
/************************************************************************************/
const int8_t ESP32_WS281x::getPin()
{
  return _pin;
}


/************************************************************************************/
/*
   setBrightness()

   Adjust output brightness. Does not immediately affect what's currently displayed
   on the LEDs. The next call to "show()" will refresh the LEDs at this level.

   NOTE:
   - brightness setting 0..255 (minimum/off to full brightest)
   - this was intended for one-time use in one's setup() function not as an
     animation effect in itself. Because of the way this library "pre-multiplies"
     LED colors in RAM, changing the brightness is often a "lossy" operation what
     you write to pixels isn't necessary the same as what you'll read back.


   - stored brightness value is different than what's passed. This simplifies the
     actual scaling math later, allowing a fast 8x8-bit multiply and taking the
     MSB. 'brightness' is a uint8_t, adding 1 here may (intentionally) roll over...
     so 0 = max brightness (color values are interpreted literally; no scaling),
     1 = min brightness (off), 255 = just below max brightness.

   - re-scale brightness existing data in RAM is potentially "lossy" process
     especially when increasing brightness. The tight timing in the WS2811/WS2812
     code means there aren't enough free cycles to perform this scaling on the fly
     as data is issued. So we make a pass through the existing color data in RAM and
     scale it (subsequent graphics commands also work at this brightness level). If
     there's a significant step up in brightness, the limited number of steps
     (quantization) in the old data will be quite visible in the re-scaled version.
     For a non-destructive change, you'll need to re-render the full strip data.
     C'est la vie.
*/
/************************************************************************************/
void ESP32_WS281x::setBrightness(uint8_t brightness)
{

  uint8_t newBrightness = brightness + 1; //see NOTE

  if (newBrightness != _brightness) //see NOTE
  {
    uint8_t  c            = 0;
    uint8_t* ptr          = _pixels;
    uint8_t oldBrightness = _brightness - 1; //de-wrap old "_brightness" value
    uint16_t scale        = 0;

    if      (oldBrightness == 0) {scale = 0;}                     //avoid /0
    else if (brightness == 255)  {scale = 65535 / oldBrightness;}
    else                         {scale = (((uint16_t)newBrightness << 8) - 1) / oldBrightness;}

    for (uint16_t i = 0; i < _numBytes; i++)
    {
      c      = *ptr;
      *ptr++ = (c * scale) >> 8;
    }

    _brightness = newBrightness;
  }
}

/************************************************************************************/
/*
   getBrightness()

   Retrieve the last-set brightness value for the LED drivers strip

   NOTE:
   - return 0..255 (minimum/off to full brightest)
*/
/************************************************************************************/
const uint8_t ESP32_WS281x::getBrightness()
{
  return (_brightness - 1);
}


/************************************************************************************/
/*
   setLength()

   Change the length of a previously-declared "ESP32_WS281x" strip object. Old data
   is deallocated and new data is cleared, pin number and pixel format are unchanged

   NOTE:
   - ledQnt, new length of strip, in pixels

   - his function is deprecated, here only for old projects that may still be
     calling it. New projects should instead use the constructor
     "ESP32_WS281x(length, pin, type)"
*/
/************************************************************************************/
void ESP32_WS281x::setLength(uint16_t ledQnt)
{
  free(_pixels); //free existing data, if any

  _numBytes = ledQnt * ((_wOffset == _rOffset) ? 3 : 4); //recalculate size of "_pixels" buffer, ALL PIXELS ARE CLEARED

  if ((_pixels = (uint8_t *)malloc(_numBytes)) != NULL)
  {
    memset(_pixels, 0, _numBytes);

    _numLEDs = ledQnt;
  }
  else
  {
    _numLEDs  = 0;
    _numBytes = 0;
  }
}


/************************************************************************************/
/*
   getLength()

   Return the number of pixels/LEDs in an "ESP32_WS281x" strip object

   NOTE:
   - return pixel count (quantity), 0 if not set
*/
/************************************************************************************/
const uint16_t ESP32_WS281x::getLength()
{
  return _numLEDs;
}


/************************************************************************************/
/*
   setPixelType()

   Change pixel format of a previously-declared "ESP32_WS281x" strip object.

   NOTE:
   - if format changes from one of the RGB variants to an RGBW variant
     (or RGBW to RGB), the old data will be deallocated and new data is cleared.
     Otherwise, the old data will remain in RAM and is not reordered to the new
     format, so it's advisable to follow up with clear()

   - his function is deprecated, here only for old projects that may still be
     calling it. New projects should instead use the constructor
     "ESP32_WS281x(length, pin, type)"
*/
/************************************************************************************/
void ESP32_WS281x::setPixelType(ledPixelType ledType)
{
  bool oldThreeBytesPerPixel = (_wOffset == _rOffset); //false if RGBW

  _wOffset = (ledType >> 6) & 0b11; //see notes in header file
  _rOffset = (ledType >> 4) & 0b11; //regarding R/G/B/W offsets
  _gOffset = (ledType >> 2) & 0b11;
  _bOffset = (ledType & 0b11);

  //if bytes-per-pixel has changed (and pixel data was previously allocated), re-allocate to new size will clear any data
  if (_pixels != NULL)
  {
    bool newThreeBytesPerPixel = (_wOffset == _rOffset);

    if (newThreeBytesPerPixel != oldThreeBytesPerPixel) {setLength(_numLEDs);}
  }
}


/************************************************************************************/
/*
   strToPixelType()

   Convert pixel color order from string (e.g. "BGR") to "ESP32_WS281x"
   color order constant (e.g. LED_BGR). This may be helpful for code
   that initializes from text configuration rather than compile-time
   constants

   NOTE:
   - strValue, input string. Should be reasonably sanitized (a 3..4 character
     NUL-terminated string) or undefined behavior may result (output is still a
     valid "ESP32_WS281x" order constant, but might not present as expected).
     Garbage in, garbage out

   - return "ESP32_WS281x" color order constants (e.g. LED_BGR)

   - this function is declared static in the class so it can be called without a
     "ESP32_WS281x" object (since it's not likely been declared in the code yet).
     Use "ESP32_WS281x::str2order()"
*/
/************************************************************************************/
ledPixelType ESP32_WS281x::strToPixelType(const char *strValue)
{
  int8_t r = 0, g = 0, b = 0, w = -1;

  if (strValue)
  {
    char c;

    for (uint8_t i=0; ((c = tolower(strValue[i]))); i++)
    {
      if      (c == 'r') {r = i;}
      else if (c == 'g') {g = i;}
      else if (c == 'b') {b = i;}
      else if (c == 'w') {w = i;}
    }

    r &= 3;
  }

  if (w < 0) w = r; //if 'w' not specified, duplicate r bits

  return (w << 6) | (r << 4) | ((g & 3) << 2) | (b & 3);
}


/************************************************************************************/
/*
   setPixelColor()

   Set a pixel's color using separate red(R), green(G) and blue(G) components in RAM

   NOTE:
   - if using RGBW pixels, white will be set to 0(off)
   - 0b00RRGGBB for RGB LED drivers
   - 0bWWRRGGBB for RGBW LED drivers

   - ledIndex, pixel index starting from 0
   - r, red  brightness 0..255 (minimum/off to maximum)
   - g, green brightness 0..255 (minimum/off to maximum)
   - b, blue brightness 0..255 (minimum/off to maximum)
*/
/************************************************************************************/
void ESP32_WS281x::setPixelColor(uint16_t ledIndex, uint8_t r, uint8_t g, uint8_t b)
{
  if (ledIndex < _numLEDs)
  {
    if (_brightness) //see notes in "setBrightness()", strip brightness 0..255 (stored as +1, e.g. 1..256)
    {
      r = (r * _brightness) >> 8;
      g = (g * _brightness) >> 8;
      b = (b * _brightness) >> 8;
    }

    uint8_t *p;

    if (_wOffset == _rOffset)   //RGB-type strip, 3-bytes per pixel
    {
      p = &_pixels[ledIndex * 3]; //3-bytes per pixel (ignore W)
    }
    else                        //WRGB-type strip, 4-bytes per pixel
    {
      p = &_pixels[ledIndex * 4]; //4-bytes per pixel

      p[_wOffset] = 0;          //set W to 0, only R,G,B passed
    }

    p[_rOffset] = r; //store R,G,B
    p[_gOffset] = g;
    p[_bOffset] = b;
  }
}


/************************************************************************************/
/*
   setPixelColor()

   Set a pixel's color using separate red(R), green(G), blue(G) and white(W)
   components in RAM (for RGBW LED drivers only)

   NOTE:
   - if using RGB pixels, white will be ignored
   - 0bxxRRGGBB for RGB LED drivers
   - 0bWWRRGGBB for RGBW LED drivers

   - ledIndex, pixel index starting from 0
   - r, red  brightness 0..255 (minimum/off to maximum)
   - g, green brightness 0..255 (minimum/off to maximum)
   - b, blue brightness 0..255 (minimum/off to maximum)
   - w, white brightness 0..255 (minimum/off to maximum)
*/
/************************************************************************************/
void ESP32_WS281x::setPixelColor(uint16_t ledIndex, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
  if (ledIndex < _numLEDs)
  {
    if (_brightness) //see note in "setBrightness()", strip brightness 0..255 (stored as +1, e.g. 1..256)
    {
      r = (r * _brightness) >> 8;
      g = (g * _brightness) >> 8;
      b = (b * _brightness) >> 8;
      w = (w * _brightness) >> 8;
    }

    uint8_t *p;

    if (_wOffset == _rOffset)   //RGB-type strip, 3-bytes per pixel
    {
      p = &_pixels[ledIndex * 3]; //3-bytes per pixel (ignore W)
    }
    else                        //WRGB-type strip, 4-bytes per pixel
    {
      p = &_pixels[ledIndex * 4]; //4-bytes per pixel

      p[_wOffset] = w;          //store W
    }

    p[_rOffset] = r; //store R,G,B
    p[_gOffset] = g;
    p[_bOffset] = b;
  }
}


/************************************************************************************/
/*
   setPixelColor()

   NOTE:
   Set a pixel's color using a 32-bit 'packed' RGB or RGBW value

   NOTE:
   - ledIndex, Pixel index starting from 0
   - color, 32-bit color value. Most significant byte is white (for RGBW
     pixels) or ignored (for RGB pixels), next is red, then green, and least
     significant byte is blue

   - 0bxxRRGGBB for RGB LED drivers
   - 0bWWRRGGBB for RGBW LED drivers
*/
/************************************************************************************/
void ESP32_WS281x::setPixelColor(uint16_t ledIndex, uint32_t color)
{
  if (ledIndex < _numLEDs)
  {
    uint8_t *p;

    uint8_t r = (uint8_t)(color >> 16);
    uint8_t g = (uint8_t)(color >> 8);
    uint8_t b = (uint8_t)color;

    if (_brightness) //see note in 'setBrightness()', strip brightness 0..255 (stored as +1, e.g. 1..256)
    {
      r = (r * _brightness) >> 8;
      g = (g * _brightness) >> 8;
      b = (b * _brightness) >> 8;
    }

    if (_wOffset == _rOffset)    //RGB-type strip, 3-bytes per pixel
    {
      p = &_pixels[ledIndex * 3];
    }
    else                         //WRGB-type strip, 4-bytes per pixel
    {
      p  = &_pixels[ledIndex * 4];

      uint8_t w  = (uint8_t)(color >> 24);

      p[_wOffset] = _brightness ? ((w * _brightness) >> 8) : w; //store W
    }

    p[_rOffset] = r; //store R,G,B
    p[_gOffset] = g;
    p[_bOffset] = b;
  }
}


/************************************************************************************/
/*
  getPixelColor()

  Get the color of a previously-set pixel

  NOTE:
   - stored color was decimated by "setBrightness()". Returned value attempts to
     scale back to an approximation of the original 24-bit value used when setting
     the pixel color, but there will always be some error (those bits are simply
     gone). Issue is most happend at low brightness levels

  - ledIndex, index of pixel to read (0=first)

  - return 'packed' 32-bit RGB or WRGB value. Most significant byte is white
           (for RGBW pixels) or 0 (for RGB pixels), next is red, then green,
           and least significant byte is blue

  - 0b00RRGGBB for RGB LED drivers
  - 0bWWRRGGBB for RGBW LED drivers
*/
/************************************************************************************/
const uint32_t ESP32_WS281x::getPixelColor(uint16_t ledIndex)
{
  if (ledIndex >= _numLEDs) {return 0;} //out of bounds, return no color

  uint8_t* p;

  if (_wOffset == _rOffset) //RGB-type strip, 3-bytes per pixel
  {
    p = &_pixels[ledIndex * 3];

    if (_brightness)        //see note in 'setBrightness()', strip brightness 0..255 (stored as +1, e.g. 1..256)
    {
      return (((uint32_t)(p[_rOffset] << 8) / _brightness) << 16) |
             (((uint32_t)(p[_gOffset] << 8) / _brightness) << 8)  |
              ((uint32_t)(p[_bOffset] << 8) / _brightness);
    }
    else                    //no "_brightness" adjustment has been made, return 'raw' color
    {

      return ((uint32_t)p[_rOffset] << 16) | ((uint32_t)p[_gOffset] << 8) | (uint32_t)p[_bOffset];
    }
  }
  else                      //WRGB-type strip, 4-bytes per pixel
  {
    p = &_pixels[ledIndex * 4];

    if (_brightness)        //return scaled color
    {
      return (((uint32_t)(p[_wOffset] << 8) / _brightness) << 24) |
             (((uint32_t)(p[_rOffset] << 8) / _brightness) << 16) |
             (((uint32_t)(p[_gOffset] << 8) / _brightness) << 8)  |
              ((uint32_t)(p[_bOffset] << 8) / _brightness);
    }
    else                    //no "_brightness" adjustment has been made, return 'raw' color
    {
      return ((uint32_t)p[_wOffset] << 24) | ((uint32_t)p[_rOffset] << 16) | ((uint32_t)p[_gOffset] << 8)  | (uint32_t)p[_bOffset];
    }
  }
}


/************************************************************************************/
/*
   getRibbonColor()

   Get a pointer directly to "ESP32_WS281x" data buffer in RAM

   NOTE:
   - this is for high-performance applications where calling "setPixelColor()" on
     every single pixel would be too slow (e.g. POV or light-painting projects).
     There is no bounds checking on the array, creating tremendous potential for
     mayhem if one writes past the ends of the buffer. Great power, great
     responsibility and all that

   - pixel data is stored in a device-native format ("ledPixelType" format) and is
     not translated here. Applications that access this buffer will need to be
     aware of the specific data format and handle colors appropriately

   - [0bxxRRGGBB,..,0bxxRRGGBB] array for RGB LED drivers
   - [0bWWRRGGBB,..,0bxxRRGGBB] array for RGBW LED drivers

   - return pointer to "ESP32_WS281x" buffer (uint8_t* array)
*/
/************************************************************************************/
const uint8_t* ESP32_WS281x::getRibbonColor()
{
  return _pixels;
}


/************************************************************************************/
/*
   fill()

   Fill all or part of the "ESP32_WS281x" data buffer in RAM with a color

   NOTE:
   - color, 32-bit color value. Most significant byte is white (for RGBW pixels) or 
     ignored (for RGB pixels), next is red, then green, and least significant byte
     is blue. If all arguments are unspecified, this will be 0 (off)

   - ledIndex, Index of first pixel to fill starting from 0. Must be in-bounds, no 
     clip_ping is performed. 0 if unspecified.
   - numOfLEDs, umber of pixels to fill, as a positive value. Passing 0 or leaving
     unspecified will fill to end of strip.
*/
/************************************************************************************/
void ESP32_WS281x::fill(uint32_t color, uint16_t ledIndex, uint16_t numOfLEDs)
{
  if (ledIndex >= _numLEDs) {return;} //if ledIndex LED is past end of strip, nothing to do

  uint16_t end;

  //calculate index ONE AFTER the last pixel to fill
  if (numOfLEDs == 0)
  {
    end = _numLEDs;          //fill to end of strip
  }
  else                       //ensure that the loop won't go past the last pixel
  {
    end = ledIndex + numOfLEDs;

    if (end > _numLEDs) {end = _numLEDs;}
  }

  for (uint16_t i = ledIndex; i < end; i++)
  {
    this->setPixelColor(i, color);
  }
}


/************************************************************************************/
/*
   rainbow()

   Fill LED strip with one or more cycles of hues. Everyone loves the rainbow
   swirl so much, now it's canon!

   NOTE:
   - firstHue, hue of first pixel 0..65535, representing one full cycle of the
     color wheel. Each subsequent pixel will be offset to complete one or more
     cycles over the length of the strip
   - reps, number of cycles of the color wheel over the length of the strip. 
     Default is 1. Negative values can be used to reverse the hue order
   - saturation, (optional)  0..255 (gray to pure hue). Defaul 255
   - brightness, (optional) 0..255 (off to max). Ddefault 255
   - gammify, if true (default), apply gamma correction to colors for better
     appearance
*/
/************************************************************************************/
void ESP32_WS281x::rainbow(uint16_t firstHue, int8_t reps, uint8_t saturation, uint8_t _brightness, bool gammify)
{
  for (uint16_t i=0; i < _numLEDs; i++)
  {
    uint16_t hue = firstHue + (i * reps * 65536) / _numLEDs;

    uint32_t color = colorHSV(hue, saturation, _brightness);

    if (gammify) color = gamma32(color);

    setPixelColor(i, color); //refresh pixel color buffer
  }
}


/************************************************************************************/
/*
   clear()

   Fill the whole LED strip data buffer in RAM with 0/black/off
*/
/************************************************************************************/
void ESP32_WS281x::clear()
{ 
  memset(_pixels, 0, _numBytes);
}


/************************************************************************************/
/*
   color()

   Convert separate red(R), green(G) and blue(B) values into a single
   "packed" 32-bit RGB color

   NOTE:
   - r, red brightness 0..255
   - g, green brightness 0..255
   - b, blue brightness 0..255

   - return 32-bit packed RGB value, which can then be assigned to a variable for
     later use or passed to the "setPixelColor()" function. Packed RGB format is
     predictable, regardless of LED strand color order
*/
/************************************************************************************/
uint32_t ESP32_WS281x::color(uint8_t r, uint8_t g, uint8_t b)
{
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}


/************************************************************************************/
/*
   color()

   Convert separate red(R), green(G), blue(B) and white(W) values into a single
   "packed" 32-bit RGB color WRGB color

   NOTE:
   - r, red brightness 0..255
   - g, green brightness 0..255
   - b, blue brightness 0..255
   - w, white brightness 0..255

   - return 32-bit packed WRGB value, which can then be assigned to a variable for
     later use or passed to the "setPixelColor()" function. Packed WRGB format is
     predictable, regardless of LED strand color order
*/
/************************************************************************************/
uint32_t ESP32_WS281x::color(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}


/************************************************************************************/
/*
   colorHSV()

   Convert hue, saturation and value into a packed 32-bit RGB color that can be
   passed to "setPixelColor()" or other RGB-compatible functions

   NOTE:
   - hue, unsigned 16-bit value 0..65535, representing one full loop of the color
     wheel, which allows 16-bit hues to "roll over" while still doing the expected
     thing (and allowing more precision than the "wheel()" function that was common
     to prior "ESP32_WS281x" examples)
   - sat, saturation 8-bit value 0..255 (min/pure grayscale to max/pure hue)
     Default of 255 if unspecified
   - brightness, 8-bit value 0..255 (min/black/off to max/full brightness)
     Default of 255 if unspecified

   - result is linearly but not perceptually correct, so you may want to pass the
     result through the "gamma32()" function (or your own gamma-correction operation)
     else colors may appear washed out. Diffusing the LEDs also really seems to help
     when using low-saturation colors

   - returned packed 32-bit RGB with the most significant byte set to 0 (the
     white element of WRGB pixels is NOT utilized)
*/
/************************************************************************************/
uint32_t ESP32_WS281x::colorHSV(uint16_t hue, uint8_t sat, uint8_t brightness)
{
  uint8_t r;
  uint8_t g;
  uint8_t b;

  /*
     Remap 0-65535 to 0-1529. Pure red is CENTERED on the 64K rollover;
     0 is not the start of pure red, but the midpoint...a few values above
     zero and a few below 65536 all yield pure red (similarly, 32768 is the
     midpoint, not start, of pure cyan). The 8-bit RGB hexcone (256 values
     each for red, green, blue) really only allows for 1530 distinct hues
     (not 1536, more on that below), but the full unsigned 16-bit type was
     chosen for hue so that one's code can easily handle a contiguous color
     wheel by allowing hue to roll over in either direction
  */
  hue = (hue * 1530L + 32768) / 65536;

  /*
     Because red is centered on the rollover point (the +32768 above,
     essentially a fixed-point +0.5), the above actually yields 0 to 1530,
     where 0 and 1530 would yield the same thing. Rather than apply a
     costly modulo operator, 1530 is handled as a special case below
  */

  /*
     So you'd think that the color "hexcone" (the thing that ramps from
     pure red, to pure yellow, to pure green and so forth back to red,
     yielding six slices), and with each color component having 256
     possible values (0-255), might have 1536 possible items (6*256),
     but in reality there's 1530. This is because the last element in
     each 256-element slice is equal to the first element of the next
     slice, and kee_ping those in there this would create small
     discontinuities in the color wheel. So the last element of each
     slice is dropped...we regard only elements 0-254, with item 255
     being picked up as element 0 of the next slice. Like this:
     Red to not-quite-pure-yellow is:        255,   0, 0 to 255, 254,   0
     Pure yellow to not-quite-pure-green is: 255, 255, 0 to   1, 255,   0
     Pure green to not-quite-pure-cyan is:     0, 255, 0 to   0, 255, 254
     and so forth. Hence, 1530 distinct hues (0 to 1529), and hence why
     the constants below are not the multiples of 256 you might expect
  */

  //convert hue to R,G,B (nested ifs faster than divide+mod+switch):
  if (hue < 510)       //red to green-1
  {
    b = 0;
    if (hue < 255)     //red to yellow-1
    {
      r = 255;
      g = hue;         //g = 0 to 254
    } 
    else               //yellow to green-1
    {
      r = 510 - hue;   //r = 255 to 1
      g = 255;
    }
  }
  else if (hue < 1020) //green to blue-1
  {
    r = 0;
    if (hue < 765)     //green to cyan-1
    {
      g = 255;
      b = hue - 510;   //b = 0 to 254
    }
    else               //cyan to blue-1
    {
      g = 1020 - hue;  //g = 255 to 1
      b = 255;
    }
  }
  else if (hue < 1530) //blue to red-1
  {
    g = 0;
    if (hue < 1275)    //blue to magenta-1
    {
      r = hue - 1020;  //r = 0 to 254
      b = 255;
    }
    else               //magenta to red-1
    {
      r = 255;
      b = 1530 - hue;  //b = 255 to 1
    }
  }
  else                 //last 0.5 red (quicker than % operator)
  {
    r = 255;
    g = b = 0;
  }

  //apply saturation and brightness to R,G,B, pack into 32-bit result
  uint32_t v1 = 1 + brightness; //1..256, allows >>8 instead of /255
  uint16_t s1 = 1 + sat;        // 1..256, same reason
  uint8_t  s2 = 255 - sat;      //255..0

  return ((((((r * s1) >> 8) + s2) * v1) & 0xff00) << 8) |
          (((((g * s1) >> 8) + s2) * v1) & 0xff00) |
          (((((b * s1) >> 8) + s2) * v1) >> 8);
}


/************************************************************************************/
/*
   gamma8()

   An 8-bit gamma-correction function for basic pixel brightnessadjustment. Makes
   color transitions appear more perceptially correct

   NOTE:
   - colorValue, input brightness 0..255 (minimum or off/black to maximum).
   - return gamma-adjusted brightness, can then be passed to one of the
     "setPixelColor()" functions. This uses a fixed gamma correction exponent
     of 2.6, which seems reasonably okay for average LED drivers in average tasks.
     If you need finer control you'll need to provide your own gamma-correction
     function instead
*/
/************************************************************************************/
uint8_t ESP32_WS281x::gamma8(uint8_t colorValue)
{
  return pgm_read_byte(&_ledPixelGammaTable[colorValue]); // 0..255 in, 0..255 out
}


/************************************************************************************/
/*
   gamma32()

   32-bit variant of "gamma8()" that applies the same function to all components
   of a packed RGB or WRGB value. Makes color transitions appear more perceptially
   correct

   NOTE:
   - colorValue, 32-bit packed RGB or WRGB color

   - return  gamma-adjusted packed color, can then be passed in one of the
     "setPixelColor()" functions. Like "gamma8()", this uses a fixed gamma correction 
     exponent of 2.6, which seems reasonably okay for average LED drivers in average
     tasks. If you need finer control you'll need to provide your own gamma-correction
     function instead

   - all four bytes of a 32-bit value are filtered even if RGB (not WRGB), to avoid
     a bunch of shifting and masking that would be necessary for properly handling
     different endianisms (and each byte is a fairly trivial operation, so it might
     not even be wasting cycles vs a check and branch for the RGB case). In theory
     this might cause trouble *if* someone's storing information in the unused most
     significant byte of an RGB value, but this seems exceedingly rare and if it's
     encountered in reality they can mask values going in or coming out
*/
/************************************************************************************/
uint32_t ESP32_WS281x::gamma32(uint32_t colorValue)
{
  uint8_t *y = (uint8_t *)&colorValue;

  for (uint8_t i = 0; i < 4; i++)
  {
    y[i] = gamma8(y[i]);
  }

  return colorValue; //packed 32-bit return
}