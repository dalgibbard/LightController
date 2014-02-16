/*
 * Name:	ArcadiaClassicaOTL-controller
 * Author:      Darren Gibbard
 * Original Name:      LightController.ino
 * Original Author:    User "benjaf" at plantedtank.net forums
 * URL:		https://github.com/benjaf/LightController
 * This is an edited version of Benjaf's original code, specifically for replacing
 *     the original Arcadia Classica OTL LED Controller Unit (which sucked.)
 * From here on out, the Lighting unit will be referred to as "The OTL" to save typing in comments.
 
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. 
 */

/*
 =================
 |  ** DETAIL ** |
 =================

NOTE: References to "Front" and "Back" for the Blue/White LED channels assume that the OTL Lighting Unit has the power cable
      coming out of the left hand side, when viewed from the front.

* The OTL has four Lighting Channels:
  ------------------------------------------------------------------------------------------------------------------------------------------
  |  LIGHT UNIT LABEL  |   CABLE COLOUR  |  DB9 PIN (MALE) |  CONTR. CHAN. NO.  |       Light        |   CODED CHANNEL NO. |  Arduino Pin  |
  ------------------------------------------------------------------------------------------------------------------------------------------
  |   Channel A        |     Yellow      |       7         |        1           |  Blue/White Front  |   channelsNo = 0    |        5      |
  |   Channel B        |     Green       |       2         |        2           |  Actinic Blue (3W) |   channelsNo = 1    |        6      | 
  |   Channel C        |     Blue        |       6         |        3           |  Day White (10W)   |   channelsNo = 2    |        9      |
  |   Channel D        |     White       |       1         |        4           |  Blue/White Back   |   channelsNo = 3    |       10      |
  ------------------------------------------------------------------------------------------------------------------------------------------
     ### NOTE: The Arduino Pins were chosen based on the Arduino Leonardo - these are all 490Hz PWM Pins.
  
 * Full DB9 Male Pinout:
 ----------------------------------------------------------
 |   PIN No.      |  Cable Colour  |   Function           |
 ----------------------------------------------------------
 |      1         |     White      |  LED Unit PWM Chan D |
 |      2         |     Green      |  LED Unit PWM Chan B |
 |      3         |     Red        |  POWER 24V (Shared)  |
 |      4         |     Black      |  GROUND    (Shared)  |
 |      5         |     Black      |  GROUND    (Shared)  |
 |      6         |     Blue       |  LED Unit PWM Chan C |
 |      7         |     Yellow     |  LED Unit PWM Chan A |
 |      8         |     Red        |  POWER 24V (Shared)  |
 |      9         |     Black      |  GROUND    (Shared)  |
 -----------------------------------------------------------
 
*/

// ----------------------- RTC Library -----------------------
// Use Wire and RTClib (https://github.com/adafruit/RTClib):
// Please note that there are a significant differences between the original JeeLabs RTCLib and the Adafruit fork!
#include <Wire.h>
#include "RTClib.h"
#include "ChannelManager.h"
#include <avr/wdt.h>

// ----------------------- Constants -----------------------

const int MaxChannels = 4;   // Max number of channels, change if more or less are required
const int MaxPoints = 10;    // Max number of light intensity points, change if more or less are required

#define EMBEDDED_LED_PIN    13
byte ledState = LOW;
#define CHAN0_LED_PIN   2
#define CHAN1_LED_PIN   7
#define CHAN2_LED_PIN   8 
#define CHAN3_LED_PIN   12


// Change the below to "True" if you need to set the RTC time on next upload.
// NOTE: Be sure to Disable again immediately after, and re-Upload, else time will be errornously set on every restart
//       of the Arduino!!
const char* setTime = "False";

// ----------------------- Variables -----------------------
// RTC
RTC_DS1307 RTC;

// Time
DateTime CurrentTime;


// secs for Time Display
int secs = 0;
int errorCount = 0;
int intcheck = 0;

// ----------------------- Lights -----------------------

// Schedule Point format: (Hour, Minute, Intensity)
// Difference in intensity between points is faded/increased gradually
// Example: 
// 	Channels[0].AddPoint(8, 0, 0);
//	Channels[0].AddPoint(8, 30, 255);
//	Channels[0].AddPoint(11, 0, 255);
//  ...
//
// Explanation:
//  00:00 - 08:00 -> Light OFF
//  08:00 - 08:30 -> Increment light towards Fully ON
//  08:30 - 11:00 -> Light Fully ON
//
// Min intensity value: 0 (OFF)
// Max intensity value: 255 (Fully ON)
//
// If only 1 point is added, channel always maintains value of that point
//
// There is the option of which fade mode to use.
// Basically there are 2 modes: Linear and Exponential
// - Linear is what you would expect - 50% on = 50% duty cycle.
// - Exponential is an estimation of how our eyes react to brightness. The 'real' formula is actually inverse logarithmic,
// 	 but for our use exponential is close enough (feel free to add more fade modes if you disagree) - and much faster to compute!

