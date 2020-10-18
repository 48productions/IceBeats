/*****************************
 * ICEBEATS MUSIC VISUALIZER *
 ****************************/

#define FASTLED_ALLOW_INTERRUPTS 0 //Seems to be required for longer strips (required for 90 LEDs, but not for 60)
#include "FastLED.h"

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include <Bounce2.h>
#include "RunningAverage.h"
#include <Keyboard.h>



/*****************
 * CONFIGURATION *
 ****************/
#define STRIP_LENGTH 90 //Number of LEDs in the strip
#define DATA_PIN_STRIP 5 //Pin connected to the LED strip

#define DEBUG_FFT_BINS true //Set true to test FFT section responsiveness - bin sections are mapped to the brightness of specific pixels

#define DEBUG_BURNIN false //Set to true to enable the light test at boot and disable timing out of it

#define PIN_BASS_LIGHT 6 //PWM-able light for the bass
#define PIN_BASS_SEL_1 7 //My setup uses an LED light string where alternating LEDs are driven by reverse polarities (drive at +29V for even lights, -29V for odd lights). These are driven using an H-bridge, these pins control the direction the lights are driven in.
#define PIN_BASS_SEL_2 8

#define PIN_DEBUG_0 17
#define PIN_DEBUG_1 14
#define PIN_DEBUG_2 15

#define PIN_LIGHTS_LAT 3 //Pins for the latch, clock, and data lines for the lighting shift registers
#define PIN_LIGHTS_CLK 2
#define PIN_LIGHTS_DAT 4

const int pinLightsMarquee[4] = {20, 21, 22, 23}; //PWM-able pins for the 4 marquee lights

// STEPMANIA IO (Keystrokes)
//  Inputs we currently send: Service, Vol Up, Vol Down
//  This code automagically handles any keys you add to this list, just remember to assign it a keycode, a pin, and add entries to keyIOPressed[] as needed!

const int keyIOCodes[] = {'`', -1, -2}; //List of keycodes we can send to the PC this is plugged into
const int keyIOPins[] = {12, 10, 11}; //List of IO pins we should read to send these above keycodes
bool keyIOPressed[] = {false, false, false}; //Is xyz key currently pressed?



/*************************************************
 * EFFECT CONFIG (AUTO GENERATED AT COMPILE TIME * 
 ************************************************/

const int STRIP_HALF = STRIP_LENGTH / 2; //Convinience variables for half/4th/etc of the strip length
const int STRIP_FOURTH = STRIP_LENGTH / 4;
const int STRIP_8TH = STRIP_LENGTH / 8;
const int STRIP_16TH = STRIP_LENGTH / 16;
const int STRIP_32ND = STRIP_LENGTH / 32;

const int PULSE_MAX_SIZE_BASS = STRIP_LENGTH / 6; //Maximum section sizes for VE Pulse (based on strip length)
const int PULSE_MAX_SIZE_MID = STRIP_LENGTH / 6;
const int PULSE_MAX_SIZE_HIGH = STRIP_LENGTH / 8;

const int PUNCH_MAX_SIZE_BASS = STRIP_LENGTH / 6; //Maximum section sizes for VE Punch
const int PUNCH_MAX_SIZE_LOW = STRIP_LENGTH / 8;
const int PUNCH_MAX_SIZE_MID = STRIP_LENGTH / 8;
const int PUNCH_MAX_SIZE_HIGH = STRIP_LENGTH / 8;

const int KICK_MAX_SIZE_BASS = STRIP_LENGTH / 6; //Maximum section sizes for VE Kick
const int KICK_MAX_SIZE_SECTION = STRIP_LENGTH / 8;

const int VOLUME_MAX_SIZE_BASS = STRIP_LENGTH / 4; //Maximum section sizes for VE Volume
const int VOLUME_MAX_SIZE_LOW = STRIP_LENGTH / 10;
const int VOLUME_MAX_SIZE_MID = STRIP_LENGTH / 10;
const int VOLUME_MAX_SIZE_HIGH = STRIP_LENGTH / 10;

const int PITCH_SECTION_SIZE = STRIP_HALF / 3; //Section size for VE Pitch's palette gradients
const double PITCH_BIN_EXP = log10(511) / STRIP_HALF; //Magic number used to help calculate the number of FFT bins per pixel in VE Pitch (More documentation below)



 /**********************
  * HARDWARE VARIABLES *
  *********************/

//Audio setup: Uses ADC for input, then uses the magic of software for the following steps:
// ADC goes into amp, amp output goes into objects to detect audio peaks and FFT analysis
  
// GUItool: begin automatically generated code
AudioInputAnalog         adc;           //xy=274,198
AudioAmplifier           amp;           //xy=333,187
AudioAnalyzePeak         peak;          //xy=431,178
AudioAnalyzeFFT1024       fft;       //xy=436,222
AudioConnection          patchCord1(adc, amp);
AudioConnection          patchCord2(amp, fft);
AudioConnection          patchCord3(amp, peak);
// GUItool: end automatically generated code

CRGB leds[STRIP_LENGTH];

Bounce debug0 = Bounce(PIN_DEBUG_0, 5); //DEBUG 0: Lighting test button
Bounce debug1 = Bounce(PIN_DEBUG_1, 5); //DEBUG 1: Swap visualization effect
Bounce debug2 = Bounce(PIN_DEBUG_2, 5); //DEBUG 2: Force getBassKickProgress() (can also force all FFT section reads, uncomment below)

RunningAverage averagePeak(10); //The average measured peak, used for AGC
float curGain = 1; //Current gain to use, used for AGC

bool heartbeatState = LOW; //Is the heartbeat LED on or off?
unsigned long lastHeartbeatFlipMs = 0; //Last time the heartbeat LED changed states
 

/***************************
 * VISUALIZATION VARIABLES *
 **************************/

enum VisualizationEffect { //A list of all visualization effects we can use - described in their individual functions
  VEDebugFFT,
  VEKick,
  VEPunch,
  VEPulse,
  VEVolume,
  VEPitch
};

const int MAX_VE = 5; //Maximum effect number we can use


const CHSV palettes[][4] = { //List of HSV color palettes to use for visualization
  {CHSV(165, 240, 255), CHSV(128, 230, 255), CHSV(128, 180, 255), CHSV(128, 40, 255) }, //Palette 0: Blue, Aqua, Light Aqua, Light Aqua/white
  {CHSV(185, 255, 255), CHSV(160, 190, 235), CHSV(200, 165, 235), CHSV(192, 65, 255) }, //Palette 1: Purple, med blue, med purple, light purple
  {CHSV(96, 200, 200), CHSV(105, 190, 215), CHSV(115, 180, 235), CHSV(120, 21, 255) }, //Palette 2: Green, med green, light green, light/white green
  {CHSV(192, 240, 200), CHSV(160, 220, 215), CHSV(128, 190, 235), CHSV(128, 60, 255) }, //Palette 3: Purple, blue, aqua, light aqua
  {CHSV(0, 220, 200), CHSV(32, 220, 215), CHSV(64, 210, 235), CHSV(64, 50, 255) }, //Palette 4: Red, orange, yellow, light yellow
  {CHSV(32, 220, 215), CHSV(50, 180, 200), CHSV(64, 210, 235), CHSV(64, 50, 255) }, //Palette 5: Orange, Dk Yellow, Yellow, Light yellow
  };

const int MAX_PALETTE = sizeof(palettes) / sizeof(palettes[0]) - 1; //Maximum palette number we can use

