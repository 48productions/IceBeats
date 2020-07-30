/*****************************
 * ICEBEATS MUSIC VISUALIZER *
 ****************************/
 
#include "FastLED.h"

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include <Bounce2.h>
#include "RunningAverage.h"



/*****************
 * CONFIGURATION *
 ****************/
#define STRIP_LENGTH 60 //Number of LEDs in the strip
#define DATA_PIN_STRIP 6 //Pin connected to the LED strip

#define DEBUG_FFT_BINS true //Set true to test FFT section responsiveness - bin sections are mapped to the brightness of specific pixels



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

Bounce debug0 = Bounce(5, 5); //DEBUG 0: Swap palette
Bounce debug1 = Bounce(4, 5); //DEBUG 1: Force bass kick/Swap visualization effect
Bounce debug2 = Bounce(3, 5); //DEBUG 2: Force getBassKickProgress() (can also force all FFT section reads, uncomment below)

RunningAverage averagePeak(10); //The average measured peak, used for AGC
float curGain = 1; //Current gain to use, used for AGC


/***************************
 * VISUALIZATION VARIABLES *
 **************************/

enum VisualizationEffect { //A list of all visualization effects we can use - described in their individual functions
  VEDebugFFT,
  VEKick,
  VEPunch,
  VEPulse
};

const int MAX_VE = 3; //Maximum effect number we can use

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

RunningAverage bassPeakAverage(25); //Running average of detected bass peaks, used for bass kick detection
RunningAverage shortBassAverage(3); //Shorter running average of the bass, smoothes out the wrinkles so we get  s m o o t h  l i n e s
float lastShortBassAverage; //The short bass average last time it was checked, also used for bass kick detection
float lastBassChange; //The last bass change last time it was checked. You know the drill.
bool bassChangeIncreasing = true; //Is the bass change increasing or decreasing? Helps detect when it peaks.
unsigned long lastBassKickMillis = 0; //Last time a bass kick happened


short bass_scroll_pos = 0; //Pulse/punch effects - The bass kick scroll's position and color
CHSV bass_scroll_color;

short sectionOffset = 1; //Kick effect - How many indexes should we offset the sections?
short avgBassSize = 0; //Kick - The average size the bass is
short scrollOffset = 0; //Kick - How many pixels should we offset our drawing by to scroll the sections?
unsigned long nextScrollUpdateMillis = 0; //Kick - When did we last update the scroll?


unsigned long nextIdleStartMillis = 0; //The millis time to start the next idle animation at, calculated when the audio falls quiet or an idle anim starts
bool idling = false; //Are we currently idling? If so, don't render new visualization data on the strip
bool prevIdling = false; //Were we idling last time we checked if we were idling?



/********************
 * FUNCTIONS N SHIZ *
 *******************/


void setup() {
  AudioMemory(16);
  Serial.begin(9600);
  LEDS.addLeds<WS2812, DATA_PIN_STRIP, GRB>(leds, STRIP_LENGTH);
  LEDS.setBrightness(84);

  pinMode(3, INPUT_PULLUP); //Pinmode the debug buttons and debug light
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);

  pinMode(LED_BUILTIN, OUTPUT);

  setNewPalette(0); //Initialize palette-related variables

  bassPeakAverage.clear(); //Clear out running averages for the bass
  shortBassAverage.clear();
  bassPeakAverage.addValue(0); //And add some initial data to the peak average
  averagePeak.clear();
}


void loop() {
  debug0.update();
  debug1.update();
  debug2.update();

  if (debug0.rose()) { //Debug 0 pressed, swap to a new color palette
    setNewPalette(curPaletteIndex + 1);
  }

  if (debug1.rose()) { //Debug 1 pressed, swap to a new visualization effect
    setNewVE(curEffect + 1);
  }

  curMillis = millis();
  
  if (peak.available()) { //We have peak data to read! READ IT!
    float curPeak = peak.read();
    //Serial.println(peak);
    
    //First, determine if we should be starting an idle animation (audio has been quiet for a while)
    idling = (curPeak <= 0.05);
    if (idling == true && prevIdling == false) { //We just got a f a l l i n g  i d l i n g  e d g e (wat)
      nextIdleStartMillis = curMillis + 10000; //Start an idle anim in 10 seconds
    }

    if (idling) {
      if (curMillis >= nextIdleStartMillis) { //Time to start an idle animation!
        nextIdleStartMillis = curMillis + 60000; //Next idle animation should start in 60 seconds
      }
    }
    prevIdling = idling;


    //Now let's handle AGC - Theorhetically
    averagePeak.addValue(curPeak);
    
    if (averagePeak.getAverage() > 0.75) { //LOUD MUSIC
      curGain *= 0.98; //Reduce gain, update the amp!
      amp.gain(curGain);
      
    } else if (averagePeak.getAverage() < 0.75 && curGain <= 4) { //we quiet bois and we haven't gotten HYPER LOUD yet
      curGain *= 1.001; //Slightly increase gain, update amp
      amp.gain(curGain);
    }
    /*Serial.print(curPeak);
    Serial.print(" ");
    Serial.println(curGain);*/
    
    
    if (curEffect == VEDebugFFT) { //Debugging the FFT? Map the peak to the first pixel!
      leds[0] = CHSV(0, 0, curPeak * 255);
    }
  }

  if (fft.available()) { //FFT has new data, let's visualize it!
    //bassPeakAverage.addValue(getFFTSection(0));
    shortBassAverage.addValue(getFFTSection(0));
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
    }
    FastLED.show();
  }
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
 * The FFT bins are divided into sections, color organ style.
 * Each section corresponds to a set of frequencies, return them here.
 * Always returns value between 0 and 1
 */