Channel Channels[MaxChannels];
Point Points[MaxChannels][MaxPoints];

// Add more timing definitions here if more channels are added:
// ***CHANGE***
void InitializeChannels(int channels) {
  
  /* ======================
      *** IMPORTANT NOTE:
     ======================
            * The original controller does no more than 80% Duty Cycle for MAXIMUM Brightness!
              Where Valid power levels are 0 - 255 in the code below, this is equivilent to a Maximum of:  ( 255 / 100 ) * 80 = 204
            * It is advisable to use the following Number Ideals:
                * OFF  = 0
                * 10%  = 20
                * 25%  = 51
                * 50%  = 102
                * 75%  = 153
                * 100% = 204
  */
                	
  /* ========================
      Current Lighting Plan:
     ========================
       Front Mixed:  13:00 - 14:30 @ 0%-80% // 14:30 - 16:30 @ 80%-0%
       Actinic:      11:00 - 11:30 @ 0%-25% // 11:30 - 15:00 @ 25%-75% // 15:00 - 21:30 @ 75%-25% // 21:30 - 22:00 @ 25%-0%      # All day 11hrs - Varied Power; Short ramp to 25%, gradual to 75% max.  4hr @ MAX 
       Daylight:     13:00 - 15:00 @ 0%-60% // 15:00 - 16:00 @ 60% // 16:00 - 17:00 @ 60%-80% // 17:00 - 18:15 @ 80 - 60%  // 18:15 - 20:15 @ 60%-0%
       Back Mixed:   15:15 - 18:45 @ 0-80% // 18:45 - 21:15 @ 80%-0%  
   */
  
	// Channel 0: Blue/White Front
	int channelNo = 0;	// Currently editing channel 0
	int pin = 6;		// Channel 0 uses pin 6
	Channels[channelNo] = Channel(pin, MaxPoints, fademode_linear, Points[channelNo]);	// Initialize channel and choose FadeMode
	Channels[channelNo].AddPoint(13, 0, 0);
	Channels[channelNo].AddPoint(14, 45, 204);
	Channels[channelNo].AddPoint(16, 30, 102);
	Channels[channelNo].AddPoint(18, 15, 0);
	
	// Channel 1: Actinic Blue
	channelNo = 1;	// Currently editing channel 1
	pin = 9;		// Channel 1 uses pin 9
	Channels[channelNo] = Channel(pin, MaxPoints, fademode_linear, Points[channelNo]);
	Channels[channelNo].AddPoint(11, 0, 0);
	Channels[channelNo].AddPoint(13, 0, 102);
	Channels[channelNo].AddPoint(14, 45, 153);
	Channels[channelNo].AddPoint(18, 15, 153);
	Channels[channelNo].AddPoint(20, 0, 102);
        Channels[channelNo].AddPoint(21, 30, 51);
	Channels[channelNo].AddPoint(22, 0, 0);

        // Channel 2: Daylight White
        channelNo = 2;  // Currently editing channel 2
        pin = 10;		// Channel 2 uses pin 10
        Channels[channelNo] = Channel(pin, MaxPoints, fademode_linear, Points[channelNo]);
	Channels[channelNo].AddPoint(14, 45, 0);
	Channels[channelNo].AddPoint(16, 30, 164);
	Channels[channelNo].AddPoint(18, 15, 0);

        // Channel 3: Blue/White Back
        channelNo = 3;  // Currently editing channel 3
        pin = 11;		// Channel 3 uses pin 11
        Channels[channelNo] = Channel(pin, MaxPoints, fademode_linear, Points[channelNo]);
	Channels[channelNo].AddPoint(14, 45, 0);
	Channels[channelNo].AddPoint(16, 30, 102);
	Channels[channelNo].AddPoint(18, 15, 204);
	Channels[channelNo].AddPoint(20, 0, 0);
}

// ----------------------- Functions -----------------------
long lastUpdateTime = 0;

// Update light intensity values
void UpdateLights(DateTime currentTime)
{
	long now = Seconds(currentTime.hour(), currentTime.minute(), currentTime.second());	// Convert current time to seconds since midnight
	if(now != lastUpdateTime)  	// Perform update only if there is a perceivable change in time (no point wasting clock cycles otherwise)
	{
           // Set out Time/Date display values.
           int nowHour = currentTime.hour();
           int nowMin = currentTime.minute();
           int nowDay = currentTime.day();
           int nowMonth = currentTime.month();
                
                for(int channel = 0; channel < MaxChannels; channel++)    		// For each Channel
		{
                        // Set the PWM Duty Cycle
			analogWrite(Channels[channel].GetPin(), Channels[channel].GetLightIntensityInt(now));	// Get updated light intensity and write value to pin (update is performed when reading value)
		}

            // Check CHAN0 LED Status
            intcheck = Channels[0].GetLightIntensityInt(now);
            if (intcheck > 0){
              digitalWrite(CHAN0_LED_PIN, HIGH);
            } else {
              digitalWrite(CHAN0_LED_PIN, LOW);
            }
            // Check CHAN1 LED Status
            intcheck = Channels[1].GetLightIntensityInt(now);
            if (intcheck > 0){
              digitalWrite(CHAN1_LED_PIN, HIGH);
            } else {
              digitalWrite(CHAN1_LED_PIN, LOW);
            }
            // Check CHAN2 LED Status
            intcheck = Channels[2].GetLightIntensityInt(now);
            if (intcheck > 0){
              digitalWrite(CHAN2_LED_PIN, HIGH);
            } else {
              digitalWrite(CHAN2_LED_PIN, LOW);
            }
            // Check CHAN3 LED Status
            intcheck = Channels[3].GetLightIntensityInt(now);
            if (intcheck > 0){
              digitalWrite(CHAN3_LED_PIN, HIGH);
            } else {
              digitalWrite(CHAN3_LED_PIN, LOW);
            }
            
	lastUpdateTime = now;
      }
}


