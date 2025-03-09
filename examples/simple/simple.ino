/***************************************************************************************************/
/*
   This is simple sketch that uses the Espressif SoC's RMT peripheral to control
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


#include <Arduino.h>
#include <WiFi.h>

#include <ESP32_WS281x.h>


#define DIN_PIN    13  //gpio pin connected to WS281x DIN (data signal input) pin
#define LED_QNT    8   //quantity of WS281x LEDs


ESP32_WS281x pixels(LED_QNT, DIN_PIN, LED_GRB);


void setup()
{
  delay(500);             //helps with boot issues on some boards

  WiFi.persistent(false); //disable saving WiFi settings in SPI flash
  WiFi.mode(WIFI_OFF);    //disable radio (STA off & softAP off)
  WiFi.persistent(true);  //enable saving WiFi settings in SPI flash

  pixels.begin(); //initialize WS281x strip object, REQUIRED!!!
  pixels.clear(); //set all pixel colors in the buffer to "off"
  pixels.show();  //send updated pixel color buffer to LED drivers

  pixels.setPixelColor(0, 255, 0, 0); //set 1-st pixel in strip is #0 in buffer to red, RGB(255, 0, 0)
  pixels.setPixelColor(2, 0, 255, 0); //set 3-rd pixel in strip is #2 in buffer to green, RGB(0, 255, 0)
  pixels.setPixelColor(4, 0, 0, 255); //set 5-th pixel in strip is #4 in buffer to blue, RGB(0, 0, 255)
  pixels.setPixelColor(6, 128,0,128); //set 7-th pixel in strip is #6 in buffer to purple, RGB(128,0,128)
  pixels.show();                      //send updated pixel color buffer to LED drivers
  
  delay(2000);
}


void loop()
{
  pixels.clear(); //set all pixel colors in the buffer to "off"

  //'for loop' for each pixel in strip
  for (int i=0; i < LED_QNT; i++) //1-st WS281x LED in strip is #0, 2-nd is #1, all the way up
  {
    //"pixels.color()" takes RGB value from 0,0,0 up to 255,255,255 and conver to uint32 value for "setPixelColor()"
    //e.g. red(255,0,0), green(0,255,0), blue(0,0,255), yellow(255,255,0), purple(128,0,128)

    pixels.setPixelColor(i, pixels.color(0, 150, 0));  //here we're using a 'moderately bright green color'

    pixels.show(); //send updated color buffer of pixels to LED drivers

    delay(100); //pause before next pass through 'for loop'
  }

  pixels.clear(); //set all pixel colors in the buffer to "off"

  pixels.setBrightness(125); //value 0(off)..255(max), call before "setPixelColor()"

  //human eye is more sensitive to lower intensities, gamma correction aims to adjust the intensity of a pixel's output
  //such that it matches how a human eye might view the change

  pixels.setPixelColor(0, pixels.color(pixels.gamma8(0), pixels.gamma8(150), pixels.gamma8(0))); //gamma corrected 'moderately bright green color' after we changed "setBrightness()"  

  pixels.show(); //send updated color buffer of pixels to LED drivers

  delay(2000); //pause before next pass through 'main loop'

  pixels.setBrightness(255); //value 0(off)..255(max), call before "setPixelColor()"
}