float getFFTSection(int id) {
  //if (!debug2.read()) { return 1; } //Holding debug 2? Make all FFT sections return their max value
  
  switch (id) { //Switch based on the id of the section to get. Each Teensy FFT bin is 43Hz wide, and we get groups of bins for each section.
    case 0: //Bass bin - 129-172Hz
      return constrain(fft.read(1) * 2, 0, 1);
    case 1: //Low - 500-1.5kHz
      return constrain(fft.read(11, 34) / 1.25, 0, 1);
    case 2: //Mid - 1.5-5k
      return constrain(fft.read(35, 117) / 1.5, 0, 1);
    case 3: //High - 6k-12.04k
      return constrain(fft.read(140, 280), 0, 1);
    default:
      return 0;
  }
}


/**
 * Returns whether a bass kick has happened since the last call to getBassKicked()
 * Used for beat-based effects
 */
bool getBassKicked() {
  float curBassValue = shortBassAverage.getAverage(); //Get the current value (short average) and (longer) running average of the bass section
  float curBassPeakAverage = bassPeakAverage.getAverage();
  float curBassChange = abs((curBassValue - lastShortBassAverage)); //Also find out how much the bass value changed this read
  bool bassKicked = false;

  if (bassChangeIncreasing && curBassChange < lastBassChange) { //We were going up but are now going down, record a peak
    bassChangeIncreasing = false;
    if (lastBassChange >= curBassPeakAverage * 0.85) { //Our new bass peak should be either greater than or at least *mostly* close to the current peak average (within 80%) to get rid of some super tiny unimportant bass spikes
      bassPeakAverage.addValue(lastBassChange);
      bassKicked = true;
      
    } else if (curMillis >= lastBassKickMillis + 650) { //No bass kicks in a while, our peak average is probably too high, so let's try lowering it
      bassPeakAverage.addValue(curBassPeakAverage * 0.8);
    }
    
  } else if (!bassChangeIncreasing && curBassChange > lastBassChange) { //We were going down but are now going up, how fun
    bassChangeIncreasing = true;
  }
  
  lastBassChange = curBassChange; //Now store the last bass change and average for the next read
  lastShortBassAverage = curBassValue;
  
  Serial.print(curBassValue * 3); //DEBUG: Print the current bass bin's value
  Serial.print(" ");
  Serial.print(curBassPeakAverage * 20); //The average bass peak value
  Serial.print(" ");
  Serial.print(curBassPeakAverage * 0.85 * 20); //The threshold to trigger a bass kick (average peak * 0.85)
  Serial.print(" ");
  Serial.print(curBassChange * 20); //And the current amount the bass has changed this update
  Serial.print(" ");
  /*Serial.print(" ");
  Serial.println(abs((curBassValue - curBassAverage)));*/
  //if (((curBassValue >= curBassAverage * 1.3 && curBassValue >= 0.15) || debug1.rose()) && curMillis >= lastBassKickMillis + 300) { //If the bass section just increased a ton (over a certain amount) or the debug button was pressed, return TRUE if we've also had a certain amount of time pass since the last kick
  //if ((curBassValue >= curBassAverage * 1.3 && curBassValue >= 0.15) && curMillis >= lastBassKickMillis + 300) {
  //if (abs((curBassValue / curBassPeakAverage)) >= 1.4  && abs((curBassValue - curBassPeakAverage)) >= 0.15 && curMillis >= lastBassKickMillis + 300) { //The bass section is above the average, is above a certain "volume" threshold, and some time has passed since the last kick
  if (bassKicked && curMillis >= lastBassKickMillis + 200) { //We detected a new peak earlier and it's been a bit since a bass kick
    lastBassKickMillis = curMillis;
    Serial.println("6");
    return true;
  }
  Serial.println("0");
  return false;
}