VisualizationEffect curEffect = VEPunch; //Current visualization in use

int curPaletteIndex = 0; //Current palette index in use
CHSV curPalette[4] = palettes[0]; //Current palette colors in use
CHSV curPaletteDim[4] = palettes[0]; //A slightly dimmed version of the current palette, used for "pulse to the beat" effects

unsigned long curMillis = 0; //Current time

RunningAverage bassChangeAverage(33); //Running average of detected bass changes, used for bass kick detection
RunningAverage shortBassAverage(3); //Shorter running average of the bass, smoothes out the wrinkles so we get  s m o o t h  l i n e s
RunningAverage shortNotBassAverage(9); //Short running average of the higher frequency bins (a.k.a. notBass), also used for bass kick detection
RunningAverage longNotBassAverage(60); //Longer running average of the NotBass (old = 70)
float longNotBassAverageHistory[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; //History of our longNotBassAverage, used to detect changes in songs/parts of a song for pallete changes
float lastShortBassAverage; //The short bass average last time it was checked, also used for bass kick detection
float lastBassChange; //The last bass change last time it was checked. You know the drill.
bool bassChangeIncreasing = true; //Is the bass change increasing or decreasing? Helps detect when it peaks.
unsigned long lastBassKickMillis = 0; //Last time a bass kick happened
RunningAverage bassKickLengthAverage(2); //Yet another running average the time between bass kicks
bool isAlternateBassKick = false; //Alternates every time the bass is kicked, used for the alternating bass lights
short bassBrightness; //Track the current and last brightnesses for the bass light, used for fading it out when music/idle animations stop
short lastBassBrightness = 0;
bool idleBassBrightnessDecreasing = false; //Is the bass brightness value increasing or decreasing?


short bass_scroll_pos = 0; //Pulse/punch effects - The bass kick scroll's position and color
CHSV bass_scroll_color;

short sectionOffset = 1; //Kick effect - How many indexes should we offset the sections?
short avgBassSize = 0; //Kick - The average size the bass is
short scrollOffset = 0; //Kick - How many pixels should we offset our drawing by to scroll the sections?
unsigned long lastScrollUpdateMillis = 0; //Kick - When did we last update the scroll?


unsigned long nextIdleStartMillis = 0; //The millis time to start the next idle animation at, calculated when the audio falls quiet or an idle anim starts
bool idling = false; //Are we currently idling? If so, don't render new visualization data on the strip
bool prevIdling = false; //Were we idling last time we checked if we were idling?
short idlePos = 0; //Position we are in the current idle animation - Animation is played when this is >= 1 and idling == true
unsigned long nextIdleUpdateMillis = 0; //The next time we should update the idle animation at
const short idleUpdateMs = 25; //How often should we update the idle animation?

short idleCurHue = 0; //Current hue, used for idle effects

bool lightTestEnabled = DEBUG_BURNIN; //If we're currently in the lighting test or not, and when we entered it (default to whether we're running a burn-in or not)
unsigned long lightTestStartMillis = 0; //Also track some timestamps of when some updates last occured
unsigned long lightTestLastToggleMillis = 0;
unsigned long lightTestLastStripUpdateMillis = 0;
short lightTestStripPos = 0; //...and some other variables to track the state of the LED strip test
short lightTestStripColor = 0;


/**************************
 * STEPMANIA IO VARIABLES * 
 *************************/

//This code also works as an IO board for Stepmania-converted dance cabinets. These variables handle all that fun stuff:
const int maxKeycode = sizeof(keyIOCodes) / sizeof(keyIOCodes[0]) - 1; //Maximum keycode number we'll scan for

byte receivedData = 0; //The byte of serial data we just got
int lightBytePos = 0; //How many bytes of the 13 bytes of light data have we received?

byte cabLEDs = 0; //Cab lights byte (4x marquee, 4x menu buttons)
byte padLEDs = 0; //Pad lights byte (4x pad led per player)
byte etcLEDs = 0; //Etc lights byte (2x bass light, room for expansion/modification)

bool marqueeOn[4] = {false, false, false, false}; //The current state and brightness of the 4 marquee lights
short marqueeBrightness[4] = {0, 0, 0, 0};



/********************
 * FUNCTIONS N SHIZ *
 *******************/


void setup() {
  Keyboard.begin();
  
  AudioMemory(16);
  fft.windowFunction(AudioWindowHamming1024);
  
  Serial.begin(9600);
  LEDS.addLeds<WS2812, DATA_PIN_STRIP, GRB>(leds, STRIP_LENGTH);
  LEDS.setBrightness(84);

  pinMode(PIN_DEBUG_0, INPUT_PULLUP); //Pinmode ALL the pins
  pinMode(PIN_DEBUG_1, INPUT_PULLUP);
  pinMode(PIN_DEBUG_2, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(DATA_PIN_STRIP, OUTPUT);
  pinMode(PIN_BASS_LIGHT, OUTPUT);
  pinMode(PIN_BASS_SEL_1, OUTPUT);
  pinMode(PIN_BASS_SEL_2, OUTPUT);
  
  pinMode(PIN_LIGHTS_LAT, OUTPUT);
  pinMode(PIN_LIGHTS_DAT, OUTPUT);
  pinMode(PIN_LIGHTS_CLK, OUTPUT);

  for (int i = 0; i <= 3; i++) { //Pinmode all the marquee lights
    pinMode(pinLightsMarquee[i], OUTPUT);
  }
  
  for (int i = 0; i <= maxKeycode; i++) { //Pinmode all the cabinet buttons we're gonna read
    pinMode(keyIOPins[i], INPUT_PULLUP);
  }

  setNewPalette(0); //Initialize palette-related variables

  bassChangeAverage.clear(); //Clear out running averages for the bass
  shortBassAverage.clear();
  shortNotBassAverage.clear();
  bassChangeAverage.addValue(0); //And add some initial data to the peak average
  averagePeak.clear();
  bassKickLengthAverage.clear();
}


void loop() {
  curMillis = millis();
  
  debug0.update();
  debug1.update();
  debug2.update();

  if (debug2.fell()) { //Debug 2 pressed, swap to a new color palette
    setNewPalette(curPaletteIndex + 1);
  }

  if (debug1.fell()) { //Debug 1 pressed, swap to a new visualization effect
    setNewVE(curEffect + 1);
  }

  if (debug0.fell()) { //Debug 0 pressed, toggle lighting test mode
    lightTestEnabled = !lightTestEnabled;
    if (lightTestEnabled) { //Going into the light test, set some variables to prepare for it
      lightTestStartMillis = curMillis;
      analogWrite(PIN_BASS_LIGHT, 255);
      lightTestStripPos = 0;
      lightTestStripColor = 0;
      FastLED.clear(); //Clear the LED strip for the test:tm:
    }
  }

  if (Serial.available() > 0) { //Do we have lights data to receive?
    while (Serial.available() > 0) { //While we have lights data to receive, receive and process it!
      receivedData = Serial.read(); //Read the next byte of serial data
      //Serial.println(receivedData);
      if (receivedData == '\n') { //If we got a newline (\n), we're done receiving new light states for this update
        lightBytePos = 0; //The next byte of lighting data will be the first byte
      } else {
        
        if (!lightTestEnabled) { //Don't process serial data if we're in the lighting test (just receive it)
          switch (lightBytePos) { //Which byte of lighting data are we now receiving?
            case 0: //First byte of lighting data (cabinet lights)
              //Read x bit from the received byte using bitRead(), then set the corresponding light to that state
              bitWrite(cabLEDs, 7, bitRead(receivedData, 0)); //Marquee up left
              bitWrite(cabLEDs, 6, bitRead(receivedData, 1)); //Marquee up right
              bitWrite(cabLEDs, 5, bitRead(receivedData, 2)); //Marquee down left
              bitWrite(cabLEDs, 4, bitRead(receivedData, 3)); //Marquee down right
              /*for (int light = 0; light <= 3; light++) { //Iterate through the 4 marquee lights
                marqueeOn[light] = bitRead(receivedData, light);
              }*/
              bitWrite(etcLEDs, 7, bitRead(receivedData, 4)); //Bass L
              bitWrite(etcLEDs, 6, bitRead(receivedData, 5)); //Bass R
              //bitWrite(cabLEDs, 7, bitRead(receivedData, 4)); //Bass L
              //bitWrite(cabLEDs, 6, bitRead(receivedData, 5)); //Bass R
              break;
            case 1: //Second byte of lighting data (P1 menu button lights)
              bitWrite(cabLEDs, 3, bitRead(receivedData, 0)); //P1 menu left
              bitWrite(cabLEDs, 2, bitRead(receivedData, 4)); //P1 start
              
              //bitWrite(cabLEDs, 5, bitRead(receivedData, 5)); //P1 select
              break;
            case 3: //Byte 4: P1 pad lights
              bitWrite(padLEDs, 4, bitRead(receivedData, 0)); //P1 Pad
              bitWrite(padLEDs, 7, bitRead(receivedData, 1));
              bitWrite(padLEDs, 6, bitRead(receivedData, 2));
              bitWrite(padLEDs, 5, bitRead(receivedData, 3));
              break;
            case 7: //Byte 8: P2 menu
              bitWrite(cabLEDs, 1, bitRead(receivedData, 0)); //P2 menu left
              bitWrite(cabLEDs, 0, bitRead(receivedData, 4)); //P2 start

              //bitWrite(cabLEDs, 4, bitRead(receivedData, 5)); //P2 select
              break;
            case 9: //Byte 10: P2 pad
              bitWrite(padLEDs, 3, bitRead(receivedData, 0)); //P2 Pad
              bitWrite(padLEDs, 0, bitRead(receivedData, 1));
              bitWrite(padLEDs, 2, bitRead(receivedData, 2));
              bitWrite(padLEDs, 1, bitRead(receivedData, 3));
              break;
          }
          lightBytePos++; //Finally, update how many bytes of lighting data we've received.
        }
      }
    }

    writeCabLighting(); //When we're done processing the serial data we have right now, write it to the shift registers!
  }

  if (lightTestEnabled) { //In the lighting test, update some lights boi
    if (curMillis - lightTestStartMillis >= 300000 && !DEBUG_BURNIN)  { //A lotta time has passed since we started the light test (and we're not doing a burn-in), let's just exit it now
      lightTestEnabled = false;
    }
    
    if (curMillis - lightTestLastToggleMillis >= 500) { //500ms has passed, toggle the digital cabinet lights
      analogWrite(PIN_BASS_LIGHT, 255);
      alternateBassSel();
      lightTestLastToggleMillis = curMillis;
      
      if (isAlternateBassKick) { //Also alternate the cabinet lights as well, based on the state of the alternating bass kick thingy above
        cabLEDs = 255; padLEDs = 255; etcLEDs = 255;
      } else {
        cabLEDs = 0; padLEDs = 0; etcLEDs = 0;
      }

      for (int light = 0; light <= 3; light++) { //Iterate through the 4 marquee lights
        marqueeOn[light] = isAlternateBassKick;
      }
      writeCabLighting();
    }

    if (curMillis - lightTestLastStripUpdateMillis >= 25) { //Time to update the LED strip!
      lightTestLastStripUpdateMillis = curMillis;
      lightTestStripPos++;
      if (lightTestStripPos >= STRIP_LENGTH) { //Wiped a color onto the entire strip, reset to the beginning of the strip using a new color
        lightTestStripPos = 0;
        lightTestStripColor++;
        if (lightTestStripColor >= 8) { lightTestStripColor = 0; }
      }

      leds[lightTestStripPos] = getLightTestStripColor();
      FastLED.show();
    }
    
  } else { //Not in the lighting test, update visualizations/idles/etc
    if (peak.available()) { //We have peak data to read! READ IT!
      float curPeak = peak.read();
      
      //First, determine if we should be starting an idle animation (audio has been quiet for a while)
      idling = (averagePeak.getAverage() <= 0.15);
      if (idling == true && prevIdling == false) { //We just got a  f a l l i n g  i d l i n g  e d g e  (wat)
        nextIdleStartMillis = curMillis + 10000; //Start an idle anim in 10 seconds
        
      } else if (idling == false && prevIdling == true) { //We've stopped idling!
        idlePos = 0; //Stop running the idle animation, there's SOUDN (prevents running the idle anim the instant we get no sound again instead of waiting 10 seconds)
      }
  
      if (idling) {
        if (curMillis >= nextIdleStartMillis) { //Time to start an idle animation!
          nextIdleStartMillis = curMillis + 60000; //Next idle animation should start in 60 seconds
          idlePos = 1; //Kickstart the animation!
          idleCurHue = 0;
        }
      }
      prevIdling = idling;
  
      //Now let's handle Automagic Gain Control (todo: Is this even the right term in this context?)
      averagePeak.addValue(curPeak);
      
      if (averagePeak.getAverage() > 0.85) { //LOUD MUSIC
        curGain *= 0.98; //Reduce gain, update the amp!
        amp.gain(curGain);
        
      } else if (averagePeak.getAverage() < 0.85 && curGain <= 4) { //we quiet bois and we haven't gotten HYPER LOUD yet
        curGain *= 1.0015; //Slightly increase gain, update amp
        amp.gain(curGain);
      }

      /**
       * AGC SERIAL DEBUG
       * 
       * Use the Arduino serial monitor to fine-tune AGC values. Uncomment to print:
       *  - The average'd peak value
       *  - The raw, current peak value
       *  - The current gain
       *  - A straight line for the maximum peak
       */
      /*Serial.print(averagePeak.getAverage() * 2);
      Serial.print(" ");
      Serial.print(curPeak * 2);
      Serial.print(" ");
      Serial.print(curGain * 2);
      Serial.print(" ");
      Serial.println(2);*/
      
      
      if (curEffect == VEDebugFFT) { //Debugging the FFT? Map the peak to the first pixel!
        leds[0] = CHSV(0, 0, curPeak * 255);
      }
    }
  
    if (!idling && fft.available()) { //FFT has new data and we aren't idling, let's visualize it!
     //bassChangeAverage.addValue(getFFTSection(0));
      shortBassAverage.addValue(getFFTSection(0)); //Add some FFT bins to our averages for beat detection
      float notBassDelta = updateNotBassAverages();
      
      //DEBUG: Print FFT bins to serial
      Serial.print(String(getFFTSection(0) * 5) + " ");
      Serial.print(String(getFFTSection(1) * 5) + " ");
      Serial.print(String(getFFTSection(2) * 5) + " ");
      Serial.print(String(getFFTSection(3) * 5) + " ");
      Serial.print(String(longNotBassAverage.getAverage() * 3) + " ");
      Serial.println(notBassDelta);
      
      if (notBassDelta >= 1.4) { //If we've gotten a drastic change in music, swap a new palette
        setNewPalette(curPaletteIndex + 1);
      }
      
      switch (curEffect) {
        case VEDebugFFT:
          visualizeFFTDebug();
          break;
        case VEPulse:
          visualizePulse();
          break;
        case VEPunch:
          visualizePunch();
          break;
        case VEKick:
          visualizeKick();
          break;
        case VEVolume:
          visualizeVolume();
          break;
        case VEPitch:
          visualizePitch();
          break;
      }
      FastLED.show();
      
    } else if (idling && idlePos >= 1 && curMillis >= nextIdleUpdateMillis) { //We're idling, playing the idle animation, and it's time for an update!
      nextIdleUpdateMillis = curMillis + idleUpdateMs;
      idlePos++;
  
      bassBrightness = abs(255 * sin(0.05 * idlePos)); //Let's also pulse on and off the bass light - calculate it's brightness
      if (bassBrightness > lastBassBrightness && idleBassBrightnessDecreasing) { alternateBassSel(); idleBassBrightnessDecreasing = false; } //Have we just started increasing the brightness? Flip which bass light is in use
      if (bassBrightness < lastBassBrightness && !idleBassBrightnessDecreasing) { idleBassBrightnessDecreasing = true; } //Also detect when we've just started decreasing, too
      analogWrite(PIN_BASS_LIGHT, bassBrightness); //Finally write the bass brightness and record the last used bass brightness
      lastBassBrightness = bassBrightness;
  
      idleRainbowWave();
      FastLED.show();
      
    } else if (idling && idlePos < 1) { //Not running a visualization or idle animation (either finished an idle or just stopped getting sound), just fade out the strip and bass lightsthis cycle
      fadeToBlackBy(leds, STRIP_LENGTH, 30);
      FastLED.show();
  
      if (bassBrightness > 0) { bassBrightness *= 0.8; analogWrite(PIN_BASS_LIGHT, bassBrightness); } //Did we not completely fade out the bass light during the last idle cycle? Let's keep fading it out here
    }
  
  }
  
  for (int i = 0; i <= maxKeycode; i++) { //Handle keypresses!
    bool buttonState = !digitalRead(keyIOPins[i]); //Read the state of this button, and invert it (they're active low)
    if (buttonState && !keyIOPressed[i]) { //Falling edge: We just pressed this button!
      if (keyIOCodes[i] < 0) { //Negative keycodes - special handling (a.k.a. do some volume control stuffs
        Keyboard.press(KEY_F3); //Volume is handled in SM by holding F3 and pressing R/T. Press F3 and pulse R/T here, and release F3 on volume button release
        switch (keyIOCodes[i]) {
          case -1:
            Keyboard.press('r');
            Keyboard.release('r');
            break;
          case -2:
            Keyboard.press('t');
            Keyboard.release('t');
            break;
        }
      } else {
        Keyboard.press(keyIOCodes[i]);
      }
      keyIOPressed[i] = true;

      
    } else if (!buttonState && keyIOPressed[i]) { //Rising edge: We just released this button!
      if (keyIOCodes[i] < 0) { //Negative keycodes - special handling (a.k.a. release F3 for volume control)
        Keyboard.release(KEY_F3);
      } else {
        Keyboard.release(keyIOCodes[i]);
      }
      keyIOPressed[i] = false;
    }
  }
  
  if (curMillis - lastHeartbeatFlipMs >= 500) { //500ms since the heartbeat LED toggled states, toggle it now!
    heartbeatState = !heartbeatState;
    lastHeartbeatFlipMs = curMillis;
    digitalWrite(LED_BUILTIN, heartbeatState);
  }

  /*for (int i = 0; i <= 3; i++) { //Handle smooth fading for the marquee lights   //Edit: Cab said no, begone!
    if (marqueeOn[i] && marqueeBrightness[i] < 255) { //If we still need to fade on this light...
      marqueeBrightness[i] = min((int)marqueeBrightness[i] + 40, 255); //...increment its brightness (but cap it at 255)
      Serial.println(marqueeBrightness[i]);
      analogWrite(pinLightsMarquee[i], marqueeBrightness[i]);
      
    } else if (!marqueeOn[i] && marqueeBrightness[i] > 0) { //What if we need to fade OFF this light?
      marqueeBrightness[i] = max((int)marqueeBrightness[i] - 40, 0); //...decrement its brightness (but prevent it from going below 0!)
      analogWrite(pinLightsMarquee[i], marqueeBrightness[i]);
    }
  }*/
  
  delay(10);
}


/**
 * Simple helper function to mirror the strip from the first half to the second
 */
void mirrorStrip() {
  for (int i = 0; i < STRIP_HALF; i++) {
    leds[STRIP_LENGTH - i - 1] = leds[i];
  }
}


/**
 * Alternate version of getFFTSection() with some exponential magic applied to hopefully make visual effects a bit more appealing
 * On louder songs, this makes subtle volume changes more distinct but tends to *really* mute quiet songs
 * TODO: Not quite ready for prime time, revisit implementing this in the future
 */
/*float getFFTSection(int id) {
  if (debug2.read()) {
    return pow(5, getRawFFTSection(id) - 1) * 1.25 - 0.25; //Extra multiplication/subtraction to center our exponential curve within the 0-1 range we need
    return pow(10, getRawFFTSection(id) - 1) * 1.1 - 0.1; //Extra multiplication/subtraction to center our exponential curve within the 0-1 range we need
  } else {
     return getRawFFTSection(id);
  }
}*/


/**
 * The FFT bins are divided into sections, color organ style.
 * Each section corresponds to a set of frequencies, return them here.
 * Always returns value between 0 and 1
 */
float getFFTSection(int id) {
  //if (!debug2.read()) { return 1; } //Holding debug 2? Make all FFT sections return their max value
  
  switch (id) { //Switch based on the id of the section to get. Each Teensy FFT bin is 43Hz wide, and we get groups of bins for each section.
    case 0: //Bass bin - 43Hz-86Hz
      return constrain(fft.read(1) * 2, 0, 1); //Todo: Constrain is jank is frig and WILL cut off the tops of audio waveforms on busier/louder songs
    case 1: //Low - 473-1.462kHz
      return constrain(fft.read(11, 34) / 1.25, 0, 1);
    case 2: //Mid - 1.505-5.031k
      return constrain(fft.read(35, 117) / 1.5, 0, 1);
    case 3: //High - 6.02k-12.04k
      return constrain(fft.read(140, 280), 0, 1);
    default:
      return 0;
  }
}


/**
 * Updates the long not bass (FFT section 2/3) averages
 * Returns the difference % between the current longNotBassAverage and the average from several updates ago - >1 means the average is getting bigger
 */
float updateNotBassAverages() {
  for (short section = 2; section <= 3; section++) { //Here, add bins 2 and 3 to the NotBass averages
    float fftSection = getFFTSection(section);
    shortNotBassAverage.addValue(fftSection);
    longNotBassAverage.addValue(fftSection);
  }

  for (short i = 0; i <= 8; i++) { //Now for the long average history: Roll all the averages back a position...
    longNotBassAverageHistory[i] = longNotBassAverageHistory[i + 1];
  }
  longNotBassAverageHistory[9] = longNotBassAverage.getAverage(); //Then add our new average to the top of the list
  return abs(longNotBassAverageHistory[9] / longNotBassAverageHistory[0]); //Finally, return the new value / the old value for a percent change
}
/**
 * Returns whether a bass kick has happened since the last call to getBassKicked()
 * Used for beat-based effects
 */
bool getBassKicked() {
  float curBassValue = shortBassAverage.getAverage(); //Get the current value (short average) and (longer) running average of the bass section
  float curNotBassValue = shortNotBassAverage.getAverage();
  //float curBassValue = getFFTSection(0);
  float curBassChange = max(curBassValue - lastShortBassAverage, 0); //Also find out how much the bass value changed this read
  //float curBassChange = abs(curBassValue - lastShortBassAverage); //Also find out how much the bass value changed this read
  //if (curBassChange > 0) { 
    bassChangeAverage.addValue(curBassChange);
  //}
  float curBassChangeAverage = bassChangeAverage.getAverage();
  float curLongNotBassValue = longNotBassAverage.getAverage();
  bool bassKicked = false;

  bool notBassBelowThreshold = curNotBassValue <= curLongNotBassValue * 0.89; //Check if our NotBass is below a certain threshold
  bool bassAboveThreshold = curBassChange >= curBassChangeAverage * (notBassBelowThreshold ? 3.35 : 3.49); //Check if our bass change is above a certain threshold (lower it slightly if notbass is low, or raise it if notbass is high) (Non-notbass ORIG: 3.45)

  if (bassAboveThreshold && curBassChangeAverage >= 0.007) { //Did our bass change enough above the average (and above an arbitrary "volume" threshold)?
    //bassChangeAverage.addValue(lastBassChange);
    bassKicked = true; 
  }
  
  lastBassChange = curBassChange; //Now store the last bass change and average for the next read
  lastShortBassAverage = curBassValue;


  /**
   * BEAT DETECTION SERIAL DEBUG
   * 
   * Use the Arduino serial monitor to fine-tune AGC values. Uncomment these and the two lines under if (bassKicked &&...) to print:
   *  - A lotta random garbage
  */
  /*Serial.print(curBassValue * 12); //DEBUG: Print the current bass bin's value
  Serial.print(" ");*/
  //Serial.print(curNotBassValue * 6);
  //Serial.print(" ");
  //Serial.print(curLongNotBassValue * 6);
  //Serial.print(" ");
  //Serial.print(notBassBelowThreshold ? 6 : 0);
  //Serial.print(" ");
  /*Serial.print(curBassChangeAverage * 48); //The average bass peak value
  Serial.print(" ");*/
  /*Serial.print(curBassChangeAverage * 48 * 3.35); //The threshold to trigger a bass kick
  Serial.print(" ");
  Serial.print(curBassChangeAverage * 48 * 3.49);
  Serial.print(" ");
  Serial.print(curBassChange * 48); //And the current amount the bass has changed this update
  Serial.print(" ");*/
  /*Serial.print(getFFTSection(0) * 6);
  Serial.print(" ");
  Serial.print(getFFTSection(1) * 6);
  Serial.print(" ");
  Serial.print(getFFTSection(2) * 6);
  Serial.print(" ");
  Serial.print(getFFTSection(3) * 6);
  Serial.print(" ");*/
  /*Serial.print(" ");
  Serial.println(abs((curBassValue - curBassAverage)));*/
  
  if (bassKicked && curMillis >= lastBassKickMillis + 200) { //We detected a new peak earlier and it's been a bit since a bass kick
    bassKickLengthAverage.addValue(curMillis - lastBassKickMillis); //Add the time between the last kick and now to the average bass kick length
    lastBassKickMillis = curMillis; //Start a new bass kick!
    //Serial.println("6");
    alternateBassSel(); //Invert the alternating bass kick variable, drive the bass light selection pins
    return true;
  }
  //Serial.println("0");
  return false;
}


/**
 * Returns true if the bass is currently kicking
 * Used for other beat-based effects
 */
float getBassKickProgress() {
  /*float curBassValue = getFFTSection(0); //Old implementation: Have we met similar criteria for a regular bass kick to happen?
  float curBassAverage = bassChangeAverage.getAverage();
  bool kicking = (curBassValue >= curBassAverage * 1.35 && curBassValue >= 0.15) || !debug2.read();*/
  //bool kicking = curMillis <= lastBassKickMillis + 150;

  //The current implementation is really just an extension of getBassKicked for effects that "pulse to the beat"
  //Right now, bass kicks "happen" for 1/3 of the average time between the last few bass kicks
  //This function returns how close we are to the start of the x ms-wide window after the bass kick happens (1 - just started kick, 0 - x ms has passed since kick)
  //Since this function pulls the time the last time getBassKicked() returned true, getBassKicked() MUST be called beforehand for this function to work!
  float kicking = 0;
  int kickLength = min(bassKickLengthAverage.getAverage() / 3, 400); //Find the average kick length here, cap it at 400ms so we don't get uber long bass kicks
  //Serial.println(kickLength);
  //if (curMillis <= lastBassKickMillis + 125) { kicking = 1 - ((float)(curMillis - lastBassKickMillis) / 125); } //Only calculate this value if we're still in the middle of the bass kick (and guaranteed to not return less than 0)
  if (curMillis <= lastBassKickMillis + kickLength) { kicking = 1 - ((float)(curMillis - lastBassKickMillis) / kickLength); } //Only calculate this value if we're still in the middle of the bass kick (and guaranteed to not return less than 0)
  
  //digitalWrite(LED_BUILTIN, kicking > 0); //Also light our debug LED while the bass kick is happening
  bassBrightness = kicking * 255; //Record the brightness of the bass light (for use with other functions)
  analogWrite(PIN_BASS_LIGHT, bassBrightness); //Also (also) write our bass output while we're at it
  
  //Serial.println(kicking);
  return kicking;
}


/**
 * Sets a new color palette to use
 */
void setNewPalette(int paletteId) {
  curPaletteIndex = wrapValue(paletteId, 0, MAX_PALETTE); //Wrap around palette values in case we lazily just give this function "curPalette + 1"
  //if (curPaletteIndex < 0) { curPaletteIndex = MAX_PALETTE; }
  //if (curPaletteIndex > MAX_PALETTE) { curPaletteIndex = 0; }
  memcpy(curPalette, palettes[curPaletteIndex], sizeof(curPalette)); //Copy the contents of the new palette to use to curPalette[]
  
  for (short i = 0; i <= 3; i++) { //Also set the current dim palette's values to be a dim version of the current palette
    curPaletteDim[i] = CHSV(curPalette[i].h, curPalette[i].s * 0.55, curPalette[i].v * 0.6);
  }

  if (curEffect == VEPulse) { //Visualizing pulse or punch, also set the color to use for bass kicks
    bass_scroll_color = CHSV(curPalette[0].h + 15, constrain(curPalette[0].s - 40, 0, 255), 130);
  } else if (curEffect == VEPunch) {
    bass_scroll_color = CHSV(curPalette[0].h + 10, constrain(curPalette[0].s - 40, 0, 255), 100);
  }
}


/**
 * Gets a non-bass color from the palette given a color index, wrapping around values (i.e. index 4 returns index 1's color, etc)
 */
CHSV getNonBassColor(short colorIndex) {
  /*while (colorIndex > 3) { colorIndex -= 3; }
  while (colorIndex < 1) { colorIndex += 3; }
  return curPalette[colorIndex];*/
  return curPalette[wrapValue(colorIndex, 1, 3)];
}


/**
 * If val is above a max value, it will wrap around to the min value
 * If a val is below the min value, it will wrap around to the max value
 */
short wrapValue(short val, short minVal, short maxVal) {
  while (val > maxVal) { val -= maxVal; }
  while (val < minVal) { val += maxVal; }
  return val;
}


/**
 * Alternates the isAlternateBassKick variable and drives the bass selection pins
 */
void alternateBassSel() {
  isAlternateBassKick = !isAlternateBassKick;
  writeBassSel(isAlternateBassKick);
}


/**
 * Writes the two bass light selection pins.
 * One must be high and the other low, otherwise we get no output when the enable pin is high.
 */
void writeBassSel(bool sel) {
  digitalWrite(PIN_BASS_SEL_1, sel);
  digitalWrite(PIN_BASS_SEL_2, !sel);
}


/**
 * Writes data to the cabinet lighting shift registers
 */
void writeCabLighting() {
  digitalWrite(PIN_LIGHTS_LAT, LOW); //Pull latch pin low, shift out data, and throw latch pin high again
  shiftOut(PIN_LIGHTS_DAT, PIN_LIGHTS_CLK, LSBFIRST, etcLEDs);
  shiftOut(PIN_LIGHTS_DAT, PIN_LIGHTS_CLK, LSBFIRST, cabLEDs);
  shiftOut(PIN_LIGHTS_DAT, PIN_LIGHTS_CLK, LSBFIRST, padLEDs);
  digitalWrite(PIN_LIGHTS_LAT, HIGH);
}


/**
 * Gets a CRGB color for the LED strip light test
 */
CRGB getLightTestStripColor() {
  switch (lightTestStripColor) {
    case 0:
      return CRGB::Red;
      break;
    case 1:
      return CRGB::Green;
      break;
    case 2:
      return CRGB::Blue;
      break;
    case 3:
      return CRGB::Cyan;
      break;
    case 4:
      return CRGB::Magenta;
      break;
    case 5:
      return CRGB::Yellow;
      break;
    case 6:
      return CRGB::White;
      break;
    default:
      return CRGB::Black;
  }
}




/**
 * Sets a new Visualization Effect to use
 */
void setNewVE(int effectId) {
  curEffect = effectId;
  if (curEffect < 1) { curEffect = MAX_VE; } //Wrap around effect values for extra sanity
  if (curEffect > MAX_VE) { curEffect = 1; }

  setNewPalette(curPaletteIndex); //Also update the related palette variables
}
 
/**
 * Visualization effect: FFT Debug
 * 
 * Used to test FFT section responsiveness, audio peak and FFT sections are mapped to specific pixels.
 */
void visualizeFFTDebug() {
  float bin = getFFTSection(0); //Map bass
  leds[6] = CHSV(160, 255, bin * 255);
  Serial.print(bin);
  Serial.print(",");
  bin = getFFTSection(1); //Map low
  leds[9] = CHSV(96, 255, bin * 255);
  Serial.print(bin);
  Serial.print(",");
  bin = getFFTSection(2); //Map mid
  leds[12] = CHSV(64, 255, bin * 255);
  Serial.print(bin);
  Serial.print(",");
  bin = getFFTSection(3); //Map high
  leds[15] = CHSV(0, 255, bin * 255);
  Serial.println(bin);
}


/**
* Visualization effect: Pulse
* 
* Bass grows from center of strip, mids grow from 1/4 and 3/4 points, highs grow from edge of strip.
* Bass kicks produce faint dots that scroll to the edge of the strip
*/
void visualizePulse() {
  short sectionSize[] = {0, 0, 0}; //Size of each visualization section in pixels
  sectionSize[0] = getFFTSection(0) * PULSE_MAX_SIZE_BASS;
  sectionSize[1] = getFFTSection(1) * PULSE_MAX_SIZE_MID;
  sectionSize[2] = getFFTSection(3) * PULSE_MAX_SIZE_HIGH;

  fadeToBlackBy(leds, STRIP_LENGTH, 90); //Fade out the last update from the strip a bit

  //Did the bass just kick? Start a new bass kick scroll from the end of the current bass section
  if (getBassKicked()) {
    bass_scroll_pos = sectionSize[0];
  }

  float bassKickProgress = getBassKickProgress() * 255; //Store the current bass kick progress too - We'll use it a bunch!

  //If we're scrolling a bass kick outwards, update it
  if (bass_scroll_pos >= 1) {
    bass_scroll_pos++;
    if (bass_scroll_pos > STRIP_HALF) { //We've scrolled past the end of the strip, reset to 0 to stop scrolling
      bass_scroll_pos = 0;
    } else { //Still scrolling, set an LED
      leds[STRIP_HALF - bass_scroll_pos] = bass_scroll_color;
    }
  }

  //fadeToBlackBy(leds, STRIP_LENGTH, 90); //Fade out the last update from the strip a bit

  //For each section to draw, iterate through the number of LEDs to draw. Then, find and add the offset to each LED to draw so it draws in the correct place on the strip
  //High offset: None - Start from 0 and increment upwards
  for (int i = 0; i < sectionSize[2]; i++) { //Set HIGH
    //leds[i] = curPalette[3]; //Static colors are boring, let's make them pulse to the beat!
    leds[i] = blend(curPaletteDim[3], curPalette[3], bassKickProgress); //If the bass is kicking, blend in a bit of a brighter version of the current palette
  }

  short sectionOffset = STRIP_FOURTH - sectionSize[1] / 2; //Mid offset: 1/4 point on the strip (the center for this section) minus half of this section's size (to draw half on each side of the 1/4 point)
  for (int i = 0; i < sectionSize[1]; i++) { //Set MID
    //leds[i + sectionOffset] = curPalette[1];
    leds[i + sectionOffset] = blend(curPaletteDim[1], curPalette[1], bassKickProgress);
  }

  sectionOffset = STRIP_HALF - sectionSize[0]; //Bass offset: 1/2 point on the strip minus this section's size
  for (int i = 0; i < sectionSize[0]; i++) { //Set BASS
    //leds[i + sectionOffset] = curPalette[0];
    leds[i + sectionOffset] = blend(curPaletteDim[0], curPalette[0], bassKickProgress);
  } 

  //Finally, mirror the first half of the strip to the second half
  mirrorStrip();
}


/**
 * Visualization effect: Punch
 * 
 * Modified version of Pulse to have 4 sections (bass, low, mid, high) instead of 3
 * Bass kicks exist, too
 */
void visualizePunch() {
  short sectionSize[] = {0, 0, 0, 0}; //Size of each visualization section in pixels
  sectionSize[0] = getFFTSection(0) * PUNCH_MAX_SIZE_BASS;
  sectionSize[1] = getFFTSection(1) * PUNCH_MAX_SIZE_LOW;
  sectionSize[2] = getFFTSection(2) * PUNCH_MAX_SIZE_MID;
  sectionSize[3] = getFFTSection(3) * PUNCH_MAX_SIZE_HIGH;

  fadeToBlackBy(leds, STRIP_LENGTH, 100); //Fade out the last update from the strip a bit

  //Did the bass just kick? Start a new bass kick scroll from the end of the current bass section
  if (getBassKicked()) {
    bass_scroll_pos = sectionSize[0];
    //bass_scroll_pos = 1;
  }

  float bassKickProgress = getBassKickProgress() * 255; //Store the current bass kick progress too - We'll use it a bunch!

  //If we're scrolling a bass kick outwards, update it
  if (bass_scroll_pos >= 1) {
    bass_scroll_pos++;
    if (bass_scroll_pos > STRIP_HALF) { //We've scrolled past the end of the strip, reset to 0 to stop scrolling
      bass_scroll_pos = 0;
    } else { //Still scrolling, set an LED
      leds[STRIP_HALF - bass_scroll_pos] = bass_scroll_color;
    }
  }

  /*Serial.print(sectionSize[0]);
  Serial.print(" ");
  Serial.println(bass_scroll_pos);*/

  //For each section to draw, iterate through the number of LEDs to draw. Then, find and add the offset to each LED to draw so it draws in the correct place on the strip
  short sectionOffset = STRIP_8TH + STRIP_16TH - sectionSize[2] / 2; //Mid: Offset 1/8 strip minus half this section's size
  for (int i = 0; i < sectionSize[2]; i++) { //Set MID
    //leds[i + sectionOffset] = curPalette[2];
    leds[i + sectionOffset] = blend(curPaletteDim[2], curPalette[2], bassKickProgress); //If the bass is kicking, blend in a bit of a brighter version of the current palette
  }

  sectionOffset = STRIP_8TH * 3 - STRIP_16TH - sectionSize[1] / 2; //Low: Offset 3/8 strip minus half this section's size
  for (int i = 0; i < sectionSize[1]; i++) { //Set LOW
    //leds[i + sectionOffset] = curPalette[1];
    leds[i + sectionOffset] = blend(curPaletteDim[1], curPalette[1], bassKickProgress);
  }

  //High: No offset - Start from 0 and increment upwards
  for (int i = 0; i < sectionSize[3]; i++) { //Set HIGH
    //leds[i] = curPalette[3];
    leds[i] = blend(curPaletteDim[3], curPalette[3], bassKickProgress);
  }
  
  sectionOffset = STRIP_HALF - sectionSize[0]; //Bass: Offset 1/2 strip minus this section's size
  for (int i = 0; i < sectionSize[0]; i++) { //Set BASS
    //leds[i + sectionOffset] = curPalette[0];
    leds[i + sectionOffset] = blend(curPaletteDim[0], curPalette[0], bassKickProgress);
  } 

  //Finally, mirror the first half of the strip to the second half
  mirrorStrip();
}


/**
 * Visualization effect: Kick
 * 
 * Three sections + bass, solid color for each. Bass is drawn proportionally to FFT size, other sections take up the rest of the strip, their sizes proportional to each other
 * Other sections always scrolling outward, speed varies depending on bass value
 */
void visualizeKick() {
  short sectionSize[] = {0, 0, 0, 0}; //Size of each visualization section (pixels)
  float rawSectionSize[] = {0, 0, 0, 0}; //Size of each section (0-1)
  sectionSize[0] = getFFTSection(0) * KICK_MAX_SIZE_BASS; //Bass section's size is directly proportional to it's FFT section's value
  rawSectionSize[1] = getFFTSection(1); //The other sections are proportional in size to each other - We'll sum their values and find out how big of a percentage of the sum each section is to find their size in pixels
  rawSectionSize[2] = getFFTSection(2);
  rawSectionSize[3] = getFFTSection(3);

  getBassKicked(); //Detect bass kick times (this value is unused, but it sets variables getBassKickProgress() requires, later)
  float bassKickProgress = getBassKickProgress();
  
  short curSection = 1; //Tracks the section we're drawing
  short curOffset = 0; //Tracks the first pixel of each non-bass section we draw

  fadeToBlackBy(leds, STRIP_LENGTH, 100); //Fade out the last update from the strip a bit

  //Serial.println(avgBassSize);

  float rawSectionSum = rawSectionSize[1] + rawSectionSize[2] + rawSectionSize[3]; //Let's find the percentage each non-bass section takes up (and each section's size in pixels)! First find the sum...
  //short nonBassSize = STRIP_HALF - avgBassSize; //...then find out how much non-bass area there is on the strip to draw these other sections to
  for (int i = 1; i <= 3; i++) {
    sectionSize[i] = (rawSectionSize[i] / rawSectionSum) * STRIP_FOURTH; //...then divide each section by the sum to find their percentage of the sum
    //Finally, multiply each section's size by the area on the strip to draw all the non-bass sections to to get how big each section is in pixels
  }


  //Is it time to update the scroll yet?
  if (curMillis >= lastScrollUpdateMillis + (bassKickProgress > 0 ? 2 : 250)) { //(Don't wait as long for an update if the bass isn't kicking)
    lastScrollUpdateMillis = curMillis; //Record the time this update happened at as reference for timing the next update
    //avgBassSize = 0;
    scrollOffset++;
    //scrollOffset += bassKickProgress > 0 ? 2 : 1; //Update the scroll offset - Offset it by more if we're kicking the bass
    if (scrollOffset > sectionSize[sectionOffset]) { //Scrolled an entire section's worth of pixels, now offset the section instead and reset the scroll
      sectionOffset--;
      scrollOffset = -sectionSize[wrapValue(sectionOffset, 1, 3)];
      if (sectionOffset <= 0) { sectionOffset = 3; } //Out of sections to offset, reset to the last section
    }
  }


  int i = 0; //i is used throughout the rest of this routine, declare/initialize it here
  curOffset = scrollOffset; //Start tracking the beginning of each section we're drawing...
  curSection = sectionOffset; //...and start drawing a new section based on the current section offset
  
  //Iterate through the strip half mark to find the color each strip pixel should be
  for (; i <= STRIP_HALF; i++) {
    /*Serial.print(i - curOffset);
    Serial.print("/");
    Serial.println(sectionSize[curSection]);*/
    if (i - curOffset < sectionSize[curSection]) { //In the middle of drawing a section
      leds[STRIP_HALF - i] = blend(curPaletteDim[curSection], curPalette[curSection], rawSectionSize[curSection] * 255); //Blend in a brighter version of this color depending on how loud this section is
      
    } else if (i - curOffset >= sectionSize[curSection]) { //This section has been fully drawn - skip an led and move onto the next section
      i += 2;
      curOffset = i; //Start drawing the next section where we are now
      curSection++;
      if (curSection >= 4) { //Drew all sections, reset to section 1
        curSection = 1;
      }
    }
    
  }


  //Now overlay the bass section on top of whatever we've drawn for the rest of the strip
  i = 0; //Start drawing at the center of the strip...
  for (; i <= sectionSize[0]; i++) { //And iterate through the LEDs to draw for the bass section
    //leds[STRIP_HALF - i] = blend(curPaletteDim[0], curPalette[0], bassKickProgress);
    leds[STRIP_HALF - i] = curPalette[0];
  }

  curOffset = i;
  for (; i <= curOffset + 2; i++) { //And also draw a few blank LEDs after the bass section
    leds[STRIP_HALF - i] = CRGB::Black;
  }
  
  //Finally, mirror the first half of the strip to the second half
  mirrorStrip();

  //DEBUG: Turn on/off an LED if the bass is kicking
  /*leds[30] = CRGB::Black;
  if (bassKickProgress > 1) { leds[30] = CHSV(96, 255, 255);}
  leds[31] = CRGB::Black; leds[29] = CRGB::Black;*/
  
}


/**
 * Visualization effect: Volume
 * 
 * Modified version of Pulse/Punch to stack FFT sections on top of each other for a "VU meter"-style effect
 * Bass is still mapped to the center, and bass kicks still scroll outwards
 */
void visualizeVolume() {
  short sectionSize[] = {0, 0, 0, 0}; //Size of each visualization section in pixels
  sectionSize[0] = getFFTSection(0) * VOLUME_MAX_SIZE_BASS;
  sectionSize[1] = getFFTSection(1) * VOLUME_MAX_SIZE_LOW;
  sectionSize[2] = getFFTSection(2) * VOLUME_MAX_SIZE_MID;
  sectionSize[3] = getFFTSection(3) * VOLUME_MAX_SIZE_HIGH;

  fadeToBlackBy(leds, STRIP_LENGTH, 95); //Fade out the last update from the strip a bit

  //Did the bass just kick? Start a new bass kick scroll from the end of the current bass section
  if (getBassKicked()) {
    bass_scroll_pos = sectionSize[0];
    //bass_scroll_pos = 1;
  }

  float bassKickProgress = getBassKickProgress() * 255; //Store the current bass kick progress too - We'll use it a bunch!

  //If we're scrolling a bass kick outwards, update it
  if (bass_scroll_pos >= 1) {
    bass_scroll_pos++;
    if (bass_scroll_pos > STRIP_HALF) { //We've scrolled past the end of the strip, reset to 0 to stop scrolling
      bass_scroll_pos = 0;
    } else { //Still scrolling, set an LED
      leds[STRIP_HALF - bass_scroll_pos] = bass_scroll_color;
    }
  }

  /*Serial.print(sectionSize[0]);
  Serial.print(" ");
  Serial.println(bass_scroll_pos);*/

  //For each section to draw, iterate through the number of LEDs to draw. Then, find and add the offset to each LED to draw so it draws in the correct place on the strip
  int i = 0; //First, we'll track the LED we're writing to as i - it'll be used for the first 3 strip sections to write to
  
  //Then for each FFT section, write a number of LEDs for that section based on how loud it is
  for (int j = 0; j < sectionSize[3]; j++) { //Set LOW
    i++;
    leds[i] = blend(curPaletteDim[3], curPalette[3], bassKickProgress); //If the bass is kicking, blend in a bit of a brighter version of the current palette
  }

  i += 3; //Let's also increment i between sections, to give gaps between the FFT sections
  for (int j = 0; j < sectionSize[2]; j++) { //Set MID
    i++;
    leds[i] = blend(curPaletteDim[2], curPalette[2], bassKickProgress);
  }

  i += 3;
  for (int j = 0; j < sectionSize[1]; j++) { //Set HIGH
    i++;
    leds[i] = blend(curPaletteDim[1], curPalette[1], bassKickProgress);
  }

  //The bass section should pulse out from the center of the strip instead of stacking with the others (idk why, is it cool???)
  for (int i = 0; i < sectionSize[0]; i++) { //Set BASS
    //leds[i + sectionOffset] = curPalette[0];
    leds[STRIP_HALF - i] = blend(curPaletteDim[0], curPalette[0], bassKickProgress);
  } 

  //Finally, mirror the first half of the strip to the second half
  mirrorStrip();
}



/**
 * Visualization effect: Pitch
 * 
 * Raw FFT bins are mapped as pixels on the strip. The strip fades between the current palette colors in a big gradient, and pulses to the beat
 */
void visualizePitch() {
  getBassKicked(); //Also update the bass kick detection
  //short bassKickProgress = getBassKickProgress() * 255;
  getBassKickProgress(); //And the bass kick progress - Neither are used here but updates values needed elsewhere

  /* Gradient handling: Smoothly fade between the four palette colors along the half strip we're working with
   * We'll divide the half of the strip we're working with into three sections (one for each transition between the four palette colors)
   * The beginning/end of each section marks a palette color to fade between, so fade between the two nearest colors inside each section
  */
  short sectionPos = 0;
  short section = 0;
  CHSV ledColor;

  short ledBrightnesses[STRIP_HALF]; //Also run through the FFT bins and set some LED brightnesses based on that
  pitchGetLEDBrightnesses(ledBrightnesses);
  
  for (int i = 0; i <= STRIP_HALF; i++) { //Iterate through the LEDs on the strip to set
    sectionPos++;
    if (sectionPos >= PITCH_SECTION_SIZE) { sectionPos = 0; section++; }
    ledColor = blend(curPalette[section], curPalette[section + 1], ((float)(sectionPos) / PITCH_SECTION_SIZE) * 255); //Now find the base color for this LED - Blend between the two palette colors we're between using the position into this "section" we're at
    ledColor.v = ledBrightnesses[i]; //Then change that color's value (brightness) based on how loud this pixel's set of FFT bins are
    leds[STRIP_HALF - i] = ledColor; //Finally, set this pixel and move onto the next
  }

  mirrorStrip();
}


/**
 * Returns a list of brightness (0-255) for each LED on the LED strip (for VE Pitch)
 * Accounts only for brightness changes from the LED's corresponding FFT bins
 */
void pitchGetLEDBrightnesses(short * ret) {
  //short ret[STRIP_HALF];

  /*Writing down my thought process to keep my sanity: We need an exponential curve of some sort to help map FFT bins to pixels. What kind of curve? I honestly have no idea
  But just to get something down imma use: max bin = 10^(a * pixel)
  To grab that a value, some substitution is used to calculate/store it in PITCH_BIN_EXP
  y = 10^(ax)

  We know a point on this curve - the max bin (y) should be at 511 (highest bin the audio library gives us) at the strip half point (x, using 45 here)
  511 = 10^(a * 45)

  Take the log of both sides and we get log(511) = a * 45 - Then just divide by STRIP_HALF and we'll have our value!
  
  PITCH_BIN_EXP = log(511) / STRIP_HALF

  If anyone actually knows what they're doing please submit a PR - 48
  */
  
  short lastMaxBin = 0;
  short newMaxBin = 0;
  for (int i = 0; i <= STRIP_HALF; i++) { //Find a brightness for each LED
    newMaxBin = pow(10, i * PITCH_BIN_EXP); //First, calculate the highest bin we should read for this pixel
    ret[i] = fft.read(lastMaxBin, newMaxBin) * 255 * 1.5; //Then, grab all FFT bins between the last bin read for the previous pixel, and our new max bin (* by 255 for an LED brightness)
    //Serial.print(ret[i]);
    //Serial.print(" ");
    //Serial.print(lastMaxBin);
    //Serial.print("-");
    //Serial.println(newMaxBin);
    lastMaxBin = newMaxBin + 1; //Then store our current max bin for the next pixel to use (add 1 so we don't use the same bin twice)
  }
  //Serial.println();
  //return ret;
}




/**
 * Idle effect: Rainbow Wave
 * 
 * Rainbowy waves of goodness wipe out and slowly recede in, like the ocean. Subsequent waves wipe farther out
 */
void idleRainbowWave() {
  fadeToBlackBy(leds, STRIP_LENGTH, 90); //Fade out the last update from the strip a bit
  short wavePos = abs(STRIP_HALF * sin(0.02 * idlePos)); //Where is the wave at during this update? Here it is!
  idleCurHue += 10 * sin(-0.04 * idlePos); //Calculate how far to move the strip-wide hue shift
  short huePos = idleCurHue; //And offset all further hues by this value
  
  for (int i = 0; i <= wavePos; i++) {
    huePos += 15; //Draw each LED on the strip, shifting the hue a tad for each LED
    leds[STRIP_HALF - i] = CHSV(huePos, 255, 255);
  }
  mirrorStrip(); //Finally, mirror the strip

  if (idlePos >= 785) { //Roughly equal to 250pi (5 cycles), which is when we should stop this idle animation
    idlePos = 0;
  }
}