void PrintTimeToSerial (DateTime currentTime){
	
        // Print the Time now:
        if (int(currentTime.second()) != int(secs)){
           if (currentTime.day() < 10){
               Serial.print("0");
           }
           Serial.print(currentTime.day(), DEC);
           Serial.print('/');
           if (currentTime.month() < 10){
               Serial.print("0");
           }
           Serial.print(currentTime.month(), DEC);
           Serial.print('/');
           Serial.print(currentTime.year(), DEC);
           Serial.print(' ');
           if (currentTime.hour() < 10){
               Serial.print("0");
           }
           Serial.print(currentTime.hour(), DEC);
           Serial.print(':');
           if (currentTime.minute() < 10){
               Serial.print("0");
           }
           Serial.print(currentTime.minute(), DEC);
           Serial.print(':');
           if (currentTime.second() < 10){
               Serial.print("0");
           }
           Serial.print(currentTime.second(), DEC);
           Serial.println();
           secs = currentTime.second();
        }
}

void Error(){
      int var = 0;
      //// CODE HERE
      for(int var = 0; var < 5; var++)
      {
        digitalWrite(EMBEDDED_LED_PIN, HIGH);
        digitalWrite(CHAN0_LED_PIN, HIGH);
        digitalWrite(CHAN1_LED_PIN, HIGH);
        digitalWrite(CHAN2_LED_PIN, HIGH);
        digitalWrite(CHAN3_LED_PIN, HIGH);
        delay(1000);
        digitalWrite(EMBEDDED_LED_PIN, LOW);
        digitalWrite(CHAN0_LED_PIN, LOW);
        digitalWrite(CHAN1_LED_PIN, LOW);
        digitalWrite(CHAN2_LED_PIN, LOW);
        digitalWrite(CHAN3_LED_PIN, LOW);
        delay(1000);
      }     
 }

// Convert HH:mm:ss -> Seconds since midnight
long Seconds(int hours, int minutes, int seconds) {
	return ((long)hours * 60 * 60) + (minutes * 60) + seconds ;
}

// ----------------------- Setup -----------------------
void setup() {        
        // Setup Serial Interface
        Serial.begin(9600);
        
        // Setup onboard LED
        pinMode(EMBEDDED_LED_PIN, OUTPUT);
        pinMode(CHAN0_LED_PIN, OUTPUT);
        pinMode(CHAN1_LED_PIN, OUTPUT);
        pinMode(CHAN2_LED_PIN, OUTPUT);
        pinMode(CHAN3_LED_PIN, OUTPUT);
        digitalWrite(EMBEDDED_LED_PIN, LOW);
        digitalWrite(CHAN0_LED_PIN, LOW);
        digitalWrite(CHAN1_LED_PIN, LOW);
        digitalWrite(CHAN2_LED_PIN, LOW);
        digitalWrite(CHAN3_LED_PIN, LOW);
      
	// Initialize channel schedules
	InitializeChannels(MaxChannels);

	// Clock
	Wire.begin();
	RTC.begin();
    
        // If the user needs to set the RTC Time and has set the Var at the start, do it:
        if( setTime == "True" ){
            RTC.adjust(DateTime(__DATE__, __TIME__));  // Set RTC time to sketch compilation time, only use for 1 (ONE) run. Will reset time at each device reset!
        }
}

void Reboot(){
  wdt_enable(WDTO_15MS);
  while(1){
  }
}

// ----------------------- Loop -----------------------
void loop() {
	// Get current time
	CurrentTime = RTC.now();
	
        // For Debugging Time based issues Via Serial
        PrintTimeToSerial(CurrentTime);
        
        // Capture unexpected Clock issues
        if (CurrentTime.second() > 59){
           // If we've noticed that the seconds are an invalid value, then we've probably
           // hit the dreaded "DS1307 165" Error due to an errornous board!
           // Blink the LCD via the "Error()" function, and Reboot if weve had the issue five times.
           
           Error();
           if (errorCount > 4){
             Reboot();
           }
           errorCount++;
        }
        else{
	   // Update lights
	   UpdateLights(CurrentTime);
        }
}