/**
 * Returns true if the bass is currently kicking
 * Used for other beat-based effects
 */
float getBassKickProgress() {
  /*float curBassValue = getFFTSection(0); //Old implementation: Have we met similar criteria for a regular bass kick to happen?
  float curBassAverage = bassPeakAverage.getAverage();
  bool kicking = (curBassValue >= curBassAverage * 1.35 && curBassValue >= 0.15) || !debug2.read();*/
  //bool kicking = curMillis <= lastBassKickMillis + 150;

  //The current implementation is really just an extension of getBassKicked for effects that "pulse to the beat"
  //Right now, bass kicks arbitrarily "happen" for a certain amount of time (a.k.a. 150ms)
  //This function returns how close we are to the start of the 150ms window after the bass kick happens (1 - just started kick, 0 - 150ms has passed since kick)
  //Since this function pulls the time the last time getBassKicked() returned true, getBassKicked() MUST be called beforehand for this function to work!
  float kicking = 0;

  if (curMillis <= lastBassKickMillis + 200) { kicking = 1 - ((float)(curMillis - lastBassKickMillis) / 200); } //Only calculate this value if we're still in the middle of the bass kick (and guaranteed to not return less than 0)
  
  digitalWrite(LED_BUILTIN, kicking > 0); //Also light our debug LED while the bass kick is happening
  //Serial.println(kicking);
  return kicking;
}


/**
 * Sets a new color palette to use
 */
void setNewPalette(int paletteId) {
  curPaletteIndex = paletteId;
  if (curPaletteIndex < 0) { curPaletteIndex = MAX_PALETTE; } //Wrap around palette values in case we lazily just give this function "curPalette + 1"
  if (curPaletteIndex > MAX_PALETTE) { curPaletteIndex = 0; }
  memcpy(curPalette, palettes[curPaletteIndex], sizeof(curPalette)); //Copy the contents of the new palette to use to curPalette[]
  
  for (short i = 0; i <= 3; i++) { //Also set the current dim palette's values to be a dim version of the current palette
    curPaletteDim[i] = CHSV(curPalette[i].h, curPalette[i].s, curPalette[i].v * 0.6);
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
    leds[i] = blend(curPaletteDim[3], curPalette[3], getBassKickProgress()); //If the bass is kicking, blend in a bit of a brighter version of the current palette
  }

  short sectionOffset = STRIP_FOURTH - sectionSize[1] / 2; //Mid offset: 1/4 point on the strip (the center for this section) minus half of this section's size (to draw half on each side of the 1/4 point)
  for (int i = 0; i < sectionSize[1]; i++) { //Set MID
    //leds[i + sectionOffset] = curPalette[1];
    leds[i + sectionOffset] = blend(curPaletteDim[1], curPalette[1], getBassKickProgress());
  }

  sectionOffset = STRIP_HALF - sectionSize[0]; //Bass offset: 1/2 point on the strip minus this section's size
  for (int i = 0; i < sectionSize[0]; i++) { //Set BASS
    //leds[i + sectionOffset] = curPalette[0];
    leds[i + sectionOffset] = blend(curPaletteDim[0], curPalette[0], getBassKickProgress());
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
  }

  //If we're scrolling a bass kick outwards, update it
  if (bass_scroll_pos >= 1) {
    bass_scroll_pos++;
    if (bass_scroll_pos > STRIP_HALF) { //We've scrolled past the end of the strip, reset to 0 to stop scrolling
      bass_scroll_pos = 0;
    } else { //Still scrolling, set an LED
      leds[STRIP_HALF - bass_scroll_pos] = bass_scroll_color;
    }
  }

  //For each section to draw, iterate through the number of LEDs to draw. Then, find and add the offset to each LED to draw so it draws in the correct place on the strip
  short sectionOffset = STRIP_8TH + STRIP_16TH - sectionSize[2] / 2; //Mid: Offset 1/8 strip minus half this section's size
  for (int i = 0; i < sectionSize[2]; i++) { //Set MID
    //leds[i + sectionOffset] = curPalette[2];
    leds[i + sectionOffset] = blend(curPaletteDim[2], curPalette[2], getBassKickProgress()); //If the bass is kicking, blend in a bit of a brighter version of the current palette
  }

  sectionOffset = STRIP_8TH * 3 - STRIP_16TH - sectionSize[1] / 2; //Low: Offset 3/8 strip minus half this section's size
  for (int i = 0; i < sectionSize[1]; i++) { //Set LOW
    //leds[i + sectionOffset] = curPalette[1];
    leds[i + sectionOffset] = blend(curPaletteDim[1], curPalette[1], getBassKickProgress());
  }

  //High: No offset - Start from 0 and increment upwards
  for (int i = 0; i < sectionSize[3]; i++) { //Set HIGH
    //leds[i] = curPalette[3];
    leds[i] = blend(curPaletteDim[3], curPalette[3], getBassKickProgress());
  }
  
  sectionOffset = STRIP_HALF - sectionSize[0]; //Bass: Offset 1/2 strip minus this section's size
  for (int i = 0; i < sectionSize[0]; i++) { //Set BASS
    //leds[i + sectionOffset] = curPalette[0];
    leds[i + sectionOffset] = blend(curPaletteDim[0], curPalette[0], getBassKickProgress());
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
  float sectionSize[] = {0, 0, 0, 0}; //Size of each visualization section
  sectionSize[0] = getFFTSection(0) * KICK_MAX_SIZE_BASS; //Bass section's size is directly proportional to it's FFT section's value
  sectionSize[1] = getFFTSection(1); //The other sections are proportional in size to each other - We'll sum their values and find out how big of a percentage of the sum each section is to find their size in pixels
  sectionSize[2] = getFFTSection(2);
  sectionSize[3] = getFFTSection(3);

  getBassKicked(); //Detect bass kick times (this value is unused, but it sets variables getBassKickProgress() requires, later)

  short curSection = 1; //Tracks the section we're drawing
  short curOffset = 0; //Tracks the first pixel of each non-bass section we draw

  fadeToBlackBy(leds, STRIP_LENGTH, 100); //Fade out the last update from the strip a bit

  //Serial.println(avgBassSize);

  float sectionSum = sectionSize[1] + sectionSize[2] + sectionSize[3]; //Let's find the percentage each non-bass section takes up (and each section's size in pixels)! First find the sum...
  //short nonBassSize = STRIP_HALF - avgBassSize; //...then find out how much non-bass area there is on the strip to draw these other sections to
  for (int i = 1; i <= 3; i++) {
    sectionSize[i] = (sectionSize[i] / sectionSum) * STRIP_FOURTH; //...then divide each section by the sum to find their percentage of the sum
    //Finally, multiply each section's size by the area on the strip to draw all the non-bass sections to to get how big each section is in pixels
  }


  //Is it time to update the scroll yet?
  if (curMillis >= nextScrollUpdateMillis) {
    nextScrollUpdateMillis = curMillis + (getBassKickProgress() > 0 ? 2 : 175); //Find the next time to update the scroll at - don't wait as long if the bass isn't kicking
    //avgBassSize = 0;
    scrollOffset++;
    if (scrollOffset > sectionSize[sectionOffset]) { //Scrolled an entire section's worth of pixels, now offset the section instead and reset the scroll
      sectionOffset--;
      scrollOffset = -sectionSize[wrapValue(sectionOffset, 1, 3)];
      if (sectionOffset <= 0) { sectionOffset = 3; } //Out of sections to offset, reset to the last section
    }
  }


  //Step 1: Draw the bass section of the strip
  int i = 0; //i is used throughout the rest of this routine, declare/initialize it here
  /*for (; i <= sectionSize[0]; i++) {
    leds[STRIP_HALF - i] = curPalette[0];
  }

  if (i <= avgBassSize - 2) { //Step 2: If we're before the point to start drawing the non-bass sections, skip ahead to that point
    i = avgBassSize;
  } else { //Otherwise, skip a few pixels before drawing the next sections
    i += 3;
  }*/

  curOffset = scrollOffset; //Start tracking the beginning of each section we're drawing...
  curSection = sectionOffset; //...and start drawing a new section based on the current section offset
  
  //Step 3: Pick up where we left off and iterate through the strip half mark to find the color each strip pixel should be
  for (; i <= STRIP_HALF; i++) {
    /*Serial.print(i - curOffset);
    Serial.print("/");
    Serial.println(sectionSize[curSection]);*/
    if (i - curOffset < sectionSize[curSection]) { //In the middle of drawing a section
      leds[STRIP_HALF - i] = curPaletteDim[curSection];
      
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
  if (getBassKickProgress()) { leds[30] = CHSV(96, 255, 255);}
  leds[31] = CRGB::Black; leds[29] = CRGB::Black;*/
  
}
